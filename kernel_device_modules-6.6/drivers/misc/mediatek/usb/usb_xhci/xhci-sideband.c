// SPDX-License-Identifier: GPL-2.0
/*
 * xHCI host controller sideband support
 *
 * Copyright (c) 2023, Intel Corporation.
 *
 * Author: Mathias Nyman
 */

#include <linux/usb/xhci-sideband.h>
#include <linux/dma-direct.h>

#include "xhci.h"

/* sideband internal helpers */

/* sideband api functions */

/**
 * xhci_sideband_add_endpoint - add endpoint to sideband access list
 * @sb: sideband instance for this usb device
 * @host_ep: usb host endpoint
 *
 * Adds an endpoint to the list of sideband accessed endpoints for this usb
 * device.
 * After an endpoint is added the sideband client can get the endpoint transfer
 * ring buffer by calling xhci_sideband_endpoint_buffer()
 *
 * Return: 0 on success, negative error otherwise.
 */

/**
 * xhci_sideband_remove_endpoint - remove endpoint from sideband access list
 * @sb: sideband instance for this usb device
 * @host_ep: usb host endpoint
 *
 * Removes an endpoint from the list of sideband accessed endpoints for this usb
 * device.
 * sideband client should no longer touch the endpoint transfer buffer after
 * calling this.
 *
 * Return: 0 on success, negative error otherwise.
 */

/**
 * xhci_sideband_get_endpoint_buffer - gets the endpoint transfer buffer address
 * @sb: sideband instance for this usb device
 * @host_ep: usb host endpoint
 *
 * Returns the address of the endpoint buffer where xHC controller reads queued
 * transfer TRBs from. This is the starting address of the ringbuffer where the
 * sideband client should write TRBs to.
 *
 * Caller needs to free the returned sg_table
 *
 * Return: struct sg_table * if successful. NULL otherwise.
 */

/**
 * xhci_sideband_get_event_buffer - return the event buffer for this device
 * @sb: sideband instance for this usb device
 *
 * If a secondary xhci interupter is set up for this usb device then this
 * function returns the address of the event buffer where xHC writes
 * the transfer completion events.
 *
 * Caller needs to free the returned sg_table
 *
 * Return: struct sg_table * if successful. NULL otherwise.
 */

/**
 * xhci_sideband_enable_interrupt - enable interrupt for secondary interrupter
 * @sb: sideband instance for this usb device
 * @imod_interval: number of event ring segments to allocate
 *
 * Enables OS owned event handling for a particular interrupter if client
 * requests for it.  In addition, set the IMOD interval for this particular
 * interrupter.
 *
 * Returns 0 on success, negative error otherwise
 */

/**
 * xhci_sideband_create_interrupter - creates a new interrupter for this sideband
 * @sb: sideband instance for this usb device
 * @num_seg: number of event ring segments to allocate
 * @ip_autoclear: IP autoclearing support such as MSI implemented
 *
 * Sets up a xhci interrupter that can be used for this sideband accessed usb
 * device. Transfer events for this device can be routed to this interrupters
 * event ring by setting the 'Interrupter Target' field correctly when queueing
 * the transfer TRBs.
 * Once this interrupter is created the interrupter target ID can be obtained
 * by calling xhci_sideband_interrupter_id()
 *
 * Returns 0 on success, negative error otherwise
 */

/**
 * xhci_sideband_remove_interrupter - remove the interrupter from a sideband
 * @sb: sideband instance for this usb device
 *
 * Removes a registered interrupt for a sideband.  This would allow for other
 * sideband users to utilize this interrupter.
 */

/**
 * xhci_sideband_interrupter_id - return the interrupter target id
 * @sb: sideband instance for this usb device
 *
 * If a secondary xhci interrupter is set up for this usb device then this
 * function returns the ID used by the interrupter. The sideband client
 * needs to write this ID to the 'Interrupter Target' field of the transfer TRBs
 * it queues on the endpoints transfer ring to ensure transfer completion event
 * are written by xHC to the correct interrupter event ring.
 *
 * Returns interrupter id on success, negative error othgerwise
 */

/**
 * xhci_sideband_register - register a sideband for a usb device
 * @udev: usb device to be accessed via sideband
 *
 * Allows for clients to utilize XHCI interrupters and fetch transfer and event
 * ring parameters for executing data transfers.
 *
 * Return: pointer to a new xhci_sideband instance if successful. NULL otherwise.
 */
struct xhci_sideband *
xhci_sideband_register_(struct usb_device *udev)
{
	struct usb_hcd *hcd = bus_to_hcd(udev->bus);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_virt_device *vdev;
	struct xhci_sideband *sb;

	/* make sure the usb device is connected to a xhci controller */
	if (!udev->slot_id)
		return NULL;

	sb = kzalloc_node(sizeof(*sb), GFP_KERNEL, dev_to_node(hcd->self.sysdev));
	if (!sb)
		return NULL;

	mutex_init(&sb->mutex);

	/* check this device isn't already controlled via sideband */
	spin_lock_irq(&xhci->lock);

	vdev = xhci->devs[udev->slot_id];

	if (!vdev || vdev->sideband) {
		xhci_warn(xhci, "XHCI sideband for slot %d already in use\n",
			  udev->slot_id);
		spin_unlock_irq(&xhci->lock);
		kfree(sb);
		return NULL;
	}

	sb->xhci = xhci;
	sb->vdev = vdev;
	vdev->sideband = sb;

	spin_unlock_irq(&xhci->lock);

	return sb;
}
EXPORT_SYMBOL_GPL(xhci_sideband_register_);

/**
 * xhci_sideband_unregister - unregister sideband access to a usb device
 * @sb: sideband instance to be unregistered
 *
 * Unregisters sideband access to a usb device and frees the sideband
 * instance.
 * After this the endpoint and interrupter event buffers should no longer
 * be accessed via sideband. The xhci driver can now take over handling
 * the buffers.
 */
MODULE_LICENSE("GPL");
