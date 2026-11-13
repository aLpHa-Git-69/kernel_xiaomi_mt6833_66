// SPDX-License-Identifier: GPL-2.0
/*
 * XHCI extended capability handling
 *
 * Copyright (c) 2017 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/pci.h>
#include "xhci.h"

#define USB_SW_DRV_NAME		"intel_xhci_usb_sw"
#define USB_SW_RESOURCE_SIZE	0x400

#define PCI_DEVICE_ID_INTEL_CHERRYVIEW_XHCI	0x22b5

static const struct property_entry role_switch_props[] = {
	PROPERTY_ENTRY_BOOL("sw_switch_disable"),
	{},
};
