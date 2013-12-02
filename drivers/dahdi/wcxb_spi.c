/*
 * wcxb SPI library
 *
 * Copyright (C) 2013 Digium, Inc.
 *
 * All rights reserved.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#define DEBUG

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <linux/io.h>

#include <dahdi/kernel.h>

#include "wcxb_spi.h"

#define BBIT(n) (1UL << (31UL - (n)))

#define SPISRR	0x40
#define SPISRR_RESET		0x0000000a /* Resets Device */

#define SPICR			0x60
#define SPICR_LSB_FIRST		BBIT(22) /* LSB First. 0=MSB first transfer */
#define SPICR_MASTER_INHIBIT	BBIT(23) /* Master Transaction Inhibit */
#define SPICR_SLAVE_SELECT	BBIT(24) /* Manual Slave Select Assert Enable */
#define SPICR_RX_FIFO_RESET	BBIT(25) /* Receive FIFO Reset */
#define SPICR_TX_FIFO_RESET	BBIT(26) /* Transmit FIFO Reset */
#define SPICR_CPHA		BBIT(27) /* Clock Phase */
#define SPICR_CPOL		BBIT(28) /* Clock Polarity 0=Active High */
#define SPICR_MASTER		BBIT(29) /* Master Enable */
#define SPICR_SPE		BBIT(30) /* SPI System Enable */
#define SPICR_LOOP		BBIT(31) /* Local Loopback Mode */

#define SPICR_START_TRANSFER	(SPICR_CPHA | SPICR_CPOL | \
				 SPICR_MASTER | SPICR_SPE)
#define SPICR_READY_TRANSFER	(SPICR_MASTER_INHIBIT | SPICR_START_TRANSFER)

#define SPISR			0x64     /* SPI Status Register */
#define SPISR_SLAVE_MODE_SEL	BBIT(26) /* Slave Mode Select Flag */
#define SPISR_MODF		BBIT(27) /* Mode-Fault Error Flag */
#define SPISR_TX_FULL		BBIT(28) /* Transmit FIFO Full */
#define SPISR_TX_EMPTY		BBIT(29) /* Transmit FIFO Empty */
#define SPISR_RX_FULL		BBIT(30) /* Receive FIFO Full */
#define SPISR_RX_EMPTY		BBIT(31) /* Receive FIFO Empty */

#define SPIDTR			0x68     /* SPI Data Transmit Register */
#define SPIDRR			0x6c     /* SPI Data Receive Register */

#define SPISSR			0x70     /* SPI Slave Select Register */

#undef SUCCESS
#define SUCCESS			0

struct wcxb_spi_master {
	struct device *parent;
	struct list_head message_queue;
	spinlock_t lock;
	void __iomem *base;
	struct wcxb_spi_message *cur_msg;
	struct wcxb_spi_transfer *cur_transfer;
	u16 bytes_left;
	u16 bytes_in_fifo;
	const u8 *cur_tx_byte;
	u8 *cur_rx_byte;
	u16 auto_cs:1;
};

static inline void _wcxb_assert_chip_select(struct wcxb_spi_master *master,
					   unsigned int cs)
{
	const int cs_mask = ~(1UL << cs);
	iowrite32be(cs_mask, master->base + SPISSR);
	ioread32be(master->base + SPISSR);
}

static inline void _wcxb_clear_chip_select(struct wcxb_spi_master *master)
{
	iowrite32be(~(0), master->base + SPISSR);
	ioread32(master->base + SPISSR);
}

static inline void wcxb_spi_reset_controller(struct wcxb_spi_master *master)
{
	u32 spicr = SPICR_READY_TRANSFER;
	spicr |= (master->auto_cs) ? 0 : SPICR_SLAVE_SELECT;
	iowrite32be(SPISRR_RESET, master->base + SPISRR);
	iowrite32be(spicr, master->base + SPICR);
	iowrite32be(0xffffffff, master->base + SPISSR);
}

struct wcxb_spi_master *wcxb_spi_master_create(struct device *parent,
					       void __iomem *membase,
					       bool auto_cs)
{
	struct wcxb_spi_master *master = NULL;
	master = kzalloc(sizeof(struct wcxb_spi_master), GFP_KERNEL);
	if (!master)
		goto error_exit;

	spin_lock_init(&master->lock);
	INIT_LIST_HEAD(&master->message_queue);
	master->base = membase;
	master->parent = parent;

	master->auto_cs = (auto_cs) ? 1 : 0;
	wcxb_spi_reset_controller(master);
	return master;

error_exit:
	kfree(master);
	return NULL;
}

void wcxb_spi_master_destroy(struct wcxb_spi_master *master)
{
	struct wcxb_spi_message *m;
	if (!master)
		return;
	while (!list_empty(&master->message_queue)) {
		m = list_first_entry(&master->message_queue,
				     struct wcxb_spi_message, node);
		list_del(&m->node);
		if (m->complete)
			m->complete(m->arg);
	}
	kfree(master);
	return;
}

static inline bool is_txfifo_empty(const struct wcxb_spi_master *master)
{
	return ((ioread32(master->base + SPISR) &
		 cpu_to_be32(SPISR_TX_EMPTY)) > 0);
}

static const u8 DUMMY_TX = 0xff;
static u8 DUMMY_RX;

static void _wcxb_spi_transfer_to_fifo(struct wcxb_spi_master *master)
{
	const unsigned int FIFO_SIZE = 16;
	u32 spicr;
	while (master->bytes_left && master->bytes_in_fifo < FIFO_SIZE) {
		iowrite32be(*master->cur_tx_byte, master->base + SPIDTR);
		master->bytes_in_fifo++;
		master->bytes_left--;
		if (&DUMMY_TX != master->cur_tx_byte)
			master->cur_tx_byte++;
	}

	spicr = (master->auto_cs) ? SPICR_START_TRANSFER :
				    SPICR_START_TRANSFER | SPICR_SLAVE_SELECT;
	iowrite32be(spicr, master->base + SPICR);
}

static void _wcxb_spi_transfer_from_fifo(struct wcxb_spi_master *master)
{
	u32 spicr;
	while (master->bytes_in_fifo) {
		*master->cur_rx_byte = ioread32be(master->base + SPIDRR);
		if (&DUMMY_RX != master->cur_rx_byte)
			master->cur_rx_byte++;
		--master->bytes_in_fifo;
	}

	spicr = SPICR_START_TRANSFER;
	spicr |= (master->auto_cs) ? 0 : SPICR_SLAVE_SELECT;
	iowrite32be(spicr | SPICR_MASTER_INHIBIT, master->base + SPICR);
}

static void _wcxb_spi_start_transfer(struct wcxb_spi_master *master,
				     struct wcxb_spi_transfer *t)
{
#ifdef DEBUG
	if (!t || !master || (!t->tx_buf && !t->rx_buf) ||
			master->cur_transfer) {
		WARN_ON(1);
		return;
	}
#endif

	master->cur_transfer = t;
	master->bytes_left = t->len;
	master->cur_tx_byte = (t->tx_buf) ?: &DUMMY_TX;
	master->cur_rx_byte = (t->rx_buf) ?: &DUMMY_RX;

	_wcxb_spi_transfer_to_fifo(master);
}

/**
 * _wcxb_spi_start_message - Start a new message transferring.
 *
 * Must be called with master->lock held.
 *
 */
static int _wcxb_spi_start_message(struct wcxb_spi_master *master,
				   struct wcxb_spi_message *message)
{
	struct wcxb_spi_transfer *t;

	if (master->cur_msg) {
		/* There is already a message in progress. Queue for later. */
		list_add_tail(&message->node, &master->message_queue);
		return 0;
	}

	if (!message->spi) {
		dev_dbg(master->parent,
			"Queueing message without SPI device specified?\n");
		return -EINVAL;
	};

	master->cur_msg = message;

	_wcxb_assert_chip_select(master, message->spi->chip_select);
	t = list_first_entry(&message->transfers,
			     struct wcxb_spi_transfer, node);
	_wcxb_spi_start_transfer(master, t);

	return 0;
}

/**
 * wcxb_spi_complete_message - Complete the current message.
 *
 * Called after all transfers in current message have been completed. This will
 * complete the current message and start the next queued message if there are
 * any.
 *
 * Must be called with the master->lock held.
 *
 */
static void _wcxb_spi_complete_cur_msg(struct wcxb_spi_master *master)
{
	struct wcxb_spi_message *message;
	if (!master->cur_msg)
		return;
	message = master->cur_msg;
	message->status = SUCCESS;
	_wcxb_clear_chip_select(master);
	master->cur_msg = NULL;
	if (!list_empty(&master->message_queue)) {
		message = list_first_entry(&master->message_queue,
					   struct wcxb_spi_message, node);
		list_del(&message->node);
		_wcxb_spi_start_message(master, message);
	}
	return;
}

static inline bool
_wcxb_spi_is_last_transfer(const struct wcxb_spi_transfer *t,
			   const struct wcxb_spi_message *message)
{
	return t->node.next == &message->transfers;
}

static inline struct wcxb_spi_transfer *
_wcxb_spi_next_transfer(struct wcxb_spi_transfer *t)
{
	return list_entry(t->node.next, struct wcxb_spi_transfer, node);
}

/**
 * wcxb_spi_handle_interrupt - Drives the transfers forward.
 *
 * Doesn't necessarily need to be called in the context of a real interrupt, but
 * should be called with interrupts disabled on the local CPU.
 *
 */
void wcxb_spi_handle_interrupt(struct wcxb_spi_master *master)
{
	struct wcxb_spi_message *msg;
	struct wcxb_spi_transfer *t;
	void (*complete)(void *arg) = NULL;
	unsigned long flags;

	/* Check if we're not in the middle of a transfer, or not finished with
	 * a part of one. */
	spin_lock_irqsave(&master->lock, flags);

	t = master->cur_transfer;
	msg = master->cur_msg;

	if (!msg || !is_txfifo_empty(master))
		goto done;

#ifdef DEBUG
	if (!t) {
		dev_dbg(master->parent,
			"No current transfer in %s\n", __func__);
		goto done;
	}
#endif

	/* First read any data out of the receive FIFO into the current
	 * transfer. */
	_wcxb_spi_transfer_from_fifo(master);
	if (master->bytes_left) {
		/* The current transfer isn't finished. */
		_wcxb_spi_transfer_to_fifo(master);
		goto done;
	}

	/* The current transfer is finished. Check for another transfer in this
	 * message or complete it and look for another message to start. */
	master->cur_transfer = NULL;

	if (_wcxb_spi_is_last_transfer(t, msg)) {
		complete = msg->complete;
		_wcxb_spi_complete_cur_msg(master);
	} else {
		t = _wcxb_spi_next_transfer(t);
		_wcxb_spi_start_transfer(master, t);
	}
done:
	spin_unlock_irqrestore(&master->lock, flags);
	/* Do not call the complete call back under the bus lock. */
	if (complete)
		complete(msg->arg);
	return;
}

int wcxb_spi_async(struct wcxb_spi_device *spi,
		   struct wcxb_spi_message *message)
{
	int res;
	unsigned long flags;
	WARN_ON(!spi || !message || !spi->master);

	if (list_empty(&message->transfers)) {
		/* No transfers in this message? */
		if (message->complete)
			message->complete(message->arg);
		message->status = -EINVAL;
		return 0;
	}
	message->status = -EINPROGRESS;
	message->spi = spi;
	spin_lock_irqsave(&spi->master->lock, flags);
	res = _wcxb_spi_start_message(spi->master, message);
	spin_unlock_irqrestore(&spi->master->lock, flags);
	return res;
}

static void wcxb_spi_complete_message(void *arg)
{
	complete((struct completion *)arg);
}

int wcxb_spi_sync(struct wcxb_spi_device *spi, struct wcxb_spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	WARN_ON(!spi || !spi->master);
	message->complete = wcxb_spi_complete_message;
	message->arg = &done;
	wcxb_spi_async(spi, message);

	/* TODO: There has got to be a better way to do this. */
	while (!try_wait_for_completion(&done)) {
		wcxb_spi_handle_interrupt(spi->master);
		cpu_relax();
	}
	return message->status;
}
