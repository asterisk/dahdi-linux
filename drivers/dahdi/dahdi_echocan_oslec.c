/*
 * DAHDI Telephony Interface to the Open Source Line Echo Canceller (OSLEC)
 *
 * Written by Tzafrir Cohen <tzafrir.cohen@xorcom.com>
 * Copyright (C) 2008 Xorcom, Inc.
 *
 * All rights reserved.
 *
 * Based on dahdi_echocan_hpec.c, Copyright (C) 2006-2008 Digium, Inc.
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

/* Fix this if OSLEC is elsewhere */
#include "../staging/echo/oslec.h"
//#include <linux/oslec.h>

#include <dahdi/kernel.h>

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec);
static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);
static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size);
static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val);
#ifdef CONFIG_DAHDI_ECHOCAN_PROCESS_TX
static void echo_can_hpf_tx(struct dahdi_echocan_state *ec,
			    short *tx, u32 size);
#endif
static const char *name = "OSLEC";
static const char *ec_name(const struct dahdi_chan *chan) { return name; }

static const struct dahdi_echocan_factory my_factory = {
	.get_name = ec_name,
	.owner = THIS_MODULE,
	.echocan_create = echo_can_create,
};

static const struct dahdi_echocan_ops my_ops = {
	.echocan_free = echo_can_free,
	.echocan_process = echo_can_process,
	.echocan_traintap = echo_can_traintap,
#ifdef CONFIG_DAHDI_ECHOCAN_PROCESS_TX
	.echocan_process_tx = echo_can_hpf_tx,
#endif
};

struct ec_pvt {
	struct oslec_state *oslec;
	struct dahdi_echocan_state dahdi;
};

#define dahdi_to_pvt(a) container_of(a, struct ec_pvt, dahdi)

static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	oslec_free(pvt->oslec);
	kfree(pvt);
}

static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);
	u32 SampleNum;

	for (SampleNum = 0; SampleNum < size; SampleNum++, iref++) {
		short iCleanSample;

		iCleanSample = oslec_update(pvt->oslec, *iref, *isig);
		*isig++ = iCleanSample;
	}
}

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec)
{
	struct ec_pvt *pvt;

	if (ecp->param_count > 0) {
		printk(KERN_WARNING "OSLEC does not support parameters; failing request\n");
		return -EINVAL;
	}

	pvt = kzalloc(sizeof(*pvt), GFP_KERNEL);
	if (!pvt)
		return -ENOMEM;

	pvt->dahdi.ops = &my_ops;

	pvt->oslec = oslec_create(ecp->tap_length, ECHO_CAN_USE_ADAPTION | ECHO_CAN_USE_NLP | ECHO_CAN_USE_CLIP | ECHO_CAN_USE_TX_HPF | ECHO_CAN_USE_RX_HPF);

	if (!pvt->oslec) {
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

#ifdef CONFIG_DAHDI_ECHOCAN_PROCESS_TX
static void echo_can_hpf_tx(struct dahdi_echocan_state *ec, short *tx, u32 size)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);
	u32 SampleNum;

	for (SampleNum = 0; SampleNum < size; SampleNum++, tx++) {
		short iCleanSample;

		iCleanSample = oslec_hpf_tx(pvt->oslec, *tx);
		*tx = iCleanSample;
	}
}
#endif

static int __init mod_init(void)
{
	if (dahdi_register_echocan_factory(&my_factory)) {
		module_printk(KERN_ERR, "could not register with DAHDI core\n");

		return -EPERM;
	}

	module_printk(KERN_INFO, "Registered echo canceler '%s'\n",
		my_factory.get_name(NULL));

	return 0;
}

static void __exit mod_exit(void)
{
	dahdi_unregister_echocan_factory(&my_factory);
}

MODULE_DESCRIPTION("DAHDI OSLEC wrapper");
MODULE_AUTHOR("Tzafrir Cohen <tzafrir.cohen@xorcom.com>");
MODULE_LICENSE("GPL");

module_init(mod_init);
module_exit(mod_exit);
