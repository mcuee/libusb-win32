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



#ifndef __USB_REGISTRY_H__
#define __USB_REGISTRY_H__

#include <windows.h>
#include <setupapi.h>

#define LIBUSB_STUB_NAME      "libusbst"

#define LIBUSB_DRIVER_NAME_NT "libusbfl"
#define LIBUSB_DRIVER_NAME_9X "libusbfl.sys"
#define LIBUSB_DRIVER_PATH    "system32\\drivers\\libusbfl.sys"

typedef int bool_t;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!(FALSE)) 
#endif

#define REGISTRY_BUF_SIZE 512

bool_t usb_registry_is_nt(void);

bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data,
				 char *buf, int size);
bool_t usb_registry_set_property(DWORD which, HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data, 
				 char *buf);

bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info, 
				    SP_DEVINFO_DATA *dev_info_data);

bool_t usb_registry_insert_filter(HDEVINFO dev_info,
				 SP_DEVINFO_DATA *dev_info_data,
				 const char *filter_name);
bool_t usb_registry_remove_filter(HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data,
				  const char *filter_name);

bool_t usb_registry_is_root_hub(HDEVINFO dev_info, 
			       SP_DEVINFO_DATA *dev_info_data);
bool_t usb_registry_is_composite_interface(HDEVINFO dev_info, 
					  SP_DEVINFO_DATA *dev_info_data);
bool_t usb_registry_is_service_libusb(HDEVINFO dev_info, 
				      SP_DEVINFO_DATA *dev_info_data);
bool_t usb_registry_is_composite_libusb(HDEVINFO dev_info, 
					SP_DEVINFO_DATA *dev_info_data);
bool_t usb_registry_is_device_present(const char *hardware_id);

#endif
