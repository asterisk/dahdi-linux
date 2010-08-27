#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include "mmapbus.h"

static int mmap_match(struct device *dev, struct device_driver *driver)
{
	return !strncmp(dev_name(dev), driver->name, strlen(driver->name));
}
static int mmap_uevent(struct device *dev, char **envp, int num_envp, char *buffer, int buffer_size)
{
	envp[0] = buffer;
	envp[1] = NULL;
	return 0;
}

static void mmap_bus_release(struct device *dev)
{
}

static void mmap_dev_release(struct device *dev)
{
}

static struct bus_type mmap_bus_type = {
	.name = "mmap",
	.match = mmap_match,
	.uevent = mmap_uevent,
};

static struct device mmap_bus = {
	.bus_id = "mmap0",
	.release = mmap_bus_release,
};



int register_mmap_device(struct mmap_device *dev)
{
	dev->dev.bus = &mmap_bus_type;
	dev->dev.parent = &mmap_bus;
	dev->dev.release = mmap_dev_release;
	strncpy(dev->dev.bus_id, dev->name, BUS_ID_SIZE);
	return device_register(&dev->dev);
}

void unregister_mmap_device(struct mmap_device *dev)
{
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL(register_mmap_device);
EXPORT_SYMBOL(unregister_mmap_device);

int register_mmap_driver(struct mmap_driver *driver)
{
	driver->driver.bus = &mmap_bus_type;
	return driver_register(&driver->driver);
}

void unregister_mmap_driver(struct mmap_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(register_mmap_driver);
EXPORT_SYMBOL(unregister_mmap_driver);

int register_mmap_bus(void)
{
	int ret = 0;
	if ((ret = bus_register(&mmap_bus_type)) < 0)
		goto bus_type_reg;
	if ((ret = device_register(&mmap_bus)) < 0)
		goto bus_reg;
	return ret;

bus_reg:
	bus_unregister(&mmap_bus_type);
bus_type_reg:
	return ret;
}

void unregister_mmap_bus(void)
{
	device_unregister(&mmap_bus);
	bus_unregister(&mmap_bus_type);
}
EXPORT_SYMBOL(register_mmap_bus);
EXPORT_SYMBOL(unregister_mmap_bus);

MODULE_AUTHOR("Alexander Landau <landau.alex@gmail.com>");
MODULE_LICENSE("GPL");
