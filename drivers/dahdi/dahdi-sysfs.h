#ifndef	DAHDI_SYSFS_H
#define	DAHDI_SYSFS_H

/*
 * Very old hotplug support
 */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 9)
#define	OLD_HOTPLUG_SUPPORT	/* for older kernels */
#define	OLD_HOTPLUG_SUPPORT_269
#endif

#ifdef	OLD_HOTPLUG_SUPPORT_269
/* Copy from new kernels lib/kobject_uevent.c */
enum kobject_action {
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
	KOBJ_MOUNT,
	KOBJ_UMOUNT,
	KOBJ_OFFLINE,
	KOBJ_ONLINE,
};
#endif

/*
 * Hotplug replaced with uevent in 2.6.16
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#define	OLD_HOTPLUG_SUPPORT	/* for older kernels */
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#define	DEVICE_ATTR_READER(name, dev, buf) \
		ssize_t name(struct device *dev, \
			struct device_attribute *attr, \
			char *buf)
#define	DEVICE_ATTR_WRITER(name, dev, buf, count) \
		ssize_t name(struct device *dev, \
			struct device_attribute *attr, \
			const char *buf, size_t count)
#define BUS_ATTR_READER(name, dev, buf) \
	ssize_t name(struct device *dev, \
		struct device_attribute *attr, \
		char *buf)
#define BUS_ATTR_WRITER(name, dev, buf, count) \
		ssize_t name(struct device *dev, \
			struct device_attribute *attr, \
			const char *buf, size_t count)
#else
#define	DEVICE_ATTR_READER(name, dev, buf)	\
		ssize_t name(struct device *dev, char *buf)
#define	DEVICE_ATTR_WRITER(name, dev, buf, count)	\
		ssize_t name(struct device *dev, const char *buf, size_t count)
#define BUS_ATTR_READER(name, dev, buf) \
		ssize_t name(struct device *dev, char *buf)
#define BUS_ATTR_WRITER(name, dev, buf, count) \
		ssize_t name(struct device *dev, const char *buf, size_t count)
#endif

#define	DRIVER_ATTR_READER(name, drv, buf)	\
		ssize_t name(struct device_driver *drv, char * buf)

/* Device file creation macros */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, NULL, "%s", name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	class_device_create(class, NULL, devt, device, name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	class_device_create(class, devt, device, name)
#else
#define CLASS_DEV_CREATE(class, devt, device, name) \
	class_simple_device_add(class, devt, device, name)
#endif

/* Device file destruction macros */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#define CLASS_DEV_DESTROY(class, devt) \
	device_destroy(class, devt)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
#define CLASS_DEV_DESTROY(class, devt) \
	class_device_destroy(class, devt)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#define CLASS_DEV_DESTROY(class, devt) \
	class_simple_device_remove(devt)
#else
#define CLASS_DEV_DESTROY(class, devt) \
	class_simple_device_remove(class, devt)
#endif

#endif	/* DAHDI_SYSFS_H */
