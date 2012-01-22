/* libusb-win32, Generic Windows USB Library
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
	int sequence;
} context_t;

static LONG sequence = 0;

static const char* read_pipe_display_names[]  = {"ctrl-read", "iso-read", "bulk-read", "int-read"};
static const char* write_pipe_display_names[] = {"ctrl-write","iso-write","bulk-write","int-write"};


typedef struct _MAIN_REQUEST_CONTEXT
{
	// List of all sub requests (SUB_REQUEST_CONTEXT structures) that
	// are allocated to handle the original main request irp.
	//
	LIST_ENTRY  SubRequestList;
	LONG		sequenceID;
	const char* dispTransfer;
} MAIN_REQUEST_CONTEXT, *PMAIN_REQUEST_CONTEXT;

// The MAIN_REQUEST_CONTEXT structure is overlaid on top of the main
// request irp Tail.Overlay.DriverContext structure instead of being
// allocated separately.  Make sure it fits!
//
C_ASSERT(sizeof(MAIN_REQUEST_CONTEXT) <= sizeof(((PIRP)0)->Tail.Overlay.DriverContext));

typedef struct _SUB_REQUEST_CONTEXT
{
	// Main request irp that caused this sub request to be allocated.
	//
	PIRP        MainIrp;

	// List entry that links this sub request into the main request irp
	// context SubRequestList.  The sub request will be inserted into
	// the list as long as the sub request is outstanding.  The sub
	// request is only removed from the list by the sub request
	// completion routine.
	//
	LIST_ENTRY  ListEntry;

	// List entry that is used only by the main request irp cancel
	// routine to build a list of all currently outstanding sub
	// requests in order to cancel them.
	//
	LIST_ENTRY  CancelListEntry;

	// Reference count incremented (set to one) before calling the sub
	// request down the driver stack, decremented by the sub request
	// completion routine, and incremented/decremented by the main
	// request irp cancl routine.  This is used to prevent the sub
	// request from being freed by either the sub request completion
	// routine or the main request irp cancel routine while the sub
	// request is simultaneously being accessed by the other routine.
	// The sub request is freed by the routine that last accesses the
	// sub request and decrements the reference count to zero.
	//
	LONG        ReferenceCount;

	// irp, Urb, and Mdl allocated to send the sub request down the
	// driver stack.
	//
	PIRP        SubIrp;
	PURB        SubUrb;
	PMDL        SubMdl;

	ULONG		startOffset;

} SUB_REQUEST_CONTEXT, *PSUB_REQUEST_CONTEXT;

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

VOID large_transfer_cancel_routine(IN PDEVICE_OBJECT DeviceObject, IN PIRP irp);
VOID large_transfer_cancel(IN PIRP irp, BOOLEAN releaseCancelSpinlock);

NTSTATUS large_transfer_complete(IN PDEVICE_OBJECT DeviceObjectIsNULL,
								 IN PIRP irp,
								 IN PVOID Context);

static int get_iso_stagesize(int totalLength, int packetSize, int maxTransferSize);

static NTSTATUS allocate_suburb(USHORT urbFunction,
								int stageSize,
								int packetSize,
								ULONG* nPackets,
								PURB* subUrbRef);

void set_urb_transfer_flags(libusb_device_t* dev,
							PIRP irp,
							PURB subUrb,
							int transfer_flags,
							int isoLatency);

NTSTATUS transfer(libusb_device_t* dev,
				  IN PIRP irp,
				  IN int direction,
				  IN int urbFunction,
				  IN libusb_endpoint_t* endpoint,
				  IN int packetSize,
				  IN int transferFlags,
				  IN int isoLatency,
				  IN PMDL mdlAddress,
				  IN int totalLength)
{
	IO_STACK_LOCATION *stack_location = NULL;
	context_t *context;
	NTSTATUS status = STATUS_SUCCESS;
	int sequenceID  = InterlockedIncrement(&sequence);
	const char* dispTransfer = GetPipeDisplayName(endpoint);

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
		USBMSG("[%s #%d] EP%02Xh length %d\n",
			dispTransfer, sequenceID, endpoint->address, totalLength);
	}
	context = ExAllocatePool(NonPagedPool, sizeof(context_t));

	if (!context)
	{
		remove_lock_release(dev);
		return complete_irp(irp, STATUS_NO_MEMORY, 0);
	}

	status = create_urb(dev, &context->urb, direction, urbFunction,
		endpoint, packetSize, mdlAddress, totalLength);

	if (!NT_SUCCESS(status))
	{
		ExFreePool(context);
		remove_lock_release(dev);
		return complete_irp(irp, status, 0);
	}


	context->sequence = sequenceID;

	stack_location = IoGetNextIrpStackLocation(irp);

	stack_location->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	stack_location->Parameters.Others.Argument1 = context->urb;
	stack_location->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;

	IoSetCompletionRoutine(irp, transfer_complete, context, TRUE, TRUE, TRUE);

	// Set the transfer flags just before we call the irp down.
	// If this is an iso transfer, set_urb_transfer_flags() might need
	// to get the start frame and set latency.
	//
	set_urb_transfer_flags(dev, irp, context->urb, transferFlags, isoLatency);

	return IoCallDriver(dev->target_device, irp);
}


NTSTATUS DDKAPI transfer_complete(DEVICE_OBJECT *device_object, IRP *irp,
								  void *context)
{
	context_t *c = (context_t *)context;
	int transmitted = 0;
	libusb_device_t *dev = device_object->DeviceExtension;

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

		USBMSG("sequence %d: %d bytes transmitted\n",
			c->sequence, transmitted);
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

	ExFreePool(c->urb);
	ExFreePool(c);

	irp->IoStatus.Information = transmitted;

	remove_lock_release(dev);

	return STATUS_SUCCESS;
}


static NTSTATUS create_urb(libusb_device_t *dev, URB **urb, int direction,
						   int urbFunction, libusb_endpoint_t* endpoint, int packetSize,
						   MDL *buffer, int size)
{
	USBD_PIPE_HANDLE pipe_handle = NULL;
	int num_packets = 0;
	int i, urb_size;

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

	*urb = ExAllocatePool(NonPagedPool, urb_size);

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

/*-----------------------------------------------------------------------------
Routine Description:
This routine splits up a main transfer request into one or more sub 
requests as necessary.

[ISOCHRONOUS TRANSFERS]
Each isoch irp/urb pair can span at most 255 packets.
Each isoch irp/urb pair can request at the most 65536 bytes.

[BULK OR INTERRUPT TRANSFERS]
Each bulk/interrupt irp/urb pair can request at the most 65536 bytes.

1. It creates a SUB_REQUEST_CONTEXT for each irp/urb pair and
attaches it to the main request irp.

2. It intializes all of the sub request irp/urb pairs, and sub mdls
too.

3. It passes down the driver stack all of the sub request irps.

4. It leaves the completion of the main request irp as the
responsibility of the sub request irp completion routine, except
in the exception case where the main request irp is canceled
prior to passing any of the the sub request irps down the driver
stack.

Arguments:

dev - pointer to device object
irp - I/O request packet
direction - URB I/O direction (IN/OUT)
urbFunction - urb transfer function
endpoint - libusb_endpoint_t* 
packetSize - isochronous packet size
maxTransferSize,
transferFlags,
isoLatency - 
mdlAddress - transfer mdl buffer
totalLength - no. of bytes to be transferred

Return Value:

NT status value
*/
NTSTATUS large_transfer(IN libusb_device_t* dev,
						IN PIRP irp,
						IN int direction,
						IN int urbFunction,
						IN libusb_endpoint_t* endpoint,
						IN int packetSize,
						IN int maxTransferSize,
						IN int transferFlags,
						IN int isoLatency,
						IN PMDL mdlAddress,
						IN int totalLength)
{
	PIO_STACK_LOCATION      irpStack;
	BOOLEAN                 read;
	ULONG                   stageSize;
	ULONG                   numIrps;
	PMAIN_REQUEST_CONTEXT   mainRequestContext;
	PSUB_REQUEST_CONTEXT *  subRequestContextArray;
	PSUB_REQUEST_CONTEXT    subRequestContext;
	CCHAR                   stackSize;
	PUCHAR                  virtualAddress;
	ULONG                   i;
	ULONG                   j;
	NTSTATUS                ntStatus;
	PIO_STACK_LOCATION      nextStack;
	USBD_PIPE_HANDLE		pipeHandle;

	LONG					sequenceID;
	const char*				dispTransfer;
	int						startOffset;

	// TODO: reset pipe flag 
	// if (urbFunction != URB_FUNCTION_ISOCH_TRANSFER && pipe_flags & RESET)
	// status = reset_endpoint(dev,endpoint->address, LIBUSB_DEFAULT_TIMEOUT);
	//
	//reset the pipe (if irps are pending this will fail)
	//
	//if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
	//	reset_endpoint(dev,endpoint->address,LIBUSB_DEFAULT_TIMEOUT);

	//
	// initialize vars
	//
	irpStack = IoGetCurrentIrpStackLocation(irp);
	sequenceID = InterlockedIncrement(&sequence);
	subRequestContextArray = NULL;

	if (!maxTransferSize) 
		maxTransferSize = endpoint->maximum_transfer_size;

	if (!packetSize)
		packetSize = endpoint->maximum_packet_size;

	startOffset = 0;

	read = (direction == USBD_TRANSFER_DIRECTION_IN) ? TRUE : FALSE;
	dispTransfer = GetPipeDisplayName(endpoint);
	pipeHandle = endpoint->handle;

	// defaults
	stageSize = totalLength;
	numIrps = 1;

	// Full speed ISO note:
	// There is an inherent limit on the number of packets that can be
	// passed down the stack with each irp/urb pair (255)
	//
	// If the number of required packets is > 255, we shall create
	// "(required-packets / 255) [+ 1]" number of irp/urb pairs.
	//
	// Each irp/urb pair transfer is also called a stage transfer.
	//

	// TODO: detect and optimize for high speed devices.
	//
	if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
	{
		stageSize = get_iso_stagesize(totalLength, packetSize, maxTransferSize);
		numIrps = (totalLength + stageSize - 1) / stageSize;
		USBMSG("[%s #%d] EP%02Xh total-size=%d stage-size=%d IRPs=%d packet-size=%d\n",
			dispTransfer, sequenceID, endpoint->address, totalLength, stageSize, numIrps, packetSize);
	}
	else
	{
		if (totalLength > (maxTransferSize))
			stageSize = maxTransferSize;
		numIrps = (totalLength + stageSize - 1) / stageSize;
		USBMSG("[%s #%d] EP%02Xh total-size=%d stage-size=%d IRPs=%d\n",
			dispTransfer, sequenceID, endpoint->address, totalLength, stageSize, numIrps);
	}

	// Initialize the main request irp read/write context, which is
	// overlaid on top of irp->Tail.Overlay.DriverContext.
	//
	mainRequestContext = (PMAIN_REQUEST_CONTEXT)irp->Tail.Overlay.DriverContext;

	mainRequestContext->dispTransfer = dispTransfer;
	mainRequestContext->sequenceID = sequenceID;

	InitializeListHead(&mainRequestContext->SubRequestList);

	stackSize = dev->target_device->StackSize;

	virtualAddress = (PUCHAR) MmGetMdlVirtualAddress(mdlAddress);
	if (!virtualAddress)
	{
		USBERR("[%s #%d] MmGetMdlVirtualAddress failed\n",
			dispTransfer, sequenceID);
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto transfer_Free;
	}

	// Allocate an array to keep track of the sub requests that will be
	// allocated below.  This array exists only during the execution of
	// this routine and is used only to keep track of the sub requests
	// before calling them down the driver stack.
	//
	subRequestContextArray = (PSUB_REQUEST_CONTEXT *)
		ExAllocatePool(NonPagedPool,
		numIrps * sizeof(PSUB_REQUEST_CONTEXT));

	if (subRequestContextArray == NULL)
	{
		USBERR("[%s #%d] failed allocating sub request context array\n",
			dispTransfer, sequenceID);

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto transfer_Free;
	}

	RtlZeroMemory(subRequestContextArray, numIrps * sizeof(PSUB_REQUEST_CONTEXT));

	//
	// Allocate the sub requests
	//
	for (i = 0; i < numIrps; i++)
	{
		PIRP    subIrp;
		PURB    subUrb;
		PMDL    subMdl;
		// ULONG   urbSize;
		ULONG   offset;
		ULONG   nPackets=0;

		// The following outer scope variables are updated during each
		// iteration of the loop:  virtualAddress, totalLength, stageSize

		//
		// For every stage of transfer we need to do the following
		// tasks:
		//
		// 1. Allocate a sub request context (SUB_REQUEST_CONTEXT).
		// 2. Allocate a sub request irp.
		// 3. Allocate a sub request urb.
		// 4. Allocate a sub request mdl.
		// 5. Initialize the above allocations.
		//

		//
		// 1. Allocate a Sub Request Context (SUB_REQUEST_CONTEXT)
		//

		subRequestContext = (PSUB_REQUEST_CONTEXT)
			ExAllocatePool(NonPagedPool, sizeof(SUB_REQUEST_CONTEXT));

		if (subRequestContext == NULL)
		{
			USBERR("[%s #%d] failed allocating sub request context\n",
				dispTransfer, sequenceID);

			ntStatus = STATUS_INSUFFICIENT_RESOURCES;

			goto transfer_Free;
		}

		RtlZeroMemory(subRequestContext, sizeof(SUB_REQUEST_CONTEXT));

		// Attach it to the main request irp.
		//
		InsertTailList(&mainRequestContext->SubRequestList, &subRequestContext->ListEntry);

		// Remember it independently so we can refer to it later without
		// walking the sub request list.
		//
		subRequestContextArray[i] = subRequestContext;

		// Set the master irp that all the sub-requests originated from.
		subRequestContext->MainIrp = irp;

		// Remember the start offset 
		subRequestContext->startOffset = startOffset;

		// The reference count on the sub request prevents it from being
		// freed until the completion routine for the sub request
		// executes.
		//
		subRequestContext->ReferenceCount = 1;

		//
		// 2. Allocate a sub request irp
		//
		subIrp = IoAllocateIrp(stackSize, FALSE);

		if (subIrp == NULL)
		{
			USBERR("[%s #%d] failed allocating subIrp\n", dispTransfer, sequenceID);

			ntStatus = STATUS_INSUFFICIENT_RESOURCES;

			goto transfer_Free;
		}

		subRequestContext->SubIrp = subIrp;

		ntStatus = allocate_suburb((USHORT)urbFunction, stageSize, packetSize, &nPackets, &subUrb);
		if (!NT_SUCCESS(ntStatus))
		{
			USBERR("[%s #%d] failed allocating subUrb\n", dispTransfer, sequenceID);
			goto transfer_Free;
		}
		else
		{
			USBDBG("[%s #%d] packets=%d irp-urb = #%d\n", 
				dispTransfer, sequenceID, nPackets, i);
		}

		subRequestContext->SubUrb = subUrb;

		//
		// 4. Allocate a sub request mdl.
		//
		subMdl = IoAllocateMdl((PVOID) virtualAddress,
			stageSize,
			FALSE,
			FALSE,
			NULL);

		if (subMdl == NULL)
		{
			USBERR("[%s #%d] failed allocating subMdl\n", dispTransfer, sequenceID);

			ntStatus = STATUS_INSUFFICIENT_RESOURCES;

			goto transfer_Free;
		}

		subRequestContext->SubMdl = subMdl;

		IoBuildPartialMdl(irp->MdlAddress,
			subMdl,
			(PVOID)virtualAddress,
			stageSize);

		// Update loop variables for next iteration.
		//
		virtualAddress += stageSize;

		totalLength    -= stageSize;

		//
		// Initialize the sub request urb.
		//
		if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
		{
			USBDBG("[%s #%d] stage-size=%d nPackets=%d\n",
				dispTransfer, sequenceID, stageSize, nPackets);

			subUrb->UrbIsochronousTransfer.PipeHandle = pipeHandle;

			// The direction is set here, other flags are set in the
			// set_urb_transfer_flags() function.
			//
			subUrb->UrbIsochronousTransfer.TransferFlags = direction;

			subUrb->UrbIsochronousTransfer.TransferBufferLength = stageSize;
			subUrb->UrbIsochronousTransfer.TransferBufferMDL = subMdl;

			//
			// when the client driver sets the ASAP flag, it basically
			// guarantees that it will make data available to the HC
			// and that the HC should transfer it in the next transfer frame
			// for the endpoint.(The HC maintains a next transfer frame
			// state variable for each endpoint). By resetting the pipe,
			// we make the pipe as virgin. If the data does not get to the HC
			// fast enough, the USBD_ISO_PACKET_DESCRIPTOR - Status is
			// USBD_STATUS_BAD_START_FRAME on uhci. On ohci it is 0xC000000E.
			//

			subUrb->UrbIsochronousTransfer.NumberOfPackets = nPackets;
			//
			// Set the offsets for every packet for reads/writes
			//
			if (read)
			{
				offset = 0;

				for (j = 0; j < nPackets; j++)
				{
					subUrb->UrbIsochronousTransfer.IsoPacket[j].Offset = offset;

					if (stageSize > (ULONG)packetSize)
					{
						subUrb->UrbIsochronousTransfer.IsoPacket[j].Length = 0;
						offset    += packetSize;
						stageSize -= packetSize;
					}
					else
					{
						subUrb->UrbIsochronousTransfer.IsoPacket[j].Length = 0;
						offset    += stageSize;
						stageSize  = 0;
					}
				}
			}
			else
			{
				offset = 0;

				for (j = 0; j < nPackets; j++)
				{
					subUrb->UrbIsochronousTransfer.IsoPacket[j].Offset = offset;

					if (stageSize > (ULONG)packetSize)
					{
						subUrb->UrbIsochronousTransfer.IsoPacket[j].Length = packetSize;
						offset    += packetSize;
						stageSize -= packetSize;
					}
					else
					{
						subUrb->UrbIsochronousTransfer.IsoPacket[j].Length = stageSize;
						offset    += stageSize;
						stageSize  = 0;
						/*
						ASSERT(offset == (subUrb->UrbIsochronousTransfer.IsoPacket[j].Length +
						subUrb->UrbIsochronousTransfer.IsoPacket[j].Offset));
						*/
					}
				}
			}
		}
		else
		{
			USBDBG("[%s #%d] stage-size=%d\n",dispTransfer, sequenceID, stageSize);

			subUrb->UrbBulkOrInterruptTransfer.PipeHandle = pipeHandle;

			subUrb->UrbBulkOrInterruptTransfer.TransferFlags = direction;

			subUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = stageSize;
			subUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL = subMdl;
		}
		// Initialize the sub irp stack location
		//
		nextStack = IoGetNextIrpStackLocation(subIrp);

		nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

		nextStack->Parameters.Others.Argument1 = (PVOID) subUrb;

		nextStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;

		IoSetCompletionRoutine(subIrp,
			(PIO_COMPLETION_ROUTINE)large_transfer_complete,
			(PVOID)subRequestContext,
			TRUE,
			TRUE,
			TRUE);

		// Update loop variables for next iteration.
		//
		if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
			stageSize = get_iso_stagesize(totalLength, packetSize, maxTransferSize);
		else
		{
			if (totalLength > (maxTransferSize))
				stageSize = maxTransferSize;
			else
				stageSize = totalLength;
		}
		startOffset += stageSize;
	}

	//
	// While we were busy create subsidiary irp/urb pairs..
	// the main read/write irp may have been cancelled !!
	//

	if (!irp->Cancel)
	{
		//
		// normal processing
		//

		USBDBG("[%s #%d] normal processing\n", dispTransfer, sequenceID);

		IoMarkIrpPending(irp);

		// The cancel routine might run simultaneously as soon as it is
		// set.  Do not access the main request irp in any way after
		// setting the cancel routine.
		//
		// Note that it is still safe to access the sub requests up to
		// the point where each sub request is called down the driver
		// stack due to the sub request reference count which must be
		// decremented by the completion routine.  Do not access a sub
		// request in any way after it is called down the driver stack.
		//
		// After setting the main request irp cancel routine we are
		// committed to calling each of the sub requests down the
		// driver stack.
		//
		IoSetCancelRoutine(irp, large_transfer_cancel_routine);

		for (i = 0; i < numIrps; i++)
		{
			subRequestContext = subRequestContextArray[i];

			USBDBG("[%s #%d] IoCallDriver subIrp %d\n", dispTransfer, sequenceID, i);

			// Set the transfer flags just before we call the irp down.
			// If this is an iso transfer, set_urb_transfer_flags() might need
			// to get the start frame and set latency.
			//
			set_urb_transfer_flags(dev,irp, subRequestContext->SubUrb, transferFlags, isoLatency);

			IoCallDriver(dev->target_device, subRequestContext->SubIrp);
		}

		// The sub requests are freed in either the sub request
		// completion routine or in the main request irp cancel routine.
		//
		// Main request irp is completed only in sub request completion
		// routine.

		ExFreePool(subRequestContextArray);

		return STATUS_PENDING;
	}
	else
	{
		//
		// The Cancel flag for the irp has been set.
		//
		USBDBG("[%s #%d] Cancel flag set\n", dispTransfer, sequenceID);

		ntStatus = STATUS_CANCELLED;
	}

	//
	// Resource allocation failure, or the main request irp was
	// cancelled before the cancel routine was set.  Free any resource
	// allocations and complete the main request irp.
	//
	// No sub requests were ever called down the driver stack in this
	// case.
	//

transfer_Free:

	if (subRequestContextArray != NULL)
	{
		for (i = 0; i < numIrps; i++)
		{
			subRequestContext = subRequestContextArray[i];

			if (subRequestContext != NULL)
			{
				if (subRequestContext->SubIrp != NULL)
				{
					IoFreeIrp(subRequestContext->SubIrp);
				}

				if (subRequestContext->SubUrb != NULL)
				{
					ExFreePool(subRequestContext->SubUrb);
				}

				if (subRequestContext->SubMdl != NULL)
				{
					IoFreeMdl(subRequestContext->SubMdl);
				}

				ExFreePool(subRequestContext);
			}
		}

		ExFreePool(subRequestContextArray);
	}

	irp->IoStatus.Status = ntStatus;
	irp->IoStatus.Information = 0;

	IoCompleteRequest(irp, IO_NO_INCREMENT);

	USBERR("[%s #%d] ntStatus=%Xh\n",
		dispTransfer, sequenceID, ntStatus);

	remove_lock_release(dev);

	return ntStatus;
}

/*-----------------------------------------------------------------------------
Routine Description:

This routine handles the completion of a Sub Request irp that was
created to handle part (or all) of the transfer for a Main Request
irp.

It updates the transfer length (IoStatus.Information) of the Main
Request irp, and completes the Main Request irp if this Sub Request
irp is the final outstanding one.

The Sub Request irp and Sub Request Context may either be freed
here, or by large_transfer_cancel(), according to which routine is
the last one to reference the Sub Request irp.

Arguments:

DeviceObject - NULL as this Sub Request irp was allocated without a
stack location for this driver to use itself.

irp - Sub Request irp

Context - Sub Request Context (PSUB_REQUEST_CONTEXT)

Return Value:

STATUS_MORE_PROCESSING_REQUIRED - Tells IoMgr not to free the Sub
Request irp as it is either explicitly freed here, or by
large_transfer_cancel()

*/
NTSTATUS large_transfer_complete(IN PDEVICE_OBJECT DeviceObjectIsNULL,
								 IN PIRP irp,
								 IN PVOID Context)
{
	PSUB_REQUEST_CONTEXT    subRequestContext;
	PURB                    subUrb;
	PIRP                    mainIrp;
	PMAIN_REQUEST_CONTEXT   mainRequestContext;
	PDEVICE_OBJECT          deviceObject;
	NTSTATUS                ntStatus;
	ULONG                   information;
	ULONG                   i;
	KIRQL                   irql;
	BOOLEAN                 completeMainRequest;
	int						subRequestByteOffset;
	int						subRequestByteCount;

	LONG					sequenceID;
	const char*				dispTransfer;
	PUCHAR					outBuffer;
	BOOLEAN					needs_cancelled = FALSE;

	UNREFERENCED_PARAMETER( DeviceObjectIsNULL );
	subRequestContext = (PSUB_REQUEST_CONTEXT)Context;

	subUrb = subRequestContext->SubUrb;
	mainIrp = subRequestContext->MainIrp;

	// The main request irp context is overlaid on top of
	// irp->Tail.Overlay.DriverContext.  Get a pointer to it.
	//
	mainRequestContext = (PMAIN_REQUEST_CONTEXT)
		mainIrp->Tail.Overlay.DriverContext;

	sequenceID = mainRequestContext->sequenceID;
	dispTransfer = mainRequestContext->dispTransfer;

	subRequestByteCount = 0;

	deviceObject = IoGetCurrentIrpStackLocation(mainIrp)->DeviceObject;

	ntStatus = irp->IoStatus.Status;

	if (NT_SUCCESS(ntStatus) && USBD_SUCCESS(subUrb->UrbHeader.Status))
	{
		if (subUrb->UrbHeader.Function == URB_FUNCTION_ISOCH_TRANSFER)
		{
			information = subUrb->UrbIsochronousTransfer.TransferBufferLength;
			USBDBG("[%s #%d] transferred=%d\n", 
				dispTransfer, 
				sequenceID, 
				information);

			for (i = 0; i < subUrb->UrbIsochronousTransfer.NumberOfPackets; i++)
			{
				if (subUrb->UrbIsochronousTransfer.IsoPacket[i].Status != 0)
				{
					USBDBG("[%s #%d] IsoPacket[%d].Length=%d IsoPacket[%d].Status=%08Xh\n",
						dispTransfer, 
						sequenceID, 
						i,
						subUrb->UrbIsochronousTransfer.IsoPacket[i].Length,
						i,
						subUrb->UrbIsochronousTransfer.IsoPacket[i].Status);
				}
			}
		}
		else
		{
			subRequestByteCount = MmGetMdlByteCount(subUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL);
			subRequestByteOffset = MmGetMdlByteOffset(subUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL);
			information = subUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
			USBDBG("[%s #%d] offset=%d requested=%d transferred=%d\n", 
				dispTransfer, 
				sequenceID, 
				subRequestByteOffset,
				subRequestByteCount, 
				information);
		}

	}
	else
	{
		information = 0;

		if (ntStatus == STATUS_CANCELLED)
		{
			USBDBG("[%s #%d] cancelled\n",
				dispTransfer, 
				sequenceID);
		}
		else
		{
			USBERR("[%s #%d] failed. status=%Xh urb-status=%Xh\n",
				dispTransfer,
				sequenceID, 
				ntStatus, 
				subUrb->UrbHeader.Status);
		}
	}

	// Prevent the cancel routine from executing simultaneously
	//
	IoAcquireCancelSpinLock(&irql);

	if (subUrb->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
	{
		/*
		USBDBG("[%s #%d] MdlAddress:0x%x Function:0x%x TransferFlags:0x%x information:%d srcOffset:%d dstOffset:%d\n",
		dispTransfer,
		sequenceID, 
		mainIrp->MdlAddress,
		subUrb->UrbHeader.Function,
		subUrb->UrbBulkOrInterruptTransfer.TransferFlags,
		information,
		mainIrp->IoStatus.Information,
		subRequestContext->startOffset);
		*/

		// If this bulk read was valid, returned a length > 0 and a 
		// previous sub irp/urb pair in this batch was short then the
		// data was written to the wrong offset in the output buffer.
		// We need to move this data to the correct location in the
		// output buffer.
		//
		if ((mainIrp->MdlAddress) &&
			((subUrb->UrbBulkOrInterruptTransfer.TransferFlags & USBD_TRANSFER_DIRECTION_IN) == USBD_TRANSFER_DIRECTION_IN) &&
			(information > 0))
		{
			if (mainIrp->IoStatus.Information < subRequestContext->startOffset)
			{
				// Translate a virtual address range described in the MDL for a user buffer
				// to a system-space address range.
				//
				outBuffer = MmGetSystemAddressForMdlSafe(mainIrp->MdlAddress, HighPagePriority);

				if (!outBuffer)
				{
					information = 0;

					USBERR("[%s #%d] failed translating a virtual address range\n",
						dispTransfer,sequenceID);
				}
				else
				{
					USBDBG("[%s #%d] adjusting outBuffer old-offset %d to new-offset %d (length=%d)\n",
						dispTransfer,
						sequenceID,
						subRequestContext->startOffset,
						mainIrp->IoStatus.Information,
						information);

					// move the data this subirp just put in the output buffer to the correct
					// location.
					//
					RtlMoveMemory(outBuffer+mainIrp->IoStatus.Information,
						outBuffer+subRequestContext->startOffset,
						information);
				}
			}
			else if (information < (ULONG)subRequestByteCount)
			{
				needs_cancelled = TRUE;
			}
		}
	}

	// add the sub transfer length to the main transfer length.
	//
	mainIrp->IoStatus.Information += information;

	// Remove the sub request from the main request sub request list.
	//
	RemoveEntryList(&subRequestContext->ListEntry);

	// If the sub request list is now empty clear the main request
	// cancel routine and note that the main request should be
	// completed.
	//
	if (IsListEmpty(&mainRequestContext->SubRequestList))
	{
		completeMainRequest = TRUE;
		needs_cancelled = FALSE;
		IoSetCancelRoutine(mainIrp, NULL);
	}
	else
	{
		completeMainRequest = FALSE;

		// If this is a bulk/interrupt transfer and we transmit less
		// bytes then what we requested, cancel the pending subIrps.
		// If the subirp(s) cannot be cancelled, any data they return
		// must be moved to the correct location in the output buffer.
		//
		if (needs_cancelled)
		{
			// IoSetCancelRoutine returns the previous value of 
			// mainIrp->CancelRoutine. If no Cancel routine was previously
			// set, or if IRP cancellation is already in progress, 
			// IoSetCancelRoutine returns NULL.
			needs_cancelled = (IoSetCancelRoutine(mainIrp, NULL) == NULL) ? FALSE : TRUE;
		}

	}

	// The cancel routine may now execute simultaneously, unless of
	// course the cancel routine was just cleared above.
	//
	// Do not access the main irp or the mainRequestContext in any way
	// beyond this point, unless this is the single instance of the sub
	// request completion routine which will complete the main irp.
	//
	IoReleaseCancelSpinLock(irql);

	// needs_cancelled is set for the first subrequest that returns "short".
	// provided it is not the final subrequest.
	if (needs_cancelled)
	{
		// we can only do this (after IoReleaseCancelSpinLock) because the 
		// cancel routine for mainIrp was removed above.
		large_transfer_cancel(mainIrp, FALSE);
	}

	if (InterlockedDecrement(&subRequestContext->ReferenceCount) == 0)
	{
		// If the reference count is now zero then the cancel routine
		// will not free the sub request.  (Either the cancel routine
		// ran and accessed the sub request and incremented the
		// reference count and then decremented it again without freeing
		// the sub request, or the cancel routine did not access the
		// sub request and can no longer access it because it has been
		// removed from the main request sub request list.)
		//
		IoFreeIrp(subRequestContext->SubIrp);

		ExFreePool(subRequestContext->SubUrb);

		IoFreeMdl(subRequestContext->SubMdl);

		ExFreePool(subRequestContext);
	}
	else
	{
		// In this case the cancel routine for the main request must be
		// executing and is accessing the sub request after
		// incrementing its reference count.  When the cancel routine
		// decrements the reference count again it will take care of
		// freeing the sub request.
	}

	if (completeMainRequest)
	{
		// The final sub request for the main request has completed so
		// now complete the main request.
		//
		USBMSG("[%s #%d] done. total transferred=%d\n",
			dispTransfer, 
			sequenceID, 
			mainIrp->IoStatus.Information);

		mainIrp->IoStatus.Status = STATUS_SUCCESS;

		IoCompleteRequest(mainIrp, IO_NO_INCREMENT);

		// the remove lock was referenced in dispatch_ioctl() when the main irp
		// was first submitted.
		remove_lock_release(deviceObject->DeviceExtension);
	}

	return STATUS_MORE_PROCESSING_REQUIRED;
}

/*++

Routine Description:

This is the cancellation routine for the main read/write irp.

It cancels all currently outstanding sub requests for the main
request.

Completing the main request is the responsibility of the sub
request completion routine after all outstanding sub requests have
completed.

Return Value:

None

--*/
VOID large_transfer_cancel_routine(IN PDEVICE_OBJECT DeviceObject, IN PIRP irp)
{
	large_transfer_cancel(irp, TRUE);
}

/*++

Routine Description:

Cancels all currently outstanding sub requests for the main
request.

Completing the main request is the responsibility of the sub
request completion routine after all outstanding sub requests have
completed.

Return Value:

None

--*/
VOID large_transfer_cancel(IN PIRP irp, BOOLEAN releaseCancelSpinlock)
{
	PMAIN_REQUEST_CONTEXT   mainRequestContext;
	LIST_ENTRY              cancelList;
	PLIST_ENTRY             subRequestEntry;
	PSUB_REQUEST_CONTEXT    subRequestContext;

	// The main request irp context is overlaid on top of
	// irp->Tail.Overlay.DriverContext.  Get a pointer to it.
	//
	mainRequestContext = (PMAIN_REQUEST_CONTEXT)
		irp->Tail.Overlay.DriverContext;

	// The mainRequestContext SubRequestList cannot be simultaneously
	// changed by anything else as long as the cancel spin lock is still
	// held, but can be changed immediately by the completion routine
	// after the cancel spin lock is released.
	//
	// Iterate over the mainRequestContext SubRequestList and add all of
	// the currently outstanding sub requests to the list of sub
	// requests to be cancelled.
	//
	InitializeListHead(&cancelList);

	subRequestEntry = mainRequestContext->SubRequestList.Flink;

	while (subRequestEntry != &mainRequestContext->SubRequestList)
	{
		subRequestContext = CONTAINING_RECORD(subRequestEntry,
			SUB_REQUEST_CONTEXT,
			ListEntry);

		// Prevent the sub request from being freed as soon as the
		// cancel spin lock is released by incrementing the reference
		// count on the sub request.
		//
		InterlockedIncrement(&subRequestContext->ReferenceCount);

		InsertTailList(&cancelList, &subRequestContext->CancelListEntry);

		subRequestEntry = subRequestEntry->Flink;
	}

	USBDBG("[%s #%d] cancel-reason=%s\n",
		mainRequestContext->dispTransfer,
		mainRequestContext->sequenceID,
		releaseCancelSpinlock?"User":"ShortTransfer");

	if (releaseCancelSpinlock)
	{

		// The main read/write irp can be completed immediately after
		// releasing the cancel spin lock.  Do not access the main
		// read/write irp or the mainRequestContext in any way beyond this
		// point.
		//
		IoReleaseCancelSpinLock(irp->CancelIrql);
	}

	// Iterate over the list that was built of sub requests to cancel
	// and cancel each sub request.
	//
	while (!IsListEmpty(&cancelList))
	{
		subRequestEntry = RemoveHeadList(&cancelList);

		subRequestContext = CONTAINING_RECORD(subRequestEntry,
			SUB_REQUEST_CONTEXT,
			CancelListEntry);

		if (!subRequestContext->SubIrp->Cancel)
		{
			IoCancelIrp(subRequestContext->SubIrp);
		}

		if (InterlockedDecrement(&subRequestContext->ReferenceCount) == 0)
		{
			// If the reference count is now zero then the completion
			// routine already ran for the sub request but did not free
			// the sub request so it can be freed now.
			//
			IoFreeIrp(subRequestContext->SubIrp);

			ExFreePool(subRequestContext->SubUrb);

			IoFreeMdl(subRequestContext->SubMdl);

			ExFreePool(subRequestContext);
		}
		else
		{
			// The completion routine for the sub request has not yet
			// executed and decremented the sub request reference count.
			// Do not free the sub request here.  It will be freed when
			// the sub request completion routine executes.
		}
	}
}

static int get_iso_stagesize(int totalLength, int packetSize, int maxTransferSize)
{
	int stageSize;

	if (totalLength > (packetSize * 255) || totalLength > maxTransferSize)
	{
		stageSize = packetSize * 255;
		if (stageSize > maxTransferSize)
			stageSize = maxTransferSize;

		//		stageSize = stageSize - (stageSize % packetSize);

	}
	else
	{
		stageSize = totalLength;
	}

	return stageSize;
}

static NTSTATUS allocate_suburb(USHORT urbFunction,
								int stageSize,
								int packetSize,
								ULONG* nPackets,
								PURB* subUrbRef)
{
	int urbSize;
	//
	// 3. Allocate a sub request urb.
	//
	if (urbFunction == URB_FUNCTION_ISOCH_TRANSFER)
	{
		*nPackets = (stageSize + packetSize - 1) / packetSize;
		urbSize = GET_ISO_URB_SIZE(*nPackets);
	}
	else
	{
		*nPackets = 0;
		urbSize = sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER);
	}

	*subUrbRef = (PURB)ExAllocatePool(NonPagedPool, urbSize);

	if ((*subUrbRef) == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;

	RtlZeroMemory(*subUrbRef, urbSize);

	(*subUrbRef)->UrbHeader.Length = (USHORT)urbSize;
	(*subUrbRef)->UrbHeader.Function = urbFunction;

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
