/* libusb-win32, Generic Windows USB Library
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


#ifndef __LIBUSB_DRIVER_H__
#define __LIBUSB_DRIVER_H__

//#define SKIP_CONFIGURE_NORMAL_DEVICES
//#define SKIP_DEVICES_WINUSB
//#define SKIP_DEVICES_PICOPP

#ifdef __GNUC__
#include <ddk/usb100.h>
#include <ddk/usbdi.h>
#include <ddk/winddk.h>
#include "usbdlib_gcc.h"
#else
#include <ntifs.h>
#include <wdm.h>
#include "usbdi.h"
#include "usbdlib.h"
#endif

#include <wchar.h>
#include <initguid.h>

#undef interface

#include "driver_debug.h"
#include "error.h"
#include "driver_api.h"

/* some missing defines */
#ifdef __GNUC__

#define USBD_TRANSFER_DIRECTION_OUT       0
#define USBD_TRANSFER_DIRECTION_BIT       0
#define USBD_TRANSFER_DIRECTION_IN        (1 << USBD_TRANSFER_DIRECTION_BIT)
#define USBD_SHORT_TRANSFER_OK_BIT        1
#define USBD_SHORT_TRANSFER_OK            (1 << USBD_SHORT_TRANSFER_OK_BIT)
#define USBD_START_ISO_TRANSFER_ASAP_BIT  2
#define USBD_START_ISO_TRANSFER_ASAP   (1 << USBD_START_ISO_TRANSFER_ASAP_BIT)

#endif

#define SET_CONFIG_ACTIVE_CONFIG -258

#define USB_RECIP_DEVICE    0x00
#define USB_RECIP_INTERFACE 0x01
#define USB_RECIP_ENDPOINT  0x02
#define USB_RECIP_OTHER     0x03

#define USB_TYPE_STANDARD   0x00
#define USB_TYPE_CLASS      0x01
#define USB_TYPE_VENDOR	    0x02


#define LIBUSB_NT_DEVICE_NAME L"\\Device\\libusb0"
#define LIBUSB_SYMBOLIC_LINK_NAME L"\\DosDevices\\libusb0-"

#define LIBUSB_MAX_NUMBER_OF_ENDPOINTS  32
#define LIBUSB_MAX_NUMBER_OF_INTERFACES 32


#define LIBUSB_DEFAULT_TIMEOUT 5000
#define LIBUSB_MAX_CONTROL_TRANSFER_TIMEOUT 5000


#ifndef __GNUC__
#define DDKAPI
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!(FALSE))
#endif

typedef int bool_t;

#define POOL_TAG (ULONG) '0BSU'
#undef ExAllocatePool
#define ExAllocatePool(type, size) ExAllocatePoolWithTag(type, size, POOL_TAG)

#define IS_PIPE_TYPE(pipeInfo, pipeType) ((((pipeInfo->pipe_type & 3)==pipeType))?TRUE:FALSE)

#define IS_CTRL_PIPE(pipeInfo) IS_PIPE_TYPE(pipeInfo,UsbdPipeTypeControl)
#define IS_ISOC_PIPE(pipeInfo) IS_PIPE_TYPE(pipeInfo,UsbdPipeTypeIsochronous)
#define IS_BULK_PIPE(pipeInfo) IS_PIPE_TYPE(pipeInfo,UsbdPipeTypeBulk)
#define IS_INTR_PIPE(pipeInfo) IS_PIPE_TYPE(pipeInfo,UsbdPipeTypeInterrupt)

#define GetMaxTransferSize(pipeInfo, reqMaxTransferSize) ((reqMaxTransferSize) ? reqMaxTransferSize : pipeInfo->maximum_transfer_size)

#define UrbFunctionFromEndpoint(PipeInfo) ((IS_ISOC_PIPE(PipeInfo)) ? URB_FUNCTION_ISOCH_TRANSFER : URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER)
#define UsbdDirectionFromEndpoint(PipeInfo) ((PipeInfo->address & 0x80) ? USBD_TRANSFER_DIRECTION_IN : USBD_TRANSFER_DIRECTION_OUT)

#define UpdateContextConfigDescriptor(DeviceContext, Descriptor, Size, Value, Index)		\
{																							\
	if (DeviceContext->config.descriptor && DeviceContext->config.descriptor!=(Descriptor))	\
		ExFreePool(DeviceContext->config.descriptor);										\
	DeviceContext->config.descriptor=(Descriptor);											\
	DeviceContext->config.total_size=(Size);												\
	DeviceContext->config.value=(Value);													\
	DeviceContext->config.index=(Index);													\
}

#ifndef __WUSBIO_H__

// Pipe policy types

// The default value is zero. To set a time-out value, in Value pass the address of a caller-allocated ULONG variable that contains the time-out interval.
// The PIPE_TRANSFER_TIMEOUT value specifies the time-out interval, in milliseconds. The host controller cancels transfers that do not complete within the specified time-out interval.
// A value of zero (default) indicates that transfers do not time out because the host controller never cancels the transfer.
#define PIPE_TRANSFER_TIMEOUT   0x03

// Device Information types
#define DEVICE_SPEED            0x01

// Device Speeds
#define LowSpeed                0x01
#define FullSpeed               0x02
#define HighSpeed               0x03

#endif

#define USB_ENDPOINT_ADDRESS_MASK 0x0F
#define USB_ENDPOINT_DIR_MASK 0x80
#define LBYTE(w) (w & 0xFF)
#define HBYTE(w) ((w>>8) & 0xFF)


#include <pshpack1.h>

typedef struct
{
    unsigned char length;
    unsigned char type;
} usb_descriptor_header_t;

#include <poppack.h>


typedef struct
{
    long usage_count;
    int remove_pending;
    KEVENT event;
} libusb_remove_lock_t;

typedef struct
{
    int address;
    USBD_PIPE_HANDLE handle;
	int maximum_packet_size;  // Maximum packet size for this pipe
    int interval;            // Polling interval in ms if interrupt pipe
    USBD_PIPE_TYPE pipe_type;   // PipeType identifies type of transfer valid for this pipe
    
	//
    // INPUT
    // These fields are filled in by the client driver
    //
    int maximum_transfer_size; // Maximum size for a single request
                               // in bytes.
    int pipe_flags;
} libusb_endpoint_t;

typedef struct
{
    bool_t valid;
    FILE_OBJECT *file_object; /* file object this interface is bound to */
    libusb_endpoint_t endpoints[LIBUSB_MAX_NUMBER_OF_ENDPOINTS];

} libusb_interface_t;

typedef struct
{
    DEVICE_OBJECT	*self;
    DEVICE_OBJECT	*physical_device_object;
    DEVICE_OBJECT	*next_stack_device;
    DEVICE_OBJECT	*target_device;
    libusb_remove_lock_t remove_lock;
    bool_t is_filter;
    bool_t is_started;
    bool_t surprise_removal_ok;
    int id;
	USB_DEVICE_DESCRIPTOR device_descriptor; 
    struct
    {
        USBD_CONFIGURATION_HANDLE handle;
        int value;
		int index;
        libusb_interface_t interfaces[LIBUSB_MAX_NUMBER_OF_INTERFACES];
		PUSB_CONFIGURATION_DESCRIPTOR descriptor; 
		int total_size;
    } config;
    POWER_STATE power_state;
    DEVICE_POWER_STATE device_power_states[PowerSystemMaximum];
	int initial_config_value;
	char device_id[256];
	bool_t disallow_power_control;
	char objname_plugplay_registry_key[512];
	GUID device_interface_guid;
	bool_t device_interface_in_use;
	UNICODE_STRING device_interface_name;
	int control_read_timeout;
	int control_write_timeout;
} libusb_device_t, DEVICE_EXTENSION, *PDEVICE_EXTENSION;


NTSTATUS DDKAPI add_device(DRIVER_OBJECT *driver_object,
                           DEVICE_OBJECT *physical_device_object);

NTSTATUS DDKAPI dispatch(DEVICE_OBJECT *device_object, IRP *irp);
NTSTATUS dispatch_pnp(libusb_device_t *dev, IRP *irp);
NTSTATUS dispatch_power(libusb_device_t *dev, IRP *irp);
NTSTATUS dispatch_ioctl(libusb_device_t *dev, IRP *irp);

NTSTATUS complete_irp(IRP *irp, NTSTATUS status, ULONG info);

#define call_usbd(dev, urb, control_code, timeout) \
	call_usbd_ex(dev, urb, control_code, timeout, LIBUSB_MAX_CONTROL_TRANSFER_TIMEOUT)

NTSTATUS call_usbd_ex(libusb_device_t *dev, 
					  void *urb, 
					  ULONG control_code, 
					  int timeout,
					  int max_timeout);

NTSTATUS pass_irp_down(libusb_device_t *dev, IRP *irp,
                       PIO_COMPLETION_ROUTINE completion_routine,
                       void *context);

bool_t accept_irp(libusb_device_t *dev, IRP *irp);

bool_t get_pipe_handle(libusb_device_t *dev, int endpoint_address,
                       USBD_PIPE_HANDLE *pipe_handle);

bool_t get_pipe_info(libusb_device_t *dev, int endpoint_address,
                       libusb_endpoint_t** pipe_info);

void clear_pipe_info(libusb_device_t *dev);
bool_t update_pipe_info(libusb_device_t *dev,
                        USBD_INTERFACE_INFORMATION *interface_info);

void remove_lock_initialize(libusb_device_t *dev);
NTSTATUS remove_lock_acquire(libusb_device_t *dev);
void remove_lock_release(libusb_device_t *dev);
void remove_lock_release_and_wait(libusb_device_t *dev);

NTSTATUS set_configuration(libusb_device_t *dev,
                           int configuration, int timeout);
NTSTATUS auto_configure(libusb_device_t *dev);

NTSTATUS get_configuration(libusb_device_t *dev,
                           unsigned char *configuration, int *ret,
                           int timeout);

NTSTATUS set_feature(libusb_device_t *dev,
                     int recipient, int index, int feature, int timeout);
NTSTATUS clear_feature(libusb_device_t *dev,
                       int recipient, int index, int feature, int timeout);
NTSTATUS get_status(libusb_device_t *dev, int recipient,
                    int index, char *status, int *ret, int timeout);
NTSTATUS set_descriptor(libusb_device_t *dev,
                        void *buffer, int size,
                        int type, int recipient, int index, int language_id,
                        int *sent, int timeout);
NTSTATUS get_descriptor(libusb_device_t *dev, void *buffer, int size,
                        int type, int recipient, int index, int language_id,
                        int *received, int timeout);

PUSB_CONFIGURATION_DESCRIPTOR get_config_descriptor(
	libusb_device_t *dev,
	int value,
	int *size,
	int* index);

NTSTATUS vendor_class_request(libusb_device_t *dev,
                              int type, int recipient,
                              int request, int value, int index,
                              void *buffer, int size, int direction,
                              int *ret, int timeout);

NTSTATUS abort_endpoint(libusb_device_t *dev, int endpoint, int timeout);
NTSTATUS reset_endpoint(libusb_device_t *dev, int endpoint, int timeout);
NTSTATUS reset_device(libusb_device_t *dev, int timeout);

#define USB_RESET_TYPE_RESET_PORT (1 << 0)
#define USB_RESET_TYPE_CYCLE_PORT (1 << 1)
#define USB_RESET_TYPE_FULL_RESET (USB_RESET_TYPE_CYCLE_PORT | USB_RESET_TYPE_RESET_PORT)
NTSTATUS reset_device_ex(libusb_device_t *dev, int timeout, unsigned int reset_type);

bool_t reg_get_hardware_id(DEVICE_OBJECT *physical_device_object,
                           char *data, int size);
bool_t reg_get_compatible_id(DEVICE_OBJECT *physical_device_object,
                           char *data, int size);

bool_t reg_get_properties(libusb_device_t *dev);


void power_set_device_state(libusb_device_t *dev,
                            DEVICE_POWER_STATE device_state, bool_t block);

USB_INTERFACE_DESCRIPTOR *
find_interface_desc(USB_CONFIGURATION_DESCRIPTOR *config_desc,
                    unsigned int size, int interface_number, int altsetting);

#define FIND_INTERFACE_INDEX_ANY		(-1)
USB_INTERFACE_DESCRIPTOR* find_interface_desc_ex(USB_CONFIGURATION_DESCRIPTOR *config_desc,
												 unsigned int size,
												 interface_request_t* intf,
												 unsigned int* size_left);

USB_ENDPOINT_DESCRIPTOR *
find_endpoint_desc_by_index(USB_INTERFACE_DESCRIPTOR *interface_desc,
                    unsigned int size, int pipe_index);

/*
Gets a device property for the device_object.

Returns: NTSTATUS code from IoGetDeviceProperty 
         STATUS_INVALID_PARAMETER
*/
NTSTATUS reg_get_device_property(PDEVICE_OBJECT device_object,
							   int property, 
							   char* data_buffer,
							   int data_length,
							   int* actual_length);

NTSTATUS reg_get_custom_property(PDEVICE_OBJECT device_object,
								 char *data_buffer, 
								 unsigned int data_length, 
								 unsigned int name_offset, 
								 int* actual_length);


NTSTATUS transfer(libusb_device_t* dev,
				  IN PIRP irp,
				  IN int direction,
				  IN int urbFunction,
				  IN libusb_endpoint_t* endpoint,
				  IN int packetSize,
				  IN int transferFlags,
				  IN int isoLatency,
				  IN PMDL mdlAddress,
				  IN int totalLength);

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
						IN int totalLength);

ULONG get_current_frame(IN PDEVICE_EXTENSION dev, IN PIRP Irp);


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
						 USHORT length);

NTSTATUS claim_interface(libusb_device_t *dev, FILE_OBJECT *file_object,
                         int interface);

NTSTATUS claim_interface_ex(libusb_device_t *dev, 
							  FILE_OBJECT *file_object, 
							  interface_request_t* interface_request);

NTSTATUS release_all_interfaces(libusb_device_t *dev,
                                FILE_OBJECT *file_object);

NTSTATUS release_interface(libusb_device_t *dev, FILE_OBJECT *file_object,
                           int interface);

NTSTATUS release_interface_ex(libusb_device_t *dev, 
							  FILE_OBJECT *file_object, 
							  interface_request_t* interface_request);

NTSTATUS set_interface(libusb_device_t *dev,
					   int interface_number, 
					   int alt_interface_number,
                       int timeout);

NTSTATUS set_interface_ex(libusb_device_t *dev, 
					   interface_request_t* interface_request, 
                       int timeout);

NTSTATUS get_interface(libusb_device_t *dev,
					   int interface_number, 
					   unsigned char *altsetting,
					   int timeout);

NTSTATUS get_interface_ex(libusb_device_t *dev, 
					   interface_request_t* interface_request, 
                       int timeout);

VOID set_filter_interface_key(libusb_device_t *dev, ULONG id);
#endif
