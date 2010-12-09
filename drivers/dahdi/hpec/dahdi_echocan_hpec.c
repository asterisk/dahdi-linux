/*
 * DAHDI Telephony Interface to Digium High-Performance Echo Canceller
 *
 * Copyright (C) 2006-2008 Digium, Inc.
 *
 * All rights reserved.
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
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

static int debug;

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)
#define debug_printk(level, fmt, args...) if (debug >= level) printk(KERN_DEBUG "%s (%s): " fmt, THIS_MODULE->name, __FUNCTION__, ## args)

#include "hpec_user.h"
#include "hpec.h"

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec);
static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);
static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, __u32 size);
static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val);
static const char *name = "HPEC";
static const char *ec_name(const struct dahdi_chan *chan) { return name; }

static const struct dahdi_echocan_factory my_factory = {
	.get_name = ec_name,
	.owner = THIS_MODULE,
	.echocan_create = echo_can_create,
};

static const struct dahdi_echocan_features my_features = {
	.NLP_automatic = 1,
	.CED_tx_detect = 1,
	.CED_rx_detect = 1,
};

static const struct dahdi_echocan_ops my_ops = {
	.echocan_free = echo_can_free,
	.echocan_process = echo_can_process,
	.echocan_traintap = echo_can_traintap,
};

struct ec_pvt {
	struct hpec_state *hpec;
	struct dahdi_echocan_state dahdi;
};

#define dahdi_to_pvt(a) container_of(a, struct ec_pvt, dahdi)

static int __attribute__((regparm(0), format(printf, 1, 2))) logger(const char *format, ...)
{
	int res;
	va_list args;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
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

static void *memalloc(size_t len)
{
	return kmalloc(len, GFP_KERNEL);
}

static void memfree(void *ptr)
{
	kfree(ptr);
}

static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	hpec_channel_free(pvt->hpec);
	kfree(pvt);
}

static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, __u32 size)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	hpec_channel_update(pvt->hpec, isig, iref);
}

DEFINE_SEMAPHORE(license_lock);

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec)
{
	struct ec_pvt *pvt;

	if (ecp->param_count > 0) {
		printk(KERN_WARNING "HPEC does not support parameters; failing request\n");
		return -EINVAL;
	}

	pvt = kzalloc(sizeof(*pvt), GFP_KERNEL);
	if (!pvt)
		return -ENOMEM;

	pvt->dahdi.ops = &my_ops;
	pvt->dahdi.features = my_features;

	if (down_interruptible(&license_lock))
		return -ENOTTY;

	pvt->hpec = hpec_channel_alloc(ecp->tap_length);

	up(&license_lock);

	if (!pvt->hpec) {
		kfree(pvt);
		*ec = NULL;
		return -ENOTTY;
	} else {
		*ec = &pvt->dahdi;
		return 0;
	}
}

static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val)
{
	return 1;
}

static int hpec_license_ioctl(unsigned int cmd, unsigned long data)
{
	struct hpec_challenge challenge;
	struct hpec_license license;
	int result = 0;

	switch (cmd) {
	case DAHDI_EC_LICENSE_CHALLENGE:
		if (down_interruptible(&license_lock))
			return -EINTR;
		memset(&challenge, 0, sizeof(challenge));
		if (hpec_license_challenge(&challenge))
			result = -ENODEV;
		if (!result && copy_to_user((unsigned char *) data, &challenge, sizeof(challenge)))
			result = -EFAULT;
		up(&license_lock);
		return result;
	case DAHDI_EC_LICENSE_RESPONSE:
		if (down_interruptible(&license_lock))
			return -EINTR;
		if (copy_from_user(&license, (unsigned char *) data, sizeof(license)))
			result = -EFAULT;
		if (!result && hpec_license_check(&license))
			result = -EACCES;
		up(&license_lock);
		return result;
	default:
		return -ENOSYS;
	}
}

static int __init mod_init(void)
{
	if (dahdi_register_echocan_factory(&my_factory)) {
		module_printk(KERN_ERR, "could not register with DAHDI core\n");

		return -EPERM;
	}

	module_printk(KERN_NOTICE, "Registered echo canceler '%s'\n",
		my_factory.get_name(NULL));

	hpec_init(logger, debug, DAHDI_CHUNKSIZE, memalloc, memfree);

	dahdi_set_hpec_ioctl(hpec_license_ioctl);

	return 0;
}

static void __exit mod_exit(void)
{
	dahdi_unregister_echocan_factory(&my_factory);

	dahdi_set_hpec_ioctl(NULL);

	hpec_shutdown();
}

module_param(debug, int, S_IRUGO | S_IWUSR);

MODULE_DESCRIPTION("DAHDI High Performance Echo Canceller");
MODULE_AUTHOR("Kevin P. Fleming <kpfleming@digium.com>");
MODULE_LICENSE("Digium Commercial");

module_init(mod_init);
module_exit(mod_exit);
