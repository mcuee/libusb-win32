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
#include <windows.h>
#include <setupapi.h>

#include "service.h"
#include "registry.h"
#include "win_debug.h"



typedef SC_HANDLE WINAPI (* open_sc_manager_t)(LPCTSTR, LPCTSTR, DWORD);
typedef SC_HANDLE WINAPI (* open_service_t)(SC_HANDLE, LPCTSTR, DWORD);
typedef BOOL WINAPI (* change_service_config_t)(SC_HANDLE, DWORD, DWORD, 
						DWORD, LPCTSTR, LPCTSTR, 
						LPDWORD, LPCTSTR, LPCTSTR, 
						LPCTSTR, LPCTSTR);
typedef BOOL WINAPI (* close_service_handle_t)(SC_HANDLE);
typedef SC_HANDLE WINAPI (* create_service_t)(SC_HANDLE, LPCTSTR, LPCTSTR,
					      DWORD, DWORD,DWORD, DWORD,
					      LPCTSTR, LPCTSTR, LPDWORD,
					      LPCTSTR, LPCTSTR, LPCTSTR);
typedef BOOL WINAPI (* delete_service_t)(SC_HANDLE);
typedef BOOL WINAPI (* start_service_t)(SC_HANDLE, DWORD, LPCTSTR);
typedef BOOL WINAPI (* query_service_status_t)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL WINAPI (* control_service_t)(SC_HANDLE, DWORD, LPSERVICE_STATUS);





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


bool_t usb_service_load_dll()
{
  if(usb_registry_is_nt())
    {
      advapi32_dll = LoadLibrary("ADVAPI32.DLL");
      
      if(!advapi32_dll)
	{
	  usb_debug_error("usb_service_load_dll(): loading DLL advapi32.dll"
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
      
      if(!open_sc_manager || !open_service || !change_service_config
	 || !close_service_handle || !create_service || !delete_service
	 || !start_service || !query_service_status || !control_service)
	{
	  FreeLibrary(advapi32_dll);
	  usb_debug_error("usb_service_load_dll(): loading exported "
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

  filter_name = usb_registry_is_nt() ? 
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
	  usb_debug_error("usb_service_start_filter(): getting "
			  "device info set failed");

	  return;
	}
      
      while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
	  if(!usb_registry_is_composite_interface(dev_info, &dev_info_data)
	     && !usb_registry_is_root_hub(dev_info, &dev_info_data))
	    {
	      if(usb_registry_insert_filter(dev_info, &dev_info_data, 
					    filter_name))
		{
		  filter_installed = 1;
		  usb_registry_set_device_state(DICS_PROPCHANGE, dev_info, 
						&dev_info_data);
		  break;
		}
	    }
	  dev_index++;
	}
      SetupDiDestroyDeviceInfoList(dev_info);
    } while(filter_installed);
}


void usb_service_stop_filter(int all)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;
  int filter_removed = 0;

  filter_name = usb_registry_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_info = SetupDiGetClassDevs(NULL, "USB", 0, DIGCF_ALLCLASSES);

  do 
    {
      filter_removed = 0;
      dev_index = 0;
      dev_info = SetupDiGetClassDevs(NULL, "USB", 0, DIGCF_ALLCLASSES);
      
      if(dev_info == INVALID_HANDLE_VALUE)
	{
	  usb_debug_error("usb_service_stop_filter(): getting "
			  "device info set failed");
	  return;
	}
      
      while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
	  if(!all)
	    {
	      if(!(usb_registry_is_composite_libusb(dev_info, &dev_info_data)
		   || usb_registry_is_service_libusb(dev_info, &dev_info_data)))
		{
		  if(usb_registry_remove_filter(dev_info, &dev_info_data, filter_name))
		    {
		      filter_removed = 1;
		      usb_registry_set_device_state(DICS_PROPCHANGE, dev_info, 
						    &dev_info_data);
		      break;
		    }
		}
	    }
	  else
	    {
	      if(usb_registry_remove_filter(dev_info, &dev_info_data, filter_name))
		{
		  filter_removed = 1;
		  usb_registry_set_device_state(DICS_PROPCHANGE, dev_info, 
						&dev_info_data);
		  break;
		}
	    }
	  dev_index++;
	}
      SetupDiDestroyDeviceInfoList(dev_info);
    } while(filter_removed);
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
	  usb_debug_error("usb_service_create(): opening service control "
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
	      usb_debug_error("usb_service_create(): changing config of "
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
	      usb_debug_error("usb_service_create(): creating "
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
	  usb_debug_error("usb_delete_service(): opening service control "
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
	  usb_debug_error("usb_service_delete(): deleting "
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
	  usb_debug_error("usb_start_service(): opening service control "
			  "manager failed");
	  break;
	}
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);
      
      if(!service)
	{
	  usb_debug_error("usb_start_service(): opening service '%s' failed",
			  name);
	  break;
	}
      
      if(!query_service_status(service, &status))
	{
	  usb_debug_error("usb_start_service(): getting status of "
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
	  usb_debug_error("usb_start_service(): starting service '%s' "
			  "failed", name);
	  break;
	}
      
      do 
	{
	  int wait = 0;
	  if(!query_service_status(service, &status))
	    {
	      usb_debug_error("usb_start_service(): getting status of "
			      "service '%s' failed", name);
	      break;
	    }
	  Sleep(500);
	  wait += 500;
	  
	  if(wait > 20000)
	    {
	      usb_debug_error("usb_start_service(): starting "
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
	  usb_debug_error("usb_stop_service(): opening service control "
			  "manager failed");
	  break;
	}
      
      service = open_service(scm, name, SERVICE_ALL_ACCESS);
      
      if(!service)
	{
	  usb_debug_error("usb_stop_service(): opening service '%s' failed",
			  name);
	  break;
	}

      if(!query_service_status(service, &status))
	{
	  usb_debug_error("usb_stop_service(): getting status of "
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
	  usb_debug_error("usb_stop_service(): stopping service '%s' failed",
			  name);
	  break;
	}
      
      do 
	{
	  int wait = 0;
	  
	  if(!query_service_status(service, &status))
	    {
	      usb_debug_error("usb_stop_service(): getting status of "
			      "service '%s' failed", name);
	      break;
	    }
	  Sleep(500);
	  wait += 500;
	  
	  if(wait > 20000)
	    {
	      usb_debug_error("usb_stop_service(): stopping "
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

bool_t usb_service_reboot_required(void)
{
  int count = 0;
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *filter_name = NULL;

  filter_name = usb_registry_is_nt() ? 
    LIBUSB_DRIVER_NAME_NT : LIBUSB_DRIVER_NAME_9X;
  
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  dev_info = SetupDiGetClassDevs(NULL, "USB", 0, 
				 DIGCF_ALLCLASSES | DIGCF_PRESENT);
  
  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_debug_error("usb_service_reboot_required(): getting "
		      "device info set failed");
      
      return FALSE;
    }
      
  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      if(usb_registry_is_service_libusb(dev_info, &dev_info_data))
	{
	  count++;

	  if(count == 2)
	    {
	      return TRUE;
	    }
	}
      dev_index++;
    }
  SetupDiDestroyDeviceInfoList(dev_info);

  return FALSE;
}
