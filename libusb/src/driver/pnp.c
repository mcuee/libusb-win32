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

static NTSTATUS DDKAPI 
on_start_complete(DEVICE_OBJECT *device_object, IRP *irp, 
                  void *context);

static NTSTATUS DDKAPI
on_device_usage_notification_complete(DEVICE_OBJECT *device_object,
                                      IRP *irp, void *context);

static NTSTATUS DDKAPI 
on_query_capabilities_complete(DEVICE_OBJECT *device_object,
                               IRP *irp, void *context);

NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, IRP *irp)
{
  NTSTATUS status = STATUS_SUCCESS;
  IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);

  status = remove_lock_acquire(&device_extension->remove_lock);
  
  if(!NT_SUCCESS(status))
    { 
      return complete_irp(irp, status, 0);
    }

  debug_print_nl();

  switch(stack_location->MinorFunction) 
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
      IoSetCompletionRoutine(irp, on_start_complete, NULL, TRUE, TRUE, TRUE);

      return IoCallDriver(device_extension->next_stack_device, irp);

    case IRP_MN_STOP_DEVICE:
      device_extension->is_started = 0;
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): IRP_MN_STOP_DEVICE");
      break;

    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
                   "IRP_MN_DEVICE_USAGE_NOTIFICATION");

      if(!device_extension->self->AttachedDevice
         || (device_extension->self->AttachedDevice->Flags & DO_POWER_PAGABLE))
        {
          device_extension->self->Flags |= DO_POWER_PAGABLE;
        }

      IoCopyCurrentIrpStackLocationToNext(irp);
      IoSetCompletionRoutine(irp, on_device_usage_notification_complete,
                             NULL, TRUE, TRUE, TRUE);
      return IoCallDriver(device_extension->next_stack_device, irp);

    case IRP_MN_QUERY_CAPABILITIES: 
      debug_printf(LIBUSB_DEBUG_MSG, "dispatch_pnp(): "
                   "IRP_MN_QUERY_CAPABILITIES");

      if(!device_extension->self->AttachedDevice)
        {
          stack_location->Parameters.DeviceCapabilities.Capabilities
            ->SurpriseRemovalOK = TRUE;
        }

      IoCopyCurrentIrpStackLocationToNext(irp);
      
      IoSetCompletionRoutine(irp, on_query_capabilities_complete, NULL,
                             TRUE, TRUE, TRUE);
      
      return IoCallDriver(device_extension->next_stack_device, irp);

    default:
      status = irp->IoStatus.Status;
    }

  irp->IoStatus.Status = status;
  IoSkipCurrentIrpStackLocation(irp);
  remove_lock_release(&device_extension->remove_lock);
  return IoCallDriver(device_extension->next_stack_device, irp);
}

static NTSTATUS DDKAPI 
on_start_complete(DEVICE_OBJECT *device_object, IRP *irp, void *context)
{
  libusb_device_extension *device_extension
    = (libusb_device_extension *)device_object->DeviceExtension;

  if(irp->PendingReturned)
    {
      IoMarkIrpPending(irp);
    }
  
  if(device_extension->next_stack_device->Characteristics 
     & FILE_REMOVABLE_MEDIA) 
    {
      device_object->Characteristics |= FILE_REMOVABLE_MEDIA;
    }
  
  if(NT_SUCCESS(irp->IoStatus.Status))
    {
      if(is_device_visible(device_extension))
        {
          if(NT_SUCCESS(control_object_create(device_extension)))
            {
              device_extension->is_started = 1;
            }   
        }
    }

  remove_lock_release(&device_extension->remove_lock);
  
  return STATUS_SUCCESS;
}

static NTSTATUS DDKAPI 
on_device_usage_notification_complete(DEVICE_OBJECT *device_object,
                                      IRP *irp, void *context)
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

  remove_lock_release(&device_extension->remove_lock);

  return STATUS_SUCCESS;
}

static NTSTATUS DDKAPI 
on_query_capabilities_complete(DEVICE_OBJECT *device_object,
                               IRP *irp, void *context)
{
  libusb_device_extension *device_extension
    = (libusb_device_extension *)device_object->DeviceExtension;

  if(irp->PendingReturned)
    {
      IoMarkIrpPending(irp);
    }

  if(!device_extension->self->AttachedDevice)
    {
      IoGetCurrentIrpStackLocation(irp)
        ->Parameters.DeviceCapabilities.Capabilities
        ->SurpriseRemovalOK = TRUE;
    }

  remove_lock_release(&device_extension->remove_lock);

  return STATUS_SUCCESS;
}
