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


#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#if  defined(_WIN64)
#include <cfgmgr32.h>
#else
#include <ddk/cfgmgr32.h>
#endif
#else
#include <cfgmgr32.h>
#define strlwr(p) _strlwr(p)
#endif

#include "registry.h"
#include "error.h"

#define CLASS_KEY_PATH_NT "SYSTEM\\CurrentControlSet\\Control\\Class\\"
#define CLASS_KEY_PATH_9X "SYSTEM\\CurrentControlSet\\Services\\Class\\"

#define USB_GET_DRIVER_NAME() \
	usb_registry_is_nt() ? driver_name_nt : driver_name_9x;

#define DISP_CLASS(FilterClass) (strlen(FilterClass->class_name) ? FilterClass->class_name : FilterClass->class_guid)

static const char *driver_name_nt = "libusb0";
static const char *driver_name_9x = "libusb0.sys";

static const char *default_class_keys_nt[] =
{
	/* USB devices */
	"{36fc9e60-c465-11cf-8056-444553540000}",
	/* HID devices */
	"{745a17a0-74d3-11d0-b6fe-00a0c90f57da}",
	/* Network devices */
	"{4d36e972-e325-11ce-bfc1-08002be10318}",
	/* Image devices */
	"{6bdd1fc6-810f-11d0-bec7-08002be2092f}",
	/* Media devices */
	"{4d36e96c-e325-11ce-bfc1-08002be10318}",
	/* Modem devices */
	"{4d36e96d-e325-11ce-bfc1-08002be10318}",
	/* SmartCardReader devices*/
	"{50dd5230-ba8a-11d1-bf5d-0000f805f530}",
	NULL
};

static bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info,
											SP_DEVINFO_DATA *dev_info_data);

static bool_t usb_registry_get_filter_device_keys(filter_class_t* filter_class,
												  HDEVINFO dev_info,
												  SP_DEVINFO_DATA *dev_info_data,
												  filter_device_t** found);

static bool_t usb_registry_get_class_filter_keys(filter_class_t* filter_class,
												 HKEY reg_class_hkey);

bool_t usb_registry_is_nt(void)
{
	return GetVersion() < 0x80000000 ? TRUE : FALSE;
}

bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info,
								 SP_DEVINFO_DATA *dev_info_data,
								 char *buf, int size)
{
	DWORD reg_type;
	DWORD length = size;
	char *p = NULL;
	HKEY reg_key = NULL;

	memset(buf, 0, size);

	if (!SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, which,
		&reg_type, (BYTE *)buf, size, &length))
	{
		return FALSE;
	}

	return TRUE;
}

bool_t usb_registry_mz_to_sz(char* buf_mz, char new_separator)
{
	bool_t success = FALSE;

	while (buf_mz && *buf_mz)
	{
		success = TRUE;
		buf_mz += strlen(buf_mz);

		if (buf_mz[1]) 
			*buf_mz = new_separator;

		buf_mz++;
	}
	return success;
}

bool_t usb_registry_set_property(DWORD which, HDEVINFO dev_info,
								 SP_DEVINFO_DATA *dev_info_data,
								 char *buf, int size)
{
	char *val_name = NULL;
	char *p = NULL;
	HKEY reg_key;
	DWORD reg_type;

	switch (which)
	{
	case SPDRP_LOWERFILTERS:
		reg_type = usb_registry_is_nt() ? REG_MULTI_SZ : REG_SZ;
		val_name = "LowerFilters";
		break;
	case SPDRP_UPPERFILTERS:
		reg_type = usb_registry_is_nt() ? REG_MULTI_SZ : REG_SZ;
		val_name = "UpperFilters";
		break;
	default:
		return 0;
	}

	if (usb_registry_is_nt())
	{
		if (size > 2)
		{
			if (!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
				which, (BYTE *)buf, size))
			{
				USBERR("setting property '%s' failed", val_name);
				return FALSE;
			}
		}
		else
		{
			if (!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
				which, NULL, 0))
			{
				USBERR("deleting property '%s' failed", val_name);
				return FALSE;
			}
		}
	}
	else
	{
		p = buf;

		while (*p)
		{
			if (*p == ',')
			{
				*p = 0;
			}
			p += (strlen(p) + 1);
		}

		reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data,
			DICS_FLAG_GLOBAL,
			0, DIREG_DEV, KEY_ALL_ACCESS);

		if (reg_key == INVALID_HANDLE_VALUE)
		{
			USBERR0("reading registry key failed\n");
			return FALSE;
		}

		if (size > 3)
		{
			if (RegSetValueEx(reg_key, val_name, 0, reg_type, (BYTE *)buf,
				size) != ERROR_SUCCESS)
			{
				USBERR("setting property '%s' failed", val_name);
				RegCloseKey(reg_key);
				return FALSE;
			}
		}
		else
		{
			if (RegDeleteValue(reg_key, val_name) != ERROR_SUCCESS)
			{
				USBERR("deleting property '%s' failed", val_name);
				RegCloseKey(reg_key);
				return FALSE;
			}
		}
		RegCloseKey(reg_key);
	}

	return TRUE;
}

bool_t usb_registry_insert_class_filter(filter_context_t* filter_context)
{
	const char *driver_name;
	filter_class_t *key;
	char buf[MAX_PATH];

	driver_name = USB_GET_DRIVER_NAME();

	if (!filter_context->class_filters)
	{
		return TRUE;
	}

	key = filter_context->class_filters;

	while (key)
	{
		if (usb_registry_get_mz_value(key->name, "UpperFilters",
			buf, sizeof(buf)))
		{
			if (usb_registry_mz_string_find(buf, driver_name, TRUE))
			{
				key = key->next;
				continue;
			}
		}

		USBMSG("inserting class filter %s..\n", DISP_CLASS(key));


		usb_registry_mz_string_insert(buf, driver_name);

		if (!usb_registry_set_mz_value(key->name, "UpperFilters", buf,
			usb_registry_mz_string_size(buf)))
		{
			USBERR0("unable to set registry value\n");
		}
		else
		{
			key->action = FT_CLASS_UPPERFILTER;
			filter_context->class_filters_modified = TRUE;
		}

		key = key->next;
	}

	return TRUE;
}

bool_t usb_registry_remove_class_filter(filter_context_t* filter_context)
{
	const char *driver_name;
	filter_class_t *key;
	char buf[MAX_PATH];

	driver_name = USB_GET_DRIVER_NAME();

	if (!filter_context->class_filters)
	{
		return TRUE;
	}

	key = filter_context->class_filters;

	while (key)
	{
		if (usb_registry_get_mz_value(key->name, "UpperFilters",
			buf, sizeof(buf)))
		{
			if (usb_registry_mz_string_find(buf, driver_name, TRUE))
			{
				key->action = FT_CLASS_UPPERFILTER;
				filter_context->class_filters_modified = TRUE;

				USBMSG("removing class filter %s..\n",  DISP_CLASS(key));
				usb_registry_mz_string_remove(buf, driver_name, TRUE);

				if (!usb_registry_set_mz_value(key->name, "UpperFilters", buf, usb_registry_mz_string_size(buf)))
				{
					USBERR("failed removing class filter %s..\n",  DISP_CLASS(key));
				}
				else
				{
					key->action = FT_CLASS_UPPERFILTER;
					filter_context->class_filters_modified = TRUE;
				}
			}
		}

		key = key->next;
	}

	return TRUE;
}

bool_t usb_registry_remove_device_filter(filter_context_t* filter_context)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index = 0;
	char filters[MAX_PATH];
	char hwid[MAX_PATH];
	const char *driver_name;
	bool_t remove_device_filters;

	driver_name = USB_GET_DRIVER_NAME();

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	dev_index = 0;

	dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);

	if (dev_info == INVALID_HANDLE_VALUE)
	{
		USBERR0("getting device info set failed\n");
		return FALSE;
	}

	while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
		if (usb_registry_get_hardware_id(dev_info, &dev_info_data, hwid))
		{
			if (filter_context->remove_all_device_filters)
			{
				// remove all device upper/lower filters.
				remove_device_filters = TRUE;
			}
			else if (usb_registry_match_filter_device(&filter_context->device_filters, dev_info, &dev_info_data))
			{
				// if not, remove only the ones specified by the user.
				remove_device_filters = TRUE;
			}
			else
			{
				// skip device filter removal for this device.
				remove_device_filters = FALSE;
			}

			if (remove_device_filters)
			{
				/* remove libusb as a device upper filter */
				if (usb_registry_get_property(SPDRP_UPPERFILTERS, dev_info,
					&dev_info_data,
					filters, sizeof(filters)))
				{
					if (usb_registry_mz_string_find(filters, driver_name, TRUE))
					{
						int size;
						USBMSG("removing device upper filter %s..\n", hwid+4);

						usb_registry_mz_string_remove(filters, driver_name, TRUE);
						size = usb_registry_mz_string_size(filters);

						usb_registry_set_property(SPDRP_UPPERFILTERS, dev_info,
							&dev_info_data, filters, size);

						if (!filter_context->class_filters)
						{
							USBMSG("restarting device %s..\n", hwid+4);
							usb_registry_restart_device(dev_info, &dev_info_data);
						}
					}
				}

				/* remove libusb as a device lower filter */
				if (usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info,
					&dev_info_data,
					filters, sizeof(filters)))
				{
					if (usb_registry_mz_string_find(filters, driver_name, TRUE))
					{
						int size;
						USBMSG("removing device lower filter %s..\n", hwid+4);
						usb_registry_mz_string_remove(filters, driver_name, TRUE);
						size = usb_registry_mz_string_size(filters);

						usb_registry_set_property(SPDRP_LOWERFILTERS, dev_info,
							&dev_info_data, filters, size);

						if (!filter_context->class_filters)
						{
							USBMSG("restarting device %s..\n", hwid+4);
							usb_registry_restart_device(dev_info, &dev_info_data);
						}
					}
				}
			}
		}
		dev_index++;
	}

	SetupDiDestroyDeviceInfoList(dev_info);

	return TRUE;
}

bool_t usb_registry_fill_filter_hwid(const char* hwid, filter_hwid_t* filter_hwid)
{
	const char* hwid_part;

	if ((hwid_part = strstr(hwid, "vid_")))
		sscanf(hwid_part,"vid_%04x",&filter_hwid->vid);
	else
		filter_hwid->vid = -1;

	if ((hwid_part = strstr(hwid, "pid_")))
		sscanf(hwid_part,"pid_%04x",&filter_hwid->pid);
	else
		filter_hwid->pid = -1;

	if ((hwid_part = strstr(hwid, "mi_")))
		sscanf(hwid_part,"mi_%02x",&filter_hwid->mi);
	else
		filter_hwid->mi = -1;

	if ((hwid_part = strstr(hwid, "rev_")))
		sscanf(hwid_part,"rev_%04u",&filter_hwid->rev);
	else
		filter_hwid->rev = -1;

	return (filter_hwid->vid != -1 && filter_hwid->pid != -1);
}

filter_device_t* usb_registry_match_filter_device(filter_device_t** head, 
												  HDEVINFO dev_info, PSP_DEVINFO_DATA dev_info_data)
{
	filter_device_t* p = *head;
	char devid[MAX_PATH];
	char hwid[MAX_PATH];
	char hwid_find[MAX_PATH];
	filter_hwid_t find_hwid;
	filter_hwid_t search_hwid;

	if (CM_Get_Device_ID(dev_info_data->DevInst, devid, sizeof(devid), 0) != CR_SUCCESS)
	{
		USBERR0("failed getting device id\n");
		return NULL;
	}

	if (!usb_registry_get_hardware_id(dev_info, dev_info_data, hwid))
	{
		USBERR0("failed getting device id\n");
		return NULL;
	}

	_strlwr(hwid);
	if (!usb_registry_fill_filter_hwid(hwid, &find_hwid))
		return NULL;

	while(p)
	{
		if (strlen(p->device_id))
		{
			// matching on device instance id
			if (_stricmp(devid, p->device_id) == 0)
			{
				// found a match
				return p;
			}
		}
		else
		{
			// matching on id parts
			strcpy(hwid_find, p->device_hwid);
			_strlwr(hwid_find);
			if (usb_registry_fill_filter_hwid(hwid_find, &search_hwid))
			{
				if (find_hwid.vid == search_hwid.vid &&
					find_hwid.pid == search_hwid.pid && 
					((search_hwid.mi == -1) || (find_hwid.mi == search_hwid.mi)) &&
					((search_hwid.rev == -1) || (find_hwid.rev == search_hwid.rev)))
				{
					return p;
				}
			}
		}
		p = p->next;
	}

	return NULL;
}

bool_t usb_registry_remove_device_regvalue(HDEVINFO dev_info, SP_DEVINFO_DATA *dev_info_data, const char* key_name)
{
	HKEY reg_key;
	reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_ALL_ACCESS);
	if (reg_key)
	{
		if (RegDeleteValueA(reg_key, key_name) == ERROR_SUCCESS)
		{
			USBMSG("removed %s from device registry..\n", key_name);
			RegCloseKey(reg_key);
			return TRUE;
		}
		RegCloseKey(reg_key);
	}
	return FALSE;
}

bool_t usb_registry_insert_device_filter(filter_context_t* filter_context, char* hwid, bool_t upper, 
										 HDEVINFO dev_info, SP_DEVINFO_DATA *dev_info_data)
{

	const char *driver_name;
	DWORD spdrp_filters;
	char filters[MAX_PATH];
	int size;

	driver_name = USB_GET_DRIVER_NAME();
	spdrp_filters = upper ? SPDRP_UPPERFILTERS : SPDRP_LOWERFILTERS;

	if (usb_registry_get_property(spdrp_filters, dev_info, dev_info_data, filters, sizeof(filters)))
	{
		if (usb_registry_mz_string_find(filters, driver_name, TRUE))
		{
			if (usb_registry_remove_device_regvalue(dev_info, dev_info_data, "SurpriseRemovalOK"))
			{
				if (!filter_context->class_filters)
				{
					USBMSG("restarting device %s..\n", hwid+4);
					usb_registry_restart_device(dev_info, dev_info_data);
				}
			}
			return TRUE;
		}
	}
	USBMSG("inserting device %s filter %s..\n",
		upper ? "upper" : "lower",  hwid+4);

	if(usb_registry_mz_string_insert(filters, driver_name))
	{
		size = usb_registry_mz_string_size(filters);
		if (usb_registry_set_property(spdrp_filters, dev_info, dev_info_data, filters, size))
		{
			usb_registry_remove_device_regvalue(dev_info, dev_info_data, "SurpriseRemovalOK");

			if (!filter_context->class_filters)
			{
				USBMSG("restarting device %s..\n", hwid+4);
				usb_registry_restart_device(dev_info, dev_info_data);
			}
			return TRUE;
		}
	}

	return FALSE;
}

bool_t usb_registry_insert_device_filters(filter_context_t* filter_context)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index = 0;
	char hwid[MAX_PATH];
	const char *driver_name;
	filter_device_t* found;
	bool_t is_libusb_service;

	if (!filter_context->device_filters)
		return TRUE;

	driver_name = USB_GET_DRIVER_NAME();

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	dev_index = 0;

	dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);

	if (dev_info == INVALID_HANDLE_VALUE)
	{
		USBERR0("getting device info set failed\n");
		return FALSE;
	}

	while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
		if (usb_registry_is_service_or_filter_libusb(dev_info, &dev_info_data, &is_libusb_service))
		{
			if (!is_libusb_service)
			{
				if (usb_registry_get_property(SPDRP_HARDWAREID, dev_info, &dev_info_data, hwid, MAX_PATH))
				{
					if ((found=usb_registry_match_filter_device(&filter_context->device_filters, dev_info, &dev_info_data)))
					{
						if (!usb_registry_get_property(SPDRP_DEVICEDESC, dev_info,
							&dev_info_data,
							found->device_name, sizeof(found->device_name)))
						{
							USBWRN0("unable to get SPDRP_DEVICEDESC\n");
						}
						if (!usb_registry_get_property(SPDRP_MFG, dev_info,
							&dev_info_data,
							found->device_mfg, sizeof(found->device_mfg)))
						{
							USBWRN0("unable to get SPDRP_MFG\n");
						}

						if (found->action & FT_DEVICE_UPPERFILTER)
						{
							if (!usb_registry_insert_device_filter(filter_context, hwid, TRUE, dev_info, &dev_info_data))
							{
								USBERR("failed adding upper device filter for %s\n",found->device_hwid);
							}
						}
						if (found->action & FT_DEVICE_LOWERFILTER)
						{
							if (!usb_registry_insert_device_filter(filter_context, hwid, FALSE, dev_info, &dev_info_data))
							{
								USBERR("failed adding lower device filter for %s\n",found->device_hwid);
							}
						}
					}
				}
			}
		}
		dev_index++;
	}

	SetupDiDestroyDeviceInfoList(dev_info);

	return TRUE;
}
static bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info,
											SP_DEVINFO_DATA *dev_info_data)
{
	SP_PROPCHANGE_PARAMS prop_params;

	memset(&prop_params, 0, sizeof(SP_PROPCHANGE_PARAMS));

	prop_params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
	prop_params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
	prop_params.StateChange = state;
	prop_params.Scope = DICS_FLAG_CONFIGSPECIFIC;//DICS_FLAG_GLOBAL;
	prop_params.HwProfile = 0;


	if (!SetupDiSetClassInstallParams(dev_info, dev_info_data,
		(SP_CLASSINSTALL_HEADER *)&prop_params,
		sizeof(SP_PROPCHANGE_PARAMS)))
	{
		USBERR0("setting class install parameters failed\n");
		return FALSE;
	}


	if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, dev_info, dev_info_data))
	{
		USBERR0("calling class installer failed\n");
		return FALSE;
	}

	return TRUE;
}

bool_t usb_registry_restart_device(HDEVINFO dev_info,
								   SP_DEVINFO_DATA *dev_info_data)
{
	return usb_registry_set_device_state(DICS_PROPCHANGE, dev_info,
		dev_info_data);
}

bool_t usb_registry_stop_device(HDEVINFO dev_info,
								SP_DEVINFO_DATA *dev_info_data)
{
	return usb_registry_set_device_state(DICS_DISABLE, dev_info,
		dev_info_data);
}

bool_t usb_registry_start_device(HDEVINFO dev_info,
								 SP_DEVINFO_DATA *dev_info_data)
{
	return usb_registry_set_device_state(DICS_ENABLE, dev_info,
		dev_info_data);
}

bool_t usb_registry_get_device_filter_type(HDEVINFO dev_info,
										   SP_DEVINFO_DATA *dev_info_data,
										   filter_type_e* filter_type)
{
	char filters[MAX_PATH];
	const char* driver_name;

	*filter_type = FT_NONE;
	driver_name = USB_GET_DRIVER_NAME();

	if (usb_registry_get_property(SPDRP_UPPERFILTERS, dev_info,
		dev_info_data,
		filters, sizeof(filters)))
	{
		if (usb_registry_mz_string_find(filters, driver_name, TRUE))
		{
			*filter_type |= FT_DEVICE_UPPERFILTER;
		}
	}

	if (usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info,
		dev_info_data,
		filters, sizeof(filters)))
	{
		if (usb_registry_mz_string_find(filters, driver_name, TRUE))
		{
			*filter_type |= FT_DEVICE_LOWERFILTER;
		}
	}

	return TRUE;
}

bool_t usb_registry_is_service_libusb(HDEVINFO dev_info,
									  SP_DEVINFO_DATA *dev_info_data,
									  bool_t* is_libusb_service)
{
	char service_name[MAX_PATH];
	const char* driver_name;

	driver_name = USB_GET_DRIVER_NAME();
	*is_libusb_service = FALSE;
	if (!usb_registry_get_property(SPDRP_SERVICE, dev_info, dev_info_data,
		service_name, sizeof(service_name)))
	{
		return FALSE;
	}

	if (_stricmp(service_name, driver_name)==0)
	{
		*is_libusb_service = TRUE;
	}

	return TRUE;
}

bool_t usb_registry_is_service_or_filter_libusb(HDEVINFO dev_info,
												SP_DEVINFO_DATA *dev_info_data,
												bool_t* is_libusb_service)
{
	char service_name[MAX_PATH];
	const char* driver_name;
	filter_type_e filter_type;

	driver_name = USB_GET_DRIVER_NAME();
	*is_libusb_service = FALSE;
	if (!usb_registry_get_property(SPDRP_SERVICE, dev_info, dev_info_data,
		service_name, sizeof(service_name)))
	{
		return FALSE;
	}

	if (_stricmp(service_name, driver_name)==0)
	{
		*is_libusb_service = TRUE;
	}
	else if ((usb_registry_get_device_filter_type(dev_info, dev_info_data, &filter_type)) && filter_type != FT_NONE)
	{
		*is_libusb_service = TRUE;
	}

	return TRUE;
}

void usb_registry_stop_libusb_devices(void)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index = 0;
	bool_t is_libusb_service;

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	dev_index = 0;

	dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
		DIGCF_ALLCLASSES | DIGCF_PRESENT);

	if (dev_info == INVALID_HANDLE_VALUE)
	{
		USBERR0("getting device info set failed\n");
		return;
	}

	USBMSG0("stopping devices..\n");
	while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
		if (usb_registry_is_service_libusb(dev_info, &dev_info_data, &is_libusb_service))
		{
			if (is_libusb_service)
			{
				usb_registry_stop_device(dev_info, &dev_info_data);
			}
		}
		dev_index++;
	}

	SetupDiDestroyDeviceInfoList(dev_info);
}

void usb_registry_start_libusb_devices(void)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index = 0;
	bool_t is_libusb_service;

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	dev_index = 0;

	dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
		DIGCF_ALLCLASSES | DIGCF_PRESENT);

	if (dev_info == INVALID_HANDLE_VALUE)
	{
		USBERR0("getting device info set failed\n");
		return;
	}

	USBMSG0("starting devices..\n");
	while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
		if (usb_registry_is_service_libusb(dev_info, &dev_info_data, &is_libusb_service))
		{
			if (is_libusb_service)
			{
				usb_registry_start_device(dev_info, &dev_info_data);
			}
		}
		dev_index++;
	}

	SetupDiDestroyDeviceInfoList(dev_info);
}

bool_t usb_registry_get_hardware_id(HDEVINFO dev_info, 
									SP_DEVINFO_DATA *dev_info_data, 
									char* max_path_buffer)
{
	if (!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, dev_info_data, 
		max_path_buffer, MAX_PATH-1))
	{
		USBWRN0("failed\n");
		return FALSE;
	}
	max_path_buffer[MAX_PATH-1]='\0';
	return TRUE;
}

bool_t usb_registry_get_mz_value(const char *key, const char *value,
								 char *buf, int size)
{
	HKEY reg_key = NULL;
	DWORD reg_type;
	DWORD reg_length = size;
	bool_t ret = FALSE;
	char *p;

	memset(buf, 0, size);

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &reg_key)
		== ERROR_SUCCESS)
	{
		if (RegQueryValueEx(reg_key, value, NULL, &reg_type,
			buf, &reg_length) == ERROR_SUCCESS)
		{
			if (reg_type == REG_SZ)
			{
				p = buf;
				while (*p)
				{
					if (*p == ',')
					{
						*p = 0;
					}
					p++;
				}
			}

			ret = TRUE;
		}
	}

	if (reg_key)
	{
		RegCloseKey(reg_key);
	}

	return ret;
}


bool_t usb_registry_set_mz_value(const char *key, const char *value,
								 char *buf, int size)
{
	HKEY reg_key = NULL;
	bool_t ret = FALSE;
	char *p;

	/* convert REG_MULTI_SZ to REG_SZ */
	if (!usb_registry_is_nt())
	{
		p = buf;

		while (*p && *(p + 1))
		{
			if (*p == 0)
			{
				*p = ',';
			}
			p++;
		}
	}

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_ALL_ACCESS, &reg_key)
		== ERROR_SUCCESS)
	{
		if (size > 2)
		{
			if (usb_registry_is_nt())
			{
				if (RegSetValueEx(reg_key, value, 0, REG_MULTI_SZ, buf, size)
					== ERROR_SUCCESS)
				{
					ret = TRUE;
				}
			}
			else
			{
				if (RegSetValueEx(reg_key, value, 0, REG_SZ, buf, size)
					== ERROR_SUCCESS)
				{
					ret = TRUE;
				}
			}
		}
		else
		{
			if (RegDeleteValue(reg_key, value) == ERROR_SUCCESS)
			{
				ret = TRUE;
			}
		}
	}

	if (reg_key)
	{
		RegCloseKey(reg_key);
	}

	return ret;
}

int usb_registry_mz_string_size(const char *src)
{
	char *p = (char *)src;

	if (!src)
	{
		return 0;
	}

	while (*p)
	{
		p += (strlen(p) + 1);
	}

	return (int)(p - src) + 1;
}

char *usb_registry_mz_string_find_sub(const char *src, const char *str)
{
	while (*src)
	{
		if (strstr(src, str))
		{
			return (char *)src;
		}
		src += (strlen(src) + 1);
	}

	return NULL;
}

char *usb_registry_mz_string_find(const char *src, const char *str, bool_t no_case)
{
	int ret;
	while (*src)
	{
		if (no_case)
		{
			ret = _stricmp(src, str);
		}
		else
		{
			ret = strcmp(src, str);
		}
		if (!ret)
		{
			return (char *)src;
		}
		src += strlen(src) + 1;
	}

	return NULL;
}

bool_t usb_registry_mz_string_insert(char *src, const char *str)
{
	while (*src)
	{
		src += (strlen(src) + 1);
	}

	memcpy(src, str, strlen(str));

	src += strlen(str);

	*src = 0;
	*(src + 1) = 0;

	return TRUE;
}

bool_t usb_registry_mz_string_remove(char *src, const char *str, bool_t no_case)
{
	char *p;
	bool_t ret = FALSE;
	int size;

	do
	{
		src = usb_registry_mz_string_find(src, str, no_case);

		if (!src)
		{
			break;
		}
		else
		{
			ret = TRUE;
		}

		p = src;
		size = 0;

		while (*p)
		{
			p += strlen(p) + 1;
			size += (long)(strlen(p) + 1);
		}

		memmove(src, src + strlen(src) + 1, size);

	}
	while (1);

	return TRUE;
}

void usb_registry_mz_string_lower(char *src)
{
	while (*src)
	{
		strlwr(src);
		src += (strlen(src) + 1);
	}
}

bool_t usb_registry_restart_all_devices(void)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index;
	char id[MAX_PATH];
	int hub_index = 0;

	dev_index = 0;
	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

	dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
		DIGCF_ALLCLASSES | DIGCF_PRESENT);

	if (dev_info == INVALID_HANDLE_VALUE)
	{
		USBERR0("getting device info set failed\n");
		return FALSE;
	}

	USBMSG0("restarting devices..\n");

	while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
		if (!usb_registry_get_hardware_id(dev_info, &dev_info_data, id))
		{
			dev_index++;
			continue;
		}
		usb_registry_mz_string_lower(id);

		/* restart root hubs */
		if (usb_registry_mz_string_find_sub(id, "root_hub"))
		{
			USBMSG("restarting root hub #%d..\n",++hub_index);
			usb_registry_restart_device(dev_info, &dev_info_data);
		}

		dev_index++;
	}

	SetupDiDestroyDeviceInfoList(dev_info);

	return TRUE;
}

bool_t usb_registry_add_filter_device_keys(filter_device_t** head,
										   const char* id,
										   const char* hwid,
										   const char* name,
										   const char* mfg,
										   const char* uppers_mz,
										   const char* lowers_mz,
										   filter_device_t** found)
{
	filter_device_t *p = *head;
	*found = NULL;

	while (p)
	{
		if (strlen(id))
		{
			if (_stricmp(p->device_id, id)==0)
			{
				*found = p;
				break;
			}
		}
		p = p->next;
	}

	if (!(*found))
	{
		p = malloc(sizeof(filter_device_t));
		if (!p)
			return FALSE;

		memset(p, 0, sizeof(filter_device_t));

		*found = p;
		p->next = *head;
		*head = p;
	}

	strcpy(p->device_id, id);
	strcpy(p->device_hwid, hwid);

	if (strlen(name))
		strcpy(p->device_name, name);

	if (strlen(mfg))
		strcpy(p->device_mfg, mfg);

	if (strlen(uppers_mz))
		memcpy(p->device_uppers, uppers_mz, (size_t)usb_registry_mz_string_size(uppers_mz));
	
	if (strlen(lowers_mz))
		memcpy(p->device_lowers, lowers_mz, (size_t)usb_registry_mz_string_size(lowers_mz));

	return TRUE;
}

bool_t usb_registry_add_filter_file_keys(filter_file_t** head,
										 const char* name,
										 filter_file_t** found)
{
	filter_file_t *p = *head;
	*found = NULL;

	while (p)
	{
		if (_stricmp(p->name, name)==0)
		{
			*found = p;
			return TRUE;
		}
		p = p->next;
	}

	p = malloc(sizeof(filter_file_t));

	if (!p)
		return FALSE;

	memset(p, 0, sizeof(filter_file_t));

	strcpy(p->name, name);

	*found = p;
	p->next = *head;
	*head = p;

	return TRUE;
}

static bool_t usb_registry_get_filter_device_keys(filter_class_t* filter_class,
												  HDEVINFO dev_info,
												  SP_DEVINFO_DATA *dev_info_data,
												  filter_device_t** found)
{

	char id[MAX_PATH];
	char hwid[MAX_PATH];
	char name[MAX_PATH];
	char mfg[MAX_PATH];
	char lowers_mz[MAX_PATH];
	char uppers_mz[MAX_PATH];

	*found = NULL;
	if (dev_info && filter_class)
	{
		if (CM_Get_Device_ID(dev_info_data->DevInst, id, sizeof(id), 0) != CR_SUCCESS)
		{
			USBWRN0("unable to get device instance id\n");
			return FALSE;
		}

		if (!usb_registry_get_property(SPDRP_HARDWAREID, dev_info,
			dev_info_data,
			hwid, sizeof(hwid)))
		{
			USBWRN0("unable to get SPDRP_HARDWAREID\n");
			return FALSE;
		}

		if (!usb_registry_get_property(SPDRP_DEVICEDESC, dev_info,
			dev_info_data,
			name, sizeof(name)))
		{
			USBWRN0("unable to get SPDRP_DEVICEDESC\n");
		}
		if (!usb_registry_get_property(SPDRP_MFG, dev_info,
			dev_info_data,
			mfg, sizeof(mfg)))
		{
			USBWRN0("unable to get SPDRP_MFG\n");
		}

		usb_registry_get_property(SPDRP_UPPERFILTERS, dev_info, dev_info_data, uppers_mz, sizeof(uppers_mz));
		usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info, dev_info_data, lowers_mz, sizeof(lowers_mz));

		return usb_registry_add_filter_device_keys(&filter_class->class_filter_devices,
			id, hwid, name, mfg, uppers_mz, lowers_mz, found);
	}

	return FALSE;
}
bool_t usb_registry_add_usb_class_key(filter_context_t* filter_context, const char* class_guid)
{
	char tmp[MAX_PATH];
	const char *class_path = CLASS_KEY_PATH_NT;
	filter_class_t* found = NULL;

	if ((strlen(class_path) + strlen(class_guid)) < sizeof(tmp))
	{
		sprintf(tmp, "%s%s", class_path, class_guid);
		return usb_registry_add_class_key(&filter_context->class_filters, tmp, "", class_guid, &found, FALSE);
	}
	return FALSE;
}

bool_t usb_registry_get_usb_class_keys(filter_context_t* filter_context, bool_t refresh_only)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index = 0;
	int i;
	char class[MAX_PATH];
	char class_name[MAX_PATH];
	char tmp[MAX_PATH];
	DWORD class_property;
	const char *class_path;
	const char **default_class_keys;
	filter_class_t* found = NULL;
	filter_device_t* found_device = NULL;
	bool_t is_libusb_service;
	bool_t add_device_classes = FALSE;
	bool_t success;

	class_property = SPDRP_CLASSGUID;
	class_path = CLASS_KEY_PATH_NT;
	default_class_keys = default_class_keys_nt;
	i = 0;

	if (filter_context->switches.add_default_classes)
	{
		while (default_class_keys[i])
		{
			if ((strlen(class_path) + strlen(default_class_keys[i])) < sizeof(tmp))
			{
				sprintf(tmp, "%s%s", class_path, default_class_keys[i]);
				usb_registry_add_class_key(&filter_context->class_filters, tmp, "", default_class_keys[i], &found, FALSE);
			}
			i++;
		}
	}
	if (filter_context->filter_mode == FM_INSTALL)
		add_device_classes = filter_context->switches.add_device_classes | filter_context->switches.add_all_classes;
	else if (filter_context->filter_mode == FM_LIST)
		add_device_classes = filter_context->switches.add_device_classes;
	else if (filter_context->filter_mode == FM_REMOVE)
		return TRUE;

	if (add_device_classes || refresh_only)
	{
		dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
		dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
			DIGCF_ALLCLASSES);

		if (dev_info == INVALID_HANDLE_VALUE)
		{
			USBERR0("getting device info set failed\n");
			return FALSE;
		}

		while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
		{
			if (filter_context->filter_mode == FM_INSTALL)
				success = usb_registry_is_service_or_filter_libusb(dev_info, &dev_info_data, &is_libusb_service);
			else
				success = usb_registry_is_service_libusb(dev_info, &dev_info_data, &is_libusb_service);

			if (success)
			{
				if (!is_libusb_service)
				{
					if (!usb_registry_get_property(SPDRP_CLASSGUID, dev_info, &dev_info_data, class, sizeof(class)))
					{
						dev_index++;
						continue;
					}

					strlwr(class);

					usb_registry_get_property(SPDRP_CLASS, dev_info, &dev_info_data, class_name, sizeof(class_name));

					if ((strlen(class_path) + strlen(class)) < sizeof(tmp))
					{
						sprintf(tmp, "%s%s", class_path, class);
						usb_registry_add_class_key(&filter_context->class_filters, tmp, class_name, class, &found,
							(add_device_classes) ? FALSE : refresh_only);

						if (found)
						{
							usb_registry_get_filter_device_keys(found, dev_info, &dev_info_data, &found_device);
						}
					}
				}
			}

			dev_index++;
		}

		SetupDiDestroyDeviceInfoList(dev_info);
	}

	return TRUE;
}

bool_t usb_registry_get_all_class_keys(filter_context_t* filter_context, bool_t refresh_only)
{
	const char *class_path;
	HKEY reg_key, reg_class_key;
	char class[MAX_PATH];
	char class_name[MAX_PATH];
	char tmp[MAX_PATH];
	filter_class_t* found = NULL;
	DWORD reg_type;
	bool_t add_all_classes = FALSE;

	switch(filter_context->filter_mode)
	{
	case FM_INSTALL:
		add_all_classes = FALSE;
		break;
	case FM_REMOVE:
		if (!refresh_only && (filter_context->switches.add_all_classes || filter_context->switches.add_device_classes))
		{
			add_all_classes = TRUE;
		}
		break;
	case FM_LIST:
		add_all_classes = filter_context->switches.add_all_classes;
		break;
	}

	if (add_all_classes || refresh_only)
	{
		if (usb_registry_is_nt())
		{
			class_path = CLASS_KEY_PATH_NT;
		}
		else
		{
			class_path = CLASS_KEY_PATH_9X;
		}

		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, class_path, 0, KEY_ALL_ACCESS, &reg_key) == ERROR_SUCCESS)
		{
			DWORD i = 0;
			DWORD size = sizeof(class);
			FILETIME junk;

			memset(class, 0, sizeof(class));

			while (RegEnumKeyEx(reg_key, i, class, &size, 0, NULL, NULL, &junk) == ERROR_SUCCESS)
			{
				strlwr(class);

				if ((strlen(class_path) + strlen(class)) < sizeof(tmp))
				{
					memset(class_name,0,sizeof(class_name));
					sprintf(tmp, "%s%s", class_path, class);
					if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, tmp, 0, KEY_ALL_ACCESS, &reg_class_key) == ERROR_SUCCESS)
					{
						size = sizeof(class_name);
						RegQueryValueExA(reg_class_key, "Class", NULL, &reg_type, class_name, &size);

						usb_registry_add_class_key(&filter_context->class_filters, tmp, class_name, class, &found,
							(add_all_classes) ? FALSE : refresh_only);

						if (found)
						{
							usb_registry_get_class_filter_keys(found, reg_class_key);
						}
						RegCloseKey(reg_class_key);
					}
				}

				memset(class, 0, sizeof(class));
				size = sizeof(class);
				i++;
			}

			RegCloseKey(reg_key);
		}
	}
	return TRUE;
}

bool_t usb_registry_lookup_class_keys_by_name(filter_class_t** head)
{
	const char *class_path;
	HKEY reg_key, reg_class_key;
	char class[MAX_PATH];
	char class_name[MAX_PATH];
	char tmp[MAX_PATH];
	filter_class_t* found = NULL;
	DWORD reg_type;

	if (usb_registry_is_nt())
	{
		class_path = CLASS_KEY_PATH_NT;
	}
	else
	{
		class_path = CLASS_KEY_PATH_9X;
	}

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, class_path, 0, KEY_ALL_ACCESS,
		&reg_key) == ERROR_SUCCESS)
	{
		DWORD i = 0;
		DWORD size = sizeof(class);
		FILETIME junk;

		memset(class, 0, sizeof(class));

		while (RegEnumKeyEx(reg_key, i, class, &size, 0, NULL, NULL, &junk) == ERROR_SUCCESS)
		{
			strlwr(class);

			if ((strlen(class_path) + strlen(class)) < sizeof(tmp))
			{
				memset(class_name,0,sizeof(class_name));
				sprintf(tmp, "%s%s", class_path, class);
				if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, tmp, 0, KEY_ALL_ACCESS, &reg_class_key) == ERROR_SUCCESS)
				{
					size = sizeof(class_name);
					RegQueryValueExA(reg_class_key, "Class", NULL, &reg_type, class_name, &size);
					RegCloseKey(reg_class_key);

					usb_registry_add_class_key(head, tmp, class_name, class, &found, TRUE);
				}
			}

			memset(class, 0, sizeof(class));
			size = sizeof(class);
			i++;
		}

		RegCloseKey(reg_key);
	}

	return TRUE;
}

bool_t usb_registry_add_class_key(filter_class_t **head,
								  const char *key,
								  const char *class_name,
								  const char *class_guid,
								  filter_class_t **found,
								  bool_t update_only)
{
	filter_class_t *p = *head;
	*found = NULL;
	if (key)
	{

		if (strlen(key) >= MAX_PATH)
			return FALSE;

		while (p)
		{
			if (!strlen(p->name))
			{
				if (!_stricmp(p->class_name, class_name))
				{
					*found = p;
				}
			}
			else
			{
				if (!_stricmp(p->name, key))
				{
					*found = p;
				}
			}
			if (*found)
			{
				strcpy(p->name, key);
				strcpy(p->class_guid, class_guid);
				strcpy(p->class_name, class_name);

				return TRUE;
			}
			p = p->next;
		}

		if (update_only)
			return TRUE;

		p = malloc(sizeof(filter_class_t));

		if (!p)
			return FALSE;

		memset(p, 0, sizeof(filter_class_t));
		strcpy(p->name, key);
		strcpy(p->class_guid, class_guid);
		strcpy(p->class_name, class_name);

		*found = p;
		p->next = *head;
		*head = p;

		return TRUE;
	}

	return FALSE;
}

bool_t usb_registry_free_class_keys(filter_class_t **head)
{
	filter_class_t *p = *head;
	filter_class_t *q;

	while (p)
	{
		q = p->next;
		usb_registry_free_filter_devices(&p->class_filter_devices);
		free(p);
		p = q;
	}

	*head = NULL;

	return TRUE;
}

bool_t usb_registry_free_filter_files(filter_file_t **head)
{
	filter_file_t *p = *head;
	filter_file_t *q;

	while (p)
	{
		q = p->next;
		free(p);
		p = q;
	}

	*head = NULL;

	return TRUE;
}

bool_t usb_registry_free_filter_devices(filter_device_t **head)
{
	filter_device_t *p = *head;
	filter_device_t *q;

	while (p)
	{
		q = p->next;
		free(p);
		p = q;
	}

	*head = NULL;

	return TRUE;
}

static bool_t usb_registry_get_class_filter_keys(filter_class_t* filter_class, HKEY reg_class_hkey)
{
	DWORD reg_type;
	DWORD size;

	// Get the class filters. A non-existent key means no filters
	size = sizeof(filter_class->class_uppers) - 1;
	if (RegQueryValueExA(reg_class_hkey, "UpperFilters", NULL, &reg_type, filter_class->class_uppers, &size) != ERROR_SUCCESS)
	{
		memset(filter_class->class_uppers, 0, sizeof(filter_class->class_uppers));
	}

	size = sizeof(filter_class->class_lowers) - 1;
	if (RegQueryValueExA(reg_class_hkey, "LowerFilters", NULL, &reg_type, filter_class->class_lowers, &size) != ERROR_SUCCESS)
	{
		memset(filter_class->class_lowers, 0, sizeof(filter_class->class_lowers));
	}
	return TRUE;
}
