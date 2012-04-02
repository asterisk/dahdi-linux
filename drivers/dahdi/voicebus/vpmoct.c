/*
 * VPMOCT Driver.
 *
 * Written by Russ Meyerriecks <rmeyerriecks@digium.com>
 *
 * Copyright (C) 2010-2011 Digium, Inc.
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

#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/crc32.h>

#include "voicebus/vpmoct.h"
#include "linux/firmware.h"

struct vpmoct_header {
	u8	header[6];
	__le32	chksum;
	u8	pad[20];
	u8	major;
	u8	minor;
} __packed;

static int _vpmoct_read(struct vpmoct *vpm, u8 address,
			void *data, size_t size,
			u8 *new_command, u8 *new_address)
{
	struct vpmoct_cmd *cmd;
	unsigned long flags;

	if (unlikely(size >= ARRAY_SIZE(cmd->data))) {
		memset(data, -1, size);
		return -1;
	}

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		dev_info(vpm->dev, "Unable to allocate memory for vpmoct_cmd\n");
		return 0;
	}

	init_completion(&cmd->complete);

	cmd->command = 0x60 + size;
	cmd->address = address;
	cmd->chunksize = size;

	spin_lock_irqsave(&vpm->list_lock, flags);
	list_add_tail(&cmd->node, &vpm->pending_list);
	spin_unlock_irqrestore(&vpm->list_lock, flags);

	/* Wait for receiveprep to process our result */
	if (!wait_for_completion_timeout(&cmd->complete, HZ/5)) {
		spin_lock_irqsave(&vpm->list_lock, flags);
		list_del(&cmd->node);
		spin_unlock_irqrestore(&vpm->list_lock, flags);
		kfree(cmd);
		dev_err(vpm->dev, "vpmoct_read_byte cmd timed out\n");
		return 0;
	}

	memcpy(data, &cmd->data[0], size);

	if (new_command)
		*new_command = cmd->command;
	if (new_address)
		*new_address = cmd->address;

	kfree(cmd);
	return 0;
}

static u8 vpmoct_read_byte(struct vpmoct *vpm, u8 address)
{
	u8 val;
	_vpmoct_read(vpm, address, &val, sizeof(val), NULL, NULL);
	return val;
}

static u32 vpmoct_read_dword(struct vpmoct *vpm, u8 address)
{
	__le32 val;
	_vpmoct_read(vpm, address, &val, sizeof(val), NULL, NULL);
	return le32_to_cpu(val);
}

static void vpmoct_write_byte(struct vpmoct *vpm, u8 address, u8 data)
{
	struct vpmoct_cmd *cmd;
	unsigned long flags;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		dev_info(vpm->dev, "Unable to allocate memory for vpmoct_cmd\n");
		return;
	}

	cmd->command = 0x21;
	cmd->address = address;
	cmd->data[0] = data;
	cmd->chunksize = 1;

	spin_lock_irqsave(&vpm->list_lock, flags);
	list_add_tail(&cmd->node, &vpm->pending_list);
	spin_unlock_irqrestore(&vpm->list_lock, flags);
}

static void vpmoct_write_dword(struct vpmoct *vpm, u8 address, u32 data)
{
	struct vpmoct_cmd *cmd;
	unsigned long flags;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		dev_info(vpm->dev, "Unable to allocate memory for vpmoct_cmd\n");
		return;
	}

	cmd->command = 0x20 + sizeof(data);
	cmd->address = address;
	*(__le32 *)(&cmd->data[0]) = cpu_to_le32(data);
	cmd->chunksize = sizeof(data);

	spin_lock_irqsave(&vpm->list_lock, flags);
	list_add_tail(&cmd->node, &vpm->pending_list);
	spin_unlock_irqrestore(&vpm->list_lock, flags);
}

static void vpmoct_write_chunk(struct vpmoct *vpm, u8 address,
			       const u8 *data, u8 chunksize)
{
	struct vpmoct_cmd *cmd;
	unsigned long flags;

	if (unlikely(chunksize > ARRAY_SIZE(cmd->data)))
		return;

	cmd = kzalloc(sizeof(*cmd), GFP_ATOMIC);
	if (unlikely(!cmd)) {
		dev_info(vpm->dev, "Unable to allocate memory for vpmoct_cmd\n");
		return;
	}

	cmd->command = 0x20 + chunksize;
	cmd->address = address;
	cmd->chunksize = chunksize;

	memcpy(cmd->data, data, chunksize);

	spin_lock_irqsave(&vpm->list_lock, flags);
	list_add_tail(&cmd->node, &vpm->pending_list);
	spin_unlock_irqrestore(&vpm->list_lock, flags);
}

static u8 vpmoct_resync(struct vpmoct *vpm)
{
	unsigned long time;
	u8 status = 0xff;
	u8 address;
	u8 command;

	/* Poll the status register until it returns valid values
	 * This is because we have to wait on the bootloader to do
	 * its thing.
	 * Timeout after 3 seconds
	 */
	time = jiffies + 3*HZ;
	while (time_after(time, jiffies) && (0xff == status)) {
		status = _vpmoct_read(vpm, VPMOCT_BOOT_STATUS, &status,
				      sizeof(status), &command, &address);

		/* Throw out invalid statuses */
		if ((0x55 != command) || (0xaa != address))
			status = 0xff;
	}

	if ((status != 0xff) && status)
		dev_info(vpm->dev, "Resync with status %x\n", status);

	return status;
}

static inline short vpmoct_erase_flash(struct vpmoct *vpm)
{
	short res;
	vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD, VPMOCT_BOOT_FLASH_ERASE);
	res = vpmoct_resync(vpm);
	if (res)
		dev_info(vpm->dev, "Unable to erase flash\n");
	return res;
}

static inline short
vpmoct_send_firmware_header(struct vpmoct *vpm, const struct firmware *fw)
{
	unsigned short i;
	short res;

	/* Send the encrypted firmware header */
	for (i = 0; i < VPMOCT_FIRM_HEADER_LEN; i++) {
		vpmoct_write_byte(vpm, VPMOCT_BOOT_RAM+i,
				  fw->data[i + sizeof(struct vpmoct_header)]);
	}
	/* Decrypt header */
	vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD, VPMOCT_BOOT_DECRYPT);
	res = vpmoct_resync(vpm);
	if (res)
		dev_info(vpm->dev, "Unable to send firmware header\n");
	return res;
}

static inline short
vpmoct_send_firmware_body(struct vpmoct *vpm, const struct firmware *fw)
{
	unsigned int i, ram_index, flash_index, flash_address;
	const u8 *buf;
	u8 chunksize;

	/* Load the body of the firmware */
	ram_index = 0;
	flash_index = 0;
	flash_address = 0;
	for (i = VPMOCT_FIRM_HEADER_LEN*2; i < fw->size;) {
		if (ram_index >= VPMOCT_BOOT_RAM_LEN) {
			/* Tell bootloader to load ram buffer into buffer */
			vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD,
					  0x10 + flash_index);
			/* Assuming the memory load doesn't take longer than 1
			 * eframe just insert a blank eframe before continuing
			 * the firmware load */
			vpmoct_read_byte(vpm, VPMOCT_BOOT_STATUS);
			ram_index = 0;
			flash_index++;
		}
		if (flash_index >= VPMOCT_FLASH_BUF_SECTIONS) {
			/* Tell the bootloader the memory address for load */
			vpmoct_write_dword(vpm, VPMOCT_BOOT_ADDRESS1,
					   flash_address);
			/* Tell the bootloader to load from flash buffer */
			vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD,
					  VPMOCT_BOOT_FLASH_COPY);
			if (vpmoct_resync(vpm))
				goto error;
			flash_index = 0;
			flash_address = i-VPMOCT_FIRM_HEADER_LEN*2;
		}
		/* Try to buffer for batch writes if possible */
		chunksize = VPMOCT_BOOT_RAM_LEN - ram_index;
		if (chunksize > VPMOCT_MAX_CHUNK)
			chunksize = VPMOCT_MAX_CHUNK;

		buf = &fw->data[i];
		vpmoct_write_chunk(vpm, VPMOCT_BOOT_RAM+ram_index,
				   buf, chunksize);
		ram_index += chunksize;
		i += chunksize;
	}

	/* Flush remaining ram buffer to flash buffer */
	vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD,
			  VPMOCT_BOOT_FLASHLOAD + flash_index);
	if (vpmoct_resync(vpm))
		goto error;
	/* Tell boot loader the memory address to flash load */
	vpmoct_write_dword(vpm, VPMOCT_BOOT_ADDRESS1, flash_address);
	/* Tell the bootloader to load flash from flash buffer */
	vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD, VPMOCT_BOOT_FLASH_COPY);
	if (vpmoct_resync(vpm))
		goto error;

	return 0;

error:
	dev_info(vpm->dev, "Unable to load firmware body\n");
	return -1;
}

/**
 * vpmoct_get_mode - Return the current operating mode of the VPMOCT032.
 * @vpm:	The vpm to query.
 *
 * Will be either BOOTLOADER, APPLICATION, or UNKNOWN.
 *
 */
static enum vpmoct_mode vpmoct_get_mode(struct vpmoct *vpm)
{
	int i;
	enum vpmoct_mode ret = UNKNOWN;
	char identifier[11] = {0};

	for (i = 0; i < ARRAY_SIZE(identifier) - 1; i++)
		identifier[i] = vpmoct_read_byte(vpm, VPMOCT_IDENT+i);

	if (!memcmp(identifier, "bootloader", sizeof(identifier) - 1))
		ret = BOOTLOADER;
	else if (!memcmp(identifier, "VPMOCT032\0", sizeof(identifier) - 1))
		ret = APPLICATION;

	dev_dbg(vpm->dev, "vpmoct identifier: %s\n", identifier);
	return ret;
}


static inline short
vpmoct_check_firmware_crc(struct vpmoct *vpm, size_t size, u8 major, u8 minor)
{
	short ret = 0;
	u8 status;

	/* Load firmware size */
	vpmoct_write_dword(vpm, VPMOCT_BOOT_RAM, size);

	/* Load firmware version */
	vpmoct_write_byte(vpm, VPMOCT_BOOT_RAM+8, major);
	vpmoct_write_byte(vpm, VPMOCT_BOOT_RAM+9, minor);

	/* Validate the firmware load */
	vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD, VPMOCT_BOOT_IMAGE_VALIDATE);

	status = vpmoct_resync(vpm);
	if (status) {
		dev_info(vpm->dev,
			 "vpmoct firmware CRC check failed: %x\n", status);
		/* TODO: Try the load again */
		ret = -1;
	} else {

		/* Switch to application code */
		vpmoct_write_dword(vpm, VPMOCT_BOOT_ADDRESS2, 0xDEADBEEF);
		vpmoct_write_byte(vpm, VPMOCT_BOOT_CMD, VPMOCT_BOOT_REBOOT);

		msleep(250);
		status = vpmoct_resync(vpm);

		if (APPLICATION != vpmoct_get_mode(vpm)) {
			dev_info(vpm->dev,
				 "vpmoct firmware failed to switch to "
				 "application. (%x)\n", status);
			ret = -1;
		} else {
			vpm->mode = APPLICATION;
			dev_info(vpm->dev,
				 "vpmoct firmware uploaded successfully\n");
		}
	}

	return ret;
}

static inline short vpmoct_switch_to_boot(struct vpmoct *vpm)
{
	vpmoct_write_dword(vpm, 0x74, 0x00009876);
	vpmoct_write_byte(vpm, 0x71, 0x02);
	if (vpmoct_resync(vpm)) {
		dev_info(vpm->dev, "Failed to switch to bootloader\n");
		return -1;
	}
	vpm->mode = BOOTLOADER;
	return 0;
}

struct vpmoct_load_work {
	struct vpmoct *vpm;
	struct work_struct work;
	struct workqueue_struct *wq;
	load_complete_func_t load_complete;
	bool operational;
};

/**
 * vpmoct_load_complete_fn -
 *
 * This function should run in the context of one of the system workqueues so
 * that it can destroy any workqueues that may have been created to setup a
 * long running firmware load.
 *
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void vpmoct_load_complete_fn(void *data)
{
	struct vpmoct_load_work *work = data;
#else
static void vpmoct_load_complete_fn(struct work_struct *data)
{
	struct vpmoct_load_work *work =
			container_of(data, struct vpmoct_load_work, work);
#endif
	/* Do not touch work->vpm after calling load complete. It may have
	 * been freed in the function by the board driver. */
	work->load_complete(work->vpm->dev, work->operational);
	destroy_workqueue(work->wq);
	kfree(work);
}

/**
 * vpmoct_load_complete - Call the load_complete function in a system workqueue.
 * @work:
 * @operational:	Whether the VPM is functioning or not.
 *
 */
static void
vpmoct_load_complete(struct vpmoct_load_work *work, bool operational)
{
	work->operational = operational;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&work->work, vpmoct_load_complete_fn, work);
#else
	INIT_WORK(&work->work, vpmoct_load_complete_fn);
#endif
	schedule_work(&work->work);
}

static bool is_valid_vpmoct_firmware(const struct firmware *fw)
{
	const struct vpmoct_header *header =
			(const struct vpmoct_header *)fw->data;
	u32 crc = crc32(~0, &fw->data[10], fw->size - 10) ^ ~0;
	return (!memcmp("DIGIUM", header->header, sizeof(header->header)) &&
		 (le32_to_cpu(header->chksum) == crc));
}

static void vpmoct_set_defaults(struct vpmoct *vpm)
{
	vpmoct_write_dword(vpm, 0x40, 0);
	vpmoct_write_dword(vpm, 0x30, 0);
}

static const char *const FIRMWARE_NAME = "dahdi-fw-vpmoct032.bin";
#if defined(HOTPLUG_FIRMWARE)
static int
vpmoct_request_firmware(const struct firmware **fw, struct device *dev)
{
	return request_firmware(fw, FIRMWARE_NAME, dev);
}

static void vpmoct_release_firmware(const struct firmware *fw)
{
	release_firmware(fw);
}
#else
static int
vpmoct_request_firmware(const struct firmware **fw_p, struct device *dev)
{
	struct firmware *fw;
	extern void _binary_dahdi_fw_vpmoct032_bin_size;
	extern u8 _binary_dahdi_fw_vpmoct032_bin_start[];

	*fw_p = fw = kzalloc(sizeof(*fw), GFP_KERNEL);
	if (!fw)
		return -ENOMEM;

	fw->data = _binary_dahdi_fw_vpmoct032_bin_start;
	/* Yes... this is weird. objcopy gives us a symbol containing
	   the size of the firmware, not a pointer a variable containing the
	   size. The only way we can get the value of the symbol is to take
	   its address, so we define it as a pointer and then cast that value
	   to the proper type.  */
	fw->size = (size_t) &_binary_dahdi_fw_vpmoct032_bin_size;

	return 0;
}

static void vpmoct_release_firmware(const struct firmware *fw)
{
	kfree(fw);
}
#endif

/**
 * vpmoct_load_flash - Check the current flash version and possibly load.
 * @vpm:  The VPMOCT032 module to check / load.
 *
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
static void vpmoct_load_flash(void *data)
{
	struct vpmoct_load_work *work = data;
#else
static void vpmoct_load_flash(struct work_struct *data)
{
	struct vpmoct_load_work *work =
			container_of(data, struct vpmoct_load_work, work);
#endif
	int res;
	struct vpmoct *const vpm = work->vpm;
	const struct firmware *fw;
	const struct vpmoct_header *header;
	char serial[VPMOCT_SERIAL_SIZE+1];
	int i;

	res = vpmoct_request_firmware(&fw, vpm->dev);
	if (res) {
		dev_warn(vpm->dev,
			 "vpmoct: Failed to load firmware from userspace! %d\n",
			 res);
		header = NULL;
		fw = NULL;
	} else {
		header = (const struct vpmoct_header *)fw->data;
	}

	if (vpm->mode == APPLICATION) {

		/* Check the running application firmware
		 * for the proper version */
		vpm->major = vpmoct_read_byte(vpm, VPMOCT_MAJOR);
		vpm->minor = vpmoct_read_byte(vpm, VPMOCT_MINOR);
		for (i = 0; i < VPMOCT_SERIAL_SIZE; i++)
			serial[i] = vpmoct_read_byte(vpm, VPMOCT_SERIAL+i);
		serial[VPMOCT_SERIAL_SIZE] = '\0';

		dev_info(vpm->dev,
			 "vpmoct: Detected firmware v%d.%d Serial: %s\n",
			 vpm->major, vpm->minor,
			 (serial[0] != -1) ? serial : "(None)");

		if (!fw) {
			/* Again, we'll use the existing loaded firmware. */
			vpmoct_set_defaults(vpm);
			vpmoct_load_complete(work, true);
			return;
		}

		if (!is_valid_vpmoct_firmware(fw)) {
			dev_warn(vpm->dev,
				 "%s is invalid. Please reinstall.\n",
				 FIRMWARE_NAME);

			/* Just use the old version of the fimware. */
			vpmoct_release_firmware(fw);
			vpmoct_set_defaults(vpm);
			vpmoct_load_complete(work, true);
			return;
		}

		if (vpm->minor == header->minor &&
		    vpm->major == header->major) {
			/* Proper version is running */
			vpmoct_release_firmware(fw);
			vpmoct_set_defaults(vpm);
			vpmoct_load_complete(work, true);
			return;
		} else {

			/* Incorrect version of application code is
			 * loaded. Reset to bootloader mode */
			if (vpmoct_switch_to_boot(vpm))
				goto error;
		}
	}

	if (!fw) {
		vpmoct_load_complete(work, false);
		return;
	} else if (!is_valid_vpmoct_firmware(fw)) {
		dev_warn(vpm->dev,
			 "%s is invalid. Please reinstall.\n", FIRMWARE_NAME);
		goto error;
	}

	dev_info(vpm->dev, "vpmoct: Uploading firmware, v%d.%d. This can "\
			"take up to 1 minute\n",
			header->major, header->minor);
	if (vpmoct_erase_flash(vpm))
		goto error;
	if (vpmoct_send_firmware_header(vpm, fw))
		goto error;
	if (vpmoct_send_firmware_body(vpm, fw))
		goto error;
	if (vpmoct_check_firmware_crc(vpm, fw->size-VPMOCT_FIRM_HEADER_LEN*2,
					header->major, header->minor))
		goto error;
	vpmoct_release_firmware(fw);
	vpmoct_set_defaults(vpm);
	vpmoct_load_complete(work, true);
	return;

error:
	dev_info(vpm->dev, "Unable to load firmware\n");
	vpmoct_release_firmware(fw);
	/* TODO: Should we disable module if the firmware doesn't load? */
	vpmoct_load_complete(work, false);
	return;
}

struct vpmoct *vpmoct_alloc(void)
{
	struct vpmoct *vpm;

	vpm = kzalloc(sizeof(*vpm), GFP_KERNEL);
	if (!vpm)
		return NULL;

	spin_lock_init(&vpm->list_lock);
	INIT_LIST_HEAD(&vpm->pending_list);
	INIT_LIST_HEAD(&vpm->active_list);
	mutex_init(&vpm->mutex);
	return vpm;
}
EXPORT_SYMBOL(vpmoct_alloc);

void vpmoct_free(struct vpmoct *vpm)
{
	unsigned long flags;
	struct vpmoct_cmd *cmd;
	LIST_HEAD(list);

	if (!vpm)
		return;

	spin_lock_irqsave(&vpm->list_lock, flags);
	list_splice(&vpm->active_list, &list);
	list_splice(&vpm->pending_list, &list);
	spin_unlock_irqrestore(&vpm->list_lock, flags);

	while (!list_empty(&list)) {
		cmd = list_entry(list.next, struct vpmoct_cmd, node);
		list_del(&cmd->node);
		kfree(cmd);
	}

	kfree(vpm);
}
EXPORT_SYMBOL(vpmoct_free);

/**
 * vpmoct_init - Check for / initialize VPMOCT032 module.
 * @vpm:		struct vpmoct allocated with vpmoct_alloc
 * @load_complete_fn:	Function to call when the load is complete.
 *
 * Check to see if there is a VPMOCT module installed. If there appears to be
 * one return 0 and perform any necessary setup in the background. The
 * load_complete function will be called in a system global workqueue when the
 * initialization is complete.
 *
 * Must be called in process context.
 */
int vpmoct_init(struct vpmoct *vpm, load_complete_func_t load_complete)
{
	struct vpmoct_load_work *work;

	if (!vpm || !vpm->dev || !load_complete)
		return -EINVAL;

	if (vpmoct_resync(vpm)) {
		load_complete(vpm->dev, false);
		return -ENODEV;
	}

	vpm->mode = vpmoct_get_mode(vpm);

	if (UNKNOWN == vpm->mode) {
		load_complete(vpm->dev, false);
		return -ENODEV;
	}

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work) {
		load_complete(vpm->dev, false);
		return -ENOMEM;
	}

	work->wq = create_singlethread_workqueue("vpmoct");
	if (!work->wq) {
		kfree(work);
		load_complete(vpm->dev, false);
		return -ENOMEM;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
	INIT_WORK(&work->work, vpmoct_load_flash, work);
#else
	INIT_WORK(&work->work, vpmoct_load_flash);
#endif

	work->vpm = vpm;
	work->load_complete = load_complete;
	queue_work(work->wq, &work->work);
	return 0;
}
EXPORT_SYMBOL(vpmoct_init);

static void
vpmoct_set_companding(struct vpmoct *vpm, int channo, int companding)
{
	u32 new_companding;
	bool do_update = false;

	mutex_lock(&vpm->mutex);
	new_companding = (DAHDI_LAW_MULAW == companding) ?
				(vpm->companding & ~(1 << channo)) :
				(vpm->companding |  (1 << channo));
	if (vpm->companding != new_companding) {
		vpm->companding = new_companding;
		if (!vpm->companding_update_active) {
			do_update = true;
			vpm->companding_update_active = 1;
		}
	}
	mutex_unlock(&vpm->mutex);

	while (do_update) {
		u32 update;

		vpmoct_write_dword(vpm, 0x40, new_companding);
		update = vpmoct_read_dword(vpm, 0x40);

		WARN_ON(new_companding != update);

		mutex_lock(&vpm->mutex);
		if (vpm->companding != new_companding) {
			new_companding = vpm->companding;
		} else {
			vpm->companding_update_active = 0;
			do_update = false;
		}
		mutex_unlock(&vpm->mutex);
	}
}

/**
 * vpmoct_echo_update - Enable / Disable the VPMOCT032 echocan state
 * @vpm:	The echocan to operate on.
 * @channo:	Which echocan timeslot to enable / disable.
 * @echo_on:	Whether we're turning the echocan on or off.
 *
 * When this function returns, the echocan is scheduled to be enabled or
 * disabled at some point in the near future.
 *
 * Must be called in process context.
 *
 */
static void vpmoct_echo_update(struct vpmoct *vpm, int channo, bool echo_on)
{
	u32 echo;
	unsigned long timeout;
	bool do_update = false;

	mutex_lock(&vpm->mutex);
	echo = (echo_on) ? (vpm->echo | (1 << channo)) :
			   (vpm->echo & ~(1 << channo));
	if (vpm->echo != echo) {
		vpm->echo = echo;
		if (!vpm->echo_update_active) {
			do_update = true;
			vpm->echo_update_active = 1;
		}
	}
	mutex_unlock(&vpm->mutex);

	timeout = jiffies + 2*HZ;
	while (do_update) {
		u32 new;

		vpmoct_write_dword(vpm, 0x30, echo);
		new = vpmoct_read_dword(vpm, 0x10);

		mutex_lock(&vpm->mutex);
		if (((vpm->echo != echo) || (new != echo)) &&
		    time_before(jiffies, timeout)) {
			echo = vpm->echo;
		} else {
			vpm->echo_update_active = 0;
			do_update = false;
		}
		mutex_unlock(&vpm->mutex);
	}

	if (!time_before(jiffies, timeout))
		dev_warn(vpm->dev, "vpmoct: Updating echo state timed out.\n");
}

int vpmoct_echocan_create(struct vpmoct *vpm, int channo, int companding)
{
	vpmoct_set_companding(vpm, channo, companding);
	vpmoct_echo_update(vpm, channo, true);
	return 0;
}
EXPORT_SYMBOL(vpmoct_echocan_create);

void vpmoct_echocan_free(struct vpmoct *vpm, int channo)
{
	vpmoct_echo_update(vpm, channo, false);
}
EXPORT_SYMBOL(vpmoct_echocan_free);

/* Enable a vpm debugging mode where the pre-echo-canceled audio
 * stream is physically output on timeslot 24.
 */
int vpmoct_preecho_enable(struct vpmoct *vpm, const int channo)
{
	int ret;
	mutex_lock(&vpm->mutex);
	if (!vpm->preecho_enabled) {
		vpm->preecho_enabled = 1;
		vpm->preecho_timeslot = channo;

		vpmoct_write_dword(vpm, 0x74, channo);

		/* Begin pre-echo stream on timeslot 24 */
		vpmoct_write_byte(vpm, 0x71, 0x0a);
		ret = 0;
	} else {
		ret = -EBUSY;
	}
	mutex_unlock(&vpm->mutex);

	return ret;
}
EXPORT_SYMBOL(vpmoct_preecho_enable);

int vpmoct_preecho_disable(struct vpmoct *vpm, const int channo)
{
	int ret;

	mutex_lock(&vpm->mutex);
	if (!vpm->preecho_enabled) {
		ret = 0;
	} else if (channo == vpm->preecho_timeslot) {
		vpm->preecho_enabled = 0;

		/* Disable pre-echo stream by loading in a non-existing
		 * channel number */
		vpmoct_write_byte(vpm, 0x74, 0xff);

		/* Stop pre-echo stream on timeslot 24 */
		vpmoct_write_byte(vpm, 0x71, 0x0a);
		ret = 0;
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&vpm->mutex);

	return ret;
}
EXPORT_SYMBOL(vpmoct_preecho_disable);
