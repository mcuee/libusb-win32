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

#include <ctype.h>
#include "registry.h"
#include "win_debug.h"

bool_t usb_registry_is_nt(void)
{
  return GetVersion() < 0x80000000 ? 1 : 0;
}

bool_t usb_registry_get_property(DWORD which, HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data,
				 char *buf, int size)
{
  DWORD reg_type;
  DWORD key_type;
  DWORD length = size;
  char *p = NULL;
  char *val_name = NULL;
  HKEY reg_key = NULL;
  
  memset(buf, 0, size);

  switch(which)
    {
    case SPDRP_LOWERFILTERS:
      val_name = "LowerFilters";
      key_type = DIREG_DEV; 
      break;
    case SPDRP_SERVICE:
      val_name = "NTMPDriver";
      key_type = DIREG_DRV;
      break;
    case SPDRP_CLASS:
      val_name = "Class";
      key_type = DIREG_DEV;    
      break;
    case SPDRP_HARDWAREID:
      val_name = "HardwareID";
      key_type = DIREG_DEV;    
      break;
    default:
      return FALSE;
    }

  if(usb_registry_is_nt())
    {
      if(!SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, which,  
					   &reg_type, (BYTE *)buf, 
					   size, &length))
	{
	  RegCloseKey(reg_key);
	  return FALSE;
	}

      if(length <= 2)
	{
	  return FALSE;
	}
    }
  else /* Win9x */
    {
      reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, 
				     DICS_FLAG_GLOBAL, 
				     0, key_type, KEY_ALL_ACCESS);
      
      if(reg_key == INVALID_HANDLE_VALUE)
	{
	  usb_debug_error("usb_reg_get_reg_property(): reading "
			  "registry key failed");
	  return FALSE;
	}
      
      if(RegQueryValueEx(reg_key, val_name, NULL, &reg_type, 
			 (BYTE *)buf, &length) != ERROR_SUCCESS
	 || length <= 2)
	{
	  RegCloseKey(reg_key);
	  return FALSE;
	}
    }
  p = buf;
  
  if((REG_MULTI_SZ == reg_type))
    {
      while(*p || *(p + 1))
	{
	  if(*p == 0)
	    {
	      *p = ',';
	    }
	  p++;
	}
    }
  
  p = buf;
  
  while(*p)
    {
      *p = tolower(*p);
      p++;
    }
  
  if(reg_key)
    {
      RegCloseKey(reg_key);
    }
  return TRUE;
}

bool_t usb_registry_set_property(DWORD which, HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data, 
				 char *buf)
{
  char *val_name = NULL;
  char *p = NULL;
  HKEY reg_key;
  DWORD reg_type;
  char tmp[REGISTRY_BUF_SIZE];
  DWORD size = 0;

  switch(which)
    {
    case SPDRP_LOWERFILTERS:
      reg_type = usb_registry_is_nt() ? REG_MULTI_SZ : REG_SZ;
      val_name = "LowerFilters";
      break;
    default:
      return 0;
    }


  memset(tmp, 0, sizeof(tmp));
  strncpy(tmp, buf, sizeof(tmp));

  if(reg_type == REG_MULTI_SZ)
    {
      p = tmp;

      while(*p && p < (tmp + sizeof(tmp) - 2))
	{
	  if(*p == ',')
	    {
	      *p = 0;
	    }
	  size++;
	  p++;
	}
      *p++ = 0;
      *p = 0;
    }

  size += reg_type == REG_MULTI_SZ ? 2 : 1;

  if(usb_registry_is_nt())
    {
      if(!SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
					   which, (BYTE *)tmp, size))
	{
	  usb_debug_error("usb_reg_set_reg_property(): setting "
			  "property '%s' failed", val_name);
	  return FALSE;
	}
    }
  else
    {
      reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, 
				     DICS_FLAG_GLOBAL, 
				     0, DIREG_DEV, KEY_ALL_ACCESS);
      
      if(reg_key == INVALID_HANDLE_VALUE)
	{
	  usb_debug_error("usb_reg_get_reg_property(): reading "
			  "registry key failed");
	  return FALSE;
	}
      
      if(RegSetValueEx(reg_key, val_name, 0, reg_type, (BYTE *)buf, 
		       size) != ERROR_SUCCESS)
	{
	  usb_debug_error("usb_reg_set_reg_property(): setting "
			  "property '%s' failed", val_name);
	  return FALSE;
	}
    }

  return TRUE;
}

bool_t usb_registry_insert_filter(HDEVINFO dev_info,
				  SP_DEVINFO_DATA *dev_info_data, 
				  const char *filter_name)
{
  char filters[REGISTRY_BUF_SIZE];
  char buffer[REGISTRY_BUF_SIZE];
  

  usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info, dev_info_data, 
			    filters, sizeof(filters));
  
  if(strstr(filters, filter_name))
    {
      return FALSE;
    }
      
  memset(buffer, 0, sizeof(buffer));
  
  if(strlen(filters))
    {
      strcpy(buffer, filters);
      strcat(buffer, ",");
    }
      
  strcat(buffer, filter_name);
  
  if(!usb_registry_set_property(SPDRP_LOWERFILTERS, dev_info, 
				dev_info_data, buffer))
    {
      return FALSE;
    }

  return TRUE;
}


bool_t usb_registry_remove_filter(HDEVINFO dev_info, 
				  SP_DEVINFO_DATA *dev_info_data, 
				  const char *filter_name)
{
  char *p = NULL;
  char filters[REGISTRY_BUF_SIZE];

  memset(filters, 0, sizeof(filters));

  if(!usb_registry_get_property(SPDRP_LOWERFILTERS, dev_info, 
				dev_info_data, filters, sizeof(filters)))
    {
      return FALSE;
    }

  p = strstr(filters, filter_name);
  
  if(p)
    {
      if(*(p + strlen(filter_name)))
	{
	  memmove(p, p + strlen(filter_name) + 1, 
		  strlen(p) - strlen(filter_name) + 1);
	}
      else
	{
	  *p = 0;
	}
    }
  else
    {
      return FALSE;
    }
      
  return usb_registry_set_property(SPDRP_LOWERFILTERS, dev_info, 
				   dev_info_data, filters);
}

bool_t usb_registry_set_device_state(DWORD state, HDEVINFO dev_info, 
				     SP_DEVINFO_DATA *dev_info_data)
{
  SP_PROPCHANGE_PARAMS params;
  
  memset(&params, 0, sizeof(SP_PROPCHANGE_PARAMS));
  
  params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
  params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
  params.StateChange = state;
  params.Scope = DICS_FLAG_CONFIGSPECIFIC;
  params.HwProfile = 0;
	  
  if(!SetupDiSetClassInstallParams(dev_info, dev_info_data,
				   (PSP_CLASSINSTALL_HEADER) &params,
				   sizeof(SP_PROPCHANGE_PARAMS)))
    {
      usb_debug_error("usb_registry_set_device_state(): setting class "
		      "install parameters failed");
      return FALSE;
    }
	  
  if(!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, dev_info, 
				dev_info_data))
    {
      usb_debug_error("usb_registry_set_device_state(): calling class "
		      "installer failed");
      return FALSE;
    }

  return TRUE;
}


bool_t usb_registry_is_root_hub(HDEVINFO dev_info, 
				SP_DEVINFO_DATA *dev_info_data)
{
  char hardware_id[REGISTRY_BUF_SIZE];
  
  if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, dev_info_data,
				hardware_id, sizeof(hardware_id)))
    {
      usb_debug_error("usb_registry_is_root_hub(): getting hardware id "
		      "failed");
      return FALSE;
    }
    
  if(strstr(hardware_id, "root_hub"))
    {
      return TRUE;
    }

  return FALSE;
}


bool_t usb_registry_is_composite_interface(HDEVINFO dev_info, 
					   SP_DEVINFO_DATA *dev_info_data)
{
  char hardware_id[REGISTRY_BUF_SIZE];
  
  if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, dev_info_data,
				hardware_id, sizeof(hardware_id)))
    {
      usb_debug_error("usb_registry_is_composite_interface(): getting "
		      "hardware id failed");
      return FALSE;
    }
    
  if(strstr(hardware_id, "&mi_"))
    {
      return TRUE;
    }

  return FALSE;
}

bool_t usb_registry_is_service_libusb(HDEVINFO dev_info, 
				      SP_DEVINFO_DATA *dev_info_data)
{
  char service_name[REGISTRY_BUF_SIZE];

  if(!usb_registry_get_property(SPDRP_SERVICE, dev_info, dev_info_data,
				service_name, sizeof(service_name)))
    {
      /* found device in the registry with VID_0000/PID_0000 which have */
      /* no service */
      return FALSE;
    }
 
   
  if(strstr(service_name, LIBUSB_STUB_NAME))
    {

      return TRUE;
    }

  return FALSE;
}

bool_t usb_registry_is_composite_libusb(HDEVINFO dev_info, 
					SP_DEVINFO_DATA *dev_info_data)
{
  HDEVINFO _dev_info;
  SP_DEVINFO_DATA _dev_info_data;
  int dev_index = 0;
  char parent[REGISTRY_BUF_SIZE];
  char tmp[REGISTRY_BUF_SIZE];
  
  memset(parent, 0, sizeof(parent));

  _dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  if(!usb_registry_get_property(SPDRP_HARDWAREID, dev_info, dev_info_data,
				tmp, sizeof(tmp)))
    {
      usb_debug_error("usb_registry_is_composite_libusb(): getting "
		      "hardware id failed");
      return FALSE;
    }
    
  if(strstr(tmp, "&mi_"))
    {
      return FALSE;
    }

  strncpy(parent, tmp, sizeof("usb\\vid_xxxx&pid_yyyy") - 1);
  strcat(parent, "&mi_");

 
  _dev_info = SetupDiGetClassDevs(NULL, "USB", 0, DIGCF_ALLCLASSES);
  
  if(_dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_is_composite_libusb(): getting "
		      "device info set failed");
      
      return FALSE;
    }
      
  while(SetupDiEnumDeviceInfo(_dev_info, dev_index, &_dev_info_data))
    {
      if(!usb_registry_get_property(SPDRP_HARDWAREID, _dev_info, 
				    &_dev_info_data, tmp, sizeof(tmp)))
	
	{
	  usb_debug_error("usb_registry_is_composite_libusb(): getting "
			  "hardware id failed");
	  dev_index++;
	  continue;
	}
	
      if(strstr(tmp, parent) 
	 && usb_registry_is_service_libusb(_dev_info, &_dev_info_data))
	{
	  return TRUE;
	}
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(_dev_info);
  return FALSE;

}

bool_t usb_registry_install_composite_filter(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;

  filter_name = usb_registry_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_info = SetupDiGetClassDevs(NULL, "USB", 0, DIGCF_ALLCLASSES);

  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_registry_install_composite_filter(): getting "
		      "device info set failed");
      return FALSE;
    }

  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_is_composite_libusb(dev_info, &dev_info_data))
	{
	  if(usb_registry_insert_filter(dev_info, &dev_info_data, 
					filter_name))
	    {
	      usb_registry_set_device_state(DICS_PROPCHANGE, dev_info, 
					    &dev_info_data);
	    }
	}
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(dev_info);

  return TRUE;
}
