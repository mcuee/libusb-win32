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
  int byte_count = sizeof(libusb_request);
  NTSTATUS status = STATUS_SUCCESS;

  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
  ULONG control_code =
    stack_location->Parameters.DeviceIoControl.IoControlCode;
  ULONG input_request_length
    = stack_location->Parameters.DeviceIoControl.InputBufferLength;
  ULONG output_request_length
    = stack_location->Parameters.DeviceIoControl.OutputBufferLength;
  ULONG transfer_buffer_length
    = stack_location->Parameters.DeviceIoControl.OutputBufferLength;
  libusb_request *request = (libusb_request *)irp->AssociatedIrp.SystemBuffer;
  MDL *transfer_buffer_mdl = irp->MdlAddress;

  URB urb, *urb1 = NULL;
  USBD_PIPE_HANDLE pipe_handle = NULL;

  status = remove_lock_acquire(&device_extension->remove_lock);

  if(!NT_SUCCESS(status))
    { 
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, status, 0);
    }

  if(!request)
    { 
      KdPrint(("LIBUSB_FILTER - dispatch_ioctl(): "
	       "invalid input or output buffer\n"));
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_INVALID_PARAMETER, 0);
    }
  
  switch(control_code) 
    {      
      
    case LIBUSB_IOCTL_SET_CONFIGURATION:

      if(input_request_length < sizeof(libusb_request))
	{	  
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_configuration: "
		   "invalid input buffer lenght\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = set_configuration(device_extension, 
				 request->configuration.configuration,
				 request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_configuration "
		   "failed\n"));
	}

      break;
      
    case LIBUSB_IOCTL_GET_CONFIGURATION:
      
      if(input_request_length < sizeof(libusb_request)
	 || output_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_configuration: "
		   "invalid input or output buffer size\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_configuration(device_extension, 
				 &(request->configuration.configuration),
				 request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_configuration "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_SET_INTERFACE:

      if(input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_interface: "
		   "invalid input buffer size\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      status = set_interface(device_extension,
			     request->interface.interface,
			     request->interface.altsetting,
			     request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_interface "
		   "failed\n"));
	}

      break;

    case LIBUSB_IOCTL_GET_INTERFACE:

      if(input_request_length < sizeof(libusb_request)
	 || output_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_interface: invalid "
		   "input or output buffer size\n"));

	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_interface(device_extension,
			     request->interface.interface,
			     &(request->interface.altsetting),
			     request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_interface "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_SET_FEATURE:

      if(input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_feature: invalid "
		   "input buffer size\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 	set_feature(device_extension,
			    request->feature.recipient,
			    request->feature.index,
			    request->feature.feature,
			    request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_feature "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_CLEAR_FEATURE:
      if(input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), clear_feature: invalid "
		   "input buffer size\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = clear_feature(device_extension,
			     request->feature.recipient,
			     request->feature.index,
			     request->feature.feature,
			     request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), clear_feature "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_GET_STATUS:

      if(input_request_length < sizeof(libusb_request)
	 || output_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_status: invalid "
		   "input or output buffer size\n"));
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_status(device_extension,
			  request->status.recipient,
			  request->status.index, 
			  &(request->status.status),
			  request->timeout);
      break;

    case LIBUSB_IOCTL_SET_DESCRIPTOR:

      if(!transfer_buffer_mdl || input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_descriptor: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = set_descriptor(device_extension, NULL, transfer_buffer_mdl, 
			      transfer_buffer_length, 
			      request->descriptor.type,
			      request->descriptor.index,
			      request->descriptor.language_id, 
			      &byte_count,
			      request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), set_descriptor "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_GET_DESCRIPTOR:

      if(!transfer_buffer_mdl || input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_descriptor: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      status = get_descriptor(device_extension, NULL, transfer_buffer_mdl, 
			      transfer_buffer_length,
			      request->descriptor.type,
			      request->descriptor.index,
			      request->descriptor.language_id, 
			      &byte_count, 
			      request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), get_descriptor "
		   "failed\n"));
	}
      break;
      
    case LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ:

      if(!transfer_buffer_mdl || input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), int_bulk_read: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = bulk_transfer(irp, device_extension,
			     request->endpoint.endpoint,
			     transfer_buffer_mdl, transfer_buffer_length, 
			     USBD_TRANSFER_DIRECTION_IN);

      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), int_bulk_write "
		   "failed\n"));
	}
      return status;

    case LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE:

      if(!transfer_buffer_mdl || input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), int_bulk_write: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = 	bulk_transfer(irp, device_extension,
			      request->endpoint.endpoint,
			      transfer_buffer_mdl, transfer_buffer_length, 
			      USBD_TRANSFER_DIRECTION_OUT);

      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), int_bulk_read "
		   "failed\n"));
	}
      return status;

    case LIBUSB_IOCTL_VENDOR_READ:

      if(!transfer_buffer_mdl || input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), vendor_read: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = vendor_request(device_extension,
			      request->vendor.request,
			      request->vendor.value,
			      request->vendor.index,
			      transfer_buffer_mdl,
			      transfer_buffer_length,
			      USBD_TRANSFER_DIRECTION_IN,
			      &byte_count,
			      request->timeout);

      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), vendor_read "
		   "failed\n"));
	}

      break;

    case LIBUSB_IOCTL_VENDOR_WRITE:
      
      if(!transfer_buffer_mdl || input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), vendor_write: invalid "
		   "input or transfer buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = vendor_request(device_extension,
			      request->vendor.request,
			      request->vendor.value,
			      request->vendor.index,
			      transfer_buffer_mdl,
			      transfer_buffer_length,
			      USBD_TRANSFER_DIRECTION_OUT, 
			      &byte_count,
			      request->timeout);

      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), vendor_read "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_RESET_ENDPOINT:

      if(input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), reset_endpoint: invalid "
		   "input buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = reset_endpoint(device_extension, 
			      request->endpoint.endpoint,
			      request->timeout);
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), reset_endpoint "
		   "failed\n"));
	}
      break;
      
    case LIBUSB_IOCTL_ABORT_ENDPOINT:
	 
      if(input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), abort_endpoint: invalid "
		   "input buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = reset_endpoint(device_extension, 
			      request->endpoint.endpoint,
			      request->timeout);

      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), abort_endpoint "
		   "failed\n"));
	}
      break;

    case LIBUSB_IOCTL_RESET_DEVICE: 
      
      if(input_request_length < sizeof(libusb_request))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), reset_device: invalid "
		   "input buffer\n"));
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = reset_device(device_extension, request->timeout);
      
      if(!NT_SUCCESS(status))
	{
	  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(), reset_device failed\n"));
	}
      break;
  
    default:
      
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
      remove_lock_release(&device_extension->remove_lock);

      return status;
    }

  complete_irp(irp, status, byte_count);
  
  KdPrint(("LIBUSB_FILTER - dispatch_ioctl(): %d bytes transfered\n", 
	   byte_count));
  
  remove_lock_release(&device_extension->remove_lock);

  return status;
}
