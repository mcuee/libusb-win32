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
  NTSTATUS m_status = STATUS_SUCCESS;
  URB *urb;
  int i, junk, tmp_size;
  
  USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
  USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;

  USB_ENDPOINT_DESCRIPTOR *endpoint_descriptor = NULL;
  USBD_INTERFACE_INFORMATION *interface_information = NULL;

  KdPrint(("LIBUSB_FILTER - set_interface(): interface %d\n", interface));
  KdPrint(("LIBUSB_FILTER - set_interface(): altsetting %d\n", altsetting));

  if(!device_extension->current_configuration)
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): invalid configuration 0")); 
      return STATUS_UNSUCCESSFUL;
    }

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, sizeof(USB_CONFIGURATION_DESCRIPTOR));

  if(!configuration_descriptor)
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): memory_allocation error\n"));
      return STATUS_NO_MEMORY;
    }

  m_status = get_descriptor(device_extension,
			    (void *)configuration_descriptor,
			    NULL, sizeof(USB_CONFIGURATION_DESCRIPTOR), 
			    USB_CONFIGURATION_DESCRIPTOR_TYPE,
			    device_extension->current_configuration - 1,
			    0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(m_status))
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): getting configuration "
	       "descriptor failed\n"));
      ExFreePool((void*) configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }
  
  tmp_size = configuration_descriptor->wTotalLength;

  ExFreePool((void*) configuration_descriptor);

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, tmp_size);
  
  if(!configuration_descriptor)
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): memory_allocation error\n"));
      return STATUS_NO_MEMORY;
    }

  m_status = get_descriptor(device_extension,
				 (void *)configuration_descriptor,
				 NULL, tmp_size, 
				 USB_CONFIGURATION_DESCRIPTOR_TYPE,
				 device_extension->current_configuration - 1,
				 0, &junk, LIBUSB_DEFAULT_TIMEOUT);
  if(!NT_SUCCESS(m_status))
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): getting configuration "
	       "descriptor failed\n"));
      ExFreePool((void*) configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }
  interface_descriptor =
    USBD_ParseConfigurationDescriptorEx(configuration_descriptor,
					(void *)configuration_descriptor,
					interface, altsetting,
					-1, -1, -1);
  if(!interface_descriptor)
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): interface %d or altsetting "
	       "%d invalid\n", interface, altsetting));
      ExFreePool((void*) configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }
  
  tmp_size = 
    GET_SELECT_INTERFACE_REQUEST_SIZE(interface_descriptor->bNumEndpoints); 

  urb = (URB *)ExAllocatePool(NonPagedPool, tmp_size);

  if(!urb)
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): memory_allocation error\n"));
      ExFreePool((void*) configuration_descriptor);
      ExFreePool((void*) urb);
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
      
      interface_information->Pipes[i].MaximumTransferSize = MAX_READ_WRITE;
      interface_information->Pipes[i].PipeFlags = 0;
    }

  m_status = call_usbd(device_extension, (void *)urb, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);


  if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
      KdPrint(("LIBUSB_FILTER - set_interface(): setting interface failed "
	       "%x %x\n", m_status, urb->UrbHeader.Status));
      ExFreePool((void*) configuration_descriptor);
      ExFreePool((void*) urb);
      return STATUS_UNSUCCESSFUL;
    }

  update_pipe_info(device_extension, interface, interface_information);
  ExFreePool((void*) configuration_descriptor);
  ExFreePool((void*) urb);

  return m_status;
}

