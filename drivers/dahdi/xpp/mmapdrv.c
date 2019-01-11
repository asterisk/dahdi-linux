#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/blackfin.h>
#include <asm/dma.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <dahdi/kernel.h>
#include "mmapbus.h"
#include "xbus-core.h"
#include "dahdi_debug.h"
#include "xdefs.h"
#include "xproto.h"
#include "xframe_queue.h"

/* Check at compile time that sizeof(xframe_t) is a multiple of 4 */
typedef char
    sizeof_xframe_t_should_be_divisible_by_4[((sizeof(xframe_t) % 4) ==
					      0) * 2 - 1];

#define ssync()			__builtin_bfin_ssync()

//#define AB_IN_BUF     PF5
/* firmware pins */
#define DATA            PG8
#define NCONFIG         PG9
#define CONF_DONE       PG10
#define DCLK            PG11
#define NSTATUS         PG12

#ifdef	DEBUG_VIA_GPIO
/*
 * For debugging we can use the following two pins.
 * These two pins are not used *after initialization*
 */
#define	DEBUG_GPIO1	CONF_DONE
#define	DEBUG_GPIO2	NSTATUS

static int rx_intr_counter;
#endif

#define FPGA_RX_IRQ	IRQ_PF7
#define FPGA_TX_IRQ	IRQ_PF4
#define FPGA_BASE_ADDR	((volatile char __iomem *)0x203FA800)

#define END_OF_FRAME	0x0001
#define GET_LEN		0x0002
#define START_RD_BURST	0x0008
/* stand alone Astribank without USB (Asterisk BlackFin Mode) */
#define AS_BF_MODE	0x0010
/*
 * all data between Astribank and USB routed
 * thru BF(EchoCanceler BlackFin Mode)
 */
#define EC_BF_MODE	0x0020
/* Astribank worke with USB only (no BlackFin Mode) */
#define NO_BF_MODE	0x0040
#define SET_XA_DIR	0x0080
#define GET_XPD_STS	0x0100
#define GET_CHECKSUM	0x0200

static const char rcsid[] = "$Id$";

static DEF_PARM(int, debug, 0, 0644, "Print DBG statements");
static DEF_PARM(int, notxrx, 0, 0644, "Don't send or receive anything");

struct counter {
	long intr_min, intr_max;
	long intr_avg, intr_count;
};

static xbus_t *global_xbus;
static bool tx_ready = 1;
static DEFINE_SPINLOCK(tx_ready_lock);
static struct xframe_queue txpool;
static unsigned int pcm_in_pool_count;
static bool disconnecting;
static struct kmem_cache *xframe_cache;
static struct counter tx_counter, rx_counter;
static unsigned long pcm_dropped;

static void print_buffer(const char *msg, const char *buf, int len)
{
	int i;
	printk(KERN_ERR "%s", msg);
	for (i = 0; i < len; i++)
		printk("%02X ", (unsigned char)buf[i]);
	printk("\n");
}

static void update_counter(struct counter *c, struct timeval *tv1)
{
	struct timeval tv2;
	long diff;
	do_gettimeofday(&tv2);
	diff = usec_diff(&tv2, tv1);
	if (c->intr_min > diff)
		c->intr_min = diff;
	if (c->intr_max < diff)
		c->intr_max = diff;
	c->intr_avg =
	    (c->intr_avg * c->intr_count + diff) / (c->intr_count + 1);
	c->intr_count++;
}

static irqreturn_t xpp_mmap_rx_irq(int irq, void *dev_id)
{
	unsigned short rxcnt;
	xbus_t *xbus;
	xframe_t *xframe;
	__u8 *buf;
	bool in_use = 0;
	struct timeval tv1;

	do_gettimeofday(&tv1);
	if (unlikely(disconnecting))
		return IRQ_HANDLED;

	xbus = xbus_num(global_xbus->num);
	BUG_ON(!xbus);
	if (!XBUS_GET(xbus)) {
		if (printk_ratelimit())
			XBUS_ERR(xbus, "Dropping packet. Is shutting down.\n");
		goto out;
	}
	in_use = 1;

	outw(GET_LEN, FPGA_BASE_ADDR + 4);
	rxcnt = inw(FPGA_BASE_ADDR);
	if (rxcnt < 3) {
		if (printk_ratelimit())
			NOTICE("Got %d bytes\n", rxcnt);
		goto out;
	}
	if (rxcnt >= XFRAME_DATASIZE) {
		if (printk_ratelimit())
			ERR("Bad rxcnt=%d\n", rxcnt);
		goto out;
	}

	xframe = ALLOC_RECV_XFRAME(xbus);
	if (!xframe) {
		if (printk_ratelimit())
			XBUS_ERR(xbus, "Empty receive_pool\n");
		goto out;
	}
	buf = xframe->packets;
	atomic_set(&xframe->frame_len, rxcnt);
	do_gettimeofday(&xframe->tv_received);
#ifdef	DEBUG_VIA_GPIO
	if (rx_intr_counter & 1)
		bfin_write_PORTGIO_SET(DEBUG_GPIO1);
	else
		bfin_write_PORTGIO_CLEAR(DEBUG_GPIO1);
#endif
	outw(START_RD_BURST, FPGA_BASE_ADDR + 4);
	insw((unsigned long)FPGA_BASE_ADDR, buf, rxcnt / 2);
#if 0
	for (count = 0; count < rxcnt; count += 2) {
		unsigned short v = inw(FPGA_BASE_ADDR);
		buf[count] = v & 0xFF;
		buf[count + 1] = v >> 8;
	}
#endif
	if (rxcnt & 1)
		buf[rxcnt - 1] = inw(FPGA_BASE_ADDR);
	/*
	 * Sanity check: length of first packet in frame
	 * should be no more than the frame length
	 */
	if (((buf[0] | (buf[1] << 8)) & 0x3FF) > rxcnt) {
		if (printk_ratelimit()) {
			ERR("Packet len=%d, frame len=%d\n",
			    (buf[0] | (buf[1] << 8)) & 0x3FF, rxcnt);
			print_buffer("16 bytes of packet: ", buf, 16);
		}
		goto free;
	}
	if (debug && buf[2] != 0x12)
		print_buffer("Received: ", buf, rxcnt);
	if (!notxrx) {
		xbus_receive_xframe(xbus, xframe);
#ifdef	DEBUG_VIA_GPIO
		if (rx_intr_counter & 1)
			bfin_write_PORTGIO_SET(DEBUG_GPIO2);
		else
			bfin_write_PORTGIO_CLEAR(DEBUG_GPIO2);
#endif
		goto out;
	}
free:
	FREE_RECV_XFRAME(xbus, xframe);
out:
	if (in_use)
		XBUS_PUT(xbus);
#ifdef	DEBUG_VIA_GPIO
	rx_intr_counter++;
#endif
	update_counter(&rx_counter, &tv1);
	return IRQ_HANDLED;
}

static void send_buffer(unsigned char *buf, unsigned long len)
{
	if (debug && len >= 3 && buf[2] != 0x11)
		print_buffer("Sent: ", buf, len);
	outsw((unsigned long)FPGA_BASE_ADDR, buf, len / 2);
	if (len & 1)
		outw((unsigned short)buf[len - 1], FPGA_BASE_ADDR);
	outw(END_OF_FRAME, FPGA_BASE_ADDR + 4);
}

static irqreturn_t xpp_mmap_tx_irq(int irq, void *dev_id)
{
	unsigned long flags;
	xbus_t *xbus;
	xframe_t *xframe;
	struct timeval tv1;

	do_gettimeofday(&tv1);
	if (unlikely(disconnecting)) {
		update_counter(&tx_counter, &tv1);
		return IRQ_HANDLED;
	}
	spin_lock_irqsave(&tx_ready_lock, flags);
	xframe = xframe_dequeue(&txpool);
	if (!xframe) {
		tx_ready = 1;
		spin_unlock_irqrestore(&tx_ready_lock, flags);
		update_counter(&tx_counter, &tv1);
		return IRQ_HANDLED;
	}
	tx_ready = 0;
	if (XPACKET_IS_PCM((xpacket_t *)xframe->packets))
		pcm_in_pool_count--;
	spin_unlock_irqrestore(&tx_ready_lock, flags);
	xbus = (xbus_t *)xframe->priv;
	BUG_ON(!xbus);
	xbus = xbus_num(xbus->num);
	BUG_ON(!xbus);
	send_buffer(xframe->packets, XFRAME_LEN(xframe));
	FREE_SEND_XFRAME(xbus, xframe);
	update_counter(&tx_counter, &tv1);
	return IRQ_HANDLED;
}

static int xframe_send_common(xbus_t *xbus, xframe_t *xframe, bool pcm)
{
	unsigned long flags;
	if (unlikely(disconnecting)) {
		FREE_SEND_XFRAME(xbus, xframe);
		return -ENODEV;
	}
	if (unlikely(notxrx)) {
		FREE_SEND_XFRAME(xbus, xframe);
		return 0;
	}
	spin_lock_irqsave(&tx_ready_lock, flags);
	if (tx_ready) {
		tx_ready = 0;
		send_buffer(xframe->packets, XFRAME_LEN(xframe));
		spin_unlock_irqrestore(&tx_ready_lock, flags);
		FREE_SEND_XFRAME(xbus, xframe);
	} else {
		if (pcm && pcm_in_pool_count >= 1) {
			static int rate_limit;
			if ((rate_limit++ % 1000) == 0)
				XBUS_ERR(xbus,
					 "Dropped PCM xframe "
					 "(pcm_in_pool_count=%d).\n",
					 pcm_in_pool_count);
			FREE_SEND_XFRAME(xbus, xframe);
			pcm_dropped++;
		} else {
			if (!xframe_enqueue(&txpool, xframe)) {
				static int rate_limit;
				spin_unlock_irqrestore(&tx_ready_lock, flags);
				if ((rate_limit++ % 1000) == 0)
					XBUS_ERR(xbus,
						 "Dropped xframe. "
						 "Cannot enqueue.\n");
				FREE_SEND_XFRAME(xbus, xframe);
				return -E2BIG;
			}
			if (pcm)
				pcm_in_pool_count++;
		}
		spin_unlock_irqrestore(&tx_ready_lock, flags);
	}
	return 0;
}

static xframe_t *alloc_xframe(xbus_t *xbus, gfp_t gfp_flags)
{
	xframe_t *xframe = kmem_cache_alloc(xframe_cache, gfp_flags);
	if (!xframe) {
		static int rate_limit;
		if ((rate_limit++ % 1000) < 5)
			XBUS_ERR(xbus, "frame allocation failed (%d)\n",
				 rate_limit);
		return NULL;
	}
	xframe_init(xbus, xframe, ((__u8 *)xframe) + sizeof(xframe_t),
		    XFRAME_DATASIZE, xbus);
	return xframe;
}

static void free_xframe(xbus_t *xbus, xframe_t *xframe)
{
	memset(xframe, 0, sizeof(*xframe));
	kmem_cache_free(xframe_cache, xframe);
}

static int xframe_send_pcm(xbus_t *xbus, xframe_t *xframe)
{
	return xframe_send_common(xbus, xframe, 1);
}

static int xframe_send_cmd(xbus_t *xbus, xframe_t *xframe)
{
	return xframe_send_common(xbus, xframe, 0);
}

static struct xbus_ops xmmap_ops = {
	.xframe_send_pcm = xframe_send_pcm,
	.xframe_send_cmd = xframe_send_cmd,
	.alloc_xframe = alloc_xframe,
	.free_xframe = free_xframe,
};

static int fill_proc_queue(char *p, struct xframe_queue *q)
{
	int len;

	len = sprintf(p,
		"%-15s: counts %3d, %3d, %3d worst %3d, overflows %3d "
		"worst_lag %02ld.%ld ms\n",
		q->name, q->steady_state_count, q->count, q->max_count,
		q->worst_count, q->overflows, q->worst_lag_usec / 1000,
		q->worst_lag_usec % 1000);
	xframe_queue_clearstats(q);
	return len;
}

static int fill_proc_counter(char *p, struct counter *c)
{
	return sprintf(p, "min=%ld\nmax=%ld\navg=%ld\ncount=%ld\n", c->intr_min,
		       c->intr_max, c->intr_avg, c->intr_count);
}

static int xpp_mmap_proc_read(char *page, char **start, off_t off, int count,
			      int *eof, void *data)
{
	int len = 0;
	len += fill_proc_queue(page + len, &txpool);
	len += sprintf(page + len, "pcm_in_pool_count=%d\n", pcm_in_pool_count);
	len += sprintf(page + len, "pcm_dropped=%lu\n", pcm_dropped);
	len += sprintf(page + len, "\nrx_counter:\n");
	len += fill_proc_counter(page + len, &rx_counter);
	len += sprintf(page + len, "\ntx_counter:\n");
	len += fill_proc_counter(page + len, &tx_counter);
	if (len <= off + count) {
		*eof = 1;
		tx_counter.intr_min = rx_counter.intr_min = INT_MAX;
		tx_counter.intr_max = rx_counter.intr_max = 0;
		tx_counter.intr_avg = rx_counter.intr_avg = 0;
		tx_counter.intr_count = rx_counter.intr_count = 0;
	}
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;
}

static int xpp_mmap_proc_write(struct file *file, const char __user *buffer,
			       unsigned long count, void *data)
{
	int i = 0;
	char *txchunk, *p, *endp;

	if (count >= XFRAME_DATASIZE * 3 + 10)
		return -EINVAL;
	p = txchunk = kmalloc(count + 1, GFP_KERNEL);
	if (copy_from_user(txchunk, buffer, count)) {
		count = -EFAULT;
		goto out;
	}
	txchunk[count] = '\0';

	while (*p) {
		unsigned long value;
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (*p == '\0')
			break;
		value = simple_strtoul(p, &endp, 16);
		if (endp == p || value > 0xFF) {
			INFO("%s: Bad input\n", __func__);
			count = -EINVAL;
			goto out;
		}
		p = endp;
		txchunk[i++] = (char)value;
	}
	send_buffer(txchunk, i);
out:
	kfree(txchunk);
	return count;
}

static struct mmap_driver astribank_driver = {
	.module = THIS_MODULE,
	.driver = {
		   .name = "xpp_mmap",
		   },
};

static struct mmap_device astribank_dev = {
	.name = "astribank0",
	.driver = &astribank_driver,
};

static int __init xpp_mmap_load_fpga(u8 * data, size_t size)
{
	size_t i;
	/* set data, nconfig and dclk to port out */
	bfin_write_PORTGIO_DIR(bfin_read_PORTGIO_DIR() | DATA | NCONFIG | DCLK);
	bfin_write_PORTG_FER(bfin_read_PORTG_FER() & ~(DATA | NCONFIG | DCLK));
	/* set conf_done and nstatus to port in */
	bfin_write_PORTGIO_DIR(
		bfin_read_PORTGIO_DIR() & ~(CONF_DONE | NSTATUS));
	bfin_write_PORTGIO_INEN(
		bfin_read_PORTGIO_INEN() & ~(DATA | NCONFIG | DCLK));
	bfin_write_PORTGIO_INEN(bfin_read_PORTGIO_INEN() | CONF_DONE | NSTATUS);

	/* reset fpga during configuration holds nCONFIG low */
	bfin_write_PORTGIO_CLEAR(NCONFIG);
	udelay(40);		/* Tcfg ~40us delay */
	/* transition nCONFIG to high - reset end. */
	bfin_write_PORTGIO_SET(NCONFIG);
	udelay(40);		/* Tcf2ck ~40us delay */
	if (!(bfin_read_PORTGIO() & NSTATUS))
		return -EIO;	/* report reset faill - Tcf2st1 pass */

#if 0
	if (!(bfin_read_PORTGIO() & CONF_DONE))
		return -EIO;
#endif
	bfin_write_PORTGIO_CLEAR(DCLK);
	for (i = 0; i < size; i++) {	/* loop EP2OUT buffer data to FPGA */
		int j;
		u8 __u8 = data[i];
		/*
		 * Send the configuration data through the DATA0 pin
		 * one bit at a time.
		 */
		for (j = 0; j < 8; j++)
		{
			if (__u8 & 1)
				bfin_write_PORTGIO_SET(DATA);
			else
				bfin_write_PORTGIO_CLEAR(DATA);
			__u8 >>= 1;
			bfin_write_PORTGIO_SET(DCLK);
			bfin_write_PORTGIO_CLEAR(DCLK);
		}
		if (!(bfin_read_PORTGIO() & NSTATUS))
			return -EIO;	/* check the nSTATUS */
	}
	bfin_write_PORTGIO_CLEAR(DATA);
	udelay(1);
	if (!(bfin_read_PORTGIO() & CONF_DONE))
		return -EIO;
#ifdef	DEBUG_VIA_GPIO
	/*
	 * Normal initialization is done. Now we can reuse
	 * some pins that were used only during initialization
	 * to be used for debugging from now on.
	 */
	/* set to port out */
	bfin_write_PORTGIO_DIR(
		bfin_read_PORTGIO_DIR() | DEBUG_GPIO1 | DEBUG_GPIO2);
	bfin_write_PORTG_FER(bfin_read_PORTG_FER() &
			     ~(DEBUG_GPIO1 | DEBUG_GPIO2));
	bfin_write_PORTGIO_INEN(bfin_read_PORTGIO_INEN() &
				~(DEBUG_GPIO1 | DEBUG_GPIO2));
#endif
	udelay(40);		/* tCD2UM - CONF_DONE high to user mode */
	return 0;
}

static void __exit xpp_mmap_unload_fpga(void)
{
	/* reset fpga during configuration holds nCONFIG low */
	bfin_write_PORTGIO_CLEAR(NCONFIG);
	udelay(40);		/* Tcfg ~40us delay */
	/* disable output pin */
	bfin_write_PORTGIO_DIR(
		bfin_read_PORTGIO_DIR() & ~(DATA | NCONFIG | DCLK));
	/* disable input buffer */
	bfin_write_PORTGIO_INEN(
		bfin_read_PORTGIO_INEN() & ~(CONF_DONE | NSTATUS));
	INFO("FPGA Firmware unloaded\n");
}

static int __init xpp_mmap_load_firmware(void)
{
	const struct firmware *fw;
	int ret;
	if ((ret =
	     request_firmware(&fw, "astribank.bin", &astribank_dev.dev)) < 0)
		return ret;
	xpp_mmap_load_fpga(fw->data, fw->size);
	release_firmware(fw);
	return ret;
}

static int __init xpp_mmap_init(void)
{
	int ret;
	struct proc_dir_entry *proc_entry;

	if ((ret = register_mmap_bus()) < 0)
		goto bus_reg;
	if ((ret = register_mmap_driver(&astribank_driver)) < 0)
		goto driver_reg;
	if ((ret = register_mmap_device(&astribank_dev)) < 0)
		goto dev_reg;
	if ((ret = xpp_mmap_load_firmware()) < 0) {
		ERR("xpp_mmap_load_firmware() failed, errno=%d\n", ret);
		goto fail_fw;
	}

	if ((ret =
	     request_irq(FPGA_RX_IRQ, xpp_mmap_rx_irq, IRQF_TRIGGER_RISING,
			 "xpp_mmap_rx", NULL)) < 0) {
		ERR("Unable to attach to RX interrupt %d\n", FPGA_RX_IRQ);
		goto fail_irq_rx;
	}
	if ((ret =
	     request_irq(FPGA_TX_IRQ, xpp_mmap_tx_irq, IRQF_TRIGGER_RISING,
			 "xpp_mmap_tx", NULL)) < 0) {
		ERR("Unable to attach to TX interrupt %d\n", FPGA_TX_IRQ);
		goto fail_irq_tx;
	}
	if (!request_region((resource_size_t) FPGA_BASE_ADDR, 8, "xpp_mmap")) {
		ERR("Unable to request memory region at %p\n", FPGA_BASE_ADDR);
		goto fail_region;
	}
	outw(AS_BF_MODE, FPGA_BASE_ADDR + 4);

	xframe_cache =
	    kmem_cache_create("xframe_cache",
			      sizeof(xframe_t) + XFRAME_DATASIZE, 0, 0,
			      NULL);
	if (!xframe_cache) {
		ret = -ENOMEM;
		goto fail_cache;
	}
	/* interface with Dahdi */
	global_xbus = xbus_new(&xmmap_ops, XFRAME_DATASIZE, NULL);
	if (!global_xbus) {
		ret = -ENOMEM;
		goto fail_xbus;
	}
	strncpy(global_xbus->connector, "mmap", XBUS_DESCLEN);
	strncpy(global_xbus->label, "mmap:0", LABEL_SIZE);

	xframe_queue_init(&txpool, 10, 200, "mmap_txpool", global_xbus);
	if (!
	    (proc_entry =
	     create_proc_entry("xpp_mmap", 0, global_xbus->proc_xbus_dir))) {
		ERR("create_proc_entry() failed\n");
		ret = -EINVAL;
		goto fail_proc;
	}
	proc_entry->write_proc = xpp_mmap_proc_write;
	proc_entry->read_proc = xpp_mmap_proc_read;
	/* Go xbus, go! */
	xbus_connect(global_xbus);
	INFO("xpp_mmap module loaded\n");

	return 0;

fail_proc:
	xbus_disconnect(global_xbus);
fail_xbus:
	kmem_cache_destroy(xframe_cache);
fail_cache:
	release_region((resource_size_t) FPGA_BASE_ADDR, 8);
fail_region:
	free_irq(FPGA_TX_IRQ, NULL);
fail_irq_tx:
	free_irq(FPGA_RX_IRQ, NULL);
fail_irq_rx:
fail_fw:
	unregister_mmap_device(&astribank_dev);
dev_reg:
	unregister_mmap_driver(&astribank_driver);
driver_reg:
	unregister_mmap_bus();
bus_reg:
	return ret;
}

static void __exit xpp_mmap_exit(void)
{
	xbus_t *xbus;
	DBG(GENERAL, "\n");
	disconnecting = 1;
	xbus = xbus_num(global_xbus->num);
	remove_proc_entry("xpp_mmap", xbus->proc_xbus_dir);
	xframe_queue_clear(&txpool);
	xbus_disconnect(xbus);
	kmem_cache_destroy(xframe_cache);

	release_region((resource_size_t) FPGA_BASE_ADDR, 8);
	free_irq(FPGA_RX_IRQ, NULL);
	free_irq(FPGA_TX_IRQ, NULL);

	unregister_mmap_device(&astribank_dev);
	unregister_mmap_driver(&astribank_driver);
	unregister_mmap_bus();
	xpp_mmap_unload_fpga();
	INFO("xpp_mmap module unloaded\n");
}

module_init(xpp_mmap_init);
module_exit(xpp_mmap_exit);
MODULE_AUTHOR("Alexander Landau <landau.alex@gmail.com>");
MODULE_LICENSE("GPL");
