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



#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>
#include <setupapi.h>

#include "service.h"



typedef SC_HANDLE WINAPI (* open_sc_manager_t)(LPCTSTR, LPCTSTR, DWORD);
typedef SC_HANDLE WINAPI (* open_service_t)(SC_HANDLE, LPCTSTR, DWORD);
typedef BOOL WINAPI (* change_service_config_t)(SC_HANDLE, DWORD, DWORD, DWORD,
					 LPCTSTR, LPCTSTR, LPDWORD, LPCTSTR,
					 LPCTSTR, LPCTSTR, LPCTSTR);
typedef BOOL WINAPI (* close_service_handle_t)(SC_HANDLE);
typedef SC_HANDLE WINAPI (* create_service_t)(SC_HANDLE, LPCTSTR, LPCTSTR,
					      DWORD, DWORD,DWORD, DWORD,
					      LPCTSTR, LPCTSTR, LPDWORD,
					      LPCTSTR, LPCTSTR, LPCTSTR);
typedef BOOL WINAPI (* delete_service_t)(SC_HANDLE);
typedef BOOL WINAPI (* start_service_t)(SC_HANDLE, DWORD, LPCTSTR);
typedef BOOL WINAPI (* query_service_status_t)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL WINAPI (* control_service_t)(SC_HANDLE, DWORD, LPSERVICE_STATUS);



static char __service_error_buf[512];


static HANDLE advapi32_dll = NULL;

static open_sc_manager_t open_sc_manager = NULL;
static open_service_t open_service = NULL;
static change_service_config_t change_service_config = NULL;
static close_service_handle_t close_service_handle = NULL;
static create_service_t create_service = NULL;
static delete_service_t delete_service = NULL;
static start_service_t start_service = NULL;
static query_service_status_t query_service_status = NULL;
static control_service_t control_service = NULL;

void usb_service_error(const char *s, ...)
{
  char tmp[512];
  va_list args;
  va_start(args, s);
  vsnprintf(__service_error_buf, sizeof(__service_error_buf) - 1, s, args);
  va_end(args);
  printf("%s\n",__service_error_buf);

  snprintf(tmp, sizeof(tmp) - 1, "LIBUSB_SERVICE: error: %s",
	   __service_error_buf);
  OutputDebugString(tmp);
}


void usb_service_error_clear(void)
{
  __service_error_buf[0] = 0;
}


const char * usb_service_get_error(void)
{
  return __service_error_buf;
}

bool_t usb_service_load_dll()
{
  if(usb_service_is_nt())
    {
      advapi32_dll = LoadLibrary("ADVAPI32.DLL");
      
      if(!advapi32_dll)
	{
	  usb_service_error("usb_service_load_dll(): loading DLL advapi32.dll"
			    "failed");
	  return FALSE;
	}
      
      open_sc_manager = (open_sc_manager_t)
	GetProcAddress(advapi32_dll, "OpenSCManagerA");
      
      open_service = (open_service_t)
	GetProcAddress(advapi32_dll, "OpenServiceA");
      
      change_service_config = (change_service_config_t)
	GetProcAddress(advapi32_dll, "ChangeServiceConfigA");
      
      close_service_handle = (close_service_handle_t)
	GetProcAddress(advapi32_dll, "CloseServiceHandle");
      
      create_service = (create_service_t)
	GetProcAddress(advapi32_dll, "CreateServiceA");
      
      delete_service = (delete_service_t)
	GetProcAddress(advapi32_dll, "DeleteService");
      
      start_service = (start_service_t)
	GetProcAddress(advapi32_dll, "StartServiceA");
      
      query_service_status = (query_service_status_t)
	GetProcAddress(advapi32_dll, "QueryServiceStatus");
      
      control_service = (control_service_t)
	GetProcAddress(advapi32_dll, "ControlService");
      
      if(!open_sc_manager | !open_service | !change_service_config
	 | !close_service_handle | !create_service | !delete_service
	 | !start_service | !query_service_status | !control_service)
	{
	  FreeLibrary(advapi32_dll);
	  usb_service_error("usb_service_load_dll(): loading exported "
			    "functions of advapi32.dll failed");

	  return FALSE;
	}
    }
  return TRUE;
}

bool_t usb_service_free_dll()
{
  if(advapi32_dll)
    {
      FreeLibrary(advapi32_dll);
    }
  return TRUE;
}


void usb_service_start_filter(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;
  int filter_installed = 0;

  filter_name = usb_service_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  do 
    {
      filter_installed = 0;
      dev_index = 0;
      dev_info = SetupDiGetClassDevs(NULL, "USB", 0, 
				     DIGCF_ALLCLASSES | DIGCF_PRESENT);
      
      if(dev_info == INVALID_HANDLE_VALUE)
	{
	  usb_service_error("usb_service_start_filter(): getting "
			    "device info set failed");

	  return;
	}
      
      while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
	  if(usb_service_insert_filter(dev_info, &dev_info_data, filter_name))
	    {
	      filter_installed = 1;
	      usb_service_set_device_state(DICS_PROPCHANGE, dev_info, 
					   &dev_info_data);
	      break;
	    }
	  dev_index++;
	}
      SetupDiDestroyDeviceInfoList(dev_info);
    } while(filter_installed);
}



void usb_service_stop_filter(bool_t stop_stub)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;

  filter_name = usb_service_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_info = SetupDiGetClassDevs(NULL, "USB", 0, DIGCF_ALLCLASSES);

  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_service_error("usb_service_stop_filter(): getting "
			"device info set failed");
      return;
    }

  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      usb_service_remove_filter(dev_info, &dev_info_data, filter_name, 
				stop_stub);
      dev_index++;
    }

  dev_index = 0;
  SetupDiDestroyDeviceInfoList(dev_info);
  dev_info = SetupDiGetClassDevs(NULL, "USB", 0, 
				 DIGCF_ALLCLASSES | DIGCF_PRESENT);

  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_service_is_root_hub(dev_info, &dev_info_data))
	{
	  usb_service_set_device_state(DICS_PROPCHANGE, dev_info, 
				       &dev_info_data);
	}
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(dev_info);
}


bool_t usb_service_is_nt(void)
{
  return GetVersion() < 0x80000000 ? 1 : 0;
}


char *usb_service_get_reg_property(DWORD which, HDEVINFO dev_info, 
				   SP_DEVINFO_DATA *dev_info_data)
{
  DWORD reg_type;
  DWORD key_type;
  DWORD size = 0;
  char *p = NULL;
  char *val_name = NULL;
  char *property = NULL;
  HKEY reg_key = NULL;

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
      return NULL;
    }

  do
    {
      if(usb_service_is_nt())
	{
	  SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, which, 
					   &reg_type, NULL, 
					   0, &size);
	  if(size < 2)
	    {
	      break;
	    }

	  property = (LPTSTR) malloc(size + 2 * sizeof(TCHAR));
	  
	  if(!property)
	    {
	      usb_service_error("usb_reg_get_reg_property(): memory "
				"allocation error");
	      return NULL;
	    }
	  
	  if(!SetupDiGetDeviceRegistryProperty(dev_info, dev_info_data, which,
					       &reg_type, (BYTE *)property, 
					       size, NULL))
	    {
	      usb_service_error("usb_reg_get_reg_property(): getting "
				"property '%s' failed", val_name);
	      break;
	    }
	}
      else /* Win9x */
	{
	  reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, 
					 DICS_FLAG_GLOBAL, 
					 0, key_type, KEY_ALL_ACCESS);
	  
	  if(reg_key == INVALID_HANDLE_VALUE)
	    {
	      usb_service_error("usb_reg_get_reg_property(): reading "
				"registry key failed");
	      break;
	    }
	  
	  if(RegQueryValueEx(reg_key, val_name, NULL, &reg_type, 
			     NULL, &size) != ERROR_SUCCESS )
	    {
	      break;
	    }

	  property = (LPTSTR) malloc(size + 2 * sizeof(TCHAR));
	  
	  if(!property)
	    {
	      usb_service_error("usb_reg_get_reg_property(): memory "
				"allocation error");
	      break;
	    }
	  
	  if(RegQueryValueEx(reg_key, val_name, NULL, &reg_type, 
			     (BYTE *)property, &size) != ERROR_SUCCESS 
	     || !size)
	    {
	      usb_service_error("usb_reg_get_reg_property(): getting "
				"property '%s' failed", val_name);
	      break;
	    }
	}
      
      if(!property || (size < 2))
	{
	  free(property);
	  return NULL;
	}

      p = property;
      
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

      p = property;
      
      while(*p)
	{
	  *p = tolower(*p);
	  p++;
	}

      return property;
    } while(0);
  
  if(property)
    {
      free(property);
    }

  if(reg_key)
    {
      RegCloseKey(reg_key);
    }

  return NULL;
}

bool_t usb_service_set_reg_property(DWORD which, HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data, 
				 char *value)
{
  char *buffer = NULL;
  char *val_name = NULL;
  char *p = NULL;
  DWORD size;
  HKEY reg_key;
  DWORD reg_type;
  int ret = FALSE;

  switch(which)
    {
    case SPDRP_LOWERFILTERS:
      reg_type = usb_service_is_nt() ? REG_MULTI_SZ : REG_SZ;
      val_name = "LowerFilters";
      break;
    default:
      return 0;
    }

  size = strlen(value);
  size += reg_type == REG_MULTI_SZ ? 2 : 1;

  buffer = (LPTSTR) malloc(size);

  if(!buffer)
    {
      usb_service_error("usb_reg_set_reg_property(): memory "
			"allocation error");
      return FALSE;
    }
  
  memset(buffer, 0, size);
 
  if(value)
    {
      strcpy(buffer, value);
    }

  if(reg_type == REG_MULTI_SZ)
    {
      p = buffer;

      while(*p)
	{
	  if(*p == ',')
	    {
	      *p = 0;
	    }
	  p++;
	}
    }

  do
    {
      if(usb_service_is_nt())
	{
	  if(SetupDiSetDeviceRegistryProperty(dev_info, dev_info_data,
					      which, (BYTE *)buffer, size))
	    {
	      ret = TRUE;
	      break;
	    }
	  else
	    {
	      usb_service_error("usb_reg_set_reg_property(): setting "
				"property '%s' failed", val_name);
	    }
	}
      else
	{
	  reg_key = SetupDiOpenDevRegKey(dev_info, dev_info_data, 
					 DICS_FLAG_GLOBAL, 
					 0, DIREG_DEV, KEY_ALL_ACCESS);
	  
	  if(reg_key == INVALID_HANDLE_VALUE)
	    {
	      usb_service_error("usb_reg_get_reg_property(): reading "
				"registry key failed");
	      ret = FALSE;
	      break;
	    }
	  
	  if(RegSetValueEx(reg_key, val_name, 0, reg_type, (BYTE *) buffer, 
			   size) != ERROR_SUCCESS)
	    {
	      usb_service_error("usb_reg_set_reg_property(): setting "
				"property '%s' failed", val_name);
	    }
	  ret = TRUE;
	}
    } while(0);

  if(buffer)
    {
      free(buffer);
    }

  return ret;
}

bool_t usb_service_insert_filter(HDEVINFO dev_info,
				 SP_DEVINFO_DATA *dev_info_data, 
				 const char *filter_name)
{
  int ret = TRUE;
  char *filters = NULL;
  char *buffer = NULL;
  int size;
  

  filters = usb_service_get_reg_property(SPDRP_LOWERFILTERS, 
					 dev_info, dev_info_data);
  
  do
    {
      if(filters && strlen(filters))
	{
	  if(strstr(filters, filter_name))
	    {
	      free(filters);
	      return FALSE;
	    }
      
	  size = strlen(filters) + strlen(filter_name) + 2;
	}
      else
	{
	  size = strlen(filter_name) + 2;
	}

      buffer = malloc(size);

      if(!buffer)
	{
	  usb_service_error("usb_service_insert_filter(): memory "
			    "allocation error");
	  ret = FALSE;
	  break;
	}

      memset(buffer, 0, size + 2);

      if(filters)
	{
	  strcpy(buffer, filters);
	  strcat(buffer, ",");
	}
      
      strcat(buffer, filter_name);

      if(!usb_service_set_reg_property(SPDRP_LOWERFILTERS, dev_info, 
				       dev_info_data, buffer))
	{
	  ret = FALSE;
	  break;
	}
    } while(0);

  if(filters)
    {
      free(filters);
    }
  
  if(buffer)
    {
      free(buffer);
    }

  return ret;
}

bool_t usb_service_remove_filter(HDEVINFO dev_info, 
				 SP_DEVINFO_DATA *dev_info_data, 
				 const char *filter_name,
				 bool_t stop_stub)
{
  char *p = NULL;
  char *filters = NULL;
  char *service_name = NULL;
  bool_t ret = FALSE;

  service_name = usb_service_get_reg_property(SPDRP_SERVICE, dev_info, 
					      dev_info_data);

  if(!service_name)
    {
      /* found device in the registry with VID_0000/PID_0000 which have */
      /* no service */
      return TRUE;
    }
    
  if(!stop_stub && strstr(service_name, LIBUSB_STUB_NAME))
    {
      free(service_name);
      return TRUE;
    }

  free(service_name);

  filters = usb_service_get_reg_property(SPDRP_LOWERFILTERS, dev_info, 
					 dev_info_data);

  do
    {
      if(!filters)
	{
	  ret = TRUE;
	  break;
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

	  p += strlen(p);
	  
	  while(p != filters)
	    {
	      if(isalpha(*p) || isdigit(*p))
		{
		  break;
		}
	      else
		{
		  *p = 0;
		}
	      p--;
	    }
	}
      else
	{
	  ret = TRUE;
	  break;
	}
      
      ret = usb_service_set_reg_property(SPDRP_LOWERFILTERS, dev_info, 
					 dev_info_data, filters);
    } while(0);

  if(filters)
    {
      free(filters);
    }

  return ret;
}

bool_t usb_service_set_device_state(DWORD state, HDEVINFO dev_info, 
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
      usb_service_error("usb_service_set_device_state(): setting class "
			"install parameters failed");
      return FALSE;
    }
	  
  if(!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, dev_info, 
				dev_info_data))
    {
      usb_service_error("usb_service_set_device_state(): calling class "
			"installer failed");
      return FALSE;
    }

  return TRUE;
}


bool_t usb_service_is_root_hub(HDEVINFO dev_info, 
			       SP_DEVINFO_DATA *dev_info_data)
{
  char *hardware_id = NULL;
  
  hardware_id = usb_service_get_reg_property(SPDRP_HARDWAREID,
					     dev_info, dev_info_data);
  if(!hardware_id)
    {
      usb_service_error("usb_service_is_root_hub(): getting hardware id "
			"failed");
      return FALSE;
    }
    
  if(strstr(hardware_id, "root_hub"))
    {
      return TRUE;
    }
  return FALSE;
}

bool_t usb_create_service(const char *name, const char *display_name,
			  const char *binary_path, unsigned long type,
			  unsigned long start_type)
{
  SC_HANDLE scm = NULL;
  SC_HANDLE service = NULL;
  bool_t ret = FALSE;

  do 
    {
      scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE, 
			    SC_MANAGER_ALL_ACCESS);

      if(!scm)
	{
	  usb_service_error("usb_service_create(): opening service control "
			    "manager failed");
	  break;
	}
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);

      if(service)
	{
	  if(!change_service_config(service,
				    type,
				    start_type,
				    SERVICE_ERROR_NORMAL,
				    binary_path,
				    NULL,
				    NULL,
				    NULL,
				    NULL,
				    NULL,
				    display_name))
	    {
	      usb_service_error("usb_service_create(): changing config of "
				"service '%s' failed", name);
	      break;
	    }
	  ret = TRUE;
	  break;
	}
  
      if(GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
	{
	  service = create_service(scm,
				   name,
				   display_name,
				   SERVICE_ALL_ACCESS,
				   type,
				   start_type,
				   SERVICE_ERROR_NORMAL, 
				   binary_path,
				   NULL, NULL, NULL, NULL, NULL);
	  
	  if(!service)
	    {
	      usb_service_error("usb_service_create(): creating "
				"service '%s' failed", name);
	    }
	  ret = TRUE;	
	}
    } while(0);

  if(service)
    {
      close_service_handle(service);
    }
  
  if(scm)
    {
      close_service_handle(scm);
    }
  
  return ret;


  return ret;
}


bool_t usb_delete_service(const char *name)
{
  bool_t ret = FALSE;
  SC_HANDLE scm = NULL;
  SC_HANDLE service = NULL;

  do 
    {
      scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE, 
			    SC_MANAGER_ALL_ACCESS);
      
      if(!scm)
	{
	  usb_service_error("usb_delete_service(): opening service control "
			    "manager failed");
	  break;
	}
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);
  
      if(!service)
	{
	  ret = TRUE;
	  break;
	}
      

      if(!delete_service(service))
	{
	  usb_service_error("usb_service_delete(): deleting "
			    "service '%s' failed", name);
	  break;
	}
      ret = TRUE;
    } while(0);

  if(service)
    {
      close_service_handle(service);
    }
  
  if(scm)
    {
      close_service_handle(scm);
    }
  
  return ret;
}

bool_t usb_start_service(const char *name)
{
  bool_t ret = FALSE;
  SC_HANDLE scm = NULL;
  SC_HANDLE service = NULL;
  SERVICE_STATUS status;
  
  
  do
    {
      scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE, 
			    SC_MANAGER_ALL_ACCESS);
      
      if(!scm)
	{
	  usb_service_error("usb_start_service(): opening service control "
			    "manager failed");
	  break;
	}
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);
      
      if(!service)
	{
	  usb_service_error("usb_start_service(): opening service '%s' failed",
			    name);
	  break;
	}
      
      if(!query_service_status(service, &status))
	{
	  usb_service_error("usb_start_service(): getting status of "
			    "service '%s' failed", name);
	  break;
	}

      if(status.dwCurrentState == SERVICE_RUNNING)
	{
	  ret = TRUE;
	  break;
	}

      if(!start_service(service, 0, NULL))
	{
	  usb_service_error("usb_start_service(): starting service '%s' "
			    "failed", name);
	  break;
	}
      
      do 
	{
	  int wait = 0;
	  if(!query_service_status(service, &status))
	    {
	      usb_service_error("usb_start_service(): getting status of "
				"service '%s' failed", name);
	      break;
	    }
	  Sleep(500);
	  wait += 500;
	  
	  if(wait > 20000)
	    {
	      usb_service_error("usb_start_service(): starting "
				"service '%s' failed, timeout", name);
	      ret = FALSE;
	      break;
	    }
	  ret = TRUE;
	} while(status.dwCurrentState != SERVICE_RUNNING);
      
    } while(0);

  if(service)
    {
      close_service_handle(service);
    }
  
  if(scm)
    {
      close_service_handle(scm);
    }
  
  return ret;
}


bool_t usb_stop_service(const char *name)
{
  bool_t ret = FALSE;
  SC_HANDLE scm = NULL;
  SC_HANDLE service = NULL;
  SERVICE_STATUS status;


  do
    {
      scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE, 
			    SC_MANAGER_ALL_ACCESS);
      
      if(!scm)
	{
	  usb_service_error("usb_stop_service(): opening service control "
			    "manager failed");
	  break;
	}
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);
      
      if(!service)
	{
	  usb_service_error("usb_stop_service(): opening service '%s' failed",
			    name);
	  break;
	}

      if(!query_service_status(service, &status))
	{
	  usb_service_error("usb_stop_service(): getting status of "
			    "service '%s' failed", name);
	  break;
	}

      if(status.dwCurrentState == SERVICE_STOPPED)
	{
	  ret = TRUE;
	  break;
	}

      if(!control_service(service, SERVICE_CONTROL_STOP, &status))
	{
	  usb_service_error("usb_stop_service(): stopping service '%s' failed",
			    name);
	  break;
	}
      
      do 
	{
	  int wait = 0;
	  
	  if(!query_service_status(service, &status))
	    {
	      usb_service_error("usb_stop_service(): getting status of "
				"service '%s' failed", name);
	      break;
	    }
	  Sleep(500);
	  wait += 500;
	  
	  if(wait > 20000)
	    {
	      usb_service_error("usb_stop_service(): stopping "
				"service '%s' failed, timeout", name);
	      ret = FALSE;
	      break;
	    }
	  ret = TRUE;
	} while(status.dwCurrentState != SERVICE_STOPPED);
    } while(0);

  if(service)
    {
      close_service_handle(service);
    }
  
  if(scm)
    {
      close_service_handle(scm);
    }
  
  return ret;
}
