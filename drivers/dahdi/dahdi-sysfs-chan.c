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
#include <linux/ctype.h>
#include <dahdi/kernel.h>
#include "dahdi.h"
#include "dahdi-sysfs.h"

/* shortcuts, for code readability */
#define MAKE_DAHDI_DEV(num, name) \
	device_create(dahdi_class, NULL, MKDEV(DAHDI_MAJOR, num), \
		      NULL, "%s", name)

#define DEL_DAHDI_DEV(num) \
	device_destroy(dahdi_class, MKDEV(DAHDI_MAJOR, num))

static struct class *dahdi_class;

static dev_t dahdi_channels_devt;	/*!< Device number of first channel */
static struct cdev dahdi_channels_cdev;	/*!< Channels chardev's */

/*
 * Flags to remember what initializations already
 * succeeded.
 */
static struct {
	u32 channel_driver:1;
	u32 channels_bus:1;
	u32 cdev:1;
} should_cleanup;

#define chan_attr(field, format_string)		\
static BUS_ATTR_READER(field##_show, dev, buf)	\
{						\
	struct dahdi_chan *chan;		\
						\
	chan = dev_to_chan(dev);		\
	return sprintf(buf, format_string, chan->field); \
}

chan_attr(name, "%s\n");
chan_attr(channo, "%d\n");
chan_attr(chanpos, "%d\n");
chan_attr(blocksize, "%d\n");
#ifdef OPTIMIZE_CHANMUTE
chan_attr(chanmute, "%d\n");
#endif

static BUS_ATTR_READER(sigcap_show, dev, buf)
{
	struct dahdi_chan *chan;
	int len = 0;
	int i;
	uint sigtypes[] = {
		DAHDI_SIG_FXSLS,
		DAHDI_SIG_FXSGS,
		DAHDI_SIG_FXSKS,
		DAHDI_SIG_FXOLS,
		DAHDI_SIG_FXOGS,
		DAHDI_SIG_FXOKS,
		DAHDI_SIG_EM,
		DAHDI_SIG_CLEAR,
		DAHDI_SIG_HDLCRAW,
		DAHDI_SIG_HDLCFCS,
		DAHDI_SIG_HDLCNET,
		DAHDI_SIG_SLAVE,
		DAHDI_SIG_SF,
		DAHDI_SIG_CAS,
		DAHDI_SIG_EM_E1,
		DAHDI_SIG_DACS_RBS,
		DAHDI_SIG_HARDHDLC,
		DAHDI_SIG_MTP2,
	};
	chan = dev_to_chan(dev);

	for (i = 0; i < ARRAY_SIZE(sigtypes); i++) {
		uint x = chan->sigcap & sigtypes[i];
		if (x == sigtypes[i])
			len += sprintf(buf + len, "%s ", sigstr(x));
	}
	while (len > 0 && isspace(buf[len - 1])) /* trim */
		len--;
	len += sprintf(buf + len, "\n");
	return len;
}

static BUS_ATTR_READER(sig_show, dev, buf)
{
	struct dahdi_chan *chan;

	chan = dev_to_chan(dev);
	return sprintf(buf, "%s\n", sigstr(chan->sig));
}

static BUS_ATTR_READER(in_use_show, dev, buf)
{
	struct dahdi_chan *chan;

	chan = dev_to_chan(dev);
	return sprintf(buf, "%d\n", test_bit(DAHDI_FLAGBIT_OPEN, &chan->flags));
}

static BUS_ATTR_READER(alarms_show, dev, buf)
{
	struct dahdi_chan *chan;
	int len;

	chan = dev_to_chan(dev);
	len = fill_alarm_string(buf, PAGE_SIZE, chan->chan_alarms);
	buf[len++] = '\n';
	return len;
}

static BUS_ATTR_READER(ec_factory_show, dev, buf)
{
	struct dahdi_chan *chan;
	int len = 0;

	chan = dev_to_chan(dev);
	if (chan->ec_factory)
		len += sprintf(buf, "%s", chan->ec_factory->get_name(chan));
	buf[len++] = '\n';
	return len;
}

static BUS_ATTR_READER(ec_state_show, dev, buf)
{
	struct dahdi_chan *chan;
	int len = 0;

	chan = dev_to_chan(dev);
	if (chan->ec_factory)
		len += sprintf(buf, "%sACTIVE", (chan->ec_state) ? "" : "IN");
	buf[len++] = '\n';
	return len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
static struct device_attribute chan_dev_attrs[] = {
	__ATTR_RO(name),
	__ATTR_RO(channo),
	__ATTR_RO(chanpos),
	__ATTR_RO(sig),
	__ATTR_RO(sigcap),
	__ATTR_RO(alarms),
	__ATTR_RO(ec_factory),
	__ATTR_RO(ec_state),
	__ATTR_RO(blocksize),
#ifdef OPTIMIZE_CHANMUTE
	__ATTR_RO(chanmute),
#endif
	__ATTR_RO(in_use),
	__ATTR_NULL,
};
#else
static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(channo);
static DEVICE_ATTR_RO(chanpos);
static DEVICE_ATTR_RO(sig);
static DEVICE_ATTR_RO(sigcap);
static DEVICE_ATTR_RO(alarms);
static DEVICE_ATTR_RO(ec_factory);
static DEVICE_ATTR_RO(ec_state);
static DEVICE_ATTR_RO(blocksize);
#ifdef OPTIMIZE_CHANMUTE
static DEVICE_ATTR_RO(chanmute);
#endif
static DEVICE_ATTR_RO(in_use);

static struct attribute *chan_dev_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_channo.attr,
	&dev_attr_chanpos.attr,
	&dev_attr_sig.attr,
	&dev_attr_sigcap.attr,
	&dev_attr_alarms.attr,
	&dev_attr_ec_factory.attr,
	&dev_attr_ec_state.attr,
	&dev_attr_blocksize.attr,
#ifdef OPTIMIZE_CHANMUTE
	&dev_attr_chanmute.attr,
#endif
	&dev_attr_in_use.attr,
	NULL,
};
ATTRIBUTE_GROUPS(chan_dev);
#endif

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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	.dev_attrs	= chan_dev_attrs,
#else
	.dev_groups	= chan_dev_groups,
#endif
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
	struct device *dev;
	struct dahdi_span *span;
	int res;
	dev_t devt;

	chan_dbg(DEVICES, chan, "Creating channel %d\n", chan->channo);
	if (test_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags))
		return 0;
	span = chan->span;
	devt = MKDEV(MAJOR(dahdi_channels_devt), chan->channo);
	dev = &chan->chan_device;
	memset(dev, 0, sizeof(*dev));
	dev->devt = devt;
	dev->bus = &chan_bus_type;
	dev->parent = span->span_device;
	/*
	 * FIXME: the name cannot be longer than KOBJ_NAME_LEN
	 */
	dev_set_name(dev, "dahdi!chan!%03d!%03d", span->spanno, chan->chanpos);
	dev_set_drvdata(dev, chan);
	dev->release = chan_release;
	res = device_register(dev);
	if (res) {
		chan_err(chan, "%s: device_register failed: %d\n",
				__func__, res);
		put_device(dev);
		return res;
	}
	set_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags);
	return 0;
}

void chan_sysfs_remove(struct dahdi_chan *chan)
{
	struct device *dev = &chan->chan_device;

	chan_dbg(DEVICES, chan, "Destroying channel %d\n", chan->channo);
	if (!dev_get_drvdata(dev))
		return;
	if (!test_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags))
		return;
	dev = &chan->chan_device;
	BUG_ON(dev_get_drvdata(dev) != chan);
	device_unregister(dev);
	/* FIXME: should have been done earlier in dahdi_chan_unreg */
	chan->channo = -1;
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
	if (should_cleanup.cdev) {
		dahdi_dbg(DEVICES, "removing channels cdev\n");
		cdev_del(&dahdi_channels_cdev);
		should_cleanup.cdev = 0;
	}
	if (dahdi_channels_devt) {
		dahdi_dbg(DEVICES, "unregistering chrdev_region\n");
		unregister_chrdev_region(dahdi_channels_devt,
			DAHDI_MAX_CHANNELS);
	}

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
	dahdi_dbg(DEVICES, "allocating chrdev_region\n");
	res = alloc_chrdev_region(&dahdi_channels_devt,
			0,
			DAHDI_MAX_CHANNELS,
			"dahdi_channels");
	if (res) {
		dahdi_err("%s: Failed allocating chrdev for %d channels (%d)",
			__func__, DAHDI_MAX_CHANNELS, res);
		goto cleanup;
	}
	dahdi_dbg(DEVICES, "adding channels cdev\n");
	cdev_init(&dahdi_channels_cdev, fops);
	res = cdev_add(&dahdi_channels_cdev, dahdi_channels_devt,
		DAHDI_MAX_CHANNELS);
	if (res) {
		dahdi_err("%s: cdev_add() failed (%d)", __func__, res);
		goto cleanup;
	}
	should_cleanup.cdev = 1;
	return 0;
cleanup:
	sysfs_channels_cleanup();
	return res;
}

void dahdi_sysfs_chan_exit(void)
{
	sysfs_channels_cleanup();
}
