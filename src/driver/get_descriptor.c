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

	// the device and active config descriptors are cached. If the request
	// is for one of these and we have already retrieved it, then this is
	// a non-blocking non-i/o call.

	if (type == USB_DEVICE_DESCRIPTOR_TYPE && 
		recipient == USB_RECIP_DEVICE && 
		index == 0 && 
		language_id == 0 && 
		size >= sizeof(USB_DEVICE_DESCRIPTOR))
	{
		// this is a device descriptor request.
		if (dev->device_descriptor.bLength == 0)
		{
			// this is the first request made for the device descriptor.
			// Cache it now and forever
			status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
			if (NT_SUCCESS(status) && urb.UrbControlDescriptorRequest.TransferBufferLength < sizeof(USB_DEVICE_DESCRIPTOR))
			{
				USBERR("Invalid device decriptor length %d\n", urb.UrbControlDescriptorRequest.TransferBufferLength);
				status = STATUS_BAD_DEVICE_TYPE;
				*received = 0;
				goto Done;
			}

			if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
			{
				USBERR("getting descriptor failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
				*received = 0;
				goto Done;
			}

			// valid device descriptor
			size = sizeof(USB_DEVICE_DESCRIPTOR);
			RtlCopyMemory(&dev->device_descriptor, buffer, size);
			*received = size;

		}
		else
		{
			// device descriptor is already cached.
			size = sizeof(USB_DEVICE_DESCRIPTOR);
			RtlCopyMemory(buffer, &dev->device_descriptor, size);
			*received = size;
		}

		goto Done;
	}

	if (type == USB_CONFIGURATION_DESCRIPTOR_TYPE && 
		recipient == USB_RECIP_DEVICE && 
		language_id == 0 && 
		size >= sizeof(USB_CONFIGURATION_DESCRIPTOR))
	{
		if (dev->device_descriptor.bLength != 0	&& index >=dev->device_descriptor.bNumConfigurations)
		{
			USBWRN("config descriptor index %d out of range.\n", index);
			status = STATUS_NO_MORE_ENTRIES;
			*received = 0;
			goto Done;
		}

		// this is a config descriptor request.
		if (!dev->config.descriptor || dev->config.index != index)
		{
			// this is either:
			// * The first request made for a config descriptor.
			// * A request for a config descriptor other than the cached index.
			status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

			if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
			{
				USBERR("getting descriptor failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
				*received = 0;
				goto Done;
			}

			if (!dev->config.descriptor && 
				urb.UrbControlDescriptorRequest.TransferBufferLength >= ((PUSB_CONFIGURATION_DESCRIPTOR)buffer)->wTotalLength)
			{
				//
				// This is a special case scenario where we cache the first
				// config deescriptor requested when dev->config.descriptor == NULL
				// and this was a request for the *entire* descriptor.
				// 
				// Nearly all windows USB devices will only have one configuration.
				// In the case a the filter driver, it does *not* auto configure the
				// device which is where the *active* config descriptor caching occurs.
				//
				// This code ensures the first config descriptor requested is always cached
				// even if the device is not configured yet.
				//

				PUSB_CONFIGURATION_DESCRIPTOR config_desc;
				size = ((PUSB_CONFIGURATION_DESCRIPTOR)buffer)->wTotalLength;

				if (!( config_desc = ExAllocatePool(NonPagedPool, size)))
				{
					USBERR0("memory allocation error\n");
					status =  STATUS_NO_MEMORY;
					goto Done;
				}

				dev->config.descriptor=config_desc;

				RtlCopyMemory(dev->config.descriptor,buffer,size);
				dev->config.value=0;
				dev->config.total_size=size;
				dev->config.index=index;

				*received = size;
			}
			else
			{
				*received = urb.UrbControlDescriptorRequest.TransferBufferLength;
			}
			goto Done;
		}
		else
		{
			// This is a request for the active configuration descriptor.
			// This is only updated upon a successful set_configuration().
			size = (size > dev->config.descriptor->wTotalLength) ? dev->config.descriptor->wTotalLength : size;
			RtlCopyMemory(buffer, dev->config.descriptor, size);

			*received = size;
			goto Done;
		}
	}

	// this is not a device or config descriptor reequest
	status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);


	if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
	{
		USBERR("getting descriptor failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
		*received = 0;
	}
	else
	{
		*received = urb.UrbControlDescriptorRequest.TransferBufferLength;
	}

Done:
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
				*index = i;
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
