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


#ifndef __DRIVER_API_H__
#define __DRIVER_API_H__

#define MAX_READ_WRITE 64000

#define LIBUSB_IOCTL_SET_CONFIGURATION CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_GET_CONFIGURATION CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_SET_INTERFACE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x803, METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_GET_INTERFACE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x804, METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_SET_FEATURE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x805, METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_CLEAR_FEATURE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x806, METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_GET_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x807, METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_SET_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x808, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_GET_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x809, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_INTERRUPT_OR_BULK_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x80A, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_INTERRUPT_OR_BULK_READ CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x80B, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_VENDOR_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x80C, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_VENDOR_READ CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x80D, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_RESET_ENDPOINT CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_ABORT_ENDPOINT CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define LIBUSB_IOCTL_RESET_DEVICE CTL_CODE(FILE_DEVICE_UNKNOWN,\
0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)


#include <pshpack1.h> 


typedef struct
{
  int endpoint;
  unsigned int len;
  int timeout;
  void *data;
} usb_bulk_transfer;

typedef struct
{
  int request;
  int value;
  int index;
  int timeout;
} usb_vendor_request;

typedef struct
{
  int configuration;
  int timeout;
} usb_configuration_request;

typedef struct
{
  int interface;
  int altsetting;
  int timeout;
} usb_interface_request;

typedef struct
{
  int recipient;
  int feature;
  int index;
  int timeout;
} usb_feature_request;

typedef struct
{
  int recipient;
  int index;
  int status;
  int timeout;
} usb_status_request;

typedef struct
{
  int type;
  int index;
  int language_id;
  int timeout;
} usb_descriptor_request;

typedef struct
{
  int endpoint;
  int timeout;
} usb_pipe_request;

typedef struct
{
  int timeout;
} usb_device_request;


#include <poppack.h>

#endif
