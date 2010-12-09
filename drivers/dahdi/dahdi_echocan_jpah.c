/*
 * ECHO_CAN_JPAH
 *
 * by Jason Parker
 *
 * Based upon mg2ec.h - sort of.
 * This "echo can" will completely hose your audio.
 * Don't use it unless you're absolutely sure you know what you're doing.
 *
 * Copyright (C) 2007-2008, Digium, Inc.
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
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>

#include <dahdi/kernel.h>

static int debug;

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)
#define debug_printk(level, fmt, args...) if (debug >= level) printk("%s (%s): " fmt, THIS_MODULE->name, __FUNCTION__, ## args)

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec);
static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);
static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size);
static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val);
static const char *name = "JPAH";
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
};

struct ec_pvt {
	struct dahdi_echocan_state dahdi;
	int blah;
};

#define dahdi_to_pvt(a) container_of(a, struct ec_pvt, dahdi)

static int echo_can_create(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			   struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec)
{
	struct ec_pvt *pvt;

	if (ecp->param_count > 0) {
		printk(KERN_WARNING "JPAH does not support parameters; failing request\n");
		return -EINVAL;
	}

	pvt = kzalloc(sizeof(*pvt), GFP_KERNEL);
	if (!pvt)
		return -ENOMEM;

	pvt->dahdi.ops = &my_ops;

	*ec = &pvt->dahdi;
	return 0;
}

static void echo_can_free(struct dahdi_chan *chan, struct dahdi_echocan_state *ec)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);

	kfree(pvt);
}

static void echo_can_process(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size)
{
	struct ec_pvt *pvt = dahdi_to_pvt(ec);
	u32 x;

	for (x = 0; x < size; x++) {
		if (pvt->blah < 2) {
			pvt->blah++;

			*isig++ = 0;
		} else {
			pvt->blah = 0;
			
			isig++;
		}
	}
}

static int echo_can_traintap(struct dahdi_echocan_state *ec, int pos, short val)
{
	return 0;
}

static int __init mod_init(void)
{
	if (dahdi_register_echocan_factory(&my_factory)) {
		module_printk(KERN_ERR, "could not register with DAHDI core\n");

		return -EPERM;
	}

	module_printk(KERN_NOTICE, "Registered echo canceler '%s'\n",
		      my_factory.get_name(NULL));

	return 0;
}

static void __exit mod_exit(void)
{
	dahdi_unregister_echocan_factory(&my_factory);
}

module_param(debug, int, S_IRUGO | S_IWUSR);

MODULE_DESCRIPTION("DAHDI Jason Parker Audio Hoser");
MODULE_AUTHOR("Jason Parker <jparker@digium.com>");
MODULE_LICENSE("GPL v2");

module_init(mod_init);
module_exit(mod_exit);
