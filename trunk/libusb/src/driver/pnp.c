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


#include "libusb_driver.h"

static NTSTATUS on_start_complete(DEVICE_OBJECT *device_object, IRP *irp, 
				  void *context);

static NTSTATUS 
on_device_usage_notification_complete(DEVICE_OBJECT *device_object,
				      IRP *irp,
				      void *context);

static NTSTATUS on_query_capabilities_complete(DEVICE_OBJECT *device_object,
					       IRP *irp,
					       void *context);

NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, IRP *irp)
{
  NTSTATUS status = STATUS_SUCCESS;
  KEVENT event;
  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);

  status = remove_lock_acquire(&device_extension->remove_lock);
  
  if(!NT_SUCCESS(status))
    { 
      return complete_irp(irp, status, 0);
    }

  debug_print_nl();

  switch(IoGetCurrentIrpStackLocation(irp)->MinorFunction) 
    {     
    case IRP_MN_REMOVE_DEVICE:
      device_extension->is_started = 0;
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): IRP_MN_REMOVE_DEVICE");
      remove_lock_release_and_wait(&device_extension->remove_lock);
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
      control_object_delete(device_extension);
      IoDetachDevice(device_extension->next_stack_device);
      IoDeleteDevice(device_extension->self);
      return status;

    case IRP_MN_SURPRISE_REMOVAL:
      device_extension->is_started = 0;
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_SURPRISE_REMOVAL");
      break;
    case IRP_MN_START_DEVICE:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): IRP_MN_START_DEVICE");
      IoCopyCurrentIrpStackLocationToNext(irp);
      IoSetCompletionRoutine(irp, on_start_complete,
			     NULL, TRUE, TRUE, TRUE);
      remove_lock_release(&device_extension->remove_lock);
      return IoCallDriver(device_extension->next_stack_device, irp);

    case IRP_MN_CANCEL_REMOVE_DEVICE:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_CANCEL_REMOVE_DEVICE");
      break;
    case IRP_MN_CANCEL_STOP_DEVICE:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_CANCEL_STOP_DEVICE");
      break;
    case IRP_MN_QUERY_STOP_DEVICE:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_QUERY_STOP_DEVICE");
      break;
    case IRP_MN_STOP_DEVICE:
      device_extension->is_started = 0;
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): IRP_MN_STOP_DEVICE");
      break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_QUERY_REMOVE_DEVICE");
      break;
    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_DEVICE_USAGE_NOTIFICATION");

      if((device_extension->self->AttachedDevice == NULL) ||
	 (device_extension->self->AttachedDevice->Flags & DO_POWER_PAGABLE))
	{
	  device_extension->self->Flags |= DO_POWER_PAGABLE;
        }

        IoCopyCurrentIrpStackLocationToNext(irp);
        IoSetCompletionRoutine(irp, on_device_usage_notification_complete,
			       NULL, TRUE, TRUE, TRUE);
	remove_lock_release(&device_extension->remove_lock);
        return IoCallDriver(device_extension->next_stack_device, irp);

    case IRP_MN_QUERY_CAPABILITIES: 
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
		   "IRP_MN_QUERY_CAPABILITIES");

      /* Win2k work-around */
      if(!IoIsWdmVersionAvailable(1, 0x20))
	{
	  KeInitializeEvent(&event, NotificationEvent, FALSE);
	  IoCopyCurrentIrpStackLocationToNext(irp);
	  
	  IoSetCompletionRoutine(irp, on_query_capabilities_complete, &event,
				 TRUE, TRUE, TRUE);
	  
	  status = IoCallDriver(device_extension->next_stack_device, irp);
	  
	  if(status == STATUS_PENDING) 
	    {
	      KeWaitForSingleObject(&event, Executive, KernelMode,
				    FALSE, NULL);
	      status = irp->IoStatus.Status;
	    }
	  
	  if(NT_SUCCESS(status))
	    {
	      stack_location->Parameters.DeviceCapabilities.Capabilities
		->SurpriseRemovalOK = TRUE;
	    }
	  
	  remove_lock_release(&device_extension->remove_lock);
	  return status;
	}
      else
	{
	  stack_location->Parameters.DeviceCapabilities.Capabilities
	    ->SurpriseRemovalOK = TRUE;
	}
      
      break;

    default:
      status = irp->IoStatus.Status;
    }

  irp->IoStatus.Status = status;
  IoSkipCurrentIrpStackLocation(irp);
  remove_lock_release(&device_extension->remove_lock);
  return IoCallDriver(device_extension->next_stack_device, irp);
}

static NTSTATUS on_start_complete(DEVICE_OBJECT *device_object, IRP *irp, 
				  void *context)
{
  libusb_device_extension *device_extension
    = (libusb_device_extension *)device_object->DeviceExtension;

  if(irp->PendingReturned)
    {
      IoMarkIrpPending(irp);
    }
  
  if(NT_SUCCESS(irp->IoStatus.Status)) 
    {
      if(device_extension->next_stack_device->Characteristics 
	 & FILE_REMOVABLE_MEDIA) 
	{
	  device_object->Characteristics |= FILE_REMOVABLE_MEDIA;
        }
    }
  
  device_extension->is_started = 1;

  return STATUS_SUCCESS;
}

static NTSTATUS 
on_device_usage_notification_complete(DEVICE_OBJECT *device_object,
				      IRP *irp,
				      void *context)
{
  libusb_device_extension *device_extension
    = (libusb_device_extension *)device_object->DeviceExtension;

  if(irp->PendingReturned)
    {
      IoMarkIrpPending(irp);
    }

  if(!(device_extension->next_stack_device->Flags & DO_POWER_PAGABLE))
    {
        device_object->Flags &= ~DO_POWER_PAGABLE;
    }

  return STATUS_SUCCESS;
}

static NTSTATUS on_query_capabilities_complete(DEVICE_OBJECT *device_object,
					       IRP *irp,
					       void *context)
{
    KEVENT *event = (KEVENT *)context;

    if(irp->PendingReturned == TRUE) 
      {
        KeSetEvent(event, IO_NO_INCREMENT, FALSE);
      }

    return STATUS_MORE_PROCESSING_REQUIRED;
}
