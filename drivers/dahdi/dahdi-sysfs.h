#ifndef	DAHDI_SYSFS_H
#define	DAHDI_SYSFS_H

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

#define	DRIVER_ATTR_READER(name, drv, buf)	\
		ssize_t name(struct device_driver *drv, char * buf)

/* Device file creation macros */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, NULL, "%s", name)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#define CLASS_DEV_CREATE(class, devt, device, name) \
	device_create(class, device, devt, name)
#else
#define CLASS_DEV_CREATE(class, devt, device, name) \
	class_device_create(class, NULL, devt, device, name)
#endif

/* Device file destruction macros */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#define CLASS_DEV_DESTROY(class, devt) \
	device_destroy(class, devt)
#else
#define CLASS_DEV_DESTROY(class, devt) \
	class_device_destroy(class, devt)
#endif

/* Global */
int __init dahdi_sysfs_chan_init(const struct file_operations *fops);
void dahdi_sysfs_chan_exit(void);

/* Channel Handling */
int chan_sysfs_create(struct dahdi_chan *chan);
void chan_sysfs_remove(struct dahdi_chan *chan);

#endif	/* DAHDI_SYSFS_H */
