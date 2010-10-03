/* libusb-win32, Generic Windows USB Library
* Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
* Copyright (c) 2010 Travis Robinson <libusbdotnet@gmail.com>
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


#define LIBUSB_DRIVER_NAME_NT "libusb0"
#define LIBUSB_DRIVER_NAME_9X "libusb0.sys"

typedef int bool_t;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!(FALSE))
#endif

#define REGISTRY_BUF_SIZE 512

typedef struct _filter_file_t filter_file_t;
struct _filter_file_t
{
	filter_file_t* next;
	char name[MAX_PATH];
};

typedef int filter_mode_e;
enum _filter_mode_e
{
	FM_NONE    = 0,
	FM_LIST    = 1 << 0,
	FM_INSTALL = 1 << 1,
	FM_REMOVE  = 1 << 2,
};

typedef int filter_type_e;
enum _filter_type_e
{
	FT_NONE              = 0,
	FT_CLASS_UPPERFILTER = 1 << 0,
	FT_CLASS_LOWERFILTER = 1 << 1,
	FT_DEVICE_UPPERFILTER = 1 << 2,
	FT_DEVICE_LOWERFILTER = 1 << 3,
};

typedef struct _filter_hwid_t filter_hwid_t;
struct _filter_hwid_t
{
	int vid;
	int pid;
	int mi;
	int rev;
};

typedef struct _filter_device_t filter_device_t;
struct _filter_device_t
{
	filter_device_t* next;

	char device_name[MAX_PATH];
	char device_hwid[MAX_PATH];
	char device_mfg[MAX_PATH];
	char device_uppers[MAX_PATH];
	char device_lowers[MAX_PATH];
	char device_id[MAX_PATH];

	filter_type_e action;
};

typedef struct _filter_class_t  filter_class_t;
struct _filter_class_t
{
	filter_class_t* next;

	char name[MAX_PATH]; // key

	char class_name[MAX_PATH];
	char class_guid[MAX_PATH];
	char class_uppers[MAX_PATH];
	char class_lowers[MAX_PATH];
	filter_device_t* class_filter_devices;
	filter_type_e action;
};

typedef struct _filter_context_t filter_context_t;
struct _filter_context_t
{
	union
	{
		int switches_value;
		struct 
		{
			bool_t           add_all_classes:1;
			bool_t           add_device_classes:1;
			bool_t           add_default_classes:1;
		};
	}switches;

	filter_mode_e       filter_mode;
	filter_class_t*     class_filters;
	filter_device_t*    device_filters;
	filter_file_t*      inf_files;
	bool_t				show_help_only;
	bool_t              remove_all_device_filters;
	bool_t				class_filters_modified;
	char*				prompt_string;
	char*				wait_string;
};

bool_t usb_registry_is_nt(void);

bool_t usb_registry_restart_device(HDEVINFO dev_info,
								   SP_DEVINFO_DATA *dev_info_data);
bool_t usb_registry_stop_device(HDEVINFO dev_info,
								SP_DEVINFO_DATA *dev_info_data);
bool_t usb_registry_start_device(HDEVINFO dev_info,
								 SP_DEVINFO_DATA *dev_info_data);

bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info,
								 SP_DEVINFO_DATA *dev_info_data,
								 char *buf, int size);
bool_t usb_registry_set_property(DWORD which, HDEVINFO dev_info,
								 SP_DEVINFO_DATA *dev_info_data,
								 char *buf, int size);

bool_t usb_registry_restart_all_devices(void);


void usb_registry_stop_libusb_devices(void);
void usb_registry_start_libusb_devices(void);

bool_t usb_registry_get_mz_value(const char *key, const char *value,
								 char *buf, int size);
bool_t usb_registry_set_mz_value(const char *key, const char *value,
								 char *buf, int size);
int usb_registry_mz_string_size(const char *src);
char *usb_registry_mz_string_find(const char *src, const char *str, bool_t no_case);
char *usb_registry_mz_string_find_sub(const char *src, const char *str);
bool_t usb_registry_mz_string_insert(char *src, const char *str);
bool_t usb_registry_mz_string_remove(char *src, const char *str, bool_t no_case);
void usb_registry_mz_string_lower(char *src);

bool_t usb_registry_get_hardware_id(HDEVINFO dev_info, 
									SP_DEVINFO_DATA *dev_info_data, 
									char* max_path_buffer);
bool_t usb_registry_is_service_libusb(HDEVINFO dev_info,
									  SP_DEVINFO_DATA *dev_info_data,
									  bool_t* is_libusb_service);
bool_t usb_registry_is_service_or_filter_libusb(HDEVINFO dev_info,
												SP_DEVINFO_DATA *dev_info_data,
												bool_t* is_libusb_service);

bool_t usb_registry_insert_class_filter(filter_context_t* filter_context);
bool_t usb_registry_remove_class_filter(filter_context_t* filter_context);
bool_t usb_registry_remove_device_filter(filter_context_t* filter_context);
bool_t usb_registry_free_class_keys(filter_class_t **head);
bool_t usb_registry_get_usb_class_keys(filter_context_t* filter_context, bool_t refresh_only);
bool_t usb_registry_get_all_class_keys(filter_context_t* filter_context, bool_t refresh_only);
bool_t usb_registry_get_device_filter_type(HDEVINFO dev_info,
										   SP_DEVINFO_DATA *dev_info_data,
										   filter_type_e* filter_type);

bool_t usb_registry_add_usb_class_key(filter_context_t* filter_context, const char* class_guid);
bool_t usb_registry_add_filter_device_keys(filter_device_t** head,
										   const char* id,
										   const char* hwid,
										   const char* name,
										   const char* mfg,
										   const char* uppers_mz,
										   const char* lowers_mz,
										   filter_device_t** found);

bool_t usb_registry_add_filter_file_keys(filter_file_t** head,
										 const char* name,
										 filter_file_t** found);

bool_t usb_registry_lookup_class_keys_by_name(filter_class_t** head);
bool_t usb_registry_add_class_key(filter_class_t **head,
								  const char *key,
								  const char *class_name,
								  const char *class_guid,
								  filter_class_t **found,
								  bool_t update_only);

bool_t usb_registry_insert_device_filters(filter_context_t* filter_context);
bool_t usb_registry_insert_device_filter(filter_context_t* filter_context, char* hwid, bool_t upper, 
										 HDEVINFO dev_info, SP_DEVINFO_DATA *dev_info_data);

bool_t usb_registry_free_filter_devices(filter_device_t **head);
bool_t usb_registry_free_filter_files(filter_file_t **head);

filter_device_t* usb_registry_match_filter_device(filter_device_t** head, 
												  HDEVINFO dev_info, PSP_DEVINFO_DATA dev_info_data);

bool_t usb_registry_mz_to_sz(char* buf_mz, char separator);
bool_t usb_registry_fill_filter_hwid(const char* hwid, filter_hwid_t* filter_hwid);

#endif
