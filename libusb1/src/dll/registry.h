/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#ifndef __USB_REGISTRY_H__
#define __USB_REGISTRY_H__

#include <windows.h>
#include <setupapi.h>


typedef int bool_t;

typedef struct {
  int vid;
  int pid;
  char name[512];
  char manufacturer[512];
  char driver[512];
  char class[512];
  char class_guid[512];
  char winusb_guid[512];
} usb_registry_device_t;

bool_t usb_registry_get_device(usb_registry_device_t *dev, int index,
                               bool_t present_only);



#endif
