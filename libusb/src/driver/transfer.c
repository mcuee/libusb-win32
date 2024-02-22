/* libusb-win32, Generic Windows USB Library
 * Copyright (C) 2017-2024 Peter Dons Tychsen <pdt@dontech.dk>
 * Copyright (c) 2010 Travis Robinson <libusbdotnet@gmail.com>
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

typedef struct
{
	URB *urb;
	int address;
	LONG sequence;
	int transferFlags;
	int isoLatency;
	int totalLength;
	int information;
	int maximum_packet_size;
	int maxTransferSize;
	IN PMDL mdlAddress;
	PMDL subMdl;
} context_t;

static LONG sequence = 0;

static const char* read_pipe_display_names[]  = {"ctrl-read", "iso-read", "bulk-read", "int-read"};
static const char* write_pipe_display_names[] = {"ctrl-write","iso-write","bulk-write","int-write"};

static const char* GetPipeDisplayName(libusb_endpoint_t* endpoint);

NTSTATUS DDKAPI transfer_complete(DEVICE_OBJECT* device_object,
								  IRP *irp,
								  void *context);

static NTSTATUS create_urb(libusb_device_t *dev,
						   URB **urb,
						   int direction,
						   int urbFunction,
						   libusb_endpoint_t* endpoint,
						   int packetSize,
						   MDL *buffer,
						   int size);

void set_urb_transfer_flags(libusb_device_t* dev,
							PIRP irp,
							PURB subUrb,
							int transfer_flags,
							int isoLatency);
static NTSTATUS transfer_next(libusb_device_t* dev,
	IN PIRP irp,
	context_t* context)
{
	NTSTATUS status;
	IO_STACK_LOCATION* stack_location = NULL;
	stack_location = IoGetNextIrpStackLocation(irp);

	stack_location->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	stack_location->Parameters.Others.Argument1 = context->urb;
	stack_location->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;

	IoSetCompletionRoutine(irp, transfer_complete, context, TRUE, TRUE, TRUE);

	// Set the transfer flags just before we call the irp down.
	// If this is an iso transfer, set_urb_transfer_flags() might need
	// to get the start frame and set latency.
	//
	set_urb_transfer_flags(dev, irp, context->urb, context->transferFlags, context->isoLatency);

	status = IoCallDriver(dev->target_device, irp);
	if (!NT_SUCCESS(status))
	{
		USBERR("xfer failed. sequence %d\n", context->sequence);
	}
	return status;
}

NTSTATUS transfer(libusb_device_t* dev,
				  IN PIRP irp,
				  IN int direction,
				  IN int urbFunction,
				  IN libusb_endpoint_t* endpoint,
				  IN int packetSize,
				  IN int transferFlags,
				  IN int isoLatency,
				  IN PMDL mdlAddress,
				  IN int totalLength,
				  IN int maxTransferSize)
{
	context_t *context = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	int sequenceID  = InterlockedIncrement(&sequence);
	const char* dispTransfer = GetPipeDisplayName(endpoint);
	int first_size;
	LONG pending_busy_taken = FALSE, pending_busy = FALSE;

	// TODO: reset pipe flag 
	// status = reset_endpoint(dev,endpoint->address, LIBUSB_DEFAULT_TIMEOUT);
	//
	if (!packetSize)
		packetSize = endpoint->maximum_packet_size;

	if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
	{
		USBMSG("[%s #%d] EP%02Xh packet-size=%d length=%d reset-status=%08Xh\n",
			dispTransfer, sequenceID, endpoint->address, packetSize, totalLength, status);
	}
	else
	{
		USBMSG("[%s #%d] EP%02Xh length=%d, packetSize=%d, maxTransferSize=%d\n",
			dispTransfer, sequenceID, endpoint->address, totalLength, packetSize, maxTransferSize);
	}

	/* Check if another transfer is on-going on same endpoint */
	pending_busy = InterlockedCompareExchange(&dev->pending_busy[endpoint->address], 1, 0);
	if(pending_busy)
	{
		USBMSG("sequence %d send aborted due to pending conflict\n", sequenceID);
		goto transfer_free;
	}
	pending_busy_taken = TRUE;

	/* Save the pending sequence, so that transfer_complete() for older requests do not
		 order new transfers after this one */
	InterlockedExchange(&dev->pending_sequence[endpoint->address], sequenceID);

	context = allocate_pool(sizeof(context_t));
	if (!context)
	{
		status = STATUS_NO_MEMORY;
		goto transfer_free;
	}

	context->isoLatency = isoLatency;
	context->transferFlags = transferFlags;
	context->totalLength = totalLength;
	context->maximum_packet_size = endpoint->maximum_packet_size;
	context->sequence = sequenceID;
	context->mdlAddress = mdlAddress;
	context->subMdl = NULL;
	context->information = 0;
	context->maxTransferSize = maxTransferSize;
	context->address = endpoint->address;

	first_size = (totalLength > context->maxTransferSize) ? context->maxTransferSize : totalLength;

	status = create_urb(dev, &context->urb, direction, urbFunction,
		endpoint, packetSize, mdlAddress, first_size);
	if (!NT_SUCCESS(status))
	{
		goto transfer_free;
	}

	/* Do not check this status code, as the request might complete during call,
     so we do *not* want to free anything here as that would lead to double-free */
  status = transfer_next(dev, irp, context);

	InterlockedExchange(&dev->pending_busy[endpoint->address], 0);
	return status;

transfer_free:
	if(pending_busy_taken)
	{
		InterlockedExchange(&dev->pending_busy[endpoint->address], 0);
	}
	if(context)
	{
		if(context->urb)
		{
			ExFreePool(context->urb);
		}
		ExFreePool(context);
	}
	remove_lock_release(dev);
	return complete_irp(irp, status, 0);
}

NTSTATUS DDKAPI transfer_complete(DEVICE_OBJECT *device_object, IRP *irp,
								  void *context)
{
	NTSTATUS status = STATUS_SUCCESS;
	context_t *c = (context_t *)context;
	int transmitted = 0;
	libusb_device_t *dev = device_object->DeviceExtension;
	LONG pending_busy_taken = FALSE, pending_busy = FALSE;

	if (irp->PendingReturned)
	{
		IoMarkIrpPending(irp);
	}

	if (NT_SUCCESS(irp->IoStatus.Status)
		&& USBD_SUCCESS(c->urb->UrbHeader.Status))
	{
		if (c->urb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
		{
			transmitted = c->urb->UrbIsochronousTransfer.TransferBufferLength;
		}
		if (c->urb->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
		{
			transmitted
				= c->urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
		}

		USBMSG("sequence %d: %d bytes transmitted, maximum_packet_size=%d, totalLength=%d\n",
			c->sequence, transmitted, c->maximum_packet_size, c->totalLength);
	}
	else
	{
		if (irp->IoStatus.Status == STATUS_CANCELLED)
		{
			USBERR("sequence %d: timeout error\n",
				c->sequence);
		}
		else
		{
			USBERR("sequence %d: transfer failed: status: 0x%x, urb-status: 0x%x\n",
				c->sequence, irp->IoStatus.Status,
				c->urb->UrbHeader.Status);
		}
	}

	/* Calculate size remaining */
	c->totalLength = (transmitted < c->totalLength) ? (c->totalLength - transmitted) : 0;

	/* Update transferred size */
	c->information += transmitted;

	/* If this is a success, and there is more data to be transferred, then lets go again
   *
	 * Note 1: We do not do this if a newer request is already pending on the endpoint,
	 * as this could change the order of the arrival of data
	 */
	if(NT_SUCCESS(irp->IoStatus.Status)
		&& USBD_SUCCESS(c->urb->UrbHeader.Status)
		&& (c->urb->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
		&& !(transmitted % c->maximum_packet_size)
		&& c->totalLength)
	{
		PUCHAR virtualAddress;
		int next_size = (c->totalLength > c->maxTransferSize) ? c->maxTransferSize : c->totalLength;

		/* Check if another transfer is on-going on same endpoint */
		pending_busy = InterlockedCompareExchange(&dev->pending_busy[c->address], 1, 0);
		if(pending_busy)
		{
			USBMSG("sequence %d resend aborted due to pending conflict\n", c->sequence);
			goto transfer_free;
		}
		pending_busy_taken = TRUE;

		/* Check if a newer sequence is pending */
		if(InterlockedAdd(&dev->pending_sequence[c->address], 0) != c->sequence)
		{
			USBMSG("sequence %d resend aborted due to newer pending\n", c->sequence);
			goto transfer_free;
		}

		virtualAddress = (PUCHAR)MmGetMdlVirtualAddress(c->mdlAddress);
		if(!virtualAddress)
		{
			USBERR("[#%d] MmGetMdlVirtualAddress failed\n", c->sequence);
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto transfer_free;
		}

		/* Skip used address space */
		virtualAddress += c->information;

		c->subMdl = IoAllocateMdl((PVOID)(virtualAddress),
			next_size, FALSE, FALSE, NULL);
		if(c->subMdl == NULL)
		{
			USBERR("[#%d] failed allocating subMdl\n", c->sequence);
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto transfer_free;
		}

		IoBuildPartialMdl(irp->MdlAddress, c->subMdl, (PVOID)virtualAddress, next_size);

		/* Re-use URB for another reception */
		c->urb->UrbBulkOrInterruptTransfer.TransferBufferLength = next_size;
		c->urb->UrbBulkOrInterruptTransfer.TransferBufferMDL = c->subMdl;

		status = transfer_next(dev, irp, c);
		if(!NT_SUCCESS(status))
		{
			goto transfer_free;
		}

		InterlockedExchange(&dev->pending_busy[c->address], 0);
		return STATUS_MORE_PROCESSING_REQUIRED;
	}

transfer_free:

	if(pending_busy_taken)
	{
		InterlockedExchange(&dev->pending_busy[c->address], 0);
	}
	irp->IoStatus.Information = c->information;
	if(c->subMdl)
	{
		IoFreeMdl(c->subMdl);
	}
	ExFreePool(c->urb);
	ExFreePool(c);

	remove_lock_release(dev);

	return status;
}

static NTSTATUS create_urb(libusb_device_t *dev, URB **urb, int direction,
						   int urbFunction, libusb_endpoint_t* endpoint, int packetSize,
						   MDL *buffer, int size)
{
	USBD_PIPE_HANDLE pipe_handle = NULL;
	int num_packets = 0;
	int i, urb_size;

	UNREFERENCED_PARAMETER(dev);

	*urb = NULL;

	pipe_handle = endpoint->handle;

	/* isochronous transfer */
	if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
	{
		if (packetSize <= 0)
		{
			USBERR("invalid packet size=%d\n", packetSize);
			return STATUS_INVALID_PARAMETER;
		}

		num_packets = size / packetSize;

		if (num_packets <= 0)
		{
			USBERR("invalid number of packets=%d\n",
				num_packets);
			return STATUS_INVALID_PARAMETER;
		}

		if (num_packets > 255)
		{
			USBERR0("transfer size too large\n");
			return STATUS_INVALID_PARAMETER;
		}

		urb_size = sizeof(struct _URB_ISOCH_TRANSFER)
			+ sizeof(USBD_ISO_PACKET_DESCRIPTOR) * num_packets;
	}
	else /* bulk or interrupt transfer */
	{
		urb_size = sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER);
	}

	*urb = allocate_pool(urb_size);

	if (!*urb)
	{
		USBERR0("memory allocation error\n");
		return STATUS_NO_MEMORY;
	}

	memset(*urb, 0, urb_size);

	(*urb)->UrbHeader.Length = (USHORT)urb_size;
	(*urb)->UrbHeader.Function = (USHORT)urbFunction;

	/* isochronous transfer */
	if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
	{
		(*urb)->UrbIsochronousTransfer.PipeHandle = pipe_handle;
		(*urb)->UrbIsochronousTransfer.TransferFlags = direction;
		(*urb)->UrbIsochronousTransfer.TransferBufferLength = size;
		(*urb)->UrbIsochronousTransfer.TransferBufferMDL = buffer;
		(*urb)->UrbIsochronousTransfer.NumberOfPackets = num_packets;

		for (i = 0; i < num_packets; i++)
		{
			(*urb)->UrbIsochronousTransfer.IsoPacket[i].Offset = i * packetSize;
			(*urb)->UrbIsochronousTransfer.IsoPacket[i].Length = packetSize;
		}
	}
	/* bulk or interrupt transfer */
	else
	{
		(*urb)->UrbBulkOrInterruptTransfer.PipeHandle = pipe_handle;
		(*urb)->UrbBulkOrInterruptTransfer.TransferFlags = direction;
		(*urb)->UrbBulkOrInterruptTransfer.TransferBufferLength = size;
		(*urb)->UrbBulkOrInterruptTransfer.TransferBufferMDL = buffer;
	}

	return STATUS_SUCCESS;
}

static const char* GetPipeDisplayName(libusb_endpoint_t* endpoint)
{
	if (endpoint->address & 0x80)
		return read_pipe_display_names[(endpoint->pipe_type & 3)];
	else
		return write_pipe_display_names[(endpoint->pipe_type & 3)];
}

void set_urb_transfer_flags(libusb_device_t* dev, PIRP irp, PURB subUrb,int transfer_flags, int isoLatency)
{
	if (subUrb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
	{
		// only keep the direction bit
		subUrb->UrbIsochronousTransfer.TransferFlags &= 1;

		// If true, allow short tranfers.
		if (!(transfer_flags & TRANSFER_FLAGS_SHORT_NOT_OK))
			subUrb->UrbIsochronousTransfer.TransferFlags |= USBD_SHORT_TRANSFER_OK;

		if (!(transfer_flags & TRANSFER_FLAGS_ISO_SET_START_FRAME))
			subUrb->UrbIsochronousTransfer.TransferFlags |= USBD_START_ISO_TRANSFER_ASAP;
		else
		{
			subUrb->UrbIsochronousTransfer.StartFrame = get_current_frame(dev, irp);
			if (transfer_flags & TRANSFER_FLAGS_ISO_ADD_LATENCY)
				subUrb->UrbIsochronousTransfer.StartFrame += isoLatency;
		}
	}

	else if (subUrb->UrbHeader.Function == URB_FUNCTION_CONTROL_TRANSFER)
	{
		// TODO: large control transfers

		// only keep the direction bit
		subUrb->UrbControlTransfer.TransferFlags &= 1;
		if (!(transfer_flags & TRANSFER_FLAGS_SHORT_NOT_OK))
			subUrb->UrbControlTransfer.TransferFlags |= USBD_SHORT_TRANSFER_OK;

	}
	else
	{
		// Default is bulk or interrupt.

		// only keep the direction bit
		subUrb->UrbBulkOrInterruptTransfer.TransferFlags &= 1;

		// If true, allow short tranfers.
		if (!(transfer_flags & TRANSFER_FLAGS_SHORT_NOT_OK))
			subUrb->UrbBulkOrInterruptTransfer.TransferFlags |= USBD_SHORT_TRANSFER_OK;
	}
}

NTSTATUS control_transfer(libusb_device_t* dev, 
						 PIRP irp,
						 PMDL mdl,
						 int size,
						 int usbd_direction,
						 int *ret,
						 int timeout,
						 UCHAR request_type,
						 UCHAR request,
						 USHORT value,
						 USHORT index,
						 USHORT length)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb;

    UNREFERENCED_PARAMETER(irp);

    *ret = 0;

	memset(&urb, 0, sizeof(struct _URB_CONTROL_TRANSFER));
	urb.UrbControlTransfer.SetupPacket[0]=request_type;
	urb.UrbControlTransfer.SetupPacket[1]=request;

	urb.UrbControlTransfer.SetupPacket[2]=LBYTE(value);
	urb.UrbControlTransfer.SetupPacket[3]=HBYTE(value);

	urb.UrbControlTransfer.SetupPacket[4]=LBYTE(index);
	urb.UrbControlTransfer.SetupPacket[5]=HBYTE(index);

	urb.UrbControlTransfer.SetupPacket[6]=LBYTE(length);
	urb.UrbControlTransfer.SetupPacket[7]=HBYTE(length);

    urb.UrbHeader.Length = sizeof(struct _URB_CONTROL_TRANSFER);
	urb.UrbHeader.Function=URB_FUNCTION_CONTROL_TRANSFER;
	urb.UrbControlTransfer.TransferFlags=usbd_direction | USBD_DEFAULT_PIPE_TRANSFER | USBD_SHORT_TRANSFER_OK;
    urb.UrbControlTransfer.TransferBufferLength = size;
    urb.UrbControlTransfer.TransferBufferMDL = mdl;
	urb.UrbControlTransfer.TransferBuffer = NULL;

    USBMSG("[%s] timeout=%d request_type=%02Xh request=%02Xh value=%04Xh index=%04Xh length=%04Xh\n", 
		(usbd_direction==USBD_TRANSFER_DIRECTION_IN) ? "read" : "write",
		timeout, request_type, request, value, index, length);

	// no maximum timeout check for control request.
    status = call_usbd_ex(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout, 0);

    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
    {
        USBERR("request failed: status: 0x%x, urb-status: 0x%x\n", status, urb.UrbHeader.Status);
    }
    else
    {
        *ret = urb.UrbControlTransfer.TransferBufferLength;
        USBMSG("%d bytes transmitted\n",
                      urb.UrbControlTransfer.TransferBufferLength);
    }

    return status;
}
