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


#include <windows.h>
#include <stdio.h>

#include "service.h"


typedef struct _stack_t {
  struct _stack_t *next;
  char text[1];
} stack_t;



static void create_kernel_service(void);
static void create_system_service(void);
static void delete_system_service(void);
static void start_system_service(void);
static void stop_system_service(void);
static void stop_kernel_service(void);
static void stop_kernel_service_full(void);
static void restart_host_controllers(void);


 
static void libusb_usage(void);
static void libusb_usage(void)
{
  printf("Usage: libusb-nsis.exe [OPTION]\n");
  printf("--create_kernel_service\n");
  printf("--create_system_service\n");
  printf("--delete_system_service\n");
  printf("--start_system_service\n");
  printf("--stop_system_service\n");
  printf("--stop_kernel_service\n");
  printf("--stop_kernel_service_full\n");
  printf("--restart_host_controllers\n");
}

int main(int argc, char **argv)
{
  if(!usb_service_load_dll())
    {      
      return 0;
    }

  if(argc != 2)
    {
      libusb_usage();
      return 0;
    }

  do
    {
      if(!strcmp(argv[1], "--create_kernel_service"))
	{ 
	  create_kernel_service(); 
	  break; 
	}
      if(!strcmp(argv[1], "--create_system_service"))
	{
	  create_system_service(); 
	  break;
	}
      if(!strcmp(argv[1], "--delete_system_service"))
	{
	  delete_system_service(); 
	  break;
	}
      if(!strcmp(argv[1], "--start_system_service"))
	{
	  start_system_service(); 
	  break;
	}
      if(!strcmp(argv[1], "--stop_system_service"))
	{
	  stop_system_service(); 
	  break;
	}
      if(!strcmp(argv[1], "--stop_kernel_service"))
	{
	  stop_kernel_service(); 
	  break;
	}
      if(!strcmp(argv[1], "--stop_kernel_service_full"))
	{
	  stop_kernel_service_full(); 
	  break;
	}
      if(!strcmp(argv[1], "--restart_host_controllers"))
	{
	  restart_host_controllers(); 
	  break;
	}

      libusb_usage();
    } while(0);

  usb_service_free_dll();
  return 0;
}


static void create_kernel_service(void)
{
  char display_name[128];

  snprintf(display_name, sizeof(display_name) - 1,
	   "LibUsb-Win32 - Filter Driver, Version %d.%d.%d.%d", 
	   VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);
  
  usb_create_service(LIBUSB_DRIVER_NAME_NT, display_name,
		     LIBUSB_DRIVER_PATH,
		     SERVICE_KERNEL_DRIVER,
		     SERVICE_DEMAND_START);
  
}


static void create_system_service(void)
{
  char display_name[128];
 
  snprintf(display_name, sizeof(display_name) - 1,
	   "LibUsb-Win32 - Daemon, Version %d.%d.%d.%d", 
	   VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);

  usb_create_service(LIBUSB_SERVICE_NAME, display_name,
		     LIBUSB_SERVICE_PATH, 
		     SERVICE_WIN32_OWN_PROCESS,
		     SERVICE_AUTO_START); 
}


static void delete_system_service(void)
{
  usb_delete_service(LIBUSB_SERVICE_NAME);
}


static void start_system_service(void)
{
  usb_start_service(LIBUSB_SERVICE_NAME);
}


static void stop_system_service(void)
{
  HANDLE win; 

  if(usb_service_is_nt())
    {
      usb_stop_service(LIBUSB_SERVICE_NAME); 
    }
  else
    {
      do
	{
	  win = FindWindow("LIBUSB_WINDOW_CLASS", NULL);
	  if(win != INVALID_HANDLE_VALUE)
	    {
	      PostMessage(win, WM_DESTROY, 0, 0);
	      Sleep(500);
	    }
	} while(win);
    } 
}


static void stop_kernel_service(void)
{
  usb_service_stop_filter(FALSE);
}


static void stop_kernel_service_full(void)
{
  usb_service_stop_filter(TRUE);
}


static void restart_host_controllers(void)
{
  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_info_data;
  int dev_index = 0;
  char *class;

  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
  dev_info = SetupDiGetClassDevs(NULL, "PCI", 0, 
				 DIGCF_ALLCLASSES | DIGCF_PRESENT);

  if(dev_info == INVALID_HANDLE_VALUE)
    {
      usb_service_error("restart_host_controllers(): getting device info "
			"failed");
      return;
    }

  while(SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
    {
      class = usb_service_get_reg_property(SPDRP_CLASS, dev_info, 
					   &dev_info_data);
      if(class)
	{
	  if(strstr(class, "usb"))
	    {
	      if(!usb_service_set_device_state(DICS_PROPCHANGE, dev_info, 
					       &dev_info_data))
		{
		  usb_service_error("restart_host_controllers: setting "
				    "device state failed");
		}
	    }
	  free(class);
	}
      dev_index++;
    }

  SetupDiDestroyDeviceInfoList(dev_info);
}

