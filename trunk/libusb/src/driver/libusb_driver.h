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


#ifndef __LIBUSB_FILTER_H__
#define __LIBUSB_FILTER_H__


#include <ddk/usb.h>
#include <ddk/usbioctl.h>

#include "usbdlib.h"
#include "driver_api.h"


/* some define and typedefs that are missing in mingw's ddk headers */

#define URB_FUNCTION_RESET_PIPE 0x001E


typedef struct { 
  UCHAR  bLength;
  UCHAR  bDescriptorType;
  USHORT wTotalLength;
  UCHAR  bNumInterfaces;
  UCHAR  bConfigurationValue; /* added, missing in mingw's ddk header */
  UCHAR  iConfiguration;
  UCHAR  bmAttributes;
  UCHAR  MaxPower;
} USB_CONFIGURATION_DESCRIPTOR_PATCHED;

#define USB_CONFIGURATION_DESCRIPTOR USB_CONFIGURATION_DESCRIPTOR_PATCHED


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

#define LIBUSB_DEFAULT_TIMEOUT  5000   


typedef struct
{
  long usage_count;
  int remove_pending;
  KEVENT event;
} libusb_remove_lock;

typedef struct
{
  int address;
  USBD_PIPE_HANDLE handle;
} libusb_endpoint_info;

typedef struct
{
  int valid;
  int claimed;
  libusb_endpoint_info endpoints[LIBUSB_MAX_NUMBER_OF_ENDPOINTS];
} libusb_interface_info;

typedef struct
{
  DEVICE_OBJECT	*self;
  DEVICE_OBJECT	*physical_device_object;
  DEVICE_OBJECT	*next_stack_device;
  DEVICE_OBJECT *control_device_object;
  DEVICE_OBJECT *main_device_object;
  DRIVER_OBJECT *driver_object;
  LONG is_control_object;
  LONG ref_count;
  LONG is_started;
  libusb_remove_lock remove_lock; 
  USBD_CONFIGURATION_HANDLE configuration_handle;
  int current_configuration;
  int device_id;
  libusb_interface_info interface_info[LIBUSB_MAX_NUMBER_OF_INTERFACES];
} libusb_device_extension;


void __stdcall unload(DRIVER_OBJECT *driver_object);
NTSTATUS __stdcall add_device(DRIVER_OBJECT *driver_object, 
                              DEVICE_OBJECT *physical_device_object);
NTSTATUS __stdcall dispatch(DEVICE_OBJECT *device_object, IRP *irp);
NTSTATUS dispatch_control(DEVICE_OBJECT *device_object, IRP *irp);
NTSTATUS dispatch_pnp(libusb_device_extension *device_extension, IRP *irp);
NTSTATUS dispatch_power(libusb_device_extension *device_extension, IRP *irp);
NTSTATUS dispatch_ioctl(libusb_device_extension *device_extension, IRP *irp);
NTSTATUS dispatch_internal_ioctl(libusb_device_extension *device_extension, 
                                 IRP *irp);
NTSTATUS complete_irp(IRP *irp, NTSTATUS status, ULONG info);

NTSTATUS call_usbd(libusb_device_extension *device_extension, void *urb,
                   ULONG control_code, int timeout);

int get_pipe_handle(libusb_device_extension *device_extension, 
                    int endpoint_address, USBD_PIPE_HANDLE *pipe_handle);
void clear_pipe_info(libusb_device_extension *device_extension);
int update_pipe_info(libusb_device_extension *device_extension, int interface,
                     USBD_INTERFACE_INFORMATION *interface_info);

int is_device_visible(libusb_device_extension *device_extension);
NTSTATUS control_object_create(libusb_device_extension *device_extension);
void control_object_delete(libusb_device_extension *device_extension);

void remove_lock_initialize(libusb_remove_lock *remove_lock);
NTSTATUS remove_lock_acquire(libusb_remove_lock *remove_lock);
void remove_lock_release(libusb_remove_lock *remove_lock);
void remove_lock_release_and_wait(libusb_remove_lock *remove_lock);

int get_device_id(libusb_device_extension *device_extension);
void release_device_id(libusb_device_extension *device_extension);

NTSTATUS set_configuration(libusb_device_extension *device_extension,
                           int configuration, int timeout);
NTSTATUS get_configuration(libusb_device_extension *device_extension,
                           char *configuration, int *ret, int timeout);
NTSTATUS set_interface(libusb_device_extension *device_extension,
                       int interface, int altsetting, int timeout);
NTSTATUS get_interface(libusb_device_extension *device_extension,
                       int interface, char *altsetting, int *ret, int timeout);
NTSTATUS set_feature(libusb_device_extension *device_extension,
                     int recipient, int index, int feature, int timeout);
NTSTATUS clear_feature(libusb_device_extension *device_extension,
                       int recipient, int index, int feature, int timeout);
NTSTATUS get_status(libusb_device_extension *device_extension, int recipient,
                    int index, char *status, int *ret, int timeout);
NTSTATUS set_descriptor(libusb_device_extension *device_extension,
                        void *buffer, int size, 
                        int type, int index, int language_id, 
                        int *sent, int timeout);
NTSTATUS get_descriptor(libusb_device_extension *device_extension,
                        void *buffer, int size, int type, 
                        int index, int language_id, int *sent, int timeout);
NTSTATUS transfer(IRP *irp, libusb_device_extension *device_extension,
                  int direction, int urb_function, int endpoint, 
                  int packet_size, MDL *buffer, int size);

NTSTATUS vendor_class_request(libusb_device_extension *device_extension,
                              int type, int recipient,
                              int request, int value, int index,
                              void *buffer, int size, int direction,
                              int *sent, int timeout);
NTSTATUS abort_endpoint(libusb_device_extension *device_extension,
                        int endpoint, int timeout);
NTSTATUS reset_endpoint(libusb_device_extension *device_extension,
                        int endpoint, int timeout);
NTSTATUS reset_device(libusb_device_extension *device_extension, int timeout);

NTSTATUS claim_interface(libusb_device_extension *device_extension,
                         int interface);
NTSTATUS release_interface(libusb_device_extension *device_extension,
                           int interface);
int is_interface_claimed(libusb_device_extension *device_extension,
                         int interface);
int is_any_interface_claimed(libusb_device_extension *device_extension);

int is_endpoint_claimed(libusb_device_extension *device_extension,
                        USBD_PIPE_HANDLE *pipe_handle);
NTSTATUS release_all_interfaces(libusb_device_extension *device_extension);


void debug_print_nl(void);
void debug_set_level(int level);
void debug_printf(int level, char *format, ...);


#endif
