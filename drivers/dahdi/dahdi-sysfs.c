#include <linux/kernel.h>
#include <linux/version.h>
#define DAHDI_PRINK_MACROS_USE_debug
#include <dahdi/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <dahdi/version.h>

#include "dahdi.h"

/* FIXME: Move to kernel.h */
#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, NULL, "%s", name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_device_create(class, NULL, devt, device, name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_device_create(class, devt, device, name)
#else
#define CLASS_DEV_CREATE(class, devt, device, name) \
        class_simple_device_add(class, devt, device, name)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define CLASS_DEV_DESTROY(class, devt) \
	device_destroy(class, devt)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
#define CLASS_DEV_DESTROY(class, devt) \
	class_device_destroy(class, devt)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
#define CLASS_DEV_DESTROY(class, devt) \
	class_simple_device_remove(devt)
#else
#define CLASS_DEV_DESTROY(class, devt) \
	class_simple_device_remove(class, devt)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
static struct class *dahdi_class = NULL;
#else
static struct class_simple *dahdi_class = NULL;
#define class_create class_simple_create
#define class_destroy class_simple_destroy
#endif


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
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, dev->minor), NULL, udevname);
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

void span_sysfs_remove(struct dahdi_span *span)
{
	int x;
	for (x = 0; x < span->channels; x++) {
		struct dahdi_chan *chan = span->chans[x];
		if (!test_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags))
			continue;

		CLASS_DEV_DESTROY(dahdi_class,
				MKDEV(DAHDI_MAJOR, chan->channo));
		clear_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags);
	}
}

int span_sysfs_create(struct dahdi_span *span)
{
	int res = 0;
	int x;

	for (x = 0; x < span->channels; x++) {
		struct dahdi_chan *chan = span->chans[x];
		char chan_name[32];
		void *dummy;

		if (chan->channo >= 250)
			continue;
		if (test_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags))
			continue;

		snprintf(chan_name, sizeof(chan_name), "dahdi!%d",
				chan->channo);
		dummy = (void *)CLASS_DEV_CREATE(dahdi_class,
				MKDEV(DAHDI_MAJOR, chan->channo),
				NULL, chan_name);
		if (IS_ERR(dummy)) {
			res = PTR_ERR(dummy);
			chan_err(chan, "Failed creating sysfs device: %d\n",
					res);
			goto cleanup;
		}

		set_bit(DAHDI_FLAGBIT_DEVFILE, &chan->flags);
	}
	return 0;

cleanup:
	span_sysfs_remove(span);
	return res;
}

#define MAKE_DAHDI_DEV(num, name) \
	CLASS_DEV_CREATE(dahdi_class, MKDEV(DAHDI_MAJOR, num), NULL, name)
#define DEL_DAHDI_DEV(num) \
	CLASS_DEV_DESTROY(dahdi_class, MKDEV(DAHDI_MAJOR, num))

/* Only used to flag that the device exists: */
static struct {
	unsigned int ctl:1;
	unsigned int timer:1;
	unsigned int channel:1;
	unsigned int pseudo:1;
} dummy_dev;

void dahdi_sysfs_exit(void)
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
	if (dahdi_class) {
		dahdi_dbg(DEVICES, "Destroying DAHDI class:\n");
		class_destroy(dahdi_class);
		dahdi_class = NULL;
	}
	unregister_chrdev(DAHDI_MAJOR, "dahdi");
}

int __init dahdi_sysfs_init(const struct file_operations *dahdi_fops)
{
	int res = 0;
	void *dev;

	res = register_chrdev(DAHDI_MAJOR, "dahdi", dahdi_fops);
	if (res) {
		module_printk(KERN_ERR, "Unable to register DAHDI character device handler on %d\n", DAHDI_MAJOR);
		return res;
	}
	module_printk(KERN_INFO, "Telephony Interface Registered on major %d\n",
			DAHDI_MAJOR);
	module_printk(KERN_INFO, "Version: %s\n", DAHDI_VERSION);

	dahdi_class = class_create(THIS_MODULE, "dahdi");
	if (!dahdi_class) {
		res = -EEXIST;
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
	dahdi_sysfs_exit();
	return res;
}

