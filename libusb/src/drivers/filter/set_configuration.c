/* LIBUSB-WIN32, Generic Windows USB Driver
 * Copyright (C) 2002-2004 Stephan Meyer, <ste_meyer@web.de>
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

NTSTATUS set_configuration(libusb_device_extension *device_extension,
			   int configuration, int timeout)
{
  NTSTATUS status = STATUS_SUCCESS;
  URB urb, *urb_ptr = NULL;
  USB_DEVICE_DESCRIPTOR device_descriptor;
  USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
  USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;
  USBD_INTERFACE_LIST_ENTRY *interfaces = NULL;
  int junk, i, j;
  volatile int config_full_size;

  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "set_configuration(): configuration %d", 
	       configuration);
  debug_printf(LIBUSB_DEBUG_MSG, "set_configuration(): timeout %d", timeout);

  if(device_extension->current_configuration == configuration)
    {
      return STATUS_SUCCESS;
    }

  if(!configuration)
    {
      UsbBuildSelectConfigurationRequest
	(&urb,
	 sizeof(struct _URB_SELECT_CONFIGURATION), 
	 NULL);

      status = call_usbd(device_extension, &urb, 
			 IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
      
      if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
	{
	  debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): setting "
		       "configuration %d failed: status: 0x%x, "
		       "urb-status: 0x%x", 
		       configuration, status, urb.UrbHeader.Status);
	  return status;
	}

      device_extension->configuration_handle =
	urb.UrbSelectConfiguration.ConfigurationHandle;
     
      device_extension->current_configuration = configuration;

      clear_pipe_info(device_extension);
      return status;
    }

  status = get_descriptor(device_extension,
			  &device_descriptor,
			  sizeof(USB_DEVICE_DESCRIPTOR), 
			  USB_DEVICE_DESCRIPTOR_TYPE,
			  0, 0, &junk, LIBUSB_DEFAULT_TIMEOUT);  

  if(!NT_SUCCESS(status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): getting device "
		   "descriptor failed");
      return status;
    }

  if(device_descriptor.bNumConfigurations < configuration)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): invalid "
		   "configuration %d", configuration);
      return STATUS_INVALID_PARAMETER;
    }

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *) 
    ExAllocatePool(NonPagedPool, sizeof(USB_CONFIGURATION_DESCRIPTOR));
  
  if(!configuration_descriptor)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): memory allocation "
		   "failed");
      return STATUS_NO_MEMORY;
    }
  
  status = get_descriptor(device_extension,
			  configuration_descriptor,
			  sizeof(USB_CONFIGURATION_DESCRIPTOR), 
			  USB_CONFIGURATION_DESCRIPTOR_TYPE,
			  configuration - 1,
			  0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): getting "
		   "configuration descriptor %d failed", configuration);
      ExFreePool(configuration_descriptor);
      return status;
    }

  config_full_size = configuration_descriptor->wTotalLength;

  ExFreePool(configuration_descriptor);

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, config_full_size);
  
  if(!configuration_descriptor)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): memory allocation "
		   "failed");
      return STATUS_NO_MEMORY;
    }

  status = get_descriptor(device_extension,
			  configuration_descriptor,
			  config_full_size, 
			  USB_CONFIGURATION_DESCRIPTOR_TYPE,
			  configuration - 1,
			  0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): getting "
		   "configuration descriptor %d failed");
      ExFreePool(configuration_descriptor);
      return status;
    }


  interfaces = (USBD_INTERFACE_LIST_ENTRY *)
    ExAllocatePool(NonPagedPool,(configuration_descriptor->bNumInterfaces + 1)
		   * sizeof(USBD_INTERFACE_LIST_ENTRY));

  if(!interfaces)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): memory allocation "
		   "failed");
      ExFreePool(configuration_descriptor);
      return STATUS_NO_MEMORY;
    }

  RtlZeroMemory(interfaces, (configuration_descriptor->bNumInterfaces + 1) 
		* sizeof(USBD_INTERFACE_LIST_ENTRY));


  for(i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
      interface_descriptor =
	USBD_ParseConfigurationDescriptorEx(configuration_descriptor,
					    configuration_descriptor,
					    i, 0, -1, -1, -1);
      if(!interface_descriptor ||
	 ((char *)interface_descriptor + interface_descriptor->bNumEndpoints 
	  * sizeof(USB_ENDPOINT_DESCRIPTOR) 
	  + sizeof(USB_INTERFACE_DESCRIPTOR)) 
	 > ((char *)configuration_descriptor + config_full_size))
	{
	  debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): interface %d "
		       "not found", i);
	  ExFreePool(interfaces);
	  ExFreePool(configuration_descriptor);
	  return STATUS_INVALID_PARAMETER;
	}
      else
	{
	  debug_printf(LIBUSB_DEBUG_MSG, "set_configuration(): interface %d "
		       "found", i);
	  interfaces[i].InterfaceDescriptor = interface_descriptor;
	}
    }


  urb_ptr = USBD_CreateConfigurationRequestEx(configuration_descriptor,
					      interfaces);
  if(!urb_ptr)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): memory allocation "
		   "failed");
      ExFreePool(interfaces);
      ExFreePool(configuration_descriptor);
      return STATUS_NO_MEMORY;
    }

  for(i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
      for(j = 0; j < (int)interfaces[i].Interface->NumberOfPipes; j++)
	{
	  interfaces[i].Interface->Pipes[j].MaximumTransferSize = 
	    LIBUSB_MAX_READ_WRITE;
	}
    }

  status = call_usbd(device_extension, urb_ptr, 
		     IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb_ptr->UrbHeader.Status))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "set_configuration(): setting "
		   "configuration %d failed: status: 0x%x, urb-status: 0x%x", 
		    configuration, status, urb_ptr->UrbHeader.Status);
      ExFreePool(interfaces);
      ExFreePool(configuration_descriptor);
      ExFreePool(urb_ptr);
      return status;
    }


  device_extension->configuration_handle =
    urb_ptr->UrbSelectConfiguration.ConfigurationHandle;
  
  device_extension->current_configuration = configuration;

  for(i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
      update_pipe_info(device_extension, i, interfaces[i].Interface);
    }

  ExFreePool(interfaces);
  ExFreePool(urb_ptr);
  ExFreePool(configuration_descriptor);

  return status;
}
