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


NTSTATUS dispatch_ioctl(libusb_device_extension *device_extension, IRP *irp)
{
  int byte_count = 0;
  NTSTATUS status = STATUS_SUCCESS;

  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
  ULONG control_code =
    stack_location->Parameters.DeviceIoControl.IoControlCode;
  ULONG input_buffer_length
    = stack_location->Parameters.DeviceIoControl.InputBufferLength;
  ULONG output_buffer_length
    = stack_location->Parameters.DeviceIoControl.OutputBufferLength;
  ULONG transfer_buffer_length
    = stack_location->Parameters.DeviceIoControl.OutputBufferLength;
  void *output_buffer = irp->AssociatedIrp.SystemBuffer;
  void *input_buffer = irp->AssociatedIrp.SystemBuffer;
  MDL *transfer_buffer_mdl = irp->MdlAddress;

  URB urb, *urb1;
  USBD_PIPE_HANDLE pipe_handle = NULL;

  status = acquire_remove_lock(&device_extension->remove_lock);

  if(!NT_SUCCESS(status))
    { 
      return complete_irp(irp, status, 0);
    }

  switch(control_code) 
    {      
          
    case LIBUSB_IOCTL_SET_CONFIGURATION:

      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_configuration: "
		   "invalid input buffer\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = 
	set_configuration(device_extension, 
			       ((usb_configuration_request *)input_buffer)
			       ->configuration,
			       ((usb_configuration_request *)input_buffer)
			       ->timeout);
      
      break;

    case LIBUSB_IOCTL_GET_CONFIGURATION:
      
      if(!input_buffer || !output_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_configuration: "
		   "invalid input or output buffer\n"));
	  status =  STATUS_INSUFFICIENT_RESOURCES;
	  break;
	}

      status = get_configuration(device_extension, 
				 &(((usb_configuration_request *)output_buffer)
				 ->configuration),
				 ((usb_configuration_request *)input_buffer)
				 ->timeout);
      byte_count = sizeof(usb_configuration_request);
      break;

    case LIBUSB_IOCTL_SET_INTERFACE:

      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_interface: "
		   "invalid input buffer\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	set_interface(device_extension,
			   ((usb_interface_request *)input_buffer)
			   ->interface,
			   ((usb_interface_request *)input_buffer)
			   ->altsetting,
			   ((usb_interface_request *)input_buffer)->timeout);
      break;

    case LIBUSB_IOCTL_GET_INTERFACE:
      
      if(!input_buffer || !output_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_interface: invalid "
		   "input or output buffer\n"));

	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_interface(device_extension,
			     ((usb_interface_request *)input_buffer)
			     ->interface,
			     &(((usb_interface_request *)output_buffer)
			       ->altsetting),
			     ((usb_interface_request *)input_buffer)->timeout);

      byte_count = sizeof(usb_interface_request);
      break;

    case LIBUSB_IOCTL_SET_FEATURE:

      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_feature: invalid "
		   "input buffer\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	set_feature(device_extension,
			 ((usb_feature_request *)input_buffer)->recipient,
			 ((usb_feature_request *)input_buffer)->index,
			 ((usb_feature_request *)input_buffer)->feature,
			 ((usb_feature_request *)input_buffer)->timeout);
      break;

    case LIBUSB_IOCTL_CLEAR_FEATURE:

      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), clear_feature: invalid "
		   "input buffer\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	clear_feature(device_extension,
			   ((usb_feature_request *)input_buffer)->recipient,
			   ((usb_feature_request *)input_buffer)->index,
			   ((usb_feature_request *)input_buffer)->feature,
			   ((usb_feature_request *)input_buffer)->timeout);
      break;

    case LIBUSB_IOCTL_GET_STATUS:

      if(!output_buffer || !input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_status: invalid "
		   "input or output buffer\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = get_status(device_extension,
			  ((usb_status_request *)input_buffer)->recipient,
			  ((usb_status_request *)input_buffer)->index, 
			  &(((usb_status_request *)output_buffer)->status),
			  ((usb_status_request *)input_buffer)->timeout);

      byte_count = sizeof(usb_status_request);
      break;

    case LIBUSB_IOCTL_SET_DESCRIPTOR:

      if(!transfer_buffer_mdl || !input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_descriptor: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	set_descriptor(device_extension, NULL, transfer_buffer_mdl, 
			    transfer_buffer_length, 
			    ((usb_descriptor_request *)input_buffer)->type,
			    ((usb_descriptor_request *)input_buffer)->index,
			    ((usb_descriptor_request *)input_buffer)
			    ->language_id, &byte_count,
			    ((usb_feature_request *)input_buffer)->timeout);
      
      break;

    case LIBUSB_IOCTL_GET_DESCRIPTOR:

      if(!transfer_buffer_mdl || !input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_descriptor: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	get_descriptor(device_extension, NULL, transfer_buffer_mdl, 
			    transfer_buffer_length,
			    ((usb_descriptor_request *)input_buffer)->type,
			    ((usb_descriptor_request *)input_buffer) ->index,
			    ((usb_descriptor_request *)input_buffer)
			    ->language_id, &byte_count, 
			    ((usb_descriptor_request *)input_buffer)->timeout);
      break;
      
    case LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ:

      if(!transfer_buffer_mdl || !input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), int_bulk_read: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	bulk_transfer(device_extension,
			   ((usb_bulk_transfer *)input_buffer)->endpoint,
			   transfer_buffer_mdl, transfer_buffer_length, 
			   USBD_TRANSFER_DIRECTION_IN, &byte_count, 
			   ((usb_bulk_transfer *)input_buffer)->timeout);
      
      break;

    case LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE:

      if(!transfer_buffer_mdl || !input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), int_bulk_write: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = 
	bulk_transfer(device_extension,
			   ((usb_bulk_transfer *)input_buffer)->endpoint,
			   transfer_buffer_mdl, transfer_buffer_length, 
			   USBD_TRANSFER_DIRECTION_OUT, &byte_count, 
			   ((usb_bulk_transfer *)input_buffer)->timeout);
      
      break;

    case LIBUSB_IOCTL_VENDOR_READ:

      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), vendor_read: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	vendor_request(device_extension,
			    ((usb_vendor_request *)input_buffer)->request,
			    ((usb_vendor_request *)input_buffer)->value,
			    ((usb_vendor_request *)input_buffer)->index,
			    transfer_buffer_mdl,
			    transfer_buffer_length,
			    USBD_TRANSFER_DIRECTION_IN, &byte_count,
			    ((usb_vendor_request *)input_buffer)->timeout);
      break;

    case LIBUSB_IOCTL_VENDOR_WRITE:
      
      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), vendor_write: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 
	vendor_request(device_extension,
			    ((usb_vendor_request *)input_buffer)->request,
			    ((usb_vendor_request *)input_buffer)->value,
			    ((usb_vendor_request *)input_buffer)->index,
			    transfer_buffer_mdl,
			    transfer_buffer_length,
			    USBD_TRANSFER_DIRECTION_OUT, &byte_count,
			    ((usb_vendor_request *)input_buffer)->timeout);
      break;

    case LIBUSB_IOCTL_RESET_ENDPOINT:
      
      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), reset_endpoint: invalid "
		   "input buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status =  
	reset_endpoint(device_extension, 
			    ((usb_pipe_request* )input_buffer)->endpoint,
			    ((usb_pipe_request* )input_buffer)->timeout);

      break;
      
    case LIBUSB_IOCTL_ABORT_ENDPOINT:
	 
      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), abort_endpoint: invalid "
		   "input buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status =  
	reset_endpoint(device_extension, 
			    ((usb_pipe_request* )input_buffer)->endpoint,
			    ((usb_pipe_request* )input_buffer)->timeout);

      break;

    case LIBUSB_IOCTL_RESET_DEVICE: 
      
      if(!input_buffer)
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), reset_device: invalid "
		   "input buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = reset_device(device_extension, 
			    ((usb_device_request* )input_buffer)->timeout);
      
      break;
  
    default:
      
      pass_irp_down(device_extension, irp);
    }

  complete_irp(irp, status, byte_count);
  
  release_remove_lock(&device_extension->remove_lock);

  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(): %d bytes transfered\n", 
	   byte_count));
  
  return status;
}
