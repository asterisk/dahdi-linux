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

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 26)
#define HAVE_RATELIMIT
#include <linux/ratelimit.h>
#endif

#include <dahdi/kernel.h>

#include <stdbool.h>

#include "wcxb.h"
#include "wcxb_spi.h"
#include "wcxb_flash.h"

/* The definition for Surprise Down was added in Linux 3.6 in (a0dee2e PCI: misc
 * pci_reg additions). It may be backported though so we won't check for the
 * version. */
#ifndef PCI_ERR_UNC_SURPDN
#define PCI_ERR_UNC_SURPDN 0x20
#endif

/* FPGA Status definitions */
#define OCT_CPU_RESET		(1 << 0)
#define OCT_CPU_DRAM_CKE	(1 << 1)
#define STATUS_LED_GREEN	(1 << 9)
#define STATUS_LED_RED		(1 << 10)
#define FALC_CPU_RESET		(1 << 11)

/* Descriptor ring definitions */
#define DRING_SIZE		(1 << 7) /* Must be in multiples of 2 */
#define DRING_SIZE_MASK		(DRING_SIZE-1)
#define DESC_EOR		(1 << 0)
#define DESC_INT		(1 << 1)
#define DESC_OWN		(1 << 31)
#define DESC_DEFAULT_STATUS	0xdeadbeef
#define DMA_CHAN_SIZE		128

/* Echocan definitions */
#define OCT_OFFSET		(xb->membase + 0x10000)
#define OCT_CONTROL_REG		(OCT_OFFSET + 0)
#define OCT_DATA_REG		(OCT_OFFSET + 0x4)
#define OCT_ADDRESS_HIGH_REG	(OCT_OFFSET + 0x8)
#define OCT_ADDRESS_LOW_REG	(OCT_OFFSET + 0xa)
#define OCT_DIRECT_WRITE_MASK	0x3001
#define OCT_INDIRECT_READ_MASK	0x0101
#define OCT_INDIRECT_WRITE_MASK	0x3101


/* DMA definitions */
#define TDM_DRING_ADDR		0x2000
#define TDM_CONTROL		(TDM_DRING_ADDR + 0x4)
#define ENABLE_ECHOCAN_TDM	(1 << 0)
#define TDM_RECOVER_CLOCK	(1 << 1)
#define ENABLE_DMA		(1 << 2)
#define	DMA_RUNNING		(1 << 3)
#define DMA_LOOPBACK		(1 << 4)
#define AUTHENTICATED		(1 << 5)
#define TDM_VERSION		(TDM_DRING_ADDR + 0x24)

/* Interrupt definitions */
#define INTERRUPT_CONTROL	0x300
#define ISR			(INTERRUPT_CONTROL + 0x0)
#define IPR			(INTERRUPT_CONTROL + 0x4)
#define IER			(INTERRUPT_CONTROL + 0x8)
#define IAR			(INTERRUPT_CONTROL + 0xc)
#define SIE			(INTERRUPT_CONTROL + 0x10)
#define CIE			(INTERRUPT_CONTROL + 0x14)
#define IVR			(INTERRUPT_CONTROL + 0x18)
#define MER			(INTERRUPT_CONTROL + 0x1c)
#define MER_ME			(1<<0)
#define MER_HIE			(1<<1)
#define DESC_UNDERRUN		(1<<0)
#define DESC_COMPLETE		(1<<1)
#define OCT_INT			(1<<2)
#define FALC_INT		(1<<3)
#define SPI_INT			(1<<4)

#define FLASH_SPI_BASE 0x200

struct wcxb_hw_desc {
	volatile __be32 status;
	__be32 tx_buf;
	__be32 rx_buf;
	volatile __be32 control;
} __packed;

struct wcxb_meta_desc {
	void *tx_buf_virt;
	void *rx_buf_virt;
};

static inline bool wcxb_is_pcie(const struct wcxb *xb)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 33)
	return pci_is_pcie(xb->pdev);
#else
#ifndef WCXB_PCI_DEV_DOES_NOT_HAVE_IS_PCIE
	return (xb->pdev->is_pcie > 0);
#else
	return (xb->flags.is_pcie > 0);
#endif
#endif
}

static const unsigned int CLK_SRC_MASK = ((1 << 13) | (1 << 12) | (1 << 1));

enum wcxb_clock_sources wcxb_get_clksrc(struct wcxb *xb)
{
	static const u32 SELF = 0x0;
	static const u32 RECOVER = (1 << 1);
	static const u32 SLAVE  = (1 << 12) | (1 << 1);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase + TDM_CONTROL) & CLK_SRC_MASK;
	spin_unlock_irqrestore(&xb->lock, flags);

	if (SELF == reg)
		return WCXB_CLOCK_SELF;
	else if (RECOVER == reg)
		return WCXB_CLOCK_RECOVER;
	else if (SLAVE == reg)
		return WCXB_CLOCK_SLAVE;
	else
		WARN_ON(1);
	return WCXB_CLOCK_SELF;
}

void wcxb_set_clksrc(struct wcxb *xb, enum wcxb_clock_sources clksrc)
{
	unsigned long flags;
	u32 clkbits = 0;

	switch (clksrc) {
	case WCXB_CLOCK_RECOVER:
		if (xb->flags.drive_timing_cable)
			clkbits = (1<<13) | (1 << 1);
		else
			clkbits = (1 << 1);
		break;
	case WCXB_CLOCK_SELF:
		if (xb->flags.drive_timing_cable)
			clkbits = (1<<13);
		else
			clkbits = 0;
		break;
	case WCXB_CLOCK_SLAVE:
		/* When we're slave, do not ever drive the timing cable. */
		clkbits = (1<<12) | (1 << 1);
		break;
	};

	/* set new clock select */
	spin_lock_irqsave(&xb->lock, flags);
	if (!wcxb_is_stopped(xb)) {
		dev_err(&xb->pdev->dev, "ERROR: Cannot set clock source while DMA engine is running.\n");
	} else {
		u32 reg;
		reg = ioread32be(xb->membase + TDM_CONTROL);
		reg &= ~CLK_SRC_MASK;
		reg |= (clkbits & CLK_SRC_MASK);
		iowrite32be(reg, xb->membase + TDM_CONTROL);
	}
	spin_unlock_irqrestore(&xb->lock, flags);
}

void wcxb_enable_echocan(struct wcxb *xb)
{
	u32 reg;
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase + TDM_CONTROL);
	reg |= ENABLE_ECHOCAN_TDM;
	iowrite32be(reg, xb->membase + TDM_CONTROL);
	spin_unlock_irqrestore(&xb->lock, flags);
}

void wcxb_disable_echocan(struct wcxb *xb)
{
	u32 reg;
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase + TDM_CONTROL);
	reg &= ~ENABLE_ECHOCAN_TDM;
	iowrite32be(reg, xb->membase + TDM_CONTROL);
	spin_unlock_irqrestore(&xb->lock, flags);
}

void wcxb_reset_echocan(struct wcxb *xb)
{
	unsigned long flags;
	int reg;
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase);
	iowrite32be((reg & ~OCT_CPU_RESET), xb->membase);
	spin_unlock_irqrestore(&xb->lock, flags);

	msleep_interruptible(1);

	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase);
	iowrite32be((reg | OCT_CPU_RESET), xb->membase);
	spin_unlock_irqrestore(&xb->lock, flags);

	dev_dbg(&xb->pdev->dev, "Reset octasic\n");
}

bool wcxb_is_echocan_present(struct wcxb *xb)
{
	return 0x1 == ioread16be(OCT_CONTROL_REG);
}

void wcxb_enable_echocan_dram(struct wcxb *xb)
{
	unsigned long flags;
	int reg;
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase);
	iowrite32be((reg | OCT_CPU_DRAM_CKE), xb->membase);
	spin_unlock_irqrestore(&xb->lock, flags);
}

u16 wcxb_get_echocan_reg(struct wcxb *xb, u32 address)
{
	uint16_t highaddress = ((address >> 20) & 0xfff);
	uint16_t lowaddress = ((address >> 4) & 0xfffff);
	unsigned long stop = jiffies + HZ/10;
	unsigned long flags;
	u16 ret;

	spin_lock_irqsave(&xb->lock, flags);
	iowrite16be(highaddress, OCT_ADDRESS_HIGH_REG);
	iowrite16be(lowaddress, OCT_ADDRESS_LOW_REG);

	iowrite16be(OCT_INDIRECT_READ_MASK | ((address & 0xe) << 8),
				OCT_CONTROL_REG);
	do {
		ret = ioread16be(OCT_CONTROL_REG);
	} while ((ret & (1<<8)) && time_before(jiffies, stop));

	WARN_ON_ONCE(time_after_eq(jiffies, stop));

	ret = ioread16be(OCT_DATA_REG);
	spin_unlock_irqrestore(&xb->lock, flags);

	return ret;
}

void wcxb_set_echocan_reg(struct wcxb *xb, u32 address, u16 val)
{
	unsigned long flags;
	uint16_t ret;
	uint16_t highaddress = ((address >> 20) & 0xfff);
	uint16_t lowaddress = ((address >> 4) & 0xffff);
	unsigned long stop = jiffies + HZ/10;

	spin_lock_irqsave(&xb->lock, flags);
	iowrite16be(highaddress, OCT_ADDRESS_HIGH_REG);
	iowrite16be(lowaddress, OCT_ADDRESS_LOW_REG);

	iowrite16be(val, OCT_DATA_REG);
	iowrite16be(OCT_INDIRECT_WRITE_MASK | ((address & 0xe) << 8),
			OCT_CONTROL_REG);

	/* No write should take longer than 100ms */
	do {
		ret = ioread16be(OCT_CONTROL_REG);
	} while ((ret & (1<<8)) && time_before(jiffies, stop));
	spin_unlock_irqrestore(&xb->lock, flags);

	WARN_ON_ONCE(time_after_eq(jiffies, stop));
}

#ifdef HAVE_RATELIMIT
static DEFINE_RATELIMIT_STATE(_underrun_rl, DEFAULT_RATELIMIT_INTERVAL,
			      DEFAULT_RATELIMIT_BURST);
#endif

/* wcxb_reset_dring needs to be called with xb->lock held. */
static void _wcxb_reset_dring(struct wcxb *xb)
{
	int x;
	struct wcxb_meta_desc *mdesc;
	struct wcxb_hw_desc *hdesc = NULL;

	xb->dma_head = xb->dma_tail = 0;

	if (unlikely(xb->latency > DRING_SIZE)) {
#ifdef HAVE_RATELIMIT
		if (__ratelimit(&_underrun_rl)) {
#else
		if (printk_ratelimit()) {
#endif
			dev_info(&xb->pdev->dev,
				 "Oops! Tried to increase latency past buffer size.\n");
		}
		xb->latency = DRING_SIZE;
	}

	for (x = 0; x < xb->latency; x++) {
		dma_addr_t dma_tmp;

		mdesc = &xb->meta_dring[x];
		hdesc = &xb->hw_dring[x];

		hdesc->status = cpu_to_be32(DESC_DEFAULT_STATUS);
		if (!mdesc->tx_buf_virt) {
			mdesc->tx_buf_virt =
				dma_pool_alloc(xb->pool, GFP_ATOMIC, &dma_tmp);
			hdesc->tx_buf = cpu_to_be32(dma_tmp);
			mdesc->rx_buf_virt =
				dma_pool_alloc(xb->pool, GFP_ATOMIC, &dma_tmp);
			hdesc->rx_buf = cpu_to_be32(dma_tmp);
		}
		hdesc->control = cpu_to_be32(DESC_INT|DESC_OWN);
		BUG_ON(!mdesc->tx_buf_virt || !mdesc->rx_buf_virt);
	}

	BUG_ON(!hdesc);
	/* Set end of ring bit in last descriptor to force hw to loop around */
	hdesc->control |= cpu_to_be32(DESC_EOR);
	iowrite32be(xb->hw_dring_phys, xb->membase + TDM_DRING_ADDR);
}

static void wcxb_handle_dma(struct wcxb *xb)
{
	struct wcxb_meta_desc *mdesc;

	while (!(xb->hw_dring[xb->dma_tail].control & cpu_to_be32(DESC_OWN))) {
		u_char *frame;

		mdesc = &xb->meta_dring[xb->dma_tail];
		frame = mdesc->rx_buf_virt;

		xb->ops->handle_receive(xb, frame);

		xb->dma_tail =
			(xb->dma_tail == xb->latency-1) ? 0 : xb->dma_tail + 1;

		mdesc = &xb->meta_dring[xb->dma_head];
		frame = mdesc->tx_buf_virt;

		xb->ops->handle_transmit(xb, frame);

		wmb();
		xb->hw_dring[xb->dma_head].control |= cpu_to_be32(DESC_OWN);
		xb->dma_head =
			(xb->dma_head == xb->latency-1) ? 0 : xb->dma_head + 1;
	}
}

static irqreturn_t _wcxb_isr(int irq, void *dev_id)
{
	struct wcxb *xb = dev_id;
	unsigned int limit = 8;
	u32 pending;

	pending = ioread32be(xb->membase + ISR);
	if (!pending)
		return IRQ_NONE;

	do {
		iowrite32be(pending, xb->membase + IAR);

		if (pending & DESC_UNDERRUN) {
			u32 reg;

			/* bump latency */
			spin_lock(&xb->lock);

			if (!xb->flags.latency_locked) {
				xb->latency++;

#ifdef HAVE_RATELIMIT
				if (__ratelimit(&_underrun_rl)) {
#else
				if (printk_ratelimit()) {
#endif
					dev_info(&xb->pdev->dev,
						 "Underrun detected by hardware. Latency bumped to: %dms\n",
						 xb->latency);
				}
			}

			/* re-setup dma ring */
			_wcxb_reset_dring(xb);

			/* set dma enable bit */
			reg = ioread32be(xb->membase + TDM_CONTROL);
			reg |= ENABLE_DMA;
			iowrite32be(reg, xb->membase + TDM_CONTROL);

			spin_unlock(&xb->lock);
		}

		if (pending & DESC_COMPLETE) {
			xb->framecount++;
			wcxb_handle_dma(xb);
		}

		if (NULL != xb->ops->handle_interrupt)
			xb->ops->handle_interrupt(xb, pending);

		pending = ioread32be(xb->membase + ISR);
	} while (pending && --limit);
	return IRQ_HANDLED;
}

DAHDI_IRQ_HANDLER(wcxb_isr)
{
	irqreturn_t ret;
	unsigned long flags;
	local_irq_save(flags);
	ret = _wcxb_isr(irq, dev_id);
	local_irq_restore(flags);
	return ret;
}

static int wcxb_alloc_dring(struct wcxb *xb, const char *board_name)
{
	xb->meta_dring =
		kzalloc(sizeof(struct wcxb_meta_desc) * DRING_SIZE,
		GFP_KERNEL);
	if (!xb->meta_dring)
		return -ENOMEM;

	xb->hw_dring = dma_alloc_coherent(&xb->pdev->dev,
			sizeof(struct wcxb_hw_desc) * DRING_SIZE,
			&xb->hw_dring_phys,
			GFP_KERNEL);
	if (!xb->hw_dring) {
		kfree(xb->meta_dring);
		return -ENOMEM;
	}

	xb->pool = dma_pool_create(board_name, &xb->pdev->dev,
			 PAGE_SIZE, PAGE_SIZE, 0);
	if (!xb->pool) {
		kfree(xb->meta_dring);
		dma_free_coherent(&xb->pdev->dev,
			sizeof(struct wcxb_hw_desc) * DRING_SIZE,
			xb->hw_dring,
			xb->hw_dring_phys);
		return -ENOMEM;
	}
	return 0;
}

/**
 * wcxb_soft_reset - Set interface registers back to known good values.
 *
 * This represents the normal default state after a reset of the FPGA. This
 * function is preferred over the hard reset function.
 *
 */
static void wcxb_soft_reset(struct wcxb *xb)
{
	/* digium_gpo */
	iowrite32be(0x0, xb->membase);

	/* xps_intc */
	iowrite32be(0x0, xb->membase + 0x300);
	iowrite32be(0x0, xb->membase + 0x308);
	iowrite32be(0x0, xb->membase + 0x310);
	iowrite32be(0x0, xb->membase + 0x31C);

	/* xps_spi_config_flash */
	iowrite32be(0xA, xb->membase + 0x200);

	/* tdm engine */
	iowrite32be(0x0, xb->membase + 0x2000);
	iowrite32be(0x0, xb->membase + 0x2004);
}

static void _wcxb_hard_reset(struct wcxb *xb)
{
	struct pci_dev *const pdev = xb->pdev;
	u32 microblaze_version;
	unsigned long stop_time = jiffies + msecs_to_jiffies(2000);

	pci_save_state(pdev);
	iowrite32be(0xe00, xb->membase + TDM_CONTROL);

	/* This sleep is to give FPGA time to bring up the PCI/PCIe interface */
	msleep(200);

	pci_restore_state(pdev);

	/* Wait for the Microblaze CPU to complete it's startup */
	do {
		msleep(20);
		/* Can return either 0xffff or 0 before it's fully booted */
		microblaze_version = ioread32be(xb->membase + 0x2018) ?: 0xffff;
	} while (time_before(jiffies, stop_time)
			&& 0xffff == microblaze_version);
}

/*
 * Since the FPGA hard reset drops the PCIe link we need to disable
 * error reporting on the upsteam link. Otherwise Surprise Down errors
 * may be reported in reponse to the link going away.
 *
 * NOTE: We cannot use pci_disable_pcie_error_reporting() because it will not
 * disable error reporting if the system firmware is attached to the advanced
 * error reporting mechanism.
 */
static void _wcxb_pcie_hard_reset(struct wcxb *xb)
{
	struct pci_dev *const parent = xb->pdev->bus->self;
	u32 aer_mask;
	int pos;

	if (!wcxb_is_pcie(xb))
		return;

	pos = pci_find_ext_capability(parent, PCI_EXT_CAP_ID_ERR);
	if (pos) {
		pci_read_config_dword(parent, pos + PCI_ERR_UNCOR_MASK,
				      &aer_mask);
		pci_write_config_dword(parent, pos + PCI_ERR_UNCOR_MASK,
				       aer_mask | PCI_ERR_UNC_SURPDN);
	}

	_wcxb_hard_reset(xb);

	if (pos) {
		pci_write_config_dword(parent, pos + PCI_ERR_UNCOR_MASK,
				       aer_mask);

		/* Clear the error as well from the status register. */
		pci_write_config_dword(parent, pos + PCI_ERR_UNCOR_STATUS,
				       PCI_ERR_UNC_SURPDN);
	}

	return;
}

/**
 * wcxb_hard_reset - Reset FPGA and reload firmware.
 *
 * This may be called in the context of device probe and therefore the PCI
 * device may be locked.
 *
 */
static void wcxb_hard_reset(struct wcxb *xb)
{
	if (wcxb_is_pcie(xb))
		_wcxb_pcie_hard_reset(xb);
	else
		_wcxb_hard_reset(xb);
}

int wcxb_init(struct wcxb *xb, const char *board_name, u32 int_mode)
{
	int res = 0;
	struct pci_dev *pdev = xb->pdev;
	u32 tdm_control;

	if (pci_enable_device(pdev))
		return -EIO;

	pci_set_master(pdev);

#ifdef WCXB_PCI_DEV_DOES_NOT_HAVE_IS_PCIE
	xb->flags.is_pcie = pci_find_capability(pdev, PCI_CAP_ID_EXP) ? 1 : 0;
#endif

	WARN_ON(!pdev);
	if (!pdev)
		return -EINVAL;

	xb->latency = WCXB_DEFAULT_LATENCY;
	spin_lock_init(&xb->lock);

	xb->membase = pci_iomap(pdev, 0, 0);
	if (pci_request_regions(pdev, board_name))
		dev_info(&xb->pdev->dev, "Unable to request regions\n");

	wcxb_soft_reset(xb);

	res = wcxb_alloc_dring(xb, board_name);
	if (res) {
		dev_err(&xb->pdev->dev,
			"Failed to allocate descriptor rings.\n");
		goto fail_exit;
	}

	/* Enable writes to fpga status register */
	iowrite32be(0, xb->membase + 0x04);

	xb->flags.have_msi = (int_mode) ? 0 : (0 == pci_enable_msi(pdev));

	if (request_irq(pdev->irq, wcxb_isr,
			(xb->flags.have_msi) ? 0 : DAHDI_IRQ_SHARED,
			board_name, xb)) {
		dev_notice(&xb->pdev->dev, "Unable to request IRQ %d\n",
			   pdev->irq);
		res = -EIO;
		goto fail_exit;
	}

	iowrite32be(0, xb->membase + TDM_CONTROL);
	tdm_control = ioread32be(xb->membase + TDM_CONTROL);
	if (!(tdm_control & 0x20)) {
		dev_err(&xb->pdev->dev,
			"This board is not authenticated and may not function properly.\n");
		msleep(1000);
	} else {
		dev_dbg(&xb->pdev->dev, "Authenticated. %08x\n", tdm_control);
	}

	return res;
fail_exit:
	pci_release_regions(xb->pdev);
	return res;
}

void wcxb_stop_dma(struct wcxb *xb)
{
	unsigned long flags;
	u32 reg;

	/* Quiesce DMA engine interrupts */
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase + TDM_CONTROL);
	reg &= ~ENABLE_DMA;
	iowrite32be(reg, xb->membase + TDM_CONTROL);
	spin_unlock_irqrestore(&xb->lock, flags);
}

int wcxb_wait_for_stop(struct wcxb *xb, unsigned long timeout_ms)
{
	unsigned long stop;
	stop = jiffies + msecs_to_jiffies(timeout_ms);
	do {
		if (time_after(jiffies, stop))
			return -EIO;
		else
			cpu_relax();
	} while (!wcxb_is_stopped(xb));

	return 0;
}

void wcxb_disable_interrupts(struct wcxb *xb)
{
	iowrite32be(0, xb->membase + IER);
}

void wcxb_stop(struct wcxb *xb)
{
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	/* Stop everything */
	iowrite32be(0, xb->membase + TDM_CONTROL);
	iowrite32be(0, xb->membase + IER);
	iowrite32be(0, xb->membase + MER);
	iowrite32be(-1, xb->membase + IAR);
	/* Flush quiesce commands before exit */
	ioread32be(xb->membase);
	spin_unlock_irqrestore(&xb->lock, flags);
	synchronize_irq(xb->pdev->irq);
}

bool wcxb_is_stopped(struct wcxb *xb)
{
	return !(ioread32be(xb->membase + TDM_CONTROL) & DMA_RUNNING);
}

static void wcxb_free_dring(struct wcxb *xb)
{
	struct wcxb_meta_desc *mdesc;
	struct wcxb_hw_desc *hdesc;
	int i;

	/* Free tx/rx buffs */
	for (i = 0; i < DRING_SIZE; i++) {
		mdesc = &xb->meta_dring[i];
		hdesc = &xb->hw_dring[i];
		if (mdesc->tx_buf_virt) {
			dma_pool_free(xb->pool,
					mdesc->tx_buf_virt,
					be32_to_cpu(hdesc->tx_buf));
			dma_pool_free(xb->pool,
					mdesc->rx_buf_virt,
					be32_to_cpu(hdesc->rx_buf));
		}
	}

	dma_pool_destroy(xb->pool);
	dma_free_coherent(&xb->pdev->dev,
		sizeof(struct wcxb_hw_desc) * DRING_SIZE,
		xb->hw_dring,
		xb->hw_dring_phys);
	kfree(xb->meta_dring);
}

void wcxb_release(struct wcxb *xb)
{
	wcxb_stop(xb);
	synchronize_irq(xb->pdev->irq);
	free_irq(xb->pdev->irq, xb);
	if (xb->flags.have_msi)
		pci_disable_msi(xb->pdev);
	if (xb->membase)
		pci_iounmap(xb->pdev, xb->membase);
	wcxb_free_dring(xb);
	pci_release_regions(xb->pdev);
	pci_disable_device(xb->pdev);
	return;
}

int wcxb_start(struct wcxb *xb)
{
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&xb->lock, flags);
	_wcxb_reset_dring(xb);
	/* Enable hardware interrupts */
	iowrite32be(-1, xb->membase + IAR);
	iowrite32be(DESC_UNDERRUN|DESC_COMPLETE, xb->membase + IER);
	/* iowrite32be(0x3f7, xb->membase + IER); */
	iowrite32be(MER_ME|MER_HIE, xb->membase + MER);

	/* Start the DMA engine processing. */
	reg = ioread32be(xb->membase + TDM_CONTROL);
	reg |= ENABLE_DMA;
	iowrite32be(reg, xb->membase + TDM_CONTROL);

	spin_unlock_irqrestore(&xb->lock, flags);

	return 0;
}

struct wcxb_meta_block {
	__le32 chksum;
	__le32 version;
	__le32 size;
} __packed;

struct wcxb_firm_header {
	u8	header[6];
	__le32	chksum;
	u8	pad[18];
	__le32	version;
} __packed;

static u32 wcxb_get_firmware_version(struct wcxb *xb)
{
	u32 version = 0;

	/* Two version registers are read and catenated into one */
	/* Firmware version goes in bits upper byte */
	version = ((ioread32be(xb->membase + 0x400) & 0xffff)<<16);

	/* Microblaze version goes in lower word */
	version += ioread32be(xb->membase + 0x2018);

	return version;
}

static int wcxb_update_firmware(struct wcxb *xb, const struct firmware *fw,
				const char *filename)
{
	u32 tdm_control;
	static const int APPLICATION_ADDRESS = 0x200000;
	static const int META_BLOCK_OFFSET   = 0x170000;
	static const int ERASE_BLOCK_SIZE    = 0x010000;
	static const int END_OFFSET = APPLICATION_ADDRESS + META_BLOCK_OFFSET +
			 ERASE_BLOCK_SIZE;
	struct wcxb_spi_master *flash_spi_master;
	struct wcxb_spi_device *flash_spi_device;
	struct wcxb_meta_block meta;
	int offset;
	struct wcxb_firm_header *head = (struct wcxb_firm_header *)(fw->data);

	if (fw->size > (META_BLOCK_OFFSET + sizeof(*head))) {
		dev_err(&xb->pdev->dev,
			"Firmware is too large to fit in available space.\n");

		return -EINVAL;
	}

	meta.size = cpu_to_le32(fw->size);
	meta.version = head->version;
	meta.chksum = head->chksum;

	flash_spi_master = wcxb_spi_master_create(&xb->pdev->dev,
						  xb->membase + FLASH_SPI_BASE,
						  false);

	flash_spi_device = wcxb_spi_device_create(flash_spi_master, 0);

	dev_info(&xb->pdev->dev,
		"Uploading %s. This can take up to 30 seconds.\n", filename);


	/* First erase all the blocks in the application area. */
	offset = APPLICATION_ADDRESS;
	while (offset < END_OFFSET) {
		wcxb_flash_sector_erase(flash_spi_device, offset);
		offset += ERASE_BLOCK_SIZE;
	}

	/* Then write the new firmware file. */
	wcxb_flash_write(flash_spi_device, APPLICATION_ADDRESS,
			 &fw->data[sizeof(struct wcxb_firm_header)],
			 fw->size - sizeof(struct wcxb_firm_header));

	/* Finally, update the meta block. */
	wcxb_flash_write(flash_spi_device,
			 APPLICATION_ADDRESS + META_BLOCK_OFFSET,
			 &meta, sizeof(meta));

	/* Reset fpga after loading firmware */
	dev_info(&xb->pdev->dev, "Firmware load complete. Reseting device.\n");
	tdm_control = ioread32be(xb->membase + TDM_CONTROL);

	wcxb_hard_reset(xb);

	iowrite32be(0, xb->membase + 0x04);
	iowrite32be(tdm_control, xb->membase + TDM_CONTROL);

	wcxb_spi_device_destroy(flash_spi_device);
	wcxb_spi_master_destroy(flash_spi_master);
	return 0;
}

int wcxb_check_firmware(struct wcxb *xb, const u32 expected_version,
			const char *firmware_filename, bool force_firmware)
{
	const struct firmware *fw;
	const struct wcxb_firm_header *header;
	int res = 0;
	u32 crc;
	u32 version = 0;

	version = wcxb_get_firmware_version(xb);

	if (0xff000000 == (version & 0xff000000)) {
		dev_info(&xb->pdev->dev,
			 "Invalid firmware %x. Please check your hardware.\n",
			 version);
		return -EIO;
	}

	if ((expected_version == version) && !force_firmware) {
		dev_info(&xb->pdev->dev, "Firmware version: %x\n", version);
		return 0;
	}

	if (force_firmware) {
		dev_info(&xb->pdev->dev,
			"force_firmware module parameter is set. Forcing firmware load, regardless of version\n");
	} else {
		dev_info(&xb->pdev->dev,
			"Firmware version %x is running, but we require version %x.\n",
			version, expected_version);
	}

	res = request_firmware(&fw, firmware_filename, &xb->pdev->dev);
	if (res) {
		dev_info(&xb->pdev->dev,
			"Firmware '%s' not available from userspace.\n",
			firmware_filename);
		goto cleanup;
	}

	header = (const struct wcxb_firm_header *)fw->data;

	/* Check the crc */
	crc = crc32(~0, &fw->data[10], fw->size - 10) ^ ~0;
	if (memcmp("DIGIUM", header->header, sizeof(header->header)) ||
		 (le32_to_cpu(header->chksum) != crc)) {
		dev_info(&xb->pdev->dev,
			"%s is invalid. Please reinstall.\n",
			firmware_filename);
		goto cleanup;
	}

	/* Check the file vs required firmware versions */
	if (le32_to_cpu(header->version) != expected_version) {
		dev_err(&xb->pdev->dev,
			"Existing firmware file %s is version %x, but we require %x. Please install the correct firmware file.\n",
			firmware_filename, le32_to_cpu(header->version),
			expected_version);
		res = -EIO;
		goto cleanup;
	}

	dev_info(&xb->pdev->dev, "Found %s (version: %x) Preparing for flash\n",
				firmware_filename, header->version);

	res = wcxb_update_firmware(xb, fw, firmware_filename);

	version = wcxb_get_firmware_version(xb);
	dev_info(&xb->pdev->dev, "Reset into firmware version: %x\n", version);

	if ((expected_version != version) && !force_firmware) {
		/* On the off chance that the interface is in a state where it
		 * cannot boot into the updated firmware image, power cycling
		 * the card can recover. A simple "reset" of the computer is not
		 * sufficient, power has to be removed completely. */
		dev_err(&xb->pdev->dev,
			"The wrong firmware is running after update. Please power cycle and try again.\n");
		res = -EIO;
		goto cleanup;
	}

	if (res) {
		dev_info(&xb->pdev->dev,
			 "Failed to load firmware %s\n", firmware_filename);
	}

cleanup:
	release_firmware(fw);
	return res;
}
