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


#ifndef __LIBUSB_DRIVER_H__
#define __LIBUSB_DRIVER_H__

#include <ddk/usb100.h>
#include <ddk/usbdi.h>
#include <wchar.h>
#include <initguid.h>

#undef interface

#include "driver_api.h"
#include "usbdlib.h"

/* some missing defines */
#define USBD_TRANSFER_DIRECTION_OUT       0   
#define USBD_TRANSFER_DIRECTION_BIT       0
#define USBD_TRANSFER_DIRECTION_IN        (1 << USBD_TRANSFER_DIRECTION_BIT)
#define USBD_SHORT_TRANSFER_OK_BIT        1
#define USBD_SHORT_TRANSFER_OK            (1 << USBD_SHORT_TRANSFER_OK_BIT)
#define USBD_START_ISO_TRANSFER_ASAP_BIT  2
#define USBD_START_ISO_TRANSFER_ASAP   (1 << USBD_START_ISO_TRANSFER_ASAP_BIT)

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
#define LIBUSB_MAX_NUMBER_OF_CHILDREN   32


#define LIBUSB_DEFAULT_TIMEOUT  5000   


#ifdef DBG

#define DEBUG_PRINT_NL() \
  if(driver_globals.debug_level >= LIBUSB_DEBUG_MSG) KdPrint(("\n"))

#define DEBUG_SET_LEVEL(level) driver_globals.debug_level = level

#define DEBUG_MESSAGE(format, args...) \
  do { \
     if(LIBUSB_DEBUG_MSG <= driver_globals.debug_level) \
        KdPrint(("LIBUSB-DRIVER - " format, ## args)); \
     } while(0)

#define DEBUG_ERROR(format, args...) \
  do { \
     if(LIBUSB_DEBUG_ERR <= driver_globals.debug_level) \
        KdPrint(("LIBUSB-DRIVER - " format, ## args)); \
     } while(0)

#else

#define DEBUG_PRINT_NL()
#define DEBUG_SET_LEVEL(level)
#define DEBUG_MESSAGE(format, args...)
#define DEBUG_ERROR(format, args...)

#endif


typedef int bool_t;

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
} libusb_endpoint_info_t;

typedef struct
{
  int valid;
  int claimed;
  libusb_endpoint_info_t endpoints[LIBUSB_MAX_NUMBER_OF_ENDPOINTS];
} libusb_interface_info_t;

typedef struct 
{
  int id;
  int port;
} child_info_t;

typedef struct
{
  int bus;
  int port;
  int parent;
  int is_root_hub;
  int is_hub;
  int num_child_pdos;
  int num_children;
  int update_children;
  DEVICE_OBJECT *child_pdos[LIBUSB_MAX_NUMBER_OF_CHILDREN];
  child_info_t children[LIBUSB_MAX_NUMBER_OF_CHILDREN];
} libusb_topology_info_t;


typedef struct _libusb_device_t libusb_device_t;

struct _libusb_device_t
{
  libusb_device_t *next;
  DEVICE_OBJECT	*self;
  DEVICE_OBJECT	*physical_device_object;
  DEVICE_OBJECT	*next_stack_device;
  DEVICE_OBJECT	*next_device;
  libusb_remove_lock_t remove_lock; 
  USBD_CONFIGURATION_HANDLE configuration_handle;
  LONG ref_count;
  int is_filter;
  int id;
  int configuration;
  libusb_topology_info_t topology_info;
  libusb_interface_info_t interfaces[LIBUSB_MAX_NUMBER_OF_INTERFACES];
};

typedef struct 
{
  libusb_device_t *head;
  KSPIN_LOCK lock;
  KIRQL old_irql;
} device_list_t;

typedef struct {
  LONG bus_index;
  int debug_level;
  device_list_t device_list;
} driver_globals_t;

#ifdef __LIBUSB_DRIVER_C__
driver_globals_t driver_globals;
#else
extern driver_globals_t driver_globals;;
#endif


NTSTATUS DDKAPI dispatch(DEVICE_OBJECT *device_object, IRP *irp);
NTSTATUS dispatch_pnp(libusb_device_t *dev, IRP *irp);
NTSTATUS dispatch_power(libusb_device_t *dev, IRP *irp);
NTSTATUS dispatch_ioctl(libusb_device_t *dev, IRP *irp);

NTSTATUS complete_irp(IRP *irp, NTSTATUS status, ULONG info);

NTSTATUS call_usbd(libusb_device_t *dev, void *urb,
                   ULONG control_code, int timeout);
NTSTATUS pass_irp_down(libusb_device_t *dev, IRP *irp);

BOOL accept_irp(libusb_device_t *dev, IRP *irp);

int get_pipe_handle(libusb_device_t *dev, 
                    int endpoint_address, USBD_PIPE_HANDLE *pipe_handle);
void clear_pipe_info(libusb_device_t *dev);
int update_pipe_info(libusb_device_t *dev, int interface,
                     USBD_INTERFACE_INFORMATION *interface_info);

void remove_lock_initialize(libusb_remove_lock_t *remove_lock);
NTSTATUS remove_lock_acquire(libusb_remove_lock_t *remove_lock);
void remove_lock_release(libusb_remove_lock_t *remove_lock);
void remove_lock_release_and_wait(libusb_remove_lock_t *remove_lock);

NTSTATUS set_configuration(libusb_device_t *dev,
                           int configuration, int timeout);
NTSTATUS get_configuration(libusb_device_t *dev,
                           unsigned char *configuration, int *ret, 
                           int timeout);
NTSTATUS set_interface(libusb_device_t *dev,
                       int interface, int altsetting, int timeout);
NTSTATUS get_interface(libusb_device_t *dev,
                       int interface, unsigned char *altsetting, 
                       int *ret, int timeout);
NTSTATUS set_feature(libusb_device_t *dev,
                     int recipient, int index, int feature, int timeout);
NTSTATUS clear_feature(libusb_device_t *dev,
                       int recipient, int index, int feature, int timeout);
NTSTATUS get_status(libusb_device_t *dev, int recipient,
                    int index, char *status, int *ret, int timeout);
NTSTATUS set_descriptor(libusb_device_t *dev,
                        void *buffer, int size, 
                        int type, int index, int language_id, 
                        int *sent, int timeout);
NTSTATUS get_descriptor(libusb_device_t *dev,
                        void *buffer, int size, int type, 
                        int index, int language_id, int *sent, int timeout);
NTSTATUS transfer(IRP *irp, libusb_device_t *dev,
                  int direction, int urb_function, int endpoint, 
                  int packet_size, MDL *buffer, int size);

NTSTATUS vendor_class_request(libusb_device_t *dev,
                              int type, int recipient,
                              int request, int value, int index,
                              void *buffer, int size, int direction,
                              int *sent, int timeout);

NTSTATUS abort_endpoint(libusb_device_t *dev, int endpoint, int timeout);
NTSTATUS reset_endpoint(libusb_device_t *dev, int endpoint, int timeout);
NTSTATUS reset_device(libusb_device_t *dev, int timeout);

NTSTATUS claim_interface(libusb_device_t *dev, int interface);
NTSTATUS release_interface(libusb_device_t *dev, int interface);
NTSTATUS release_all_interfaces(libusb_device_t *dev);

NTSTATUS get_device_info(libusb_device_t *dev, libusb_request *request, 
                         int *ret);

int reg_is_usb_device(DEVICE_OBJECT *physical_device_object);
int reg_is_root_hub(DEVICE_OBJECT *physical_device_object);
int reg_is_hub(DEVICE_OBJECT *physical_device_object);
int reg_is_composite_interface(DEVICE_OBJECT *physical_device_object);
int reg_get_id(DEVICE_OBJECT *physical_device_object, char *data, int size);


void device_list_init(void);
void device_list_insert(libusb_device_t *dev);
void device_list_remove(libusb_device_t *dev);
void device_list_update_info(libusb_device_t *dev);

int reg_is_filter_driver(DEVICE_OBJECT *physical_device_object);


void power_set_device_state(libusb_device_t *dev, 
                            DEVICE_POWER_STATE device_state);


#endif
