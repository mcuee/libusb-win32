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

typedef struct {
  URB urb;
  IRP *main_irp;
  IRP *sub_irp;
  KIRQL irql;
  libusb_remove_lock *remove_lock;
} Context;


NTSTATUS on_bulk_int_complete(DEVICE_OBJECT *device_object, 
			  IRP *irp,  void *context);
void on_bulk_int_cancel(DEVICE_OBJECT *device_object, IRP *irp);


NTSTATUS bulk_int_transfer(IRP *irp, libusb_device_extension *device_extension,
			   int endpoint, MDL *buffer, 
			   int size, int direction)
{
  NTSTATUS m_status = STATUS_SUCCESS;
  USBD_PIPE_HANDLE pipe_handle = NULL;
  IO_STACK_LOCATION *next_irp_stack = NULL;
  Context *context;
  //  KIRQL irql;

  debug_print_nl();
  debug_printf(LIBUSB_DEBUG_MSG, "bulk_int_transfer(): endpoint %02xh", 
	       endpoint);
  debug_printf(LIBUSB_DEBUG_MSG, "bulk_int_transfer(): size %d", size);

  if(direction == USBD_TRANSFER_DIRECTION_IN)
    debug_printf(LIBUSB_DEBUG_MSG, "bulk_int_transfer(): direction in");
  else
    debug_printf(LIBUSB_DEBUG_MSG, "bulk_int_transfer(): direction out");

  if(!device_extension->current_configuration)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "bulk_int_transfer(): invalid "
		   "configuration 0");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_INVALID_DEVICE_STATE, 0);
    }
  
  if(!get_pipe_handle(device_extension, endpoint, &pipe_handle))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "bulk_int_transfer(): getting endpoint "
		   "pipe failed");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_INVALID_PARAMETER, 0);
    }
      
  context = (Context *)ExAllocatePool(NonPagedPool, sizeof(Context));
  
  if(!context)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "bulk_int_transfer(): memory allocation "
		   "error");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_NO_MEMORY, 0);
    }

  RtlZeroMemory(context, sizeof(Context));

  UsbBuildInterruptOrBulkTransferRequest
    (&(context->urb), sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
     pipe_handle, NULL, buffer, size, 
     direction | USBD_SHORT_TRANSFER_OK, NULL);
  
  context->main_irp = irp;
  context->remove_lock = &device_extension->remove_lock;
  context->sub_irp = 
    IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB, 
				  device_extension->next_stack_device,
				  NULL, 0, NULL, 0, TRUE, 
				  NULL, NULL);

  next_irp_stack = IoGetNextIrpStackLocation(context->sub_irp);
  next_irp_stack->Parameters.Others.Argument1 = &(((Context *)context)->urb);
  next_irp_stack->Parameters.Others.Argument2 = NULL;

  irp->Tail.Overlay.DriverContext[0] = context;

  IoAcquireCancelSpinLock(&context->irql);
  IoSetCancelRoutine(context->main_irp, on_bulk_int_cancel);
  IoReleaseCancelSpinLock(context->irql);

  IoSetCompletionRoutine(context->sub_irp, on_bulk_int_complete, 
			 context, TRUE, TRUE, TRUE);   

  IoMarkIrpPending(context->main_irp);
  IoCallDriver(device_extension->next_stack_device, context->sub_irp);
  
  return STATUS_PENDING;  
}


NTSTATUS on_bulk_int_complete(DEVICE_OBJECT *device_object, 
			      IRP *irp, void *context)
{
  URB *urb = &(((Context *)context)->urb);
  libusb_remove_lock *lock = ((Context *)context)->remove_lock;
  int transmitted = 0;

  if(NT_SUCCESS(irp->IoStatus.Status))
    {
      transmitted = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
      debug_printf(LIBUSB_DEBUG_MSG, "on_bulk_int_complete(): %d bytes "
		   "transmitted", transmitted);
    }
  else
    {
      debug_printf(LIBUSB_DEBUG_ERR, "on_bulk_int_complete(): transfer "
		   "failed");
    }

  ((Context *)context)->main_irp->Tail.Overlay.DriverContext[0] = NULL;

  complete_irp(((Context *)context)->main_irp, irp->IoStatus.Status,
	       transmitted);

  ExFreePool(context);
  remove_lock_release(lock);

  return STATUS_SUCCESS;
}


void on_bulk_int_cancel(DEVICE_OBJECT *device_object, IRP *irp)
{
  Context *context = (Context *)irp->Tail.Overlay.DriverContext[0];
  debug_printf(LIBUSB_DEBUG_WARN, "on_bulk_int_cancel(): IRP cancelled");

  if(context)
    {
      IoReleaseCancelSpinLock(context->irql);
      IoCancelIrp(context->sub_irp);
    }
}
