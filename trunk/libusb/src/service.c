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



#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsvc.h>

#include "service.h"
#include "win_debug.h"



bool_t usb_create_service(const char *name, const char *display_name,
                          const char *binary_path, unsigned long type,
                          unsigned long start_type)
{
  SC_HANDLE scm = NULL;
  SC_HANDLE service = NULL;
  bool_t ret = FALSE;

  do 
    {
      scm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, 
                          SC_MANAGER_ALL_ACCESS);
      
      if(!scm)
        {
          usb_debug_error("usb_service_create(): opening service control "
                          "manager failed");
          break;
        }
      
      service = OpenService(scm, name, SERVICE_ALL_ACCESS);

      if(service)
        {
          if(!ChangeServiceConfig(service, type, start_type,
                                  SERVICE_ERROR_NORMAL, binary_path,
                                  NULL, NULL, NULL, NULL, NULL,
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
          service = CreateService(scm, name, display_name, GENERIC_EXECUTE,
                                  type, start_type, SERVICE_ERROR_NORMAL, 
                                  binary_path, NULL, NULL, NULL, NULL, NULL);
          
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
      CloseServiceHandle(service);
    }
  
  if(scm)
    {
      CloseServiceHandle(scm);
    }
  
  return ret;
}
