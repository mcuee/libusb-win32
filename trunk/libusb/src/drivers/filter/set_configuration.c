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

NTSTATUS set_configuration(libusb_device_extension *device_extension,
			   int configuration, int timeout)
{
  NTSTATUS m_status = STATUS_SUCCESS;
  URB urb, *urb_ptr;
  USB_DEVICE_DESCRIPTOR device_descriptor;
  USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
  USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;
  USBD_INTERFACE_LIST_ENTRY *interfaces = NULL;
  int junk, i, j;
  volatile int tmp_size;

  KdPrint(("LIBUSB_FILTER - set_configuration(): configuration %d\n", 
	   configuration));
  KdPrint(("LIBUSB_FILTER - set_configuration(): timeout %d\n", timeout));

  if(!configuration)
    {
      UsbBuildSelectConfigurationRequest(&urb,
					 sizeof(struct _URB_SELECT_CONFIGURATION), 
					 NULL);
      m_status = call_usbd(device_extension, (void *)&urb, 
			   IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
      
      if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb.UrbHeader.Status))
	{
	  KdPrint(("LIBUSB_FILTER - set_configuration(): setting config %d "
		   "failed\n", configuration));
	  return STATUS_UNSUCCESSFUL;
	}

      device_extension->configuration_handle =
	urb.UrbSelectConfiguration.ConfigurationHandle;
     
      device_extension->current_configuration = configuration;

      update_pipe_info(device_extension, 0, NULL);
      return m_status;
    }

  m_status = get_descriptor(device_extension,
			    (void *)&device_descriptor,
			    NULL, sizeof(USB_DEVICE_DESCRIPTOR), 
			    USB_DEVICE_DESCRIPTOR_TYPE,
			    0, 0, &junk, LIBUSB_DEFAULT_TIMEOUT);  

  if(!NT_SUCCESS(m_status))
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): getting device "
	       "descriptor failed\n"));
      return STATUS_UNSUCCESSFUL;
    }

  if(device_descriptor.bNumConfigurations < configuration)
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): invalid configuration "
	       "%d\n", configuration));
      return STATUS_UNSUCCESSFUL;
    }


  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *) 
    ExAllocatePool(NonPagedPool, sizeof(USB_CONFIGURATION_DESCRIPTOR));
  
  if(!configuration_descriptor)
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): memory allocation "
	       "failed\n"));
      return STATUS_NO_MEMORY;
    }
  
  m_status = get_descriptor(device_extension,
			    (void *)configuration_descriptor,
			    NULL, sizeof(USB_CONFIGURATION_DESCRIPTOR), 
			    USB_CONFIGURATION_DESCRIPTOR_TYPE,
			    configuration - 1,
			    0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(m_status))
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): getting configuration "
	       "descriptor %d failed\n", configuration));
      ExFreePool((void*) configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }

  tmp_size = configuration_descriptor->wTotalLength;

      KdPrint(("LIBUSB_FILTER - set_configuration(): configuration descriptor "
	       "total length: %d\n", tmp_size));
  
  ExFreePool((void*) configuration_descriptor);

  configuration_descriptor = (USB_CONFIGURATION_DESCRIPTOR *)
    ExAllocatePool(NonPagedPool, tmp_size);
  
  if(!configuration_descriptor)
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): memory allocation "
	       "failed\n"));
      return STATUS_NO_MEMORY;
    }

  m_status = get_descriptor(device_extension,
			    (void *)configuration_descriptor,
			    NULL, tmp_size, 
			    USB_CONFIGURATION_DESCRIPTOR_TYPE,
			    configuration - 1,
			    0, &junk, LIBUSB_DEFAULT_TIMEOUT);

  if(!NT_SUCCESS(m_status))
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): getting configuration "
	       "descriptor %d failed\n"));
      ExFreePool((void*) configuration_descriptor);
      return STATUS_UNSUCCESSFUL;
    }


  interfaces = (USBD_INTERFACE_LIST_ENTRY *)
    ExAllocatePool(NonPagedPool,(configuration_descriptor->bNumInterfaces + 1)
		   * sizeof(USBD_INTERFACE_LIST_ENTRY));

  if(!interfaces)
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): memory allocation "
	       "failed\n"));
      ExFreePool((void*) configuration_descriptor);
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
      if(interface_descriptor)
	{
	  KdPrint(("LIBUSB_FILTER - set_configuration(): interface %d "
		   "found\n", i));
	  interfaces[i].InterfaceDescriptor = interface_descriptor;
	}
      else
	{
	  KdPrint(("LIBUSB_FILTER - set_configuration(): interface %d not "
		   "found\n",i));
	  ExFreePool((void*) configuration_descriptor);
	  return STATUS_INVALID_PARAMETER;
	}
    }


  urb_ptr = USBD_CreateConfigurationRequestEx(configuration_descriptor,
					      interfaces);
  if(!urb_ptr)
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): memory allocation "
	       "failed\n"));
      ExFreePool((void*) configuration_descriptor);
      ExFreePool((void *)urb_ptr);
      return STATUS_NO_MEMORY;
    }


  for(i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
      for(j = 0; j < (int)interfaces[i].Interface->NumberOfPipes; j++)
	{
	  interfaces[i].Interface->Pipes[j].MaximumTransferSize = 
	    MAX_READ_WRITE;
	}
    }

  m_status = call_usbd(device_extension, (void *)urb_ptr, 
		       IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);
  
  if(!NT_SUCCESS(m_status) || !USBD_SUCCESS(urb_ptr->UrbHeader.Status))
    {
      KdPrint(("LIBUSB_FILTER - set_configuration(): setting configuration "
	       "%d %x %x failed\n", configuration, m_status, 
	       urb_ptr->UrbHeader.Status));
      ExFreePool((void*) configuration_descriptor);
      ExFreePool((void *)urb_ptr);
      return STATUS_UNSUCCESSFUL;
    }


  device_extension->configuration_handle =
    urb_ptr->UrbSelectConfiguration.ConfigurationHandle;
  
  device_extension->current_configuration = configuration;

  for(i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
      update_pipe_info(device_extension, i, interfaces[i].Interface);
    }

  ExFreePool(interfaces);
  ExFreePool((void *)urb_ptr);
  ExFreePool((void*) configuration_descriptor);

  return m_status;
}
