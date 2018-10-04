#ifndef MMAPBUS_H
#define MMAPBUS_H

#include <linux/device.h>

struct mmap_device {
	char *name;
	struct mmap_driver *driver;
	struct device dev;
};
#define to_mmap_device(x)	container_of(x, struct mmap_device, dev)

struct mmap_driver {
	struct module *module;
	struct device_driver driver;
};
#define to_mmap_driver(x)	container_of(x, struct mmap_driver, driver)

int register_mmap_bus(void);
void unregister_mmap_bus(void);
int register_mmap_device(struct mmap_device *dev);
void unregister_mmap_device(struct mmap_device *dev);
int register_mmap_driver(struct mmap_driver *driver);
void unregister_mmap_driver(struct mmap_driver *driver);

#endif
