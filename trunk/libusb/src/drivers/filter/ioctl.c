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
  libusb_request *request = (libusb_request *)irp->AssociatedIrp.SystemBuffer;
  char *output_buffer = (char *)irp->AssociatedIrp.SystemBuffer;
  char *input_buffer = (char *)irp->AssociatedIrp.SystemBuffer;
  MDL *transfer_buffer_mdl = irp->MdlAddress;

  status = remove_lock_acquire(&device_extension->remove_lock);

  if(!NT_SUCCESS(status))
    { 
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, status, 0);
    }

  if(!request || input_buffer_length < sizeof(libusb_request))
    { 
      debug_printf(DEBUG_ERR, "dispatch_ioctl(): "
		   "invalid input or output buffer\n");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_INVALID_PARAMETER, 0);
    }
  
  debug_print_nl();

  switch(control_code) 
    {     
    case LIBUSB_IOCTL_SET_CONFIGURATION:

      status = set_configuration(device_extension, 
				 request->configuration.configuration,
				 request->timeout);
      break;
      
    case LIBUSB_IOCTL_GET_CONFIGURATION:
      
      if(!output_buffer || output_buffer_length < sizeof(libusb_request))
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), get_configuration: "
		       "output buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_configuration(device_extension, 
				 &request->configuration.configuration,
				 request->timeout);

      if(NT_SUCCESS(status))
	{
	  byte_count = sizeof(libusb_request);
	}
      break;

    case LIBUSB_IOCTL_SET_INTERFACE:

      status = set_interface(device_extension,
			     request->interface.interface,
			     request->interface.altsetting,
			     request->timeout);
      break;

    case LIBUSB_IOCTL_GET_INTERFACE:

      if(!output_buffer || output_buffer_length < sizeof(libusb_request))
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), get_interface: invalid "
		       "output buffer");
	  status =  STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_interface(device_extension,
			     request->interface.interface,
			     &(request->interface.altsetting),
			     request->timeout);

      if(NT_SUCCESS(status))
	{
	  byte_count = sizeof(libusb_request);
	}
      break;

    case LIBUSB_IOCTL_SET_FEATURE:

      status = set_feature(device_extension,
			   request->feature.recipient,
			   request->feature.index,
			   request->feature.feature,
			   request->timeout);
      
      break;

    case LIBUSB_IOCTL_CLEAR_FEATURE:

      status = clear_feature(device_extension,
			     request->feature.recipient,
			     request->feature.index,
			     request->feature.feature,
			     request->timeout);
      
      break;

    case LIBUSB_IOCTL_GET_STATUS:

      if(!output_buffer || output_buffer_length < sizeof(libusb_request))
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), get_status: invalid "
		       "output buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_status(device_extension,
			  request->status.recipient,
			  request->status.index, 
			  &(request->status.status),
			  request->timeout);

      if(NT_SUCCESS(status))
	{
	  byte_count = sizeof(libusb_request);
	}

      break;

    case LIBUSB_IOCTL_SET_DESCRIPTOR:

      if(input_buffer_length <= sizeof(libusb_request))
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), set_descriptor: invalid "
		       "input buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      
      status = set_descriptor(device_extension, 
			      input_buffer + sizeof(libusb_request), 
			      input_buffer_length - sizeof(libusb_request), 
			      request->descriptor.type,
			      request->descriptor.index,
			      request->descriptor.language_id, 
			      &byte_count,
			      request->timeout);
      
      break;

    case LIBUSB_IOCTL_GET_DESCRIPTOR:

      if(!output_buffer || !output_buffer_length)
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), get_descriptor: invalid "
		       "output buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = get_descriptor(device_extension, output_buffer, 
			      output_buffer_length,
			      request->descriptor.type,
			      request->descriptor.index,
			      request->descriptor.language_id, 
			      &byte_count, 
			      request->timeout);
      
      break;
      
    case LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ:

      if(!transfer_buffer_mdl)
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), bulk_int_read: invalid "
		       "transfer buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      return bulk_int_transfer(irp, device_extension,
			       request->endpoint.endpoint,
			       transfer_buffer_mdl, 
			       transfer_buffer_length, 
			       USBD_TRANSFER_DIRECTION_IN);

    case LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE:

      if(!transfer_buffer_mdl)
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), bulk_int_write: invalid "
		       "transfer buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      return bulk_int_transfer(irp, device_extension,
			       request->endpoint.endpoint,
			       transfer_buffer_mdl,
			       transfer_buffer_length, 
			       USBD_TRANSFER_DIRECTION_OUT);

    case LIBUSB_IOCTL_VENDOR_READ:

      if(output_buffer_length && !output_buffer)
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), vendor_read: invalid "
		       "output buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}

      status = vendor_class_request(device_extension,
				    request->vendor.type, 
				    request->vendor.recipient,
				    request->vendor.request,
				    request->vendor.value,
				    request->vendor.index,
				    output_buffer,
				    output_buffer_length,
				    USBD_TRANSFER_DIRECTION_IN,
				    &byte_count,
				    request->timeout);
      break;

    case LIBUSB_IOCTL_VENDOR_WRITE:
      
      status = vendor_class_request(device_extension,
				    request->vendor.type, 
				    request->vendor.recipient,
				    request->vendor.request,
				    request->vendor.value,
				    request->vendor.index,
				    input_buffer_length == sizeof(libusb_request) ?
				    NULL : input_buffer + sizeof(libusb_request),
				    input_buffer_length - sizeof(libusb_request),
				    USBD_TRANSFER_DIRECTION_OUT, 
				    &byte_count,
				    request->timeout);
      break;

    case LIBUSB_IOCTL_RESET_ENDPOINT:

      status = reset_endpoint(device_extension, 
			      request->endpoint.endpoint,
			      request->timeout);
      break;
      
    case LIBUSB_IOCTL_ABORT_ENDPOINT:
	 
      status = abort_endpoint(device_extension, 
			      request->endpoint.endpoint,
			      request->timeout);
      break;

    case LIBUSB_IOCTL_RESET_DEVICE: 
      
      status = reset_device(device_extension, request->timeout);
      break;

    case LIBUSB_IOCTL_SET_DEBUG_LEVEL:

      debug_set_level(request->debug.level);
      break;

    case LIBUSB_IOCTL_GET_VERSION:

      if(!output_buffer || output_buffer_length < sizeof(libusb_request))
	{
	  debug_printf(DEBUG_ERR, "dispatch_ioctl(), get_version: "
		       "invalid output buffer");
	  status = STATUS_INVALID_PARAMETER;
	  break;
	}
      request->version.major = LIBUSB_VERSION_MAJOR;
      request->version.minor = LIBUSB_VERSION_MINOR;
      request->version.micro = LIBUSB_VERSION_MICRO;
      request->version.nano  = LIBUSB_VERSION_NANO;

      byte_count = sizeof(libusb_request);
      break;

    default:
      
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
      remove_lock_release(&device_extension->remove_lock);

      return status;
    }

  complete_irp(irp, status, byte_count);  
  remove_lock_release(&device_extension->remove_lock);

  return status;
}
