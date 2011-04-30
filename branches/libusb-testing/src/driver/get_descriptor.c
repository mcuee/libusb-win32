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



NTSTATUS get_descriptor(libusb_device_t *dev,
                        void *buffer, int size, int type, int recipient,
                        int index, int language_id, int *received, int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;

	USBMSG("buffer size: %d type: %04d recipient: %04d index: %04d language id: %04d timeout: %d\n", 
		size, type, recipient, index, language_id, timeout);

    memset(&urb, 0, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST));

    switch (recipient)
    {
    case USB_RECIP_DEVICE:
        urb.UrbHeader.Function = URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE;
        break;
    case USB_RECIP_INTERFACE:
        urb.UrbHeader.Function = URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE;
        break;
    case USB_RECIP_ENDPOINT:
        urb.UrbHeader.Function = URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT;
        break;
    default:
        USBERR0("invalid recipient\n");
        return STATUS_INVALID_PARAMETER;
    }


    urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
    urb.UrbControlDescriptorRequest.TransferBufferLength = size;
    urb.UrbControlDescriptorRequest.TransferBuffer = buffer;
    urb.UrbControlDescriptorRequest.DescriptorType = (UCHAR)type;
    urb.UrbControlDescriptorRequest.Index = (UCHAR)index;
    urb.UrbControlDescriptorRequest.LanguageId = (USHORT)language_id;


    status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);


    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("getting descriptor failed: status: 0x%x, urb-status: 0x%x\n",
                    status, urb.UrbHeader.Status);
        *received = 0;
    }
    else
    {
        *received = urb.UrbControlDescriptorRequest.TransferBufferLength;
    }

    return status;
}

PUSB_CONFIGURATION_DESCRIPTOR get_config_descriptor(
	libusb_device_t *dev,
	int value,
	int *size,
	int* index)
{
    NTSTATUS status;
    USB_CONFIGURATION_DESCRIPTOR *desc = NULL;
    USB_DEVICE_DESCRIPTOR device_descriptor;
    int i;
    volatile int desc_size;
	
	*index = 0;

    status = get_descriptor(dev, &device_descriptor,
                            sizeof(USB_DEVICE_DESCRIPTOR),
                            USB_DEVICE_DESCRIPTOR_TYPE,
                            USB_RECIP_DEVICE,
                            0, 0, size, LIBUSB_DEFAULT_TIMEOUT);

    if (!NT_SUCCESS(status) || *size != sizeof(USB_DEVICE_DESCRIPTOR))
    {
        USBERR0("getting device descriptor failed\n");
        return NULL;
    }

    if (!(desc = ExAllocatePool(NonPagedPool,
                                sizeof(USB_CONFIGURATION_DESCRIPTOR))))
    {
        USBERR0("memory allocation error\n");
        return NULL;
    }

    for (i = 0; i < device_descriptor.bNumConfigurations; i++)
    {

        if (!NT_SUCCESS(get_descriptor(dev, desc,
                                       sizeof(USB_CONFIGURATION_DESCRIPTOR),
                                       USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                       USB_RECIP_DEVICE,
                                       i, 0, size, LIBUSB_DEFAULT_TIMEOUT)))
        {
            USBERR0("getting configuration descriptor failed\n");
            break;
        }

		// if value is negative, get the descriptor by index
		// if positive, get it by value.
        if ((value > 0 && desc->bConfigurationValue == value) ||
			(value < 0 && -(i+1) == (value)))
        {
            desc_size = desc->wTotalLength;
            ExFreePool(desc);

            if (!(desc = ExAllocatePool(NonPagedPool, desc_size)))
            {
                USBERR0("memory allocation error\n");
                break;
            }

            if (!NT_SUCCESS(get_descriptor(dev, desc, desc_size,
                                           USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                           USB_RECIP_DEVICE,
                                           i, 0, size, LIBUSB_DEFAULT_TIMEOUT)))
            {
                USBERR0("getting configuration descriptor failed\n");
                break;
            }
			else
			{
				*index = i+1;
			}

            return desc;
        }
    }

    if (desc)
    {
        ExFreePool(desc);
    }

    return NULL;
}
NTSTATUS interface_query_settings(libusb_device_t *dev,
								  int interface_index, 
								  int alt_index, 
								  PUSB_INTERFACE_DESCRIPTOR interface_descriptor)
{
    NTSTATUS status = STATUS_SUCCESS;

    PUSB_CONFIGURATION_DESCRIPTOR configuration_descriptor = dev->config.descriptor;
    PUSB_INTERFACE_DESCRIPTOR interface_descriptor_src = NULL;
	interface_request_t find_interface;

    USBMSG("query interface index=%d alt-index=%d\n", 
		interface_index, alt_index);

	if (!dev->config.value || !configuration_descriptor)
    {
        USBERR0("device is not configured\n");
        return STATUS_INVALID_DEVICE_STATE;
    }


	memset(&find_interface,0, sizeof(find_interface));
	find_interface.intf_use_index=1;
	find_interface.altf_use_index=0;
	find_interface.interface_index=(short)interface_index;
	find_interface.altsetting_number=(short)alt_index;

	interface_descriptor_src = find_interface_desc_ex(
		configuration_descriptor, 
		dev->config.total_size,
		&find_interface,
		NULL);

	if (!interface_descriptor_src)
	{
		status = STATUS_NO_MORE_ENTRIES;
	}
	else
	{
		RtlCopyMemory(interface_descriptor, interface_descriptor_src, sizeof(*interface_descriptor));
	}

	return status;

}

NTSTATUS pipe_query_information(libusb_device_t *dev,
								  int interface_index, 
								  int alt_index, 
								  int pipe_index, 
								  PPIPE_INFORMATION pipe_information)
{
    NTSTATUS status = STATUS_SUCCESS;
	unsigned int size_left=0;
    PUSB_CONFIGURATION_DESCRIPTOR configuration_descriptor = dev->config.descriptor;
    PUSB_INTERFACE_DESCRIPTOR interface_descriptor_src = NULL;
    PUSB_ENDPOINT_DESCRIPTOR endpoint_descriptor_src = NULL;
	interface_request_t find_interface;

    USBMSG("query pipe interface-index=%d alt-index=%d pipe-index=%d\n", 
		interface_index, alt_index, pipe_index);

	if (!dev->config.value || !configuration_descriptor)
    {
        USBERR0("device is not configured\n");
        return STATUS_INVALID_DEVICE_STATE;
    }

	memset(&find_interface,0, sizeof(find_interface));
	find_interface.altf_use_index=1;
	find_interface.intf_use_index=1;
	find_interface.interface_index=(short)interface_index;
	find_interface.altsetting_index=(short)alt_index;

	interface_descriptor_src = find_interface_desc_ex(
		configuration_descriptor, 
		dev->config.total_size,
		&find_interface,
		&size_left);

	if (!interface_descriptor_src)
	{
		status = STATUS_NO_MORE_ENTRIES;
	}
	else
	{
		endpoint_descriptor_src = find_endpoint_desc_by_index(interface_descriptor_src, size_left, pipe_index);
		if (!endpoint_descriptor_src)
		{
			status = STATUS_NO_MORE_ENTRIES;
		}
		else
		{
			pipe_information->Interval = endpoint_descriptor_src->bInterval;
			pipe_information->MaximumPacketSize=endpoint_descriptor_src->wMaxPacketSize;
			pipe_information->PipeId = endpoint_descriptor_src->bEndpointAddress;
			pipe_information->PipeType = endpoint_descriptor_src->bmAttributes & 0x3;
		}
	}

	return status;
}

