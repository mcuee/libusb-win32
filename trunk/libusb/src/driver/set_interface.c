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


NTSTATUS set_interface(libusb_device_t *dev, int interface, int altsetting,
                       int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB *urb;
  int i, junk;
  volatile int tmp_size, config_full_size;

  USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
  USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;
  USBD_INTERFACE_INFORMATION *interface_information = NULL;

  DEBUG_PRINT_NL();
  DEBUG_MESSAGE("set_interface(): interface %d", interface);
  DEBUG_MESSAGE("set_interface(): altsetting %d", altsetting);
  DEBUG_MESSAGE("set_interface(): timeout %d", timeout);

  if(!dev->configuration)
    {
      DEBUG_ERROR("set_interface(): invalid configuration 0"); 
      return STATUS_INVALID_DEVICE_STATE;
    }

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, sizeof(USB_CONFIGURATION_DESCRIPTOR));

  if(!configuration_descriptor)
    {
      DEBUG_ERROR("set_interface(): memory_allocation error");
      return STATUS_NO_MEMORY;
    }

  status = get_descriptor(dev, configuration_descriptor,
                          sizeof(USB_CONFIGURATION_DESCRIPTOR), 
                          USB_CONFIGURATION_DESCRIPTOR_TYPE,
                          dev->configuration - 1,
                          0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(status))
    {
      DEBUG_ERROR("set_interface(): getting configuration descriptor failed");
      ExFreePool(configuration_descriptor);
      return status;
    }
  
  config_full_size = configuration_descriptor->wTotalLength;

  ExFreePool(configuration_descriptor);

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, config_full_size);
  
  if(!configuration_descriptor)
    {
      DEBUG_ERROR("set_interface(): memory_allocation error");
      return STATUS_NO_MEMORY;
    }

  status = get_descriptor(dev, configuration_descriptor,
                          config_full_size, 
                          USB_CONFIGURATION_DESCRIPTOR_TYPE,
                          dev->configuration - 1,
                          0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(status))
    {
      DEBUG_ERROR("set_interface(): getting configuration descriptor failed");
      ExFreePool(configuration_descriptor);
      return status;
    }

  interface_descriptor =
    USBD_ParseConfigurationDescriptorEx(configuration_descriptor,
                                        configuration_descriptor,
                                        interface, altsetting,
                                        -1, -1, -1);

  if(!interface_descriptor)
    {
      DEBUG_ERROR("set_interface(): interface %d or altsetting %d invalid", 
                  interface, altsetting);
      ExFreePool(configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }
  
  tmp_size = sizeof(struct _URB_SELECT_INTERFACE)
    + (interface_descriptor->bNumEndpoints - 1)
    * sizeof(USBD_PIPE_INFORMATION);


  urb = (URB *)ExAllocatePool(NonPagedPool, tmp_size);

  if(!urb)
    {
      DEBUG_ERROR("set_interface(): memory_allocation error");
      ExFreePool(configuration_descriptor);
      return STATUS_NO_MEMORY;
    }

  memset(urb, 0, tmp_size);

  urb->UrbHeader.Function = URB_FUNCTION_SELECT_INTERFACE;
  urb->UrbHeader.Length = (USHORT)tmp_size;

  urb->UrbSelectInterface.ConfigurationHandle = dev->configuration_handle;
  urb->UrbSelectInterface.Interface.Length =
    sizeof(struct _USBD_INTERFACE_INFORMATION);

  if(interface_descriptor->bNumEndpoints > 1)
    {
      urb->UrbSelectInterface.Interface.Length +=
        (interface_descriptor->bNumEndpoints - 1)
        * sizeof(struct _USBD_PIPE_INFORMATION);
    }

  urb->UrbSelectInterface.Interface.InterfaceNumber = (UCHAR)interface;
  urb->UrbSelectInterface.Interface.AlternateSetting = (UCHAR)altsetting;

  interface_information = &urb->UrbSelectInterface.Interface;

  for(i = 0; i < interface_descriptor->bNumEndpoints; i++)
    {
      interface_information->Pipes[i].MaximumTransferSize 
        = LIBUSB_MAX_READ_WRITE;
    }

  status = call_usbd(dev, urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);


  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
      DEBUG_ERROR("set_interface(): setting interface failed: status: 0x%x, "
                  "urb-status: 0x%x", status, urb->UrbHeader.Status);
      ExFreePool(configuration_descriptor);
      ExFreePool(urb);
      return STATUS_UNSUCCESSFUL;
    }

  update_pipe_info(dev, interface, interface_information);

  ExFreePool(configuration_descriptor);
  ExFreePool(urb);

  return status;
}

