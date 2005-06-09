/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
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
  int seq_num;
  libusb_remove_lock_t *remove_lock;
} context_t;

static int count = 0;

NTSTATUS DDKAPI transfer_complete(DEVICE_OBJECT *device_object, 
                                     IRP *irp, void *context);

static URB *create_urb(libusb_device_extension *device_extension,
                       int direction, int urb_function, int endpoint, 
                       int packet_size, MDL *buffer, int size);

NTSTATUS transfer(IRP *irp, libusb_device_extension *device_extension,
                  int direction, int urb_function, int endpoint, 
                  int packet_size, MDL *buffer, int size)
{
  IO_STACK_LOCATION *stack_location = NULL;
  context_t *context;

 
  DEBUG_PRINT_NL();

  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    DEBUG_MESSAGE("transfer(): isochronous transfer");
  else
    DEBUG_MESSAGE("transfer(): %d: bulk or interrupt transfer");

  if(direction == USBD_TRANSFER_DIRECTION_IN)
    DEBUG_MESSAGE("transfer(): direction in");
  else
    DEBUG_MESSAGE("transfer(): direction out");

  DEBUG_MESSAGE("transfer(): endpoint 0x%02x", endpoint);

  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    DEBUG_MESSAGE("transfer(): packet_size 0x%x", packet_size);

  DEBUG_MESSAGE("transfer(): size %d", size);
  DEBUG_MESSAGE("transfer(): sequence %d", count);

  if(!device_extension->configuration)
    {
      DEBUG_ERROR("transfer(): invalid configuration 0");
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_INVALID_DEVICE_STATE, 0);
    }
  
  context = (context_t *)ExAllocatePool(NonPagedPool, sizeof(context_t));

  if(!context)
    {
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_NO_MEMORY, 0);
    }

  context->urb = create_urb(device_extension, direction, urb_function, 
                            endpoint, packet_size, buffer, size);
    
  if(!context->urb)
    {
      remove_lock_release(&device_extension->remove_lock);
      return complete_irp(irp, STATUS_NO_MEMORY, 0);
    }

  context->remove_lock = &device_extension->remove_lock;
  context->seq_num = count++;

  stack_location = IoGetNextIrpStackLocation(irp);
    
  stack_location->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
  stack_location->Parameters.Others.Argument1 = context->urb;
  stack_location->Parameters.DeviceIoControl.IoControlCode 
    = IOCTL_INTERNAL_USB_SUBMIT_URB;
    
  IoSetCompletionRoutine(irp, transfer_complete, context,
                         TRUE, TRUE, TRUE);
    
  return IoCallDriver(device_extension->physical_device_object, irp);
}


NTSTATUS DDKAPI transfer_complete(DEVICE_OBJECT *device_object, IRP *irp, 
                                  void *context)
{
  context_t *c = (context_t *)context;
  int transmitted = 0;
  libusb_remove_lock_t *remove_lock = c->remove_lock;


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
      
      DEBUG_MESSAGE("transfer_complete(): sequence %d: %d bytes transmitted", 
                    c->seq_num, transmitted);
    }
  else
    {
      if(irp->IoStatus.Status == STATUS_CANCELLED)
        {
          DEBUG_ERROR("transfer_complete(): sequence %d: timeout error",
                      c->seq_num);
        }
      else
        {
          DEBUG_ERROR("transfer_complete(): sequence %d: transfer failed: "
                      "status: 0x%x, urb-status: 0x%x", 
                      c->seq_num,irp->IoStatus.Status, 
                      c->urb->UrbHeader.Status);
        }
    }

  ExFreePool(c->urb);
  ExFreePool(c);

  complete_irp(irp, irp->IoStatus.Status, transmitted);
  remove_lock_release(remove_lock);

  return STATUS_MORE_PROCESSING_REQUIRED;
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
      DEBUG_ERROR("create_urb(): getting endpoint pipe failed");
      return NULL;
    }
  
  /* isochronous transfer */
  if(urb_function == URB_FUNCTION_ISOCH_TRANSFER)
    {
      num_packets = (size + packet_size - 1) / packet_size;
      
      if(num_packets > 255)
        {
          DEBUG_ERROR("create_urb(): transfer size too large");
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
      DEBUG_ERROR("create_urb(): memory allocation error");
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
      urb->UrbBulkOrInterruptTransfer.TransferBufferLength = size;
      urb->UrbBulkOrInterruptTransfer.TransferBufferMDL = buffer;
    }

  return urb;
}

