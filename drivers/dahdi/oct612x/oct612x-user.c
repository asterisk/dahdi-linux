/*
 * Octasic OCT6100 Interface
 *
 * Copyright (C) 2013 Digium, Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>

#include <dahdi/kernel.h>

#include "oct612x.h"

UINT32 Oct6100UserGetTime(tPOCT6100_GET_TIME f_pTime)
{
	/* Why couldn't they just take a timeval like everyone else? */
	struct timeval tv;
	unsigned long long total_usecs;
	unsigned int mask = ~0;

	do_gettimeofday(&tv);
	total_usecs = (((unsigned long long)(tv.tv_sec)) * 1000000) +
				  (((unsigned long long)(tv.tv_usec)));
	f_pTime->aulWallTimeUs[0] = (total_usecs & mask);
	f_pTime->aulWallTimeUs[1] = (total_usecs >> 32);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserMemSet(PVOID f_pAddress, UINT32 f_ulPattern,
			 UINT32 f_ulLength)
{
	memset(f_pAddress, f_ulPattern, f_ulLength);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserMemCopy(PVOID f_pDestination, const void *f_pSource,
			  UINT32 f_ulLength)
{
	memcpy(f_pDestination, f_pSource, f_ulLength);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserCreateSerializeObject(
			tPOCT6100_CREATE_SERIALIZE_OBJECT f_pCreate)
{
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserDestroySerializeObject(
			tPOCT6100_DESTROY_SERIALIZE_OBJECT f_pDestroy)
{
#ifdef OCTASIC_DEBUG
	pr_debug("I should never be called! (destroy serialize object)\n");
#endif
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserSeizeSerializeObject(
		tPOCT6100_SEIZE_SERIALIZE_OBJECT f_pSeize)
{
	/* Not needed */
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserReleaseSerializeObject(
		tPOCT6100_RELEASE_SERIALIZE_OBJECT f_pRelease)
{
	/* Not needed */
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserDriverWriteApi(tPOCT6100_WRITE_PARAMS f_pWriteParams)
{
	struct oct612x_context *context = f_pWriteParams->pProcessContext;
#ifdef OCTASIC_DEBUG
	if (!context || !context->ops || !context->ops->write) {
		pr_debug("Invalid call to %s\n", __func__);
		return cOCT6100_ERR_BASE;
	}
#endif
	context->ops->write(context, f_pWriteParams->ulWriteAddress,
			    f_pWriteParams->usWriteData);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserDriverWriteSmearApi(tPOCT6100_WRITE_SMEAR_PARAMS f_pSmearParams)
{
	struct oct612x_context *context = f_pSmearParams->pProcessContext;
#ifdef OCTASIC_DEBUG
	if (!context || !context->ops || !context->ops->write_smear) {
		pr_debug("Invalid call to %s\n", __func__);
		return cOCT6100_ERR_BASE;
	}
#endif
	context->ops->write_smear(context, f_pSmearParams->ulWriteAddress,
				  f_pSmearParams->usWriteData,
				  f_pSmearParams->ulWriteLength);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserDriverWriteBurstApi(
		tPOCT6100_WRITE_BURST_PARAMS f_pBurstParams)
{
	struct oct612x_context *context = f_pBurstParams->pProcessContext;
#ifdef OCTASIC_DEBUG
	if (!context || !context->ops || !context->ops->write_burst) {
		pr_debug("Invalid call to %s\n", __func__);
		return cOCT6100_ERR_BASE;
	}
#endif
	context->ops->write_burst(context, f_pBurstParams->ulWriteAddress,
				  f_pBurstParams->pusWriteData,
				  f_pBurstParams->ulWriteLength);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserDriverReadApi(tPOCT6100_READ_PARAMS f_pReadParams)
{
	struct oct612x_context *context = f_pReadParams->pProcessContext;
#ifdef OCTASIC_DEBUG
	if (!context || !context->ops || !context->ops->read) {
		pr_debug("Invalid call to %s\n", __func__);
		return cOCT6100_ERR_BASE;
	}
#endif
	context->ops->read(context, f_pReadParams->ulReadAddress,
			   f_pReadParams->pusReadData);
	return cOCT6100_ERR_OK;
}

UINT32 Oct6100UserDriverReadBurstApi(tPOCT6100_READ_BURST_PARAMS f_pBurstParams)
{
	struct oct612x_context *context = f_pBurstParams->pProcessContext;
#ifdef OCTASIC_DEBUG
	if (!context || !context->ops || !context->ops->read_burst) {
		pr_debug("Invalid call to %s\n", __func__);
		return cOCT6100_ERR_BASE;
	}
#endif
	context->ops->read_burst(context, f_pBurstParams->ulReadAddress,
				 f_pBurstParams->pusReadData,
				 f_pBurstParams->ulReadLength);
	return cOCT6100_ERR_OK;
}

EXPORT_SYMBOL(Oct6100ChipOpen);
EXPORT_SYMBOL(Oct6100ChipClose);
EXPORT_SYMBOL(Oct6100ChipCloseDef);
EXPORT_SYMBOL(Oct6100GetInstanceSize);
EXPORT_SYMBOL(Oct6100GetInstanceSizeDef);
EXPORT_SYMBOL(Oct6100ChipOpenDef);
EXPORT_SYMBOL(Oct6100ChannelModify);
EXPORT_SYMBOL(Oct6100ToneDetectionEnableDef);
EXPORT_SYMBOL(Oct6100InterruptServiceRoutine);
EXPORT_SYMBOL(Oct6100InterruptServiceRoutineDef);
EXPORT_SYMBOL(Oct6100ApiGetCapacityPins);
EXPORT_SYMBOL(Oct6100ToneDetectionEnable);
EXPORT_SYMBOL(Oct6100EventGetToneDef);
EXPORT_SYMBOL(Oct6100EventGetTone);
EXPORT_SYMBOL(Oct6100ApiGetCapacityPinsDef);
EXPORT_SYMBOL(Oct6100ChannelOpen);
EXPORT_SYMBOL(Oct6100ChannelOpenDef);
EXPORT_SYMBOL(Oct6100ChannelModifyDef);

static int __init oct612x_module_init(void)
{
	/* This registration with dahdi.ko will fail since the span is not
	 * defined, but it will make sure that this module is a dependency of
	 * dahdi.ko, so that when it is being unloded, this module will be
	 * unloaded as well. */
	dahdi_register_device(NULL, NULL);
	return 0;
}
module_init(oct612x_module_init);

static void __exit oct612x_module_cleanup(void)
{
	/* Nothing to do */;
}
module_exit(oct612x_module_cleanup);

MODULE_AUTHOR("Digium Incorporated <support@digium.com>");
MODULE_DESCRIPTION("Octasic OCT6100 Hardware Echocan Library");
MODULE_LICENSE("GPL v2");
