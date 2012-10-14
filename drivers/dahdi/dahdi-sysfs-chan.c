/* dahdi-sysfs-chan.c
 *
 * Copyright (C) 2011-2012, Xorcom
 * Copyright (C) 2011-2012, Digium, Inc.
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

#include <linux/version.h>

#define DAHDI_PRINK_MACROS_USE_debug
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <dahdi/kernel.h>
#include "dahdi.h"
#include "dahdi-sysfs.h"

/* shortcuts, for code readability */
#define MAKE_DAHDI_DEV(num, name) \
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, num), NULL, name)
#define DEL_DAHDI_DEV(num) \
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, num))

static struct class *dahdi_class;


/*
 * Flags to remember what initializations already
 * succeeded.
 */
static struct {
	int channel_driver:1;
	int channels_bus:1;
} should_cleanup;

static struct device_attribute chan_dev_attrs[] = {
       __ATTR_NULL,
};

static void chan_release(struct device *dev)
{
	struct dahdi_chan *chan;

	BUG_ON(!dev);
	chan = dev_to_chan(dev);
	chan_dbg(DEVICES, chan, "SYSFS\n");
}

static int chan_match(struct device *dev, struct device_driver *driver)
{
	struct dahdi_chan *chan;

	chan = dev_to_chan(dev);
	chan_dbg(DEVICES, chan, "SYSFS\n");
	return 1;
}

static struct bus_type chan_bus_type = {
	.name		= "dahdi_channels",
	.match		= chan_match,
	.dev_attrs	= chan_dev_attrs,
};

static int chan_probe(struct device *dev)
{
	struct dahdi_chan *chan;

	chan = dev_to_chan(dev);
	chan_dbg(DEVICES, chan, "SYSFS\n");
	return 0;
}

static int chan_remove(struct device *dev)
{
	struct dahdi_chan *chan;

	chan = dev_to_chan(dev);
	chan_dbg(DEVICES, chan, "SYSFS\n");
	return 0;
}

static struct device_driver chan_driver = {
	.name = "dahdi",
	.bus = &chan_bus_type,
#ifndef OLD_HOTPLUG_SUPPORT
	.owner = THIS_MODULE,
#endif
	.probe = chan_probe,
	.remove = chan_remove
};

int chan_sysfs_create(struct dahdi_chan *chan)
{
	char chan_name[32];
	void *dummy;
	int res = 0;

	if (chan->channo >= 250)
		return 0;
	if (test_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags))
		return 0;
	snprintf(chan_name, sizeof(chan_name), "dahdi!%d", chan->channo);
	dummy = (void *)MAKE_DAHDI_DEV(chan->channo, chan_name);
	if (IS_ERR(dummy)) {
		res = PTR_ERR(dummy);
		chan_err(chan, "Failed creating sysfs device: %d\n",
				res);
		return res;
	}
	set_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags);
	return 0;
}

void chan_sysfs_remove(struct dahdi_chan *chan)
{
	if (!test_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags))
		return;
	DEL_DAHDI_DEV(chan->channo);
	clear_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags);
}

/*
 * Used by dahdi_transcode.c
 */
int dahdi_register_chardev(struct dahdi_chardev *dev)
{
	static const char *DAHDI_STRING = "dahdi!";
	char *udevname;

	udevname = kzalloc(strlen(dev->name) + sizeof(DAHDI_STRING) + 1,
			   GFP_KERNEL);
	if (!udevname)
		return -ENOMEM;

	strcpy(udevname, DAHDI_STRING);
	strcat(udevname, dev->name);
	MAKE_DAHDI_DEV(dev->minor, udevname);
	kfree(udevname);
	return 0;
}
EXPORT_SYMBOL(dahdi_register_chardev);

/*
 * Used by dahdi_transcode.c
 */
int dahdi_unregister_chardev(struct dahdi_chardev *dev)
{
	DEL_DAHDI_DEV(dev->minor);
	return 0;
}
EXPORT_SYMBOL(dahdi_unregister_chardev);

/*--------- Sysfs Device handling ----*/

/*
 * Describe fixed device files and maintain their
 * pointer so fixed_devfiles_remove() can always be called
 * and work cleanly
 */
static struct {
	int	minor;
	char	*name;
	void	*dev;	/* FIXME: wrong type because of old kernels */
} fixed_minors[] = {
	{ DAHDI_CTL,		"dahdi!ctl",	},
	{ DAHDI_TIMER,		"dahdi!timer",	},
	{ DAHDI_CHANNEL,	"dahdi!channel",},
	{ DAHDI_PSEUDO,		"dahdi!pseudo",	},
};

/*
 * Removes /dev/dahdi/{ctl,timer,channel,pseudo}
 *
 * It is safe to call it during initialization error handling,
 * as it skips non existing objects.
 */
static void fixed_devfiles_remove(void)
{
	int i;

	if (!dahdi_class)
		return;
	for (i = 0; i < ARRAY_SIZE(fixed_minors); i++) {
		void *d = fixed_minors[i].dev;
		if (d && !IS_ERR(d))
			dahdi_dbg(DEVICES, "Removing fixed device file %s\n",
				fixed_minors[i].name);
			DEL_DAHDI_DEV(fixed_minors[i].minor);
	}
}

/*
 * Creates /dev/dahdi/{ctl,timer,channel,pseudo}
 */
static int fixed_devfiles_create(void)
{
	int i;
	int res = 0;

	if (!dahdi_class) {
		dahdi_err("%s: dahdi_class is not initialized yet!\n",
				__func__);
		res = -ENODEV;
		goto cleanup;
	}
	for (i = 0; i < ARRAY_SIZE(fixed_minors); i++) {
		char *name = fixed_minors[i].name;
		int minor = fixed_minors[i].minor;
		void *dummy;

		dahdi_dbg(DEVICES, "Making fixed device file %s\n", name);
		dummy = (void *)MAKE_DAHDI_DEV(minor, name);
		if (IS_ERR(dummy)) {
			int res = PTR_ERR(dummy);

			dahdi_err("%s: failed (%d: %s). Error: %d\n",
					__func__, minor, name, res);
			goto cleanup;
		}
		fixed_minors[i].dev = dummy;
	}
	return 0;
cleanup:
	fixed_devfiles_remove();
	return res;
}

/*
 * Called during driver unload and while handling any error during
 * driver load.
 * Always clean any (and only) objects that were initialized (invariant)
 */
static void sysfs_channels_cleanup(void)
{
	fixed_devfiles_remove();
	if (dahdi_class) {
		dahdi_dbg(DEVICES, "Destroying DAHDI class:\n");
		class_destroy(dahdi_class);
		dahdi_class = NULL;
	}
	if (should_cleanup.channel_driver) {
		dahdi_dbg(DEVICES, "Removing channel driver\n");
		driver_unregister(&chan_driver);
		should_cleanup.channel_driver = 0;
	}
	if (should_cleanup.channels_bus) {
		dahdi_dbg(DEVICES, "Removing channels bus\n");
		bus_unregister(&chan_bus_type);
		should_cleanup.channels_bus = 0;
	}
}

int __init dahdi_sysfs_chan_init(const struct file_operations *fops)
{
	int res = 0;

	dahdi_dbg(DEVICES, "Registering channels bus\n");
	res = bus_register(&chan_bus_type);
	if (res) {
		dahdi_err("%s: bus_register(%s) failed. Error number %d\n",
				__func__, chan_bus_type.name, res);
		goto cleanup;
	}
	should_cleanup.channels_bus = 1;

	dahdi_dbg(DEVICES, "Registering channel driver\n");
	res = driver_register(&chan_driver);
	if (res) {
		dahdi_err("%s: driver_register(%s) failed. Error number %d",
				__func__, chan_driver.name, res);
		goto cleanup;
	}
	should_cleanup.channel_driver = 1;

	dahdi_class = class_create(THIS_MODULE, "dahdi");
	if (IS_ERR(dahdi_class)) {
		res = PTR_ERR(dahdi_class);
		dahdi_err("%s: class_create(dahi_chan) failed. Error: %d\n",
				__func__, res);
		goto cleanup;
	}
	res = fixed_devfiles_create();
	if (res)
		goto cleanup;
	return 0;
cleanup:
	sysfs_channels_cleanup();
	return res;
}

void dahdi_sysfs_chan_exit(void)
{
	sysfs_channels_cleanup();
}
