/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#ifdef	PROTOCOL_DEBUG
#include <linux/ctype.h>
#endif
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/delay.h>	/* for msleep() to debug */
#include "xpd.h"
#include "xpp_dahdi.h"
#include "xbus-core.h"
#include "dahdi_debug.h"

static const char rcsid[] = "$Id$";

/* Command line parameters */
extern int debug;

/*--------- xpp driver attributes -*/
static ssize_t sync_show(struct device_driver *driver, char *buf)
{
	DBG(SYNC, "\n");
	return fill_sync_string(buf, PAGE_SIZE);
}

static ssize_t sync_store(struct device_driver *driver, const char *buf,
			  size_t count)
{
	/* DBG(SYNC, "%s\n", buf); */
	return exec_sync_command(buf, count);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
static struct driver_attribute xpp_attrs[] = {
	__ATTR(sync, S_IRUGO | S_IWUSR, sync_show, sync_store),
	__ATTR_NULL,
};
#else
static DRIVER_ATTR_RW(sync);
static struct attribute *xpp_attrs[] = {
	&driver_attr_sync.attr,
	NULL,
};
ATTRIBUTE_GROUPS(xpp);
#endif

/*--------- Sysfs Bus handling ----*/
static DEVICE_ATTR_READER(xbus_state_show, dev, buf)
{
	xbus_t *xbus;
	int ret;

	xbus = dev_to_xbus(dev);
	ret = XBUS_STATE(xbus);
	ret = snprintf(buf, PAGE_SIZE, "%s (%d)\n", xbus_statename(ret), ret);
	return ret;
}

static DEVICE_ATTR_WRITER(xbus_state_store, dev, buf, count)
{
	xbus_t *xbus;

	xbus = dev_to_xbus(dev);
	XBUS_DBG(GENERAL, xbus, "%s\n", buf);
	if (strncmp(buf, "stop", 4) == 0)
		xbus_deactivate(xbus);
	else if (XBUS_IS(xbus, IDLE) && strncmp(buf, "start", 5) == 0)
		xbus_activate(xbus);
	else {
		XBUS_NOTICE(xbus,
			    "%s: Illegal action %s in state %s. Ignored.\n",
			    __func__, buf, xbus_statename(XBUS_STATE(xbus)));
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR_READER(status_show, dev, buf)
{
	xbus_t *xbus;
	int ret;

	xbus = dev_to_xbus(dev);
	ret =
	    snprintf(buf, PAGE_SIZE, "%s\n",
		     (XBUS_FLAGS(xbus, CONNECTED)) ? "connected" : "missing");
	return ret;
}

static DEVICE_ATTR_READER(timing_show, dev, buf)
{
	xbus_t *xbus;
	struct xpp_drift *driftinfo;
	int len = 0;
	struct timeval now;

	do_gettimeofday(&now);
	xbus = dev_to_xbus(dev);
	driftinfo = &xbus->drift;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%-3s",
		     sync_mode_name(xbus->sync_mode));
	if (xbus->sync_mode == SYNC_MODE_PLL) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     " %5d: lost (%4d,%4d) : ", xbus->ticker.cycle,
			     driftinfo->lost_ticks, driftinfo->lost_tick_count);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "DRIFT %3d %ld sec ago", xbus->sync_adjustment,
			     (xbus->pll_updated_at ==
			      0) ? 0 : now.tv_sec - xbus->pll_updated_at);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

#ifdef	SAMPLE_TICKS
/*
 * tick sampling: Measure offset from reference ticker:
 *   - Recording start when writing to:
 *       /sys/bus/astribanks/devices/xbus-??/samples
 *   - Recording ends when filling SAMPLE_SIZE ticks
 *   - Results are read from the same sysfs file.
 *   - Trying to read/write during recording, returns -EBUSY.
 */
static DEVICE_ATTR_READER(samples_show, dev, buf)
{
	xbus_t *xbus;
	int len = 0;
	int i;

	xbus = dev_to_xbus(dev);
	if (xbus->sample_running)
		return -EBUSY;
	for (i = 0; i < SAMPLE_SIZE; i++) {
		if (len > PAGE_SIZE - 20)
			break;
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			     xbus->sample_ticks[i]);
	}
	return len;
}

static DEVICE_ATTR_WRITER(samples_store, dev, buf, count)
{
	xbus_t *xbus;

	xbus = dev_to_xbus(dev);
	if (xbus->sample_running)
		return -EBUSY;
	memset(xbus->sample_ticks, 0, sizeof(*xbus->sample_ticks));
	xbus->sample_pos = 0;
	xbus->sample_running = 1;
	return count;
}
#endif

/*
 * Clear statistics
 */
static DEVICE_ATTR_WRITER(cls_store, dev, buf, count)
{
	xbus_t *xbus;
	struct xpp_drift *driftinfo;

	xbus = dev_to_xbus(dev);
	driftinfo = &xbus->drift;
	driftinfo->lost_ticks = 0;
	driftinfo->lost_tick_count = 0;
	xbus->min_tx_sync = INT_MAX;
	xbus->max_tx_sync = 0;
	xbus->min_rx_sync = INT_MAX;
	xbus->max_rx_sync = 0;
#ifdef	SAMPLE_TICKS
	memset(xbus->sample_ticks, 0, sizeof(*xbus->sample_ticks));
#endif
	return count;
}

static DEVICE_ATTR_READER(waitfor_xpds_show, dev, buf)
{
	xbus_t *xbus;
	int len;

	xbus = dev_to_xbus(dev);
	len = waitfor_xpds(xbus, buf);
	return len;
}

static DEVICE_ATTR_READER(refcount_xbus_show, dev, buf)
{
	xbus_t *xbus;
	int len;

	xbus = dev_to_xbus(dev);
	len = sprintf(buf, "%d\n", refcount_xbus(xbus));
	return len;
}

static DEVICE_ATTR_READER(driftinfo_show, dev, buf)
{
	xbus_t *xbus;
	struct xpp_drift *di;
	struct xpp_ticker *ticker;
	struct timeval now;
	int len = 0;
	int hours;
	int minutes;
	int seconds;
	int speed_range;
	int uframes_inaccuracy;
	int i;

	xbus = dev_to_xbus(dev);
	di = &xbus->drift;
	ticker = &xbus->ticker;
	/*
	 * Calculate lost ticks time
	 */
	do_gettimeofday(&now);
	seconds = now.tv_sec - di->last_lost_tick.tv.tv_sec;
	minutes = seconds / 60;
	seconds = seconds % 60;
	hours = minutes / 60;
	minutes = minutes % 60;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"%-15s: %8d (was %d:%02d:%02d ago)\n", "lost_ticks",
		di->lost_ticks, hours, minutes, seconds);
	speed_range = abs(di->max_speed - di->min_speed);
	uframes_inaccuracy = di->sync_inaccuracy / 125;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"%-15s: %8d ", "instability",
		speed_range + uframes_inaccuracy);
	if (xbus->sync_mode == SYNC_MODE_AB) {
		buf[len++] = '-';
	} else {
		for (i = 0;
		     len < PAGE_SIZE - 1
		     && i < speed_range + uframes_inaccuracy; i++)
			buf[len++] = '#';
	}
	buf[len++] = '\n';
	len += snprintf(buf + len, PAGE_SIZE - len, "%-15s: %8d (uframes)\n",
		"inaccuracy", uframes_inaccuracy);
	len += snprintf(buf + len, PAGE_SIZE - len, "%-15s: %8d\n",
		"speed_range", speed_range);
#define	SHOW(ptr, item) \
	do { \
		len += snprintf(buf + len, PAGE_SIZE - len, \
		"%-15s: %8d\n", #item, (ptr)->item); \
	} while (0)
	SHOW(xbus, sync_adjustment);
	len += snprintf(buf + len, PAGE_SIZE - len, "%-15s: %8d\n",
		"offset (usec)", di->offset_prev);
	SHOW(di, offset_range);
	len += snprintf(buf + len, PAGE_SIZE - len, "%-15s: %8d\n",
		"best_speed", (di->max_speed + di->min_speed) / 2);
	SHOW(di, min_speed);
	SHOW(di, max_speed);
	SHOW(ticker, cycle);
	SHOW(ticker, tick_period);
	SHOW(ticker, count);
#undef	SHOW
	return len;
}

#define xbus_attr(field, format_string)                                    \
static ssize_t                                                             \
field##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{                                                                          \
	xbus_t	*xbus;                                                     \
	xbus = dev_to_xbus(dev);                                           \
	return sprintf(buf, format_string, xbus->field);                   \
}

xbus_attr(connector, "%s\n");
xbus_attr(label, "%s\n");

static struct device_attribute xbus_dev_attrs[] = {
	__ATTR_RO(connector),
	__ATTR_RO(label),
	__ATTR_RO(status),
	__ATTR_RO(timing),
	__ATTR_RO(refcount_xbus),
	__ATTR_RO(waitfor_xpds),
	__ATTR_RO(driftinfo),
	__ATTR(cls, S_IWUSR, NULL, cls_store),
	__ATTR(xbus_state, S_IRUGO | S_IWUSR, xbus_state_show,
	       xbus_state_store),
#ifdef	SAMPLE_TICKS
	__ATTR(samples, S_IWUSR | S_IRUGO, samples_show, samples_store),
#endif
	__ATTR_NULL,
};

static int astribank_match(struct device *dev, struct device_driver *driver)
{
	DBG(DEVICES, "SYSFS MATCH: dev->bus_id = %s, driver->name = %s\n",
	    dev_name(dev), driver->name);
	return 1;
}

#define	XBUS_VAR_BLOCK	\
	do {		\
		XBUS_ADD_UEVENT_VAR("XPP_INIT_DIR=%s", initdir);	\
		XBUS_ADD_UEVENT_VAR("XBUS_NUM=%02d", xbus->num);	\
		XBUS_ADD_UEVENT_VAR("XBUS_NAME=%s", xbus->busname);	\
	} while (0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#define XBUS_ADD_UEVENT_VAR(fmt, val...)			\
	do {							\
		int err = add_uevent_var(envp, num_envp, &i,	\
				buffer, buffer_size, &len,	\
				fmt, val);			\
		if (err)					\
			return err;				\
	} while (0)

static int astribank_uevent(struct device *dev, char **envp, int num_envp,
			    char *buffer, int buffer_size)
{
	xbus_t *xbus;
	int i = 0;
	int len = 0;
	extern char *initdir;

	if (!dev)
		return -ENODEV;
	xbus = dev_to_xbus(dev);
	DBG(GENERAL, "SYFS bus_id=%s xbus=%s\n", dev_name(dev), xbus->busname);
	XBUS_VAR_BLOCK;
	envp[i] = NULL;
	return 0;
}

#else
#define XBUS_ADD_UEVENT_VAR(fmt, val...)			\
	do {							\
		int err = add_uevent_var(kenv, fmt, val);	\
		if (err)					\
			return err;				\
	} while (0)

static int astribank_uevent(struct device *dev, struct kobj_uevent_env *kenv)
{
	xbus_t *xbus;
	extern char *initdir;

	if (!dev)
		return -ENODEV;
	xbus = dev_to_xbus(dev);
	DBG(GENERAL, "SYFS bus_id=%s xbus=%s\n", dev_name(dev), xbus->busname);
	XBUS_VAR_BLOCK;
	return 0;
}

#endif

void astribank_uevent_send(xbus_t *xbus, enum kobject_action act)
{
	struct kobject *kobj;

	kobj = &xbus->astribank.kobj;
	XBUS_DBG(DEVICES, xbus, "SYFS bus_id=%s action=%d\n",
		 dev_name(&xbus->astribank), act);
	kobject_uevent(kobj, act);
}

static void astribank_release(struct device *dev)
{
	xbus_t *xbus;

	BUG_ON(!dev);
	xbus = dev_to_xbus(dev);
	if (XBUS_FLAGS(xbus, CONNECTED)) {
		XBUS_ERR(xbus, "Try to release CONNECTED device.\n");
		BUG();
	}
	if (!XBUS_IS(xbus, IDLE) && !XBUS_IS(xbus, FAIL)
	    && !XBUS_IS(xbus, DEACTIVATED)) {
		XBUS_ERR(xbus, "Try to release in state %s\n",
			 xbus_statename(XBUS_STATE(xbus)));
		BUG();
	}
	XBUS_INFO(xbus, "[%s] Astribank Release\n", xbus->label);
	xbus_free(xbus);
}

static struct bus_type toplevel_bus_type = {
	.name = "astribanks",
	.match = astribank_match,
	.uevent = astribank_uevent,
	.dev_attrs = xbus_dev_attrs,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
	.drv_attrs = xpp_attrs,
#else
	.drv_groups = xpp_groups,
#endif
};

static int astribank_probe(struct device *dev)
{
	xbus_t *xbus;

	xbus = dev_to_xbus(dev);
	XBUS_DBG(DEVICES, xbus, "SYSFS\n");
	return 0;
}

static int astribank_remove(struct device *dev)
{
	xbus_t *xbus;

	xbus = dev_to_xbus(dev);
	XBUS_INFO(xbus, "[%s] Atribank Remove\n", xbus->label);
	return 0;
}

static struct device_driver xpp_driver = {
	.name = "xppdrv",
	.bus = &toplevel_bus_type,
	.probe = astribank_probe,
	.remove = astribank_remove,
	.owner = THIS_MODULE
};

/*--------- Sysfs XPD handling ----*/

static DEVICE_ATTR_READER(chipregs_show, dev, buf)
{
	xpd_t *xpd;
	unsigned long flags;
	reg_cmd_t *regs;
	bool do_datah;
	char datah_str[50];
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	regs = &xpd->last_reply;
	len += sprintf(buf + len,
		"# Writing bad data to this file may damage your hardware!\n");
	len += sprintf(buf + len, "# Consult firmware docs first\n");
	len += sprintf(buf + len, "#\n");
	do_datah = REG_FIELD(regs, do_datah) ? 1 : 0;
	if (do_datah) {
		snprintf(datah_str, ARRAY_SIZE(datah_str), "\t%02X",
			 REG_FIELD(regs, data_high));
	} else
		datah_str[0] = '\0';
	if (REG_FIELD(regs, do_subreg)) {
		len +=
		    sprintf(buf + len, "#CH\tOP\tReg.\tSub\tDL%s\n",
			    (do_datah) ? "\tDH" : "");
		len +=
		    sprintf(buf + len, "%2d\tRS\t%02X\t%02X\t%02X%s\n",
			    regs->portnum, REG_FIELD(regs, regnum),
			    REG_FIELD(regs, subreg), REG_FIELD(regs, data_low),
			    datah_str);
	} else {
		len +=
		    sprintf(buf + len, "#CH\tOP\tReg.\tDL%s\n",
			    (do_datah) ? "\tDH" : "");
		len +=
		    sprintf(buf + len, "%2d\tRD\t%02X\t%02X%s\n", regs->portnum,
			    REG_FIELD(regs, regnum), REG_FIELD(regs, data_low),
			    datah_str);
	}
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_WRITER(chipregs_store, dev, buf, count)
{
	xpd_t *xpd;
	const char *p;
	char tmp[MAX_PROC_WRITE];
	int i;
	int ret;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	//XPD_DBG(GENERAL, xpd, "%s\n", buf);
	if (!xpd)
		return -ENODEV;
	p = buf;
	while ((p - buf) < count) {
		i = strcspn(p, "\r\n");
		if (i > 0) {
			if (i >= MAX_PROC_WRITE) {
				XPD_NOTICE(xpd, "Command too long (%d chars)\n",
					   i);
				return -E2BIG;
			}
			memcpy(tmp, p, i);
			tmp[i] = '\0';
			ret = parse_chip_command(xpd, tmp);
			if (ret < 0) {
				XPD_NOTICE(xpd,
					   "Failed writing command: '%s'\n",
					   tmp);
				return ret;
			}
		}
		p += i + 1;
		/* Don't flood command_queue */
		if (xframe_queue_count(&xpd->xbus->command_queue) > 5)
			msleep(6);
	}
	return count;
}

static DEVICE_ATTR_READER(blink_show, dev, buf)
{
	xpd_t *xpd;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	len += sprintf(buf, "0x%lX\n", xpd->blink_mode);
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_WRITER(blink_store, dev, buf, count)
{
	xpd_t *xpd;
	char *endp;
	unsigned long blink;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	//XPD_DBG(GENERAL, xpd, "%s\n", buf);
	if (!xpd)
		return -ENODEV;
	blink = simple_strtoul(buf, &endp, 0);
	if (*endp != '\0' && *endp != '\n' && *endp != '\r')
		return -EINVAL;
	if (blink > 0xFFFF)
		return -EINVAL;
	XPD_DBG(GENERAL, xpd, "BLINK channels: 0x%lX\n", blink);
	xpd->blink_mode = blink;
	return count;
}

static DEVICE_ATTR_READER(span_show, dev, buf)
{
	xpd_t *xpd;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	len +=
	    sprintf(buf, "%d\n",
		    SPAN_REGISTERED(xpd) ? PHONEDEV(xpd).span.spanno : 0);
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEFINE_MUTEX(span_store_mutex);

/*
 * For backward compatibility with old dahdi-tools
 * Remove after dahdi_registration is upgraded
 */
static DEVICE_ATTR_WRITER(span_store, dev, buf, count)
{
	xpd_t *xpd;
	int dahdi_reg;
	int ret;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	ret = sscanf(buf, "%d", &dahdi_reg);
	if (ret != 1)
		return -EINVAL;
	if (!XBUS_IS(xpd->xbus, READY))
		return -ENODEV;
	XPD_DBG(DEVICES, xpd, "%s -- deprecated (should use assigned-spans)\n",
		(dahdi_reg) ? "register" : "unregister");
	ret = mutex_lock_interruptible(&span_store_mutex);
	if (ret < 0) {
		XBUS_ERR(xpd->xbus, "span_store_mutex already taken\n");
		return ret;
	}
	if (xbus_is_registered(xpd->xbus)) {
		if (dahdi_reg) {
			XPD_DBG(DEVICES, xpd,
				"already registered %s. Ignored.\n",
				xpd->xbus->busname);
		} else {
			xbus_unregister_dahdi_device(xpd->xbus);
		}
	} else {
		if (!dahdi_reg) {
			XPD_DBG(DEVICES, xpd,
				"already unregistered %s. Ignored.\n",
				xpd->xbus->busname);
		} else {
			xbus_register_dahdi_device(xpd->xbus);
		}
	}
	mutex_unlock(&span_store_mutex);
	return count;
}

static DEVICE_ATTR_READER(type_show, dev, buf)
{
	xpd_t *xpd;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	len += sprintf(buf, "%s\n", xpd->type_name);
	return len;
}

static DEVICE_ATTR_READER(offhook_show, dev, buf)
{
	xpd_t *xpd;
	int len = 0;
	int i;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	for_each_line(xpd, i) {
		len += sprintf(buf + len, "%d ", IS_OFFHOOK(xpd, i));
	}
	if (len)
		len--;		/* backout last space */
	len += sprintf(buf + len, "\n");
	return len;
}

static DEVICE_ATTR_READER(timing_priority_show, dev, buf)
{
	xpd_t *xpd;
	unsigned long flags;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	len += sprintf(buf + len, "%d\n", PHONEDEV(xpd).timing_priority);
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_READER(refcount_xpd_show, dev, buf)
{
	xpd_t *xpd;
	int len = 0;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	len += sprintf(buf + len, "%d\n", refcount_xpd(xpd));
	return len;
}

static int xpd_match(struct device *dev, struct device_driver *driver)
{
	struct xpd_driver *xpd_driver;
	xpd_t *xpd;

	xpd_driver = driver_to_xpd_driver(driver);
	xpd = dev_to_xpd(dev);
	if (xpd_driver->type != xpd->type) {
		XPD_DBG(DEVICES, xpd,
			"SYSFS match fail: xpd->type = %d, "
			"xpd_driver->type = %d\n",
			xpd->type, xpd_driver->type);
		return 0;
	}
	XPD_DBG(DEVICES, xpd,
		"SYSFS MATCH: type=%d dev->bus_id = %s, driver->name = %s\n",
		xpd->type, dev_name(dev), driver->name);
	return 1;
}

static struct device_attribute xpd_dev_attrs[] = {
	__ATTR(chipregs, S_IRUGO | S_IWUSR, chipregs_show, chipregs_store),
	__ATTR(blink, S_IRUGO | S_IWUSR, blink_show, blink_store),
	__ATTR(span, S_IRUGO | S_IWUSR, span_show, span_store),
	__ATTR_RO(type),
	__ATTR_RO(offhook),
	__ATTR_RO(timing_priority),
	__ATTR_RO(refcount_xpd),
	__ATTR_NULL,
};

static struct bus_type xpd_type = {
	.name = "xpds",
	.match = xpd_match,
	.dev_attrs = xpd_dev_attrs,
};

int xpd_driver_register(struct device_driver *driver)
{
	int ret;

	DBG(DEVICES, "%s\n", driver->name);
	driver->bus = &xpd_type;
	if ((ret = driver_register(driver)) < 0) {
		ERR("%s: driver_register(%s) failed. Error number %d", __func__,
		    driver->name, ret);
	}
	return ret;
}
EXPORT_SYMBOL(xpd_driver_register);

void xpd_driver_unregister(struct device_driver *driver)
{
	DBG(DEVICES, "%s\n", driver->name);
	driver_unregister(driver);
}
EXPORT_SYMBOL(xpd_driver_unregister);

static void xpd_release(struct device *dev)
{
	xpd_t *xpd;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	xpd_remove(xpd);
}

int xpd_device_register(xbus_t *xbus, xpd_t *xpd)
{
	struct device *dev = &xpd->xpd_dev;
	int ret;

	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	dev->bus = &xpd_type;
	dev->parent = &xbus->astribank;
	dev_set_name(dev, "%02d:%1x:%1x", xbus->num, xpd->addr.unit,
		     xpd->addr.subunit);
	dev_set_drvdata(dev, xpd);
	dev->release = xpd_release;
	ret = device_register(dev);
	if (ret) {
		XPD_ERR(xpd, "%s: device_register failed: %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

void xpd_device_unregister(xpd_t *xpd)
{
	xbus_t *xbus;
	struct device *dev;

	xbus = xpd->xbus;
	BUG_ON(!xbus);
	XPD_DBG(DEVICES, xpd, "SYSFS\n");
	dev = &xpd->xpd_dev;
	if (!dev_get_drvdata(dev))
		return;
	BUG_ON(dev_get_drvdata(dev) != xpd);
	device_unregister(dev);
	dev_set_drvdata(dev, NULL);
}

static DEVICE_ATTR_READER(echocancel_show, dev, buf)
{
	xpd_t *xpd;
	unsigned long flags;
	int len = 0;
	xpp_line_t ec_mask = 0;
	int i;
	int ret;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	if (!xpd)
		return -ENODEV;
	if (!ECHOOPS(xpd->xbus))
		return -ENODEV;
	spin_lock_irqsave(&xpd->lock, flags);
	for (i = 0; i < PHONEDEV(xpd).channels; i++) {
		ret = CALL_EC_METHOD(ec_get, xpd->xbus, xpd, i);
		if (ret < 0) {
			LINE_ERR(xpd, i, "ec_get failed\n");
			len = -ENODEV;
			goto out;
		}
		if (ret)
			ec_mask |= (1 << i);
	}
	len += sprintf(buf, "0x%08X\n", ec_mask);
out:
	spin_unlock_irqrestore(&xpd->lock, flags);
	return len;
}

static DEVICE_ATTR_WRITER(echocancel_store, dev, buf, count)
{
	xpd_t *xpd;
	char *endp;
	unsigned long mask;
	int channels;
	int ret;

	BUG_ON(!dev);
	xpd = dev_to_xpd(dev);
	XPD_DBG(GENERAL, xpd, "%s\n", buf);
	if (!xpd)
		return -ENODEV;
	if (!ECHOOPS(xpd->xbus)) {
		XPD_ERR(xpd, "No echo canceller in this XBUS\n");
		return -ENODEV;
	}
	if (!IS_PHONEDEV(xpd)) {
		XPD_ERR(xpd, "Not a phone device\n");
		return -ENODEV;
	}
	channels = PHONEDEV(xpd).channels;
	mask = simple_strtoul(buf, &endp, 0);
	if (*endp != '\0' && *endp != '\n' && *endp != '\r') {
		XPD_ERR(xpd, "Too many channels: %d\n", channels);
		return -EINVAL;
	}
	if (mask != 0 && __ffs(mask) > channels) {
		XPD_ERR(xpd,
			"Channel mask (0x%lX) larger than "
			"available channels (%d)\n",
			mask, channels);
		return -EINVAL;
	}
	XPD_DBG(GENERAL, xpd, "ECHOCANCEL channels: 0x%lX\n", mask);
	ret = CALL_PHONE_METHOD(echocancel_setmask, xpd, mask);
	if (ret < 0) {
		XPD_ERR(xpd, "echocancel_setmask failed\n");
		return ret;
	}
	return count;
}

static DEVICE_ATTR(echocancel, S_IRUGO | S_IWUSR, echocancel_show,
		   echocancel_store);

int echocancel_xpd(xpd_t *xpd, int on)
{
	int ret;

	XPD_DBG(GENERAL, xpd, "echocancel_xpd(%s)\n", (on) ? "on" : "off");
	if (!on) {
		device_remove_file(xpd->echocancel, &dev_attr_echocancel);
		return 0;
	}

	ret = device_create_file(&xpd->xpd_dev, &dev_attr_echocancel);
	if (ret)
		XPD_ERR(xpd, "%s: device_create_file(echocancel) failed: %d\n",
			__func__, ret);

	return ret;
}
EXPORT_SYMBOL(echocancel_xpd);

/*--------- Sysfs Device handling ----*/

void xbus_sysfs_transport_remove(xbus_t *xbus)
{
	struct device *astribank;

	BUG_ON(!xbus);
	XBUS_DBG(DEVICES, xbus, "\n");
	astribank = &xbus->astribank;
	sysfs_remove_link(&astribank->kobj, "transport");
}

int xbus_sysfs_transport_create(xbus_t *xbus)
{
	struct device *astribank;
	struct device *transport_device;
	int ret = 0;

	BUG_ON(!xbus);
	XBUS_DBG(DEVICES, xbus, "\n");
	astribank = &xbus->astribank;
	BUG_ON(!astribank);
	transport_device = xbus->transport.transport_device;
	if (!transport_device) {
		XBUS_ERR(xbus, "%s: Missing transport_device\n", __func__);
		return -ENODEV;
	}
	ret =
	    sysfs_create_link(&astribank->kobj, &transport_device->kobj,
			      "transport");
	if (ret < 0) {
		XBUS_ERR(xbus, "%s: sysfs_create_link failed: %d\n", __func__,
			 ret);
		dev_set_drvdata(astribank, NULL);
	}
	return ret;
}

void xbus_sysfs_remove(xbus_t *xbus)
{
	struct device *astribank;

	BUG_ON(!xbus);
	XBUS_DBG(DEVICES, xbus, "\n");
	astribank = &xbus->astribank;
	if (!dev_get_drvdata(astribank))
		return;
	BUG_ON(dev_get_drvdata(astribank) != xbus);
	device_unregister(astribank);
	dev_set_drvdata(astribank, NULL);
}

int xbus_sysfs_create(xbus_t *xbus)
{
	struct device *astribank;
	int ret = 0;

	BUG_ON(!xbus);
	astribank = &xbus->astribank;
	XBUS_DBG(DEVICES, xbus, "\n");
	astribank->bus = &toplevel_bus_type;
	astribank->parent = xbus->transport.transport_device;
	dev_set_name(astribank, "xbus-%02d", xbus->num);
	dev_set_drvdata(astribank, xbus);
	astribank->release = astribank_release;
	ret = device_register(astribank);
	if (ret) {
		XBUS_ERR(xbus, "%s: device_register failed: %d\n", __func__,
			 ret);
		dev_set_drvdata(astribank, NULL);
	}
	return ret;
}

int __init xpp_driver_init(void)
{
	int ret;

	DBG(DEVICES, "SYSFS\n");
	if ((ret = bus_register(&toplevel_bus_type)) < 0) {
		ERR("%s: bus_register(%s) failed. Error number %d", __func__,
		    toplevel_bus_type.name, ret);
		goto failed_toplevel;
	}
	if ((ret = driver_register(&xpp_driver)) < 0) {
		ERR("%s: driver_register(%s) failed. Error number %d", __func__,
		    xpp_driver.name, ret);
		goto failed_xpp_driver;
	}
	if ((ret = bus_register(&xpd_type)) < 0) {
		ERR("%s: bus_register(%s) failed. Error number %d", __func__,
		    xpd_type.name, ret);
		goto failed_xpd_bus;
	}
	return 0;
failed_xpd_bus:
	driver_unregister(&xpp_driver);
failed_xpp_driver:
	bus_unregister(&toplevel_bus_type);
failed_toplevel:
	return ret;
}

void xpp_driver_exit(void)
{
	DBG(DEVICES, "SYSFS\n");
	bus_unregister(&xpd_type);
	driver_unregister(&xpp_driver);
	bus_unregister(&toplevel_bus_type);
}
