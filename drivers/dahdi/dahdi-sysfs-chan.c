#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  warning "This module is tested only with 2.6 kernels"
#endif

#define DAHDI_PRINK_MACROS_USE_debug
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <dahdi/kernel.h>
#include "dahdi.h"
#include "dahdi-sysfs.h"

#define MAKE_DAHDI_DEV(num, name) \
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, num), NULL, name)
#define DEL_DAHDI_DEV(num) \
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, num))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
static struct class *dahdi_class;
#else
static struct class_simple *dahdi_class;
#define class_create class_simple_create
#define class_destroy class_simple_destroy
#endif

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
	dummy = (void *)CLASS_DEV_CREATE(dahdi_class,
		MKDEV(DAHDI_MAJOR, chan->channo),
		NULL, chan_name);
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
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, chan->channo));
	clear_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags);
}

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
	CLASS_DEV_CREATE(dahdi_class,
		MKDEV(DAHDI_MAJOR, dev->minor), NULL, udevname);
	kfree(udevname);
	return 0;
}
EXPORT_SYMBOL(dahdi_register_chardev);

int dahdi_unregister_chardev(struct dahdi_chardev *dev)
{
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, dev->minor));

	return 0;
}
EXPORT_SYMBOL(dahdi_unregister_chardev);

/* Only used to flag that the device exists: */
static struct {
	unsigned int ctl:1;
	unsigned int timer:1;
	unsigned int channel:1;
	unsigned int pseudo:1;
} dummy_dev;

int __init dahdi_sysfs_chan_init(const struct file_operations *fops)
{
	int res = 0;
	void *dev;

	dahdi_class = class_create(THIS_MODULE, "dahdi");
	if (IS_ERR(dahdi_class)) {
		res = PTR_ERR(dahdi_class);
		dahdi_err("%s: class_create(dahi_chan) failed. Error: %d\n",
			__func__, res);
		goto cleanup;
	}
	dahdi_dbg(DEVICES, "Creating /dev/dahdi/timer:\n");
	dev = MAKE_DAHDI_DEV(DAHDI_TIMER, "dahdi!timer");
	if (IS_ERR(dev)) {
		res = PTR_ERR(dev);
		goto cleanup;
	}
	dummy_dev.timer = 1;

	dahdi_dbg(DEVICES, "Creating /dev/dahdi/channel:\n");
	dev = MAKE_DAHDI_DEV(DAHDI_CHANNEL, "dahdi!channel");
	if (IS_ERR(dev)) {
		res = PTR_ERR(dev);
		goto cleanup;
	}
	dummy_dev.channel = 1;

	dahdi_dbg(DEVICES, "Creating /dev/dahdi/pseudo:\n");
	dev = MAKE_DAHDI_DEV(DAHDI_PSEUDO, "dahdi!pseudo");
	if (IS_ERR(dev)) {
		res = PTR_ERR(dev);
		goto cleanup;
	}
	dummy_dev.pseudo = 1;

	dahdi_dbg(DEVICES, "Creating /dev/dahdi/ctl:\n");
	dev = MAKE_DAHDI_DEV(DAHDI_CTL, "dahdi!ctl");
	if (IS_ERR(dev)) {
		res = PTR_ERR(dev);
		goto cleanup;
	}
	dummy_dev.ctl = 1;
	return 0;
cleanup:
	dahdi_sysfs_chan_exit();
	return res;
}

void dahdi_sysfs_chan_exit(void)
{
	if (dummy_dev.pseudo) {
		dahdi_dbg(DEVICES, "Removing /dev/dahdi/pseudo:\n");
		DEL_DAHDI_DEV(DAHDI_PSEUDO);
		dummy_dev.pseudo = 0;
	}
	if (dummy_dev.channel) {
		dahdi_dbg(DEVICES, "Removing /dev/dahdi/channel:\n");
		DEL_DAHDI_DEV(DAHDI_CHANNEL);
		dummy_dev.channel = 0;
	}
	if (dummy_dev.timer) {
		dahdi_dbg(DEVICES, "Removing /dev/dahdi/timer:\n");
		DEL_DAHDI_DEV(DAHDI_TIMER);
		dummy_dev.timer = 0;
	}
	if (dummy_dev.ctl) {
		dahdi_dbg(DEVICES, "Removing /dev/dahdi/ctl:\n");
		DEL_DAHDI_DEV(DAHDI_CTL);
		dummy_dev.ctl = 0;
	}
	if (dahdi_class && !IS_ERR(dahdi_class)) {
		dahdi_dbg(DEVICES, "Destroying DAHDI class\n");
		class_destroy(dahdi_class);
		dahdi_class = NULL;
	}
}
