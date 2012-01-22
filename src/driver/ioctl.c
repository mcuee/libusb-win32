/* libusb-win32, Generic Windows USB Library
* Copyright (c) 2010 Travis Robinson <libusbdotnet@gmail.com>
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
#include "libusb-win32_version.h"

// used but all transfer functions to get a valid libusb_endpoint_t *
// for the request.
//
#define TRANSFER_IOCTL_GET_PIPEINFO()										\
	/* check if the pipe exists and get the pipe information */				\
	if (!get_pipe_info(dev, request->endpoint.endpoint,&pipe_info))			\
	{																		\
	USBERR("%s: failed getting pipe info for endpoint: %02Xh\n",		\
	dispCtlCode, request->endpoint.endpoint);						\
	status = STATUS_INVALID_PARAMETER;									\
	goto IOCTL_Done;													\
	}

// warns if receive buffer is not an interval of the maximum packet size.
#define TRANSFER_IOCTL_CHECK_READ_BUFFER()									\
	/* read buffer lengthd must be equal to or an interval of the max */	\
	/* packet size */														\
	if (!pipe_info->maximum_packet_size)									\
	{																		\
	USBWRN("%s: wMaxPacketSize=0 for endpoint %02Xh.\n",				\
	dispCtlCode, request->endpoint.endpoint);							\
	status = STATUS_INVALID_PARAMETER;									\
	goto IOCTL_Done;													\
	}																		\
	else if (transfer_buffer_length % pipe_info->maximum_packet_size)		\
	{																		\
	USBWRN("%s: buffer length %d is not an interval wMaxPacketSize "	\
	"for endpoint %02Xh.\n",										\
	dispCtlCode, transfer_buffer_length, request->endpoint.endpoint);	\
	}

// validates the urb function and direction against libusb_endpoint_t
#define TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION()						\
	if (urbFunction   != UrbFunctionFromEndpoint(pipe_info) ||				\
	usbdDirection != UsbdDirectionFromEndpoint(pipe_info))				\
	{																		\
	USBERR("%s: not compatible with endpoint %02Xh.\n"					\
	"\turbFunction  =%Xh usbdDirection  =%Xh\n"						\
	"\tpipeFunction =%Xh pipeDirection  =%Xh\n",					\
	dispCtlCode,													\
	pipe_info->address,												\
	urbFunction,usbdDirection,										\
	UrbFunctionFromEndpoint(pipe_info),								\
	UsbdDirectionFromEndpoint(pipe_info));							\
	status = STATUS_INVALID_PARAMETER;									\
	goto IOCTL_Done;													\
	}

// calls the transfer function and returns NTSTATUS
#define TRANSFER_IOCTL_EXECUTE()										\
	if (transfer_buffer_length > (ULONG)(maxTransferSize))				\
	/* split large transfers */											\
	return large_transfer(dev, irp,					\
	usbdDirection,									\
	urbFunction,									\
	pipe_info,										\
	request->endpoint.packet_size,					\
	maxTransferSize,								\
	request->endpoint.transfer_flags,				\
	request->endpoint.iso_start_frame_latency,		\
	transfer_buffer_mdl,							\
	transfer_buffer_length);						\
	else											\
	/* normal transfer */							\
	return transfer(dev, irp,						\
	usbdDirection,									\
	urbFunction,									\
	pipe_info,										\
	request->endpoint.packet_size,					\
	request->endpoint.transfer_flags,				\
	request->endpoint.iso_start_frame_latency,		\
	transfer_buffer_mdl,							\
	transfer_buffer_length);

NTSTATUS dispatch_ioctl(libusb_device_t *dev, IRP *irp)
{
	int maxTransferSize;
	int ret = 0;
	NTSTATUS status = STATUS_SUCCESS;

	IO_STACK_LOCATION *stack_location = IoGetCurrentIrpStackLocation(irp);

	ULONG control_code			 = stack_location->Parameters.DeviceIoControl.IoControlCode;

	ULONG input_buffer_length	 = stack_location->Parameters.DeviceIoControl.InputBufferLength;
	ULONG output_buffer_length	 = stack_location->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG transfer_buffer_length = stack_location->Parameters.DeviceIoControl.OutputBufferLength;

	libusb_request *request		 = (libusb_request *)irp->AssociatedIrp.SystemBuffer;
	char *output_buffer			 = (char *)irp->AssociatedIrp.SystemBuffer;
	char *input_buffer			 = (char *)irp->AssociatedIrp.SystemBuffer;
	MDL *transfer_buffer_mdl	 = irp->MdlAddress;
	libusb_endpoint_t* pipe_info = NULL;
	const char* dispCtlCode		 = NULL;
	int urbFunction				 = -1;
	int usbdDirection			 = -1;

	status = remove_lock_acquire(dev);

	if (!NT_SUCCESS(status))
	{
		status = complete_irp(irp, status, 0);
		remove_lock_release(dev);
		return status;
	}

	///////////////////////////////////
	// DIRECT control codes          //
	///////////////////////////////////
	switch(control_code)
	{
	case LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ:

		dispCtlCode = "INTERRUPT_OR_BULK_READ";

		// check if the request and buffer is valid
		if (!request || !transfer_buffer_mdl || input_buffer_length < sizeof(libusb_request))
		{
			USBERR("%s: invalid transfer request\n", 
				dispCtlCode);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

		// must be a bulk or interrupt pipe
		if (!IS_BULK_PIPE(pipe_info) && !IS_INTR_PIPE(pipe_info))
		{
			goto IoctlIsochronousRead;
			/*
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
			*/
		}

		// read buffer length must be equal to or an interval of the max packet size
		TRANSFER_IOCTL_CHECK_READ_BUFFER();

		urbFunction = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
		usbdDirection = USBD_TRANSFER_DIRECTION_IN;
		maxTransferSize = GetMaxTransferSize(pipe_info, request->endpoint.max_transfer_size);

		// ensure that the urb function and direction we set matches the
		// pipe information
		//
		TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION();

		// calls the transfer function and returns NTSTATUS
		TRANSFER_IOCTL_EXECUTE();

	case LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE:

		dispCtlCode = "INTERRUPT_OR_BULK_WRITE";

		/* we don't check 'transfer_buffer_mdl' here because it might be NULL */
		/* if the DLL requests to send a zero-length packet */
		if (!request || input_buffer_length < sizeof(libusb_request))
		{
			USBERR("%s: invalid transfer request\n", dispCtlCode);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

		// must be a bulk or interrupt pipe
		if (!IS_BULK_PIPE(pipe_info) && !IS_INTR_PIPE(pipe_info))
		{
			goto IoctlIsochronousWrite;
			/*
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
			*/
		}

		urbFunction = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
		usbdDirection = USBD_TRANSFER_DIRECTION_OUT;
		maxTransferSize = GetMaxTransferSize(pipe_info, request->endpoint.max_transfer_size);

		// ensure that the urb function and direction we set matches the
		// pipe information
		//
		//TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION();

		// calls the transfer function and returns NTSTATUS
		TRANSFER_IOCTL_EXECUTE();

	case LIBUSB_IOCTL_ISOCHRONOUS_READ:

		dispCtlCode = "ISOCHRONOUS_READ";

		// check if the request and buffer is valid
		if (!request || !transfer_buffer_mdl || input_buffer_length < sizeof(libusb_request))
		{
			USBERR("%s: invalid transfer request\n", dispCtlCode);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

IoctlIsochronousRead:
		dispCtlCode = "ISOCHRONOUS_READ";
		// must be an isochronous endpoint
		if (!IS_ISOC_PIPE(pipe_info))
		{
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		// read buffer length must be equal to or an interval of the max packet size
		TRANSFER_IOCTL_CHECK_READ_BUFFER();

		urbFunction = URB_FUNCTION_ISOCH_TRANSFER;
		usbdDirection = USBD_TRANSFER_DIRECTION_IN;

		// Do not use large transfers/splitting for ISO.
		maxTransferSize = 0x7fffffff;

		// ensure that the urb function and direction we set matches the
		// pipe information
		//
		TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION();

		// calls the transfer function and returns NTSTATUS
		TRANSFER_IOCTL_EXECUTE();

	case LIBUSB_IOCTL_ISOCHRONOUS_WRITE:

		dispCtlCode = "ISOCHRONOUS_WRITE";

		// check if the request and buffer is valid
		if (!transfer_buffer_mdl || !request || input_buffer_length < sizeof(libusb_request))
		{
			USBERR("%s: invalid transfer request\n", dispCtlCode);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

IoctlIsochronousWrite:
		dispCtlCode = "ISOCHRONOUS_WRITE";
		// must be an isochronous endpoint
		if (!IS_ISOC_PIPE(pipe_info))
		{
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		urbFunction = URB_FUNCTION_ISOCH_TRANSFER;
		usbdDirection = USBD_TRANSFER_DIRECTION_OUT;
		TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION();

		// Do not use large transfers/splitting for ISO.
		maxTransferSize = 0x7fffffff;

		// ensure that the urb function and direction we set matches the
		// pipe information
		//
		TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION();

		TRANSFER_IOCTL_EXECUTE();
	}

	///////////////////////////////////
	// METHOD_BUFFERED control codes //
	///////////////////////////////////
	if (!request || input_buffer_length < sizeof(libusb_request)
		|| input_buffer_length > LIBUSB_MAX_READ_WRITE
		|| output_buffer_length > LIBUSB_MAX_READ_WRITE
		|| transfer_buffer_length > LIBUSB_MAX_READ_WRITE)
	{
		USBERR0("invalid input or output buffer\n");

		status = complete_irp(irp, STATUS_INVALID_PARAMETER, 0);
		remove_lock_release(dev);
		return status;
	}

	switch(control_code)
	{
	case LIBUSB_IOCTL_SET_CONFIGURATION:

		status = set_configuration(dev,
			request->configuration.configuration, 
			request->timeout);
		break;

	case LIBUSB_IOCTL_GET_CACHED_CONFIGURATION:
	case LIBUSB_IOCTL_GET_CONFIGURATION:

		if (!output_buffer || output_buffer_length < 1)
		{
			USBERR0("get_configuration: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		if (control_code == LIBUSB_IOCTL_GET_CACHED_CONFIGURATION)
		{
			ret = 0;
			if (dev->config.value >= 0)
			{
				*output_buffer = (char)dev->config.value;
				ret = 1;
			}
			status = STATUS_SUCCESS;
		}
		else
		{
			status = get_configuration(dev, output_buffer, &ret, request->timeout);
		}
		break;

	case LIBUSB_IOCTL_SET_INTERFACE:

		status = set_interface(
			dev, 
			request->intf.interface_number,
			request->intf.altsetting_number,
			request->timeout);

		break;

	case LIBUSB_IOCTL_GET_INTERFACE:

		if (!output_buffer || output_buffer_length < 1)
		{
			USBERR0("get_interface: invalid output buffer\n");
			status =  STATUS_INVALID_PARAMETER;
			break;
		}

		status = get_interface(
			dev,
			request->intf.interface_number,
			output_buffer, 
			request->timeout);

		if (NT_SUCCESS(status))
			ret = 1;

		break;

	case LIBUSB_IOCTL_SET_FEATURE:

		status = set_feature(dev, request->feature.recipient,
			request->feature.index, request->feature.feature,
			request->timeout);

		break;

	case LIBUSB_IOCTL_CLEAR_FEATURE:

		status = clear_feature(dev, request->feature.recipient,
			request->feature.index, request->feature.feature,
			request->timeout);

		break;

	case LIBUSB_IOCTL_GET_STATUS:

		if (!output_buffer || output_buffer_length < 2)
		{
			USBERR0("get_status: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = get_status(dev, request->status.recipient,
			request->status.index, output_buffer,
			&ret, request->timeout);

		break;

	case LIBUSB_IOCTL_SET_DESCRIPTOR:

		if (input_buffer_length <= sizeof(libusb_request))
		{
			USBERR0("set_descriptor: invalid input buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = set_descriptor(dev,
			input_buffer + sizeof(libusb_request),
			input_buffer_length - sizeof(libusb_request),
			request->descriptor.type,
			request->descriptor.recipient,
			request->descriptor.index,
			request->descriptor.language_id,
			&ret, request->timeout);

		break;

	case LIBUSB_IOCTL_GET_DESCRIPTOR:

		if (!output_buffer || !output_buffer_length)
		{
			USBERR0("get_descriptor: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = get_descriptor(dev, output_buffer,
			output_buffer_length,
			request->descriptor.type,
			request->descriptor.recipient,
			request->descriptor.index,
			request->descriptor.language_id,
			&ret, request->timeout);

		break;

	case LIBUSB_IOCTL_VENDOR_READ:

		if (output_buffer_length && !output_buffer)
		{
			USBERR0("vendor_read: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = vendor_class_request(dev,
			request->vendor.type,
			request->vendor.recipient,
			request->vendor.request,
			request->vendor.value,
			request->vendor.index,
			output_buffer,
			output_buffer_length,
			USBD_TRANSFER_DIRECTION_IN,
			&ret, request->timeout);
		break;

	case LIBUSB_IOCTL_VENDOR_WRITE:

		status =
			vendor_class_request(dev,
			request->vendor.type,
			request->vendor.recipient,
			request->vendor.request,
			request->vendor.value,
			request->vendor.index,
			input_buffer_length == sizeof(libusb_request) ?
NULL : input_buffer + sizeof(libusb_request),
	   input_buffer_length - sizeof(libusb_request),
	   USBD_TRANSFER_DIRECTION_OUT,
	   &ret, request->timeout);
		break;

	case LIBUSB_IOCTL_RESET_ENDPOINT:

		status = reset_endpoint(dev, request->endpoint.endpoint,
			request->timeout);
		break;

	case LIBUSB_IOCTL_ABORT_ENDPOINT:

		status = abort_endpoint(dev, request->endpoint.endpoint,
			request->timeout);
		break;

	case LIBUSB_IOCTL_RESET_DEVICE:

		status = reset_device(dev, request->timeout);
		break;

	case LIBUSB_IOCTL_RESET_DEVICE_EX:

		status = reset_device_ex(dev, request->timeout, request->reset_ex.reset_type);
		break;

	case LIBUSB_IOCTL_SET_DEBUG_LEVEL:
		usb_log_set_level(request->debug.level);
		break;

	case LIBUSB_IOCTL_GET_VERSION:

		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("get_version: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		request->version.major = VERSION_MAJOR;
		request->version.minor = VERSION_MINOR;
		request->version.micro = VERSION_MICRO;
		request->version.nano  = VERSION_NANO;
		request->version.mod_value = 1;

		ret = sizeof(libusb_request);
		break;

	case LIBUSB_IOCTL_CLAIM_INTERFACE:
		status = claim_interface(dev, stack_location->FileObject,
			request->intf.interface_number);
		break;

	case LIBUSB_IOCTL_RELEASE_INTERFACE:
		status = release_interface(dev, stack_location->FileObject,
			request->intf.interface_number);
		break;

	case LIBUSB_IOCTL_GET_DEVICE_PROPERTY:
		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("get_device_property: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		status = reg_get_device_property(
			dev->physical_device_object,
			request->device_property.property,
			output_buffer, 
			output_buffer_length, &ret);
		break;

	case LIBUSB_IOCTL_GET_CUSTOM_REG_PROPERTY:
		if (!input_buffer || (input_buffer_length < sizeof(libusb_request)))
		{
			USBERR0("get_custom_reg_property: invalid buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		status=reg_get_custom_property(
			dev->physical_device_object,
			input_buffer, 
			output_buffer_length, 
			request->device_registry_key.name_offset, 
			&ret);
		break;

	case LIBUSB_IOCTL_GET_OBJECT_NAME:
		if (!request || output_buffer_length < 2)
		{
			USBERR0("get_object_name: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		switch (request->objname.objname_index)
		{
		case 0:
			ret = (int)strlen(dev->objname_plugplay_registry_key)+1;
			ret = ret > (int)output_buffer_length ? (int)output_buffer_length : ret;
			RtlCopyMemory(output_buffer, dev->objname_plugplay_registry_key,(SIZE_T) (ret-1));
			output_buffer[ret-1]='\0';
			break;
		default:
			status = STATUS_INVALID_PARAMETER;
		}
		break;

	case LIBUSB_IOCTL_QUERY_DEVICE_INFORMATION:	// METHOD_BUFFERED (QUERY_DEVICE_INFORMATION)
		status = STATUS_NOT_IMPLEMENTED;
		break;

	case LIBUSB_IOCTL_SET_PIPE_POLICY:			// METHOD_BUFFERED (SET_PIPE_POLICY)
		if (!request || !input_buffer || (input_buffer_length <= sizeof(libusb_request)))
		{
			USBERR0("set_pipe_policy: invalid output buffer\n");
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		input_buffer+=sizeof(libusb_request);
		input_buffer_length-=sizeof(libusb_request);
		if (request->pipe_policy.policy_type==PIPE_TRANSFER_TIMEOUT)
		{
			if (input_buffer_length < sizeof(ULONG))
			{
				USBERR0("set_pipe_policy:pipe_transfer_timeout: invalid input buffer\n");
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			if (request->pipe_policy.pipe_id & USB_ENDPOINT_ADDRESS_MASK)
			{
				status = STATUS_NOT_IMPLEMENTED;
				break;
			}

			if (request->pipe_policy.pipe_id & USB_ENDPOINT_DIR_MASK)
				dev->control_read_timeout=*((PULONG)input_buffer);
			else
				dev->control_write_timeout=*((PULONG)input_buffer);

			status = STATUS_SUCCESS;
			break;
		}

		break;

	case LIBUSB_IOCTL_GET_PIPE_POLICY:			// METHOD_BUFFERED (GET_PIPE_POLICY)
		if (!request || input_buffer_length < sizeof(libusb_request) || !output_buffer || (output_buffer_length < 1))
		{
			USBERR0("get_pipe_policy: invalid output buffer\n");
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		if (request->pipe_policy.policy_type==PIPE_TRANSFER_TIMEOUT)
		{
			if (output_buffer_length < sizeof(ULONG))
			{
				USBERR0("get_pipe_policy:pipe_transfer_timeout: invalid output buffer\n");
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			if (request->pipe_policy.pipe_id & USB_ENDPOINT_ADDRESS_MASK)
			{
				status = STATUS_NOT_IMPLEMENTED;
				break;
			}
			if (request->pipe_policy.pipe_id & USB_ENDPOINT_DIR_MASK)
				*((PULONG)output_buffer) = dev->control_read_timeout;
			else
				*((PULONG)output_buffer) = dev->control_write_timeout;

			status = STATUS_SUCCESS;
			break;
		}
		break;

	case LIBUSB_IOCTL_SET_POWER_POLICY:			// METHOD_BUFFERED (SET_POWER_POLICY)
		status = STATUS_NOT_IMPLEMENTED;
		break;

	case LIBUSB_IOCTL_GET_POWER_POLICY:			// METHOD_BUFFERED (GET_POWER_POLICY)
		status = STATUS_NOT_IMPLEMENTED;
		break;

	case LIBUSB_IOCTL_CONTROL_WRITE:			// METHOD_IN_DIRECT (CONTROL_WRITE)
		// check if the request and buffer is valid
		if (!request || !transfer_buffer_mdl || input_buffer_length < sizeof(libusb_request))
		{
			USBERR("%s: invalid transfer request\n", dispCtlCode);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		status = control_transfer(
			dev,
			irp,
			transfer_buffer_mdl,
			transfer_buffer_length,
			USBD_TRANSFER_DIRECTION_OUT,
			&ret,
			dev->control_write_timeout,
			request->control.RequestType,
			request->control.Request,
			request->control.Value,
			request->control.Index,
			request->control.Length);

		break;

	case LIBUSB_IOCTL_CONTROL_READ:				// METHOD_OUT_DIRECT (CONTROL_READ)
		// check if the request and buffer is valid
		if (!request || !transfer_buffer_mdl || input_buffer_length < sizeof(libusb_request))
		{
			USBERR("%s: invalid transfer request\n", dispCtlCode);
			status = STATUS_INVALID_PARAMETER;
			goto IOCTL_Done;
		}

		status = control_transfer(
			dev,
			irp,
			transfer_buffer_mdl,
			transfer_buffer_length,
			USBD_TRANSFER_DIRECTION_IN,
			&ret,
			dev->control_read_timeout,
			request->control.RequestType,
			request->control.Request,
			request->control.Value,
			request->control.Index,
			request->control.Length);

		break;

	case LIBUSB_IOCTL_FLUSH_PIPE:				// METHOD_BUFFERED (FLUSH_PIPE)

		status = STATUS_SUCCESS;
		break;


	case LIBUSBK_IOCTL_CLAIM_INTERFACE:			// METHOD_BUFFERED (CLAIM_INTERFACE)
		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("claim_interfaceK: invalid output buffer\n");
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		status = claim_interface_ex(dev, stack_location->FileObject, &request->intf);
		if (NT_SUCCESS(status))
		{
			ret = sizeof(libusb_request);
		}
		break;

	case LIBUSBK_IOCTL_RELEASE_INTERFACE:		// METHOD_BUFFERED (RELEASE_INTERFACE)

		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("release_interfaceK: invalid output buffer\n");
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		status = release_interface_ex(dev, stack_location->FileObject, &request->intf);
		if (NT_SUCCESS(status))
		{
			ret = sizeof(libusb_request);
		}
		break;

	case LIBUSBK_IOCTL_SET_INTERFACE:			// METHOD_BUFFERED (SET_INTERFACE)

		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("set_interfaceK: invalid output buffer\n");
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		status = set_interface_ex(dev, &request->intf, request->timeout);
		if (NT_SUCCESS(status))
		{
			ret = sizeof(libusb_request);
		}
		break;

	case LIBUSBK_IOCTL_GET_INTERFACE:			// METHOD_BUFFERED (GET_INTERFACE)

		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("get_interfaceK: invalid output buffer\n");
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		status = get_interface_ex(dev, &request->intf, request->timeout);
		if (NT_SUCCESS(status))
		{
			ret = sizeof(libusb_request);
		}
		break;

	default:
		status = STATUS_INVALID_PARAMETER;
	}

IOCTL_Done:
	status = complete_irp(irp, status, ret);
	remove_lock_release(dev);

	return status;
}
