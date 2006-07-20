/* LIBUSB-WIN32, Generic Windows USB Library
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

  DEBUG_PRINT_NL();
  DEBUG_MESSAGE("get_descriptor(): buffer size %d", size);
  DEBUG_MESSAGE("get_descriptor(): type %04d", type);
  DEBUG_MESSAGE("get_descriptor(): recipient %04d", recipient);
  DEBUG_MESSAGE("get_descriptor(): index %04d", index);
  DEBUG_MESSAGE("get_descriptor(): language id %04d", language_id);
  DEBUG_MESSAGE("get_descriptor(): timeout %d", timeout);
  

  memset(&urb, 0, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST));

  switch(recipient)
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
      DEBUG_ERROR("get_descriptor(): invalid recipient");
      return STATUS_INVALID_PARAMETER;
    }


  urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
  urb.UrbControlDescriptorRequest.TransferBufferLength = size;
  urb.UrbControlDescriptorRequest.TransferBuffer = buffer;
  urb.UrbControlDescriptorRequest.DescriptorType = (UCHAR)type;
  urb.UrbControlDescriptorRequest.Index = (UCHAR)index;
  urb.UrbControlDescriptorRequest.LanguageId = (USHORT)language_id;
  

  status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

      
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
      DEBUG_ERROR("get_descriptor(): getting descriptor "
                  "failed: status: 0x%x, urb-status: 0x%x", 
                  status, urb.UrbHeader.Status);
      *received = 0;
    }
  else
    {
      *received = urb.UrbControlDescriptorRequest.TransferBufferLength;
    }

  return status;
}

USB_CONFIGURATION_DESCRIPTOR *
get_config_descriptor(libusb_device_t *dev, int configuration, int *size)
{
  USB_CONFIGURATION_DESCRIPTOR *desc;
  volatile int desc_size = sizeof(USB_CONFIGURATION_DESCRIPTOR);

  if(!(desc = ExAllocatePool(NonPagedPool, desc_size)))
    {
      return NULL;
    }

  if(!NT_SUCCESS(get_descriptor(dev, desc, desc_size, 
                                USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                USB_RECIP_DEVICE,
                                configuration - 1,
                                0, size, LIBUSB_DEFAULT_TIMEOUT)))
    {
      ExFreePool(desc);
      return NULL;
    }
  
  desc_size = desc->wTotalLength;
  ExFreePool(desc);
    
  if(!(desc = ExAllocatePool(NonPagedPool, desc_size)))
    {
      return NULL;
    }

  if(!NT_SUCCESS(get_descriptor(dev, desc, desc_size,
                                USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                USB_RECIP_DEVICE,
                                configuration - 1,
                                0, size, LIBUSB_DEFAULT_TIMEOUT)))
    {
      ExFreePool(desc);
      return NULL;
    }

  return desc;
}
