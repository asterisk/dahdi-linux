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

#ifndef __WCXB_H__
#define __WCXB_H__

#define WCXB_DEFAULT_LATENCY	3U
#define WCXB_DEFAULT_MAXLATENCY 20U
#define WCXB_DMA_CHAN_SIZE	128

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
/* The is_pcie member was backported but I'm not sure in which version. */
#  ifndef RHEL_RELEASE_VERSION
#define WCXB_PCI_DEV_DOES_NOT_HAVE_IS_PCIE
#  endif
#else
#endif

struct wcxb;

struct wcxb_operations {
	void (*handle_receive)(struct wcxb *xb, void *frame);
	void (*handle_transmit)(struct wcxb *xb, void *frame);
	void (*handle_error)(struct wcxb *xb);
	void (*handle_interrupt)(struct wcxb *xb, u32 pending);
};

struct wcxb_meta_desc;
struct wcxb_hw_desc;

struct wcxb {
	struct pci_dev			*pdev;
	spinlock_t			lock;
	const struct wcxb_operations	*ops;
	unsigned int			*debug;
	unsigned int			max_latency;
	unsigned int			latency;
	struct {
		u32	have_msi:1;
		u32	latency_locked:1;
		u32	drive_timing_cable:1;
#ifdef WCXB_PCI_DEV_DOES_NOT_HAVE_IS_PCIE
		u32	is_pcie:1;
#endif
	} flags;
	void __iomem			*membase;
	struct wcxb_meta_desc		*meta_dring;
	struct wcxb_hw_desc		*hw_dring;
	unsigned int			dma_head;
	unsigned int			dma_tail;
	dma_addr_t			hw_dring_phys;
	struct dma_pool			*pool;
	unsigned long			framecount;
};

extern int wcxb_init(struct wcxb *xb, const char *board_name, u32 int_mode);
extern void wcxb_release(struct wcxb *xb);
extern int wcxb_start(struct wcxb *xb);
extern void wcxb_stop(struct wcxb *xb);
extern int wcxb_wait_for_stop(struct wcxb *xb, unsigned long timeout_ms);
extern bool wcxb_is_stopped(struct wcxb *xb);

enum wcxb_clock_sources {
	WCXB_CLOCK_SELF,	/* Use the internal oscillator for timing. */
	WCXB_CLOCK_RECOVER,	/* Recover the clock from a framer. */
#ifdef RPC_RCLK
	WCXB_CLOCK_RECOVER_ALT,	/* Recover the clock from a framer. */
#endif
	WCXB_CLOCK_SLAVE	/* Recover clock from any timing header. */
};

extern enum wcxb_clock_sources wcxb_get_clksrc(struct wcxb *xb);
extern void wcxb_set_clksrc(struct wcxb *xb, enum wcxb_clock_sources clksrc);

static inline void wcxb_enable_timing_header_driver(struct wcxb *xb)
{
	xb->flags.drive_timing_cable = 1;
}
static inline bool wcxb_is_timing_header_driver_enabled(struct wcxb *xb)
{
	return 1 == xb->flags.drive_timing_cable;
}

static inline void wcxb_disable_timing_header_driver(struct wcxb *xb)
{
	xb->flags.drive_timing_cable = 0;
}

extern int wcxb_check_firmware(struct wcxb *xb, const u32 expected_version,
			       const char *firmware_filename,
			       bool force_firmware);
extern void wcxb_stop_dma(struct wcxb *xb);
extern void wcxb_disable_interrupts(struct wcxb *xb);

static inline void wcxb_gpio_set(struct wcxb *xb, u32 bits)
{
	u32 reg;
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase);
	iowrite32be(reg | bits, xb->membase);
	spin_unlock_irqrestore(&xb->lock, flags);
}

static inline void wcxb_gpio_clear(struct wcxb *xb, u32 bits)
{
	u32 reg;
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	reg = ioread32be(xb->membase);
	iowrite32be(reg & (~bits), xb->membase);
	spin_unlock_irqrestore(&xb->lock, flags);
}

static inline void
wcxb_set_maxlatency(struct wcxb *xb, unsigned int max_latency)
{
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	xb->max_latency = clamp(max_latency,
				xb->latency,
				WCXB_DEFAULT_MAXLATENCY);
	spin_unlock_irqrestore(&xb->lock, flags);
}

static inline void
wcxb_set_minlatency(struct wcxb *xb, unsigned int min_latency)
{
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	xb->latency = clamp(min_latency, WCXB_DEFAULT_LATENCY,
			    WCXB_DEFAULT_MAXLATENCY);
	spin_unlock_irqrestore(&xb->lock, flags);
}

static inline void
wcxb_lock_latency(struct wcxb *xb)
{
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	xb->flags.latency_locked = 1;
	spin_unlock_irqrestore(&xb->lock, flags);
	return;
}

static inline void
wcxb_unlock_latency(struct wcxb *xb)
{
	unsigned long flags;
	spin_lock_irqsave(&xb->lock, flags);
	xb->flags.latency_locked = 0;
	spin_unlock_irqrestore(&xb->lock, flags);
	return;
}

/* Interface for the echocan block */
extern void wcxb_enable_echocan(struct wcxb *xb);
extern void wcxb_disable_echocan(struct wcxb *xb);
extern void wcxb_reset_echocan(struct wcxb *xb);
extern void wcxb_enable_echocan_dram(struct wcxb *xb);
extern bool wcxb_is_echocan_present(struct wcxb *xb);
extern u16 wcxb_get_echocan_reg(struct wcxb *xb, u32 address);
extern void wcxb_set_echocan_reg(struct wcxb *xb, u32 address, u16 val);

#endif
