/*
 * DAHDI Telephony Interface to VPMADT032 Firmware Loader
 *
 * Copyright (C) 2008-2011 Digium, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>

#include <dahdi/kernel.h>

static int debug;

#include "voicebus/voicebus.h"
#include "voicebus/vpmadtreg.h"
#include "vpmadt032_loader.h"

vpmlinkage static int __attribute__((format (printf, 1, 2)))
logger(const char *format, ...)
{
	int res;
	va_list args;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
	va_start(args, format);
	res = vprintk(format, args);
	va_end(args);
#else
	char buf[256];

	va_start(args, format);
	res = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	printk(KERN_INFO "%s" buf);
#endif

	return res;
}

vpmlinkage static void *memalloc(size_t len)
{
	return kmalloc(len, GFP_KERNEL);
}

vpmlinkage static void memfree(void *ptr)
{
	kfree(ptr);
}

struct private_context {
	struct voicebus *vb;
	void *pvt;
	struct completion done;
	struct voicebus_operations ops;
};

static void handle_receive(struct voicebus *vb, struct list_head *buffers)
{
	struct vbb *vbb;
	struct private_context *ctx = container_of(vb->ops,
						struct private_context, ops);
	list_for_each_entry(vbb, buffers, entry) {
		__vpmadt032_receive(ctx->pvt, vbb->data);
		if (__vpmadt032_done(ctx->pvt))
			complete(&ctx->done);
	}
}

static void handle_transmit(struct voicebus *vb, struct list_head *buffers)
{
	struct vbb *vbb;
	struct private_context *ctx = container_of(vb->ops,
						struct private_context, ops);
	list_for_each_entry(vbb, buffers, entry)
		__vpmadt032_transmit(ctx->pvt, vbb->data);
}

static void init_private_context(struct private_context *ctx)
{
	init_completion(&ctx->done);
	ctx->ops.handle_receive = handle_receive;
	ctx->ops.handle_transmit = handle_transmit;
}

static int vpmadt032_load_firmware(struct voicebus *vb)
{
	int ret = 0;
	struct private_context *ctx;
	const struct voicebus_operations *old;
	int id;
	might_sleep();
	ctx = kzalloc(sizeof(struct private_context), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	init_private_context(ctx);
	ctx->vb = vb;

	if (0x8007 == vb->pdev->device || 0x8008 == vb->pdev->device)
		id = vb->pdev->vendor << 16 | 0x2400;
	else
		id = vb->pdev->vendor << 16 | vb->pdev->device;

	ret = __vpmadt032_start_load(0, id, &ctx->pvt);
	if (ret)
		goto error_exit;
	old = vb->ops;
	vb->ops = &ctx->ops;
	if (!wait_for_completion_timeout(&ctx->done, HZ*20))
		ret = -EIO;
	vb->ops = old;
	__vpmadt032_cleanup(ctx->pvt);
error_exit:
	kfree(ctx);
	return ret;
}

static struct vpmadt_loader loader = {
	.owner = THIS_MODULE,
	.load = vpmadt032_load_firmware,
};

static int __init vpmadt032_loader_init(void)
{
	__vpmadt032_init(logger, debug, memalloc, memfree);
	vpmadtreg_register(&loader);
	return 0;
}

static void __exit vpmadt032_loader_exit(void)
{
	vpmadtreg_unregister(&loader);
	return;
}

module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_DESCRIPTION("DAHDI VPMADT032 (Hardware Echo Canceller) Firmware Loader");
MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_LICENSE("Digium Commercial");

module_init(vpmadt032_loader_init);
module_exit(vpmadt032_loader_exit);
