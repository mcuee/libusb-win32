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


#include <windows.h>
#include <setupapi.h>
#include <stdio.h>

#include "service.h"
#include "registry.h"
#include "win_debug.h"


#define LIBUSB_DRIVER_PATH "system32\\drivers\\libusb0.sys"


static void create_service(void);
static void delete_service(void);
static void driver_post_install(void);

 
static void usage(void);
static void usage(void)
{
  printf("Usage: libusbis.exe [OPTION]\n");
  printf("Options:\n");
  printf("--create-service\n");
  printf("--delete-service\n");
  printf("--driver-post-install\n");
}

int main(int argc, char **argv)
{
  if(!usb_service_load_dll())
    {      
      return 0;
    }

  if(argc < 2)
    {
      usage();
      return 0;
    }

  do
    {
      if(!strcmp(argv[1], "--create-service"))
	{ 
	  create_service(); 
	  break; 
	}
      if(!strcmp(argv[1], "--delete-service"))
	{
	  delete_service(); 
	  break;
	}
      if(!strcmp(argv[1], "--driver-post-install"))
	{
	  driver_post_install();
	  break;
	}

      usage();
    } while(0);

  usb_service_free_dll();
  return 0;
}


static void create_service(void)
{
  char display_name[REGISTRY_BUF_SIZE];
  
  usb_registry_stop_filter(TRUE); /* remove all filter drivers */
  usb_registry_stop_libusb_devices(); /* stop all libusb devices */

  /* the old driver is unloaded now */ 

  if(usb_registry_is_nt())
    {
      snprintf(display_name, sizeof(display_name) - 1,
	       "LibUsb-Win32 - Kernel Driver, Version %d.%d.%d.%d", 
	       VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);

      /* create the kernel service */
      usb_create_service(LIBUSB_DRIVER_NAME_NT, display_name,
			 LIBUSB_DRIVER_PATH,
			 SERVICE_KERNEL_DRIVER,
			 SERVICE_DEMAND_START);
    }
     
  usb_registry_start_libusb_devices(); /* restart libusb devices */
  usb_registry_start_filter();

  if(usb_registry_is_nt())
    {
      snprintf(display_name, sizeof(display_name) - 1,
	       "LibUsb-Win32 - Daemon, Version %d.%d.%d.%d", 
	       VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);
      
      /* create the system service */
      usb_create_service(LIBUSB_SERVICE_NAME, display_name,
			 LIBUSB_SERVICE_PATH, 
			 SERVICE_WIN32_OWN_PROCESS,
			 SERVICE_AUTO_START); 
      usb_start_service(LIBUSB_SERVICE_NAME);
    }
}


static void delete_service(void)
{
  HANDLE win; 

  if(usb_registry_is_nt())
    {
      usb_stop_service(LIBUSB_SERVICE_NAME); 
      usb_delete_service(LIBUSB_SERVICE_NAME);
    }
  else
    {
      do {
	win = FindWindow("LIBUSB_WINDOW_CLASS", NULL);
	if(win != INVALID_HANDLE_VALUE)
	  {
	    PostMessage(win, WM_DESTROY, 0, 0);
	    Sleep(500);
	  }
      } while(win);
    } 

  usb_registry_stop_filter(FALSE); /* remove filter drivers */
}

static void driver_post_install(void)
{
  HANDLE win; 

  /* if the service is running then temporaryly stop it */
  if(usb_registry_is_nt())
    {
      usb_pause_service(LIBUSB_SERVICE_NAME);
    }
  else
    {
      win = FindWindow("LIBUSB_WINDOW_CLASS", NULL);
      if(win != INVALID_HANDLE_VALUE)
	{
	  PostMessage(win, WM_USER + LIBUSB_SERVICE_CONTROL_PAUSE, 0, 0);
	}
    }

  /* stop all libusb devices */
  usb_registry_stop_libusb_devices();
  /* remove all filters from composite devices that are handled by libusb */
  usb_registry_remove_composite_filter();
  
  /* the old driver is unloaded now */

  /* restart libusb devices */
  usb_registry_start_libusb_devices();
  /* reinsert filters to libusb's composite devices */
  usb_registry_insert_composite_filter();

  /* if the service is running then restart it */
  if(usb_registry_is_nt())
    {
      usb_continue_service(LIBUSB_SERVICE_NAME);
    }
  else
    {
      win = FindWindow("LIBUSB_WINDOW_CLASS", NULL);
      if(win != INVALID_HANDLE_VALUE)
	{
	  PostMessage(win, WM_USER + LIBUSB_SERVICE_CONTROL_CONTINUE, 0, 0);
	}
    }
}
