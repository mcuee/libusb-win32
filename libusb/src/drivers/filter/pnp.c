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
  
  status = remove_lock_acquire(&device_extension->remove_lock);
  
  if(!NT_SUCCESS(status))
    { 
      return complete_irp(irp, status, 0);
    }

  debug_print_nl();

  switch(IoGetCurrentIrpStackLocation(irp)->MinorFunction) 
    {     
    case IRP_MN_REMOVE_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_REMOVE_DEVICE");
      remove_lock_release_and_wait(&device_extension->remove_lock);
      IoSkipCurrentIrpStackLocation(irp);
      status = IoCallDriver(device_extension->next_stack_device, irp);
      control_object_delete(device_extension);
      IoDetachDevice(device_extension->next_stack_device);
      IoDeleteDevice(device_extension->self);
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_REMOVE_DEVICE test");
      return status;

    case IRP_MN_SURPRISE_REMOVAL:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_SURPRISE_REMOVAL");
      break;
    case IRP_MN_START_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_START_DEVICE");
      IoCopyCurrentIrpStackLocationToNext(irp);
      IoSetCompletionRoutine(irp, on_start_complete,
			     NULL, TRUE, TRUE, TRUE);
      remove_lock_release(&device_extension->remove_lock);
      return IoCallDriver(device_extension->next_stack_device, irp);

    case IRP_MN_CANCEL_REMOVE_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_CANCEL_REMOVE_DEVICE");
      break;
    case IRP_MN_CANCEL_STOP_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_CANCEL_STOP_DEVICE");
      break;
    case IRP_MN_QUERY_STOP_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_QUERY_STOP_DEVICE");
      break;
    case IRP_MN_STOP_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_STOP_DEVICE");
      break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
      debug_printf(DEBUG_MSG, "dispatch_pnp(): IRP_MN_QUERY_REMOVE_DEVICE");
      break;
    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
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

    default:
      status = irp->IoStatus.Status;
    }

  irp->IoStatus.Status = status;
  IoSkipCurrentIrpStackLocation(irp);
  remove_lock_release(&device_extension->remove_lock);
  return IoCallDriver(device_extension->next_stack_device, irp);
}

