/* libusb-win32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "libusb_driver.h"


NTSTATUS set_interface(libusb_device_t *dev, 
					   bool_t use_index, 
					   int interface_number, 
					   int alt_interface_number,
					   int interface_index, 
					   int alt_interface_index,
                       int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB *urb;
    int i, tmp_size;

    USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;
    USBD_INTERFACE_INFORMATION *interface_information = NULL;

	if (use_index)
	{
		USBMSG("interface-index=%d alt-index=%d timeout=%d\n", 
			interface_index,alt_interface_index,timeout);
	}
	else
	{
		USBMSG("interface-number=%d alt-number=%d timeout=%d\n", 
			interface_number,alt_interface_number,timeout);
	}

	if (!dev->config.value || !dev->config.descriptor)
    {
        USBERR0("device is not configured\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

	if (use_index)
	{
		interface_descriptor = find_interface_desc_by_index(dev->config.descriptor, dev->config.total_size, interface_index, alt_interface_index, NULL);
	}
	else
	{
		interface_descriptor = find_interface_desc(dev->config.descriptor, dev->config.total_size, interface_number, alt_interface_number);
	}
    if (!interface_descriptor)
    {
		if (use_index)
		{
			USBWRN("interface-index=%d alt-index=%d does not exists.\n", 
				interface_index,alt_interface_index);
	        return STATUS_NO_MORE_ENTRIES;

		}
		else
		{
			USBERR("interface-number=%d alt-number=%d does not exists.\n", 
				interface_number,alt_interface_number,timeout);
	        return STATUS_INVALID_PARAMETER;
		}
    }

    tmp_size = sizeof(struct _URB_SELECT_INTERFACE) + interface_descriptor->bNumEndpoints * sizeof(USBD_PIPE_INFORMATION);


    urb = ExAllocatePool(NonPagedPool, tmp_size);

    if (!urb)
    {
        USBERR0("memory_allocation error\n");
        return STATUS_NO_MEMORY;
    }

    memset(urb, 0, tmp_size);

    urb->UrbHeader.Function = URB_FUNCTION_SELECT_INTERFACE;
    urb->UrbHeader.Length = (USHORT)tmp_size;

    urb->UrbSelectInterface.ConfigurationHandle = dev->config.handle;
    urb->UrbSelectInterface.Interface.Length = sizeof(struct _USBD_INTERFACE_INFORMATION);
    urb->UrbSelectInterface.Interface.NumberOfPipes = interface_descriptor->bNumEndpoints;
    urb->UrbSelectInterface.Interface.Length += interface_descriptor->bNumEndpoints * sizeof(struct _USBD_PIPE_INFORMATION);

    urb->UrbSelectInterface.Interface.InterfaceNumber = (UCHAR)interface_descriptor->bInterfaceNumber;
    urb->UrbSelectInterface.Interface.AlternateSetting = (UCHAR)interface_descriptor->bAlternateSetting;

    interface_information = &urb->UrbSelectInterface.Interface;

    for (i = 0; i < interface_descriptor->bNumEndpoints; i++)
    {
        interface_information->Pipes[i].MaximumTransferSize = LIBUSB_MAX_READ_WRITE;
    }

    status = call_usbd(dev, urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);


    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
        USBERR("setting interface failed: status: 0x%x, urb-status: 0x%x\n", status, urb->UrbHeader.Status);
        ExFreePool(urb);
        return STATUS_UNSUCCESSFUL;
    }

    update_pipe_info(dev, interface_information);

    ExFreePool(urb);

    return status;
}

