/* LIBUSB-WIN32, Generic Windows USB Library
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
#include "libusb_version.h"

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

// used but all READ (OUT_DIRECT) transfer functions verify the transfer
// buffer size is an interfal of the maximum packet size.
//
#define TRANSFER_IOCTL_CHECK_READ_BUFFER()									\
	/* read buffer lengthd must be equal to or an interval of the max */	\
	/* packet size */														\
	if (transfer_buffer_length % pipe_info->maximum_packet_size)			\
	{																		\
		USBWRN("%s: buffer length %d is not an interval wMaxPacketSize "	\
			   "for endpoint %02Xh.\n",										\
		dispCtlCode, transfer_buffer_length, request->endpoint.endpoint);	\
		/* status = STATUS_INVALID_PARAMETER; */							\
		/* goto IOCTL_Done; */												\
	}

// validates the urb function and direction against libusb_endpoint_t
#define TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION()						\
	if (urbFunction   != UrbFunctionFromEndpoint(pipe_info) ||				\
	    usbdDirection != UsbdDirectionFromEndpoint(pipe_info))				\
	{																		\
		USBERR("%s: not compatible with endpoint %02Xh\n",					\
			dispCtlCode, pipe_info->address);								\
		status = STATUS_INVALID_PARAMETER;									\
		goto IOCTL_Done;													\
	}

// calls the transfer function and returns NTSTATUS
#define TRANSFER_IOCTL_EXECUTE()										\
	if (transfer_buffer_length > (ULONG)(maxTransferSize))				\
		/* split large transfers */										\
		return large_transfer(dev, irp,									\
						usbdDirection,									\
						urbFunction,									\
						pipe_info,										\
						request->endpoint.packet_size,					\
						maxTransferSize,								\
						request->endpoint.transfer_flags,				\
						request->endpoint.iso_start_frame_latency,		\
						transfer_buffer_mdl,							\
						transfer_buffer_length);						\
	else																\
		/* normal transfer */											\
		return transfer(dev, irp,										\
						usbdDirection,									\
						urbFunction,									\
						pipe_info,										\
						request->endpoint.packet_size,					\
						maxTransferSize,								\
						request->endpoint.transfer_flags,				\
						request->endpoint.iso_start_frame_latency,		\
						transfer_buffer_mdl,							\
						transfer_buffer_length);

	
#define TRANSFER_IOCTL_CHECK_AND_AUTOCONFIGURE()							\
{																			\
	CHECK_AND_AUTOCONFIGURE(dev);											\
	if (!dev->config.value)													\
	{																		\
		USBERR("device %s not configured\n", dev->device_id);				\
		status = STATUS_INVALID_PARAMETER;									\
		goto IOCTL_Done;													\
	}																		\
}

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

		// if the device is not configured
		TRANSFER_IOCTL_CHECK_AND_AUTOCONFIGURE();

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

		// must be a bulk or interrupt pipe
		if (!IS_BULK_PIPE(pipe_info) && !IS_INTR_PIPE(pipe_info))
		{
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
            status = STATUS_INVALID_PARAMETER;
            goto IOCTL_Done;
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

		// if the device is not configured
		TRANSFER_IOCTL_CHECK_AND_AUTOCONFIGURE();

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

		// must be a bulk or interrupt pipe
		if (!IS_BULK_PIPE(pipe_info) && !IS_INTR_PIPE(pipe_info))
		{
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
            status = STATUS_INVALID_PARAMETER;
            goto IOCTL_Done;
		}

		urbFunction = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
		usbdDirection = USBD_TRANSFER_DIRECTION_OUT;
		maxTransferSize = GetMaxTransferSize(pipe_info, request->endpoint.max_transfer_size);
		
		// ensure that the urb function and direction we set matches the
		// pipe information
		//
		TRANSFER_IOCTL_CHECK_FUNCTION_AND_DIRECTION();

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

		// check if the device is configured
		TRANSFER_IOCTL_CHECK_AND_AUTOCONFIGURE();

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

		// read buffer length must be equal to or an interval of the max packet size
		TRANSFER_IOCTL_CHECK_READ_BUFFER();

		// must be an isochronous endpoint
		if (!IS_ISOC_PIPE(pipe_info))
		{
			USBERR("%s: incorrect pipe type: %02Xh\n", 
				dispCtlCode, pipe_info->pipe_type);
            status = STATUS_INVALID_PARAMETER;
            goto IOCTL_Done;
		}

		urbFunction = URB_FUNCTION_ISOCH_TRANSFER;
		usbdDirection = USBD_TRANSFER_DIRECTION_IN;
		maxTransferSize = GetMaxTransferSize(pipe_info, request->endpoint.max_transfer_size);
		
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

		// check if the device is configured
		TRANSFER_IOCTL_CHECK_AND_AUTOCONFIGURE();

		// check if the pipe exists and get the pipe information
		TRANSFER_IOCTL_GET_PIPEINFO();

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

		maxTransferSize = GetMaxTransferSize(pipe_info, request->endpoint.max_transfer_size);

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

    case LIBUSB_IOCTL_GET_CONFIGURATION:

        if (!output_buffer || output_buffer_length < 1)
        {
            USBERR0("get_configuration: invalid output buffer\n");
            status = STATUS_INVALID_PARAMETER;
            break;
        }

		// We use the cached value here, even if it's 0.
		// it's possible for this value is be out of sync, however
		// this is what the driver is using so it becomes
		// a question of which value is correct or correctly incorrect.
		//if (dev->config.value)
		//{
			output_buffer[0] = (char)dev->config.value;
			status = STATUS_SUCCESS;
			ret = 1;
			break;
		//}
        //status = get_configuration(dev, output_buffer, &ret, request->timeout);
		//break;

    case LIBUSB_IOCTL_SET_INTERFACE:

        status = set_interface(dev, request->interface.interface,
                               request->interface.altsetting, request->timeout);
        break;

    case LIBUSB_IOCTL_GET_INTERFACE:

        if (!output_buffer || output_buffer_length < 1)
        {
            USBERR0("get_interface: invalid output buffer\n");
            status =  STATUS_INVALID_PARAMETER;
            break;
        }

        status = get_interface(dev, request->interface.interface,
                               output_buffer, &ret, request->timeout);
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
                                 request->interface.interface);
        break;

    case LIBUSB_IOCTL_RELEASE_INTERFACE:
        status = release_interface(dev, stack_location->FileObject,
                                   request->interface.interface);
        break;

	case LIBUSB_IOCTL_GET_DEVICE_PROPERTY:
		if (!request || output_buffer_length < sizeof(libusb_request))
		{
			USBERR0("get_device_property: invalid output buffer\n");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		status = reg_get_device_property(
			dev->target_device,
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
			dev->target_device, 
			input_buffer, 
			output_buffer_length, 
			request->device_registry_key.name_offset, 
			&ret);
		break;

	default:

        status = STATUS_INVALID_PARAMETER;
    }
IOCTL_Done:
    status = complete_irp(irp, status, ret);
    remove_lock_release(dev);

    return status;
}
