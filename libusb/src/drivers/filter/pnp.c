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


NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, IRP *irp)
{
  NTSTATUS status = STATUS_SUCCESS;
  KEVENT event;
  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);
  
  status = acquire_remove_lock(&device_extension->remove_lock);

  if(!NT_SUCCESS(status))
    { 
      return complete_irp(irp, status, 0);
    }

  switch(stack_location->MinorFunction) 
    {      
    case IRP_MN_START_DEVICE:

      KeInitializeEvent(&event, NotificationEvent, FALSE);
      
      IoCopyCurrentIrpStackLocationToNext(irp);
      IoSetCompletionRoutine(irp, 
			     (PIO_COMPLETION_ROUTINE)on_start_completion, 
			     (void*)&event, TRUE, TRUE, TRUE);
      
      status = IoCallDriver(device_extension->next_stack_device, irp);
      
      if(status == STATUS_PENDING) 
	{
	  KeWaitForSingleObject(&event, Executive, 
				KernelMode, FALSE, NULL);
	  status = irp->IoStatus.Status;
	}

      if(!(NT_SUCCESS(status) && NT_SUCCESS(irp->IoStatus.Status)))
	{ 
	  KdPrint(("LIBUSB_FILTER - dispatch_pnp(): calling lower driver"
		   "failed\n"));
	  break;
	}

      irp->IoStatus.Status = status;
      IoCompleteRequest(irp, IO_NO_INCREMENT);
      break;

    case IRP_MN_REMOVE_DEVICE:

      delete_control_object(device_extension);

      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);


      if(!NT_SUCCESS(status))
	{ 
	  KdPrint(("LIBUSB_FILTER - dispatch_pnp(): calling lower driver failed\n"));
	  return status;
	}

      release_remove_lock_and_wait(&device_extension->remove_lock);

      IoDetachDevice(device_extension->next_stack_device);
      IoDeleteDevice(device_extension->self);

      return status;

    case IRP_MN_CANCEL_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_STOP_DEVICE:
    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_SURPRISE_REMOVAL:
    default:
      IoSkipCurrentIrpStackLocation (irp);
      status = IoCallDriver (device_extension->next_stack_device, irp);

    }
  release_remove_lock(&device_extension->remove_lock);

  return status;
}



NTSTATUS on_start_completion(DEVICE_OBJECT *device_object, 
			     IRP *irp, void *event)
{
  KeSetEvent((KEVENT *)event, IO_NO_INCREMENT, FALSE);
  return STATUS_MORE_PROCESSING_REQUIRED;
}
