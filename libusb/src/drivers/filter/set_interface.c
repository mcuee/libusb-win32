/* LIBUSB-WIN32, Generic Windows USB Driver
 * Copyright (C) 2002-2003 Stephan Meyer, <ste_meyer@web.de>
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


#include "libusb_filter.h"


NTSTATUS set_interface(libusb_device_extension *device_extension,
		       int interface, int altsetting, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB *urb;
  int i, junk;
  volatile int tmp_size, config_full_size;
  
  USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
  USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;
  USB_ENDPOINT_DESCRIPTOR *endpoint_descriptor = NULL;
  USBD_INTERFACE_INFORMATION *interface_information = NULL;

  debug_print_nl();
  debug_printf(DEBUG_MSG, "set_interface(): interface %d", interface);
  debug_printf(DEBUG_MSG, "set_interface(): altsetting %d", altsetting);
  debug_printf(DEBUG_MSG, "set_interface(): timeout %d", timeout);

  if(!device_extension->current_configuration)
    {
      debug_printf(DEBUG_ERR, "set_interface(): invalid configuration 0"); 
      return STATUS_INVALID_DEVICE_STATE;
    }

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, sizeof(USB_CONFIGURATION_DESCRIPTOR));

  if(!configuration_descriptor)
    {
      debug_printf(DEBUG_ERR, "set_interface(): memory_allocation error");
      return STATUS_NO_MEMORY;
    }

  status = get_descriptor(device_extension,
			  configuration_descriptor,
			  sizeof(USB_CONFIGURATION_DESCRIPTOR), 
			  USB_CONFIGURATION_DESCRIPTOR_TYPE,
			  device_extension->current_configuration - 1,
			  0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(status))
    {
      debug_printf(DEBUG_ERR, "set_interface(): getting configuration "
	       "descriptor failed");
      ExFreePool(configuration_descriptor);
      return status;
    }
  
  config_full_size = configuration_descriptor->wTotalLength;

  ExFreePool(configuration_descriptor);

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, config_full_size);
  
  if(!configuration_descriptor)
    {
      debug_printf(DEBUG_ERR, "set_interface(): memory_allocation error");
      return STATUS_NO_MEMORY;
    }

  status = get_descriptor(device_extension,
			  configuration_descriptor,
			  config_full_size, 
			  USB_CONFIGURATION_DESCRIPTOR_TYPE,
			  device_extension->current_configuration - 1,
			  0, &junk, LIBUSB_DEFAULT_TIMEOUT);
  if(!NT_SUCCESS(status))
    {
      debug_printf(DEBUG_ERR, "set_interface(): getting configuration "
		   "descriptor failed");
      ExFreePool(configuration_descriptor);
      return status;
    }
  interface_descriptor =
    USBD_ParseConfigurationDescriptorEx(configuration_descriptor,
					configuration_descriptor,
					interface, altsetting,
					-1, -1, -1);

  if(!interface_descriptor ||
     ((char *)interface_descriptor + interface_descriptor->bNumEndpoints 
      * sizeof(USB_ENDPOINT_DESCRIPTOR) + sizeof(USB_INTERFACE_DESCRIPTOR)) 
     > ((char *)configuration_descriptor + config_full_size))
    {
      debug_printf(DEBUG_ERR, "set_interface(): interface %d or altsetting "
		   "%d invalid", interface, altsetting);
      ExFreePool(configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }
  
  tmp_size = 
    GET_SELECT_INTERFACE_REQUEST_SIZE(interface_descriptor->bNumEndpoints); 

  urb = (URB *)ExAllocatePool(NonPagedPool, tmp_size);

  if(!urb)
    {
      debug_printf(DEBUG_ERR, "set_interface(): memory_allocation error\n");
      ExFreePool(configuration_descriptor);
      return STATUS_NO_MEMORY;
    }


  urb->UrbHeader.Function = URB_FUNCTION_SELECT_INTERFACE;
  urb->UrbHeader.Length = (USHORT)tmp_size;
  urb->UrbSelectInterface.ConfigurationHandle =
    device_extension->configuration_handle;
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
  urb->UrbSelectInterface.Interface.Class 
    = interface_descriptor->bInterfaceClass;
  urb->UrbSelectInterface.Interface.SubClass =
    interface_descriptor->bInterfaceSubClass;
  urb->UrbSelectInterface.Interface.Protocol =
    interface_descriptor->bInterfaceProtocol;
  urb->UrbSelectInterface.Interface.NumberOfPipes =
    interface_descriptor->bNumEndpoints;

   interface_information = &urb->UrbSelectInterface.Interface;
   endpoint_descriptor = (USB_ENDPOINT_DESCRIPTOR *)
     ((ULONG)interface_descriptor + (ULONG)interface_descriptor->bLength);

  for(i = 0; i < interface_descriptor->bNumEndpoints; i++)
    {
      interface_information->Pipes[i].MaximumPacketSize
	= endpoint_descriptor->wMaxPacketSize;
      interface_information->Pipes[i].EndpointAddress 
	= endpoint_descriptor->bEndpointAddress;
      interface_information->Pipes[i].Interval 
	= endpoint_descriptor->bInterval;
      
      switch(endpoint_descriptor->bmAttributes)
	{
	case 0x00:
	  interface_information->Pipes[i].PipeType = UsbdPipeTypeControl;
	  break;
	case 0x01:
	  interface_information->Pipes[i].PipeType = UsbdPipeTypeIsochronous;
	  break;
	case 0x02:
	  interface_information->Pipes[i].PipeType = UsbdPipeTypeBulk;
	  break;
	case 0x03:
	  interface_information->Pipes[i].PipeType = UsbdPipeTypeInterrupt;
	  break;
	default:
	  ;	
	}
      
      endpoint_descriptor++;
      
      interface_information->Pipes[i].MaximumTransferSize 
	= LIBUSB_MAX_READ_WRITE;
      interface_information->Pipes[i].PipeFlags = 0;
    }

  status = call_usbd(device_extension, urb, 
		     IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);


  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
      debug_printf(DEBUG_ERR, "set_interface(): setting interface failed "
		   "%x %x", status, urb->UrbHeader.Status);
      ExFreePool(configuration_descriptor);
      ExFreePool(urb);
      return STATUS_UNSUCCESSFUL;
    }

  update_pipe_info(device_extension, interface, interface_information);

  ExFreePool(configuration_descriptor);
  ExFreePool(urb);

  return status;
}

