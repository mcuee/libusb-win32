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


typedef struct {
  URB *urb;
  IRP *main_irp;
  IRP *sub_irp;
  KEVENT event;
  libusb_remove_lock *remove_lock; 
} Context;


NTSTATUS __stdcall transfer_complete(DEVICE_OBJECT *device_object, 
                                     IRP *irp, void *context);
void __stdcall transfer_cancel(DEVICE_OBJECT *device_object, IRP *irp);

static URB *create_urb(libusb_device_extension *device_extension,
                       int direction, int urb_function, int endpoint, 
                       int packet_size, MDL *buffer, int size);

NTSTATUS transfer(IRP *irp, libusb_device_extension *device_extension,
                  int direction, int urb_function, int endpoint, 
                  int packet_size, MDL *buffer, int size)
{
  NTSTATUS status = STATUS_SUCCESS;
  IO_STACK_LOCATION *stack_location = NULL;
  Context *context = NULL;
  CHAR stack_size;
  KIRQL irql;

  debug_print_nl();

  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    debug_printf(LIBUSB_DEBUG_MSG, "transfer(): isochronous transfer");
  else
    debug_printf(LIBUSB_DEBUG_MSG, "transfer(): bulk or interrupt transfer");
 
  if(direction == USBD_TRANSFER_DIRECTION_IN)
    debug_printf(LIBUSB_DEBUG_MSG, "transfer(): direction in");
  else
    debug_printf(LIBUSB_DEBUG_MSG, "transfer(): direction out");

  debug_printf(LIBUSB_DEBUG_MSG, "transfer(): endpoint 0x%02x", endpoint);

  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    debug_printf(LIBUSB_DEBUG_MSG, "transfer(): packet_size 0x%x", 
                 packet_size);

  debug_printf(LIBUSB_DEBUG_MSG, "transfer(): size %d", size);


  if(!device_extension->current_configuration)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "transfer(): invalid configuration 0");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_INVALID_DEVICE_STATE, 0);
    }
  
  
  context = (Context *)ExAllocatePool(NonPagedPool, sizeof(Context));
  
  if(!context)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "transfer(): memory allocation error");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_NO_MEMORY, 0);
    }
  
  memset(context, 0, sizeof(Context));
    
  context->urb = create_urb(device_extension, direction, urb_function, 
                            endpoint, packet_size, buffer, size);
    
  if(!context->urb)
    {
      ExFreePool(context);
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_NO_MEMORY, 0);
    }
    
  context->main_irp = irp;
  context->remove_lock = &device_extension->remove_lock;
    
  KeInitializeEvent(&context->event, NotificationEvent, FALSE);
 
  stack_size = device_extension->next_stack_device->StackSize;
  context->sub_irp = IoAllocateIrp(stack_size, FALSE);
    
  if(!context->sub_irp)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "transfer(): memory allocation error");
	
      ExFreePool(context->urb);
      ExFreePool(context);
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_NO_MEMORY, 0);
    }
    
  irp->Tail.Overlay.DriverContext[0] = context;

  stack_location = IoGetNextIrpStackLocation(context->sub_irp);
    
  stack_location->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
  stack_location->Parameters.Others.Argument1 = context->urb;
  stack_location->Parameters.DeviceIoControl.IoControlCode 
    = IOCTL_INTERNAL_USB_SUBMIT_URB;
    
  IoSetCompletionRoutine(context->sub_irp, transfer_complete, context,
                         TRUE, TRUE, TRUE);
    
  IoAcquireCancelSpinLock(&irql);
    
  if(irp->Cancel) 
    {
      status = STATUS_CANCELLED;
    } 
  else 
    {
      IoSetCancelRoutine(irp, transfer_cancel);
    }
    
  IoReleaseCancelSpinLock(irql);
    
  if(status == STATUS_CANCELLED)  
    { 
      IoFreeIrp(context->sub_irp);
      ExFreePool(context->urb);
      ExFreePool(context); 
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_CANCELLED, 0);
    } 
    
  IoMarkIrpPending(irp);
  IoCallDriver(device_extension->next_stack_device, context->sub_irp);
  return STATUS_PENDING;  
}


NTSTATUS __stdcall transfer_complete(DEVICE_OBJECT *device_object, IRP *irp, 
                                     void *context)
{
  Context *c = (Context *)context;
  libusb_remove_lock *lock = c->remove_lock;
  int transmitted = 0;
  KIRQL irql;
  PDRIVER_CANCEL cancel;

  IoAcquireCancelSpinLock(&irql);
  cancel = IoSetCancelRoutine(c->main_irp, NULL);
  IoReleaseCancelSpinLock(irql);

  /* cancel routine of main IRP has not started yet */
  if(cancel)
    {
      if(NT_SUCCESS(irp->IoStatus.Status) 
         && USBD_SUCCESS(c->urb->UrbHeader.Status))
        {
          if(c->urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
            {
              transmitted 
                = c->urb->UrbIsochronousTransfer.TransferBufferLength;
            }
          if(c->urb->UrbHeader.Function 
             == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
            {
              transmitted 
                = c->urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
            }
	  
          debug_printf(LIBUSB_DEBUG_MSG, "transfer_complete(): %d bytes "
                       "transmitted", transmitted);
        }
      else
        {
          debug_printf(LIBUSB_DEBUG_ERR, "transfer_complete(): transfer "
                       "failed: status: 0x%x, urb-status: 0x%x", 
                       irp->IoStatus.Status, c->urb->UrbHeader.Status);
        }

      /* clean up */
      complete_irp(c->main_irp, irp->IoStatus.Status, transmitted);
      IoFreeIrp(irp);
      ExFreePool(c->urb);
      ExFreePool(c);
      remove_lock_release(lock);
    }
  /* cancel routine has already been called and is waiting for the event */
  else
    {
      KeSetEvent(&c->event, 0, FALSE);
    }

  return STATUS_MORE_PROCESSING_REQUIRED;
}


void __stdcall transfer_cancel(DEVICE_OBJECT *device_object, IRP *irp)
{
  Context *c = (Context *)irp->Tail.Overlay.DriverContext[0];
  libusb_remove_lock *lock = c->remove_lock;
 
  IoReleaseCancelSpinLock(irp->CancelIrql);

  debug_printf(LIBUSB_DEBUG_ERR, "transfer_cancel(): timeout error");

  /* cancel sub IRP and wait for completion */
  IoCancelIrp(c->sub_irp);
  KeWaitForSingleObject(&c->event, Executive, KernelMode, FALSE, NULL);

  /* clean up */
  IoFreeIrp(c->sub_irp);
  ExFreePool(c->urb);
  ExFreePool(c);
  complete_irp(irp, STATUS_CANCELLED, 0);
  remove_lock_release(lock);
}

static URB *create_urb(libusb_device_extension *device_extension,
                       int direction, int urb_function, int endpoint, 
                       int packet_size, MDL *buffer, int size)
{
  USBD_PIPE_HANDLE pipe_handle = NULL;
  int num_packets = 0;
  int i, urb_size;
  URB *urb = NULL;
  
  if(!get_pipe_handle(device_extension, endpoint, &pipe_handle))
    {
      debug_printf(LIBUSB_DEBUG_ERR, "create_urb(): getting endpoint pipe "
                   "failed");
      return NULL;
    }
  
  /* isochronous transfer */
  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    {
      num_packets = (size + packet_size - 1) / packet_size;
      
      if(num_packets > 255)
        {
          debug_printf(LIBUSB_DEBUG_ERR, "create_urb(): transfer size "
                       "too large");
          return NULL;
        }
      
      urb_size = sizeof(struct _URB_ISOCH_TRANSFER)
        + sizeof(USBD_ISO_PACKET_DESCRIPTOR) * num_packets;
    }
  else /* bulk or interrupt transfer */
    {
      urb_size = sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER);
    }
  
  urb = (URB *)ExAllocatePool(NonPagedPool, urb_size);
  
  if(!urb)
    {
      debug_printf(LIBUSB_DEBUG_ERR, "create_urb(): memory allocation "
                   "error");
      return NULL;
    }
  
  memset(urb, 0, urb_size);
  
  urb->UrbHeader.Length = (USHORT)urb_size;
  urb->UrbHeader.Function = (USHORT)urb_function;
  
  /* isochronous transfer */
  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    {
      urb->UrbIsochronousTransfer.PipeHandle = pipe_handle;
      urb->UrbIsochronousTransfer.TransferFlags 
        = direction | USBD_SHORT_TRANSFER_OK | USBD_START_ISO_TRANSFER_ASAP;
      urb->UrbIsochronousTransfer.TransferBufferLength = size;
      urb->UrbIsochronousTransfer.TransferBufferMDL = buffer;
      urb->UrbIsochronousTransfer.NumberOfPackets = num_packets;
      
      for(i = 0; i < num_packets; i++)
        {
          urb->UrbIsochronousTransfer.IsoPacket[i].Offset = i * packet_size;
          urb->UrbIsochronousTransfer.IsoPacket[i].Length = packet_size;
        }
    }
  /* bulk or interrupt transfer */
  else
    {
      urb->UrbBulkOrInterruptTransfer.PipeHandle = pipe_handle;
      urb->UrbBulkOrInterruptTransfer.TransferFlags 
        = direction | USBD_SHORT_TRANSFER_OK;
      urb->UrbIsochronousTransfer.TransferBufferLength = size;
      urb->UrbIsochronousTransfer.TransferBufferMDL = buffer;
    }

  return urb;
}

