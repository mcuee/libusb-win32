/* libusb-win32, Generic Windows USB Library
* Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
* Copyright (c) 2010 Travis Robinson <libusbdotnet@gmail.com>
* Parts of the code from libwdi by Pete Batard
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
#include <winsvc.h>
#include <setupapi.h>
#include <stdio.h>
#include <regstr.h>
#include <wchar.h>
#include <string.h>
#include <ShellAPI.h>
#include <process.h>

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

#include "usb.h"
#include "registry.h"
#include "error.h"
#include "driver_api.h"
#include "libusb-win32_version.h"


#define LIBUSB_DRIVER_PATH  "system32\\drivers\\libusb0.sys"
#define LIBUSB_OLD_SERVICE_NAME_NT "libusbd"

#define INSTALLFLAG_FORCE 0x00000001

#define IDL_INSTALL_PROGRESS_TEXT 10001
#define UM_SET_PROGRESS_TEXT WM_USER+1
#define UM_PROGRESS_STOP WM_USER+2
#define UM_PROGRESS_START WM_USER+3

LPCSTR install_lock_sem_name="libusb-win32-installer-{1298B356-F6E3-4455-9FEC-3932714AF49B}";

/* commands */
LPCWSTR paramcmd_list[] = {
	L"list",
	L"-l",
	L"l",
	0
};

LPCWSTR paramcmd_install[] = {
	L"install",
	L"-i",
	L"i",
	0
};

LPCWSTR paramcmd_uninstall[] = {
	L"uninstall",
	L"-u",
	L"u",
	0
};

LPCWSTR paramcmd_help[] = {
	L"--help",
	L"help",
	L"-h",
	L"/?",
	L"-?",
	L"h",
	0
};

/* switches */
LPCWSTR paramsw_all_classes[] = {
	L"--all-classes",
	L"-ac",
	0
};

LPCWSTR paramsw_device_classes[] = {
	L"--device-classes",
	L"-dc",
	0
};

LPCWSTR paramsw_class[] = {
	L"--class=",
	L"-c=",
	0
};

LPCWSTR paramsw_device_upper[] = {
	L"--device=",
	L"-d=",
	L"--device-upper=",
	L"-duf=",
	0
};

LPCWSTR paramsw_device_lower[] = {
	L"--device-lower=",
	L"-dlf=",
	0
};

LPCWSTR paramsw_inf[] = {
	L"--inf=",
	L"-f=",
	0
};

typedef struct _LONGGUID {
    unsigned int  Data1;
    unsigned int Data2;
    unsigned int Data3;
    unsigned int  Data4[ 8 ];
} LONGGUID;

typedef struct _install_progress_context_t
{
	HWND parent;
	HINSTANCE instance;
	HWND progress_hwnd;
	HWND hwnd_progress_label;
	filter_context_t* filter_context;
	uintptr_t thread_id;
	int ret;
} install_progress_context_t;

static install_progress_context_t g_install_progress_context = {0};

static bool_t usb_install_get_argument(LPWSTR param_value, LPCWSTR* out_param,  LPCWSTR* out_value, LPCWSTR* param_names);
void usb_install_report(filter_context_t* filter_context);
int usb_install(HWND hwnd, HINSTANCE instance, LPCWSTR cmd_line_w, int starg_arg, filter_context_t** out_filter_context);
void usage(void);

/* kernel32.dll exports */
typedef BOOL (WINAPI * is_wow64_process_t)(HANDLE, PBOOL);

/* newdev.dll exports */
typedef BOOL (WINAPI * update_driver_for_plug_and_play_devices_t)(HWND,
																  LPCSTR,
																  LPCSTR,
																  DWORD,
																  PBOOL);

typedef BOOL (WINAPI * rollback_driver_t)(HDEVINFO,
										  PSP_DEVINFO_DATA,
										  HWND,
										  DWORD,
										  PBOOL);

typedef BOOL (WINAPI * uninstall_device_t)(HWND,
										   HDEVINFO,
										   PSP_DEVINFO_DATA,
										   DWORD,
										   PBOOL);

/* setupapi.dll exports */
typedef BOOL (WINAPI * setup_copy_oem_inf_t)(PCSTR, PCSTR, DWORD, DWORD,
											 PSTR, DWORD, PDWORD, PSTR*);


/* advapi32.dll exports */
typedef SC_HANDLE (WINAPI * open_sc_manager_t)(LPCTSTR, LPCTSTR, DWORD);
typedef SC_HANDLE (WINAPI * open_service_t)(SC_HANDLE, LPCTSTR, DWORD);
typedef BOOL (WINAPI * change_service_config_t)(SC_HANDLE, DWORD, DWORD,
												DWORD, LPCTSTR, LPCTSTR,
												LPDWORD, LPCTSTR, LPCTSTR,
												LPCTSTR, LPCTSTR);
typedef BOOL (WINAPI * close_service_handle_t)(SC_HANDLE);
typedef SC_HANDLE (WINAPI * create_service_t)(SC_HANDLE, LPCTSTR, LPCTSTR,
											  DWORD, DWORD,DWORD, DWORD,
											  LPCTSTR, LPCTSTR, LPDWORD,
											  LPCTSTR, LPCTSTR, LPCTSTR);
typedef BOOL (WINAPI * delete_service_t)(SC_HANDLE);
typedef BOOL (WINAPI * start_service_t)(SC_HANDLE, DWORD, LPCTSTR);
typedef BOOL (WINAPI * query_service_status_t)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL (WINAPI * control_service_t)(SC_HANDLE, DWORD, LPSERVICE_STATUS);


static HINSTANCE advapi32_dll = NULL;

static open_sc_manager_t open_sc_manager = NULL;
static open_service_t open_service = NULL;
static change_service_config_t change_service_config = NULL;
static close_service_handle_t close_service_handle = NULL;
static create_service_t create_service = NULL;
static delete_service_t delete_service = NULL;
static start_service_t start_service = NULL;
static query_service_status_t query_service_status = NULL;
static control_service_t control_service = NULL;

// Work around for CreateFont on DDK (would require end user apps linking with Gdi32 othwerwise)
static HFONT (WINAPI *pCreateFontA)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) = NULL;
#define INIT_CREATEFONT if (pCreateFontA== NULL) {	\
	pCreateFontA = (HFONT (WINAPI *)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR))	\
	GetProcAddress(GetModuleHandleA("Gdi32"), "CreateFontA"); \
}
#define IS_CREATEFONT_AVAILABLE (pCreateFontA != NULL)

static bool_t usb_service_load_dll(void);
static bool_t usb_service_free_dll(void);

static bool_t usb_service_create(const char *name, const char *display_name,
								 const char *binary_path, unsigned long type,
								 unsigned long start_type);
static bool_t usb_service_stop(const char *name);
static bool_t usb_service_delete(const char *name);
static bool_t usb_install_iswow64(void);
static BOOL usb_install_admin_check(void);

// Detect Windows version
/*
 * Windows versions
 */
enum windows_version {
	WINDOWS_UNDEFINED,
	WINDOWS_UNSUPPORTED,
	WINDOWS_2K,
	WINDOWS_XP,
	WINDOWS_2003_XP64,
	WINDOWS_VISTA,
	WINDOWS_7
};
enum windows_version windows_version = WINDOWS_UNDEFINED;
#define GET_WINDOWS_VERSION do{ if (windows_version == WINDOWS_UNDEFINED) detect_version(); } while(0)
static void detect_version(void)
{
	OSVERSIONINFO os_version;

	memset(&os_version, 0, sizeof(OSVERSIONINFO));
	os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	windows_version = WINDOWS_UNSUPPORTED;
	if ((GetVersionEx(&os_version) != 0) && (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT)) {
		if ((os_version.dwMajorVersion == 5) && (os_version.dwMinorVersion == 0)) {
			windows_version = WINDOWS_2K;
		} else if ((os_version.dwMajorVersion == 5) && (os_version.dwMinorVersion == 1)) {
			windows_version = WINDOWS_XP;
		} else if ((os_version.dwMajorVersion == 5) && (os_version.dwMinorVersion == 2)) {
			windows_version = WINDOWS_2003_XP64;
		} else if (os_version.dwMajorVersion >= 6) {
			if (os_version.dwBuildNumber < 7000) {
				windows_version = WINDOWS_VISTA;
			} else {
				windows_version = WINDOWS_7;
			}
		}
	}
}

BOOL sem_create_lock(HANDLE* sem_handle_out, LPCSTR unique_name, LONG remaining, LONG max);
BOOL sem_destroy_lock(HANDLE* sem_handle_in);
BOOL sem_release_lock(HANDLE sem_handle);
BOOL sem_try_lock(HANDLE sem_handle, DWORD dwMilliseconds);

BOOL sem_create_lock(HANDLE* sem_handle_out, LPCSTR unique_name, LONG remaining, LONG max)
{
	SECURITY_ATTRIBUTES sem_attributes;

	memset(&sem_attributes,0,sizeof(sem_attributes));
	sem_attributes.nLength = sizeof(sem_attributes);
	sem_attributes.bInheritHandle = TRUE;

	*sem_handle_out = CreateSemaphoreA(&sem_attributes, remaining, max, unique_name);

	return *sem_handle_out != NULL;

}

BOOL sem_destroy_lock(HANDLE* sem_handle_in)
{
	if (sem_handle_in && *sem_handle_in)
	{
		return CloseHandle(*sem_handle_in);
		*sem_handle_in = NULL;
	}
	return FALSE;

}

BOOL sem_release_lock(HANDLE sem_handle)
{
	if (!sem_handle) return FALSE;
	return ReleaseSemaphore(sem_handle, 1, NULL);
}

BOOL sem_try_lock(HANDLE sem_handle, DWORD dwMilliseconds)
{
	if (!sem_handle) return FALSE;
	return WaitForSingleObject(sem_handle, dwMilliseconds) == WAIT_OBJECT_0;
}

void CALLBACK usb_touch_inf_file_rundll(HWND wnd, HINSTANCE instance,
										LPSTR cmd_line, int cmd_show);

void CALLBACK usb_install_service_np_rundll(HWND wnd, HINSTANCE instance,
											LPSTR cmd_line, int cmd_show)
{
	usb_install_service_np();
}

void CALLBACK usb_uninstall_service_np_rundll(HWND wnd, HINSTANCE instance,
											  LPSTR cmd_line, int cmd_show)
{
	usb_uninstall_service_np();
}

void CALLBACK usb_install_driver_np_rundll(HWND wnd, HINSTANCE instance,
										   LPSTR cmd_line, int cmd_show)
{
	usb_install_driver_np(cmd_line);
}

int usb_install_service(filter_context_t* filter_context)
{
	char display_name[MAX_PATH];

	int ret = 0;

	const char* driver_name = LIBUSB_DRIVER_NAME_NT;
	/* stop devices that are handled by libusb's device driver */
	usb_registry_stop_libusb_devices();

	/* the old driver is unloaded now */
	if (usb_registry_is_nt())
	{
		memset(display_name, 0, sizeof(display_name));

		/* create the Display Name */
		_snprintf(display_name, sizeof(display_name) - 1,
			"libusb-win32 - Kernel Driver, Version %d.%d.%d.%d",
			VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_NANO);

		/* create the kernel service */
		USBMSG("creating %s service..\n",driver_name);
		if (!usb_service_create(driver_name, display_name,
			LIBUSB_DRIVER_PATH,
			SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START))
		{
			USBERR("failed creating service %s\n",driver_name);
			ret = -1;
		}
	}
	/* restart devices that are handled by libusb's device driver */
	usb_registry_start_libusb_devices();

	/* insert device filter drivers */
	usb_registry_insert_device_filters(filter_context);

	/* insert class filter driver */
	usb_registry_insert_class_filter(filter_context);

	/* restart the whole USB system so that the new drivers will be loaded */
	usb_registry_restart_all_devices();

	return ret;
}

int usb_install_service_np(void)
{
	return usb_install_np(NULL, NULL, L"install", 0);
}

int usb_uninstall_service(filter_context_t* filter_context)
{
	if (!usb_registry_is_nt()) return -1;

	/* older version of libusb used a system service, just remove it */
	usb_service_stop(LIBUSB_OLD_SERVICE_NAME_NT);
	usb_service_delete(LIBUSB_OLD_SERVICE_NAME_NT);

	/* old versions used device filters that have to be removed */
	usb_registry_remove_device_filter(filter_context);

	/* remove class filter driver */
	usb_registry_remove_class_filter(filter_context);

	/* unload filter drivers */
	usb_registry_restart_all_devices();

	return 0;
}

int usb_uninstall_service_np(void)
{
	return usb_install_np(NULL, NULL, L"uninstall", 0);
}

BOOL usb_install_find_model_section(HINF inf_handle, PINFCONTEXT inf_context)
{
	// find the .inf file's model-section. This is normally a [Devices]
	// section, but could be anything.

	char tmp[MAX_PATH];
	char* model[8];
	BOOL success = FALSE;
	DWORD field_index;

	// first find [Manufacturer].  The models are listeed in this section.
	if (SetupFindFirstLine(inf_handle, "Manufacturer", NULL, inf_context))
	{
		memset(model,0,sizeof(model));
		for (field_index = 1; field_index < ( sizeof(model) / sizeof(model[0]) ); field_index++)
		{
			success = SetupGetStringField(inf_context, field_index, tmp, sizeof(tmp), NULL);
			if (!success) break;

			model[(field_index-1)] = _strdup(tmp);
			switch(field_index)
			{
			case 1:	// The first field is the base model-section-name, "Devices" for example.
				USBDBG("model-section-name=%s\n", tmp);
				break;
			default: // These are the target OS field(s), "NT" or "NTX86" for example. 
				USBDBG("target-os-version=%s\n", tmp);
			}
		}

		// if the base model-section-name was found..
		if (field_index > 1)
		{
			// find the model-section
			field_index = 0;
			strcpy(tmp, model[0]);
			while (!(success = SetupFindFirstLine(inf_handle, tmp, NULL, inf_context)))
			{
				field_index++;
				if (!model[field_index]) break;
				sprintf(tmp, "%s.%s", model[0], model[field_index]);
			}
		}

		// these were allocated with _strdup above.
		for (field_index=0; model[field_index] != NULL; field_index++)
			free(model[field_index]);

		// model-section-name or model-section not found
		if (!success)
		{
			USBERR0(".inf file does not contain a valid model-section-name\n");
		}
	}
	else
	{
		USBERR0(".inf file does not contain a valid Manufacturer section\n");
	}

	return success;
}

int usb_install_inf_np(const char *inf_file, bool_t remove_mode, bool_t copy_oem_inf_mode)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	INFCONTEXT inf_context;
	HINF inf_handle;
	DWORD config_flags, problem, status;
	BOOL reboot;
	char inf_path[MAX_PATH];
	char id[MAX_PATH];
	char tmp_id[MAX_PATH];
	char *p;
	int dev_index;
	HINSTANCE newdev_dll = NULL;
	HMODULE setupapi_dll = NULL;
	CONFIGRET cr;
	update_driver_for_plug_and_play_devices_t UpdateDriverForPlugAndPlayDevices;
	rollback_driver_t RollBackDriver;
	uninstall_device_t UninstallDevice;
	bool_t usb_reset_required = FALSE;
	int field_index;
	setup_copy_oem_inf_t SetupCopyOEMInf;

	newdev_dll = LoadLibrary("newdev.dll");
	if (!newdev_dll)
	{
		USBERR0("loading newdev.dll failed\n");
		return -1;
	}

	UpdateDriverForPlugAndPlayDevices = (update_driver_for_plug_and_play_devices_t)GetProcAddress(newdev_dll, "UpdateDriverForPlugAndPlayDevicesA");
	if (!UpdateDriverForPlugAndPlayDevices)
	{
		USBERR0("loading newdev.dll failed\n");
		return -1;
	}
	UninstallDevice = (uninstall_device_t)GetProcAddress(newdev_dll, "DiUninstallDevice");
	RollBackDriver = (rollback_driver_t)GetProcAddress(newdev_dll, "DiRollbackDriver");

	setupapi_dll = GetModuleHandle("setupapi.dll");
	if (!setupapi_dll)
	{
		USBERR0("loading setupapi.dll failed\n");
		return -1;
	}
	SetupCopyOEMInf = (setup_copy_oem_inf_t)GetProcAddress(setupapi_dll, "SetupCopyOEMInfA");
	if (!SetupCopyOEMInf)
	{
		USBERR0("loading setupapi.dll failed\n");
		return -1;
	}

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);


	/* retrieve the full .inf file path */
	if (!GetFullPathName(inf_file, MAX_PATH, inf_path, NULL))
	{
		USBERR(".inf file %s not found\n",
			inf_file);
		return -1;
	}

	/* open the .inf file */
	inf_handle = SetupOpenInfFile(inf_path, NULL, INF_STYLE_WIN4, NULL);

	if (inf_handle == INVALID_HANDLE_VALUE)
	{
		USBERR("unable to open .inf file %s\n",
			inf_file);
		return -1;
	}

	if (!usb_install_find_model_section(inf_handle, &inf_context))
	{
		SetupCloseInfFile(inf_handle);
		return -1;
	}

	do
	{
		/* get the device ID from the .inf file */

		field_index = 2;
		while(SetupGetStringField(&inf_context, field_index++, id, sizeof(id), NULL))
		{
			/* convert the string to lowercase */
			strlwr(id);

			if (strncmp(id,"usb\\", 4) != 0)
			{
				USBERR("invalid hardware id %s\n", id);
				SetupCloseInfFile(inf_handle);
				return -1;
			}

			USBMSG("%s device %s..\n",
				remove_mode ? "removing" : "installing", id+4);

			reboot = FALSE;

			if (!remove_mode)
			{
				if (copy_oem_inf_mode)
				{
					/* copy the .inf file to the system directory so that is will be found */
					/* when new devices are plugged in */
					if (SetupCopyOEMInf(inf_path, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL))
					{
						USBDBG("SetupCopyOEMInf failed for %s\n",inf_path);
					}
					else
					{
						USBDBG(".inf file %s copied to system directory\n",inf_path);
					}
				}
				/* update all connected devices matching this ID, but only if this */
				/* driver is better or newer */
				UpdateDriverForPlugAndPlayDevices(NULL, id, inf_path, INSTALLFLAG_FORCE,
					&reboot);
			}

			/* now search the registry for device nodes representing currently  */
			/* unattached devices or remove devices depending on the mode */

			/* get all USB device nodes from the registry, present and non-present */
			/* devices */
			dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);

			if (dev_info == INVALID_HANDLE_VALUE)
			{
				SetupCloseInfFile(inf_handle);
				return -1;
			}

			dev_index = 0;

			/* enumerate the device list to find all attached and unattached */
			/* devices */
			while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
			{
				/* get the harware ID from the registry, this is a multi-zero string */
				if (SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
					SPDRP_HARDWAREID, NULL,
					(BYTE *)tmp_id,
					sizeof(tmp_id), NULL))
				{
					/* check all possible IDs contained in that multi-zero string */
					for (p = tmp_id; *p; p += (strlen(p) + 1))
					{
						/* convert the string to lowercase */
						strlwr(p);

						/* found a match? */
						if (strstr(p, id))
						{
							if (remove_mode)
							{
								if (RollBackDriver)
								{
									if (RollBackDriver(dev_info, &dev_info_data, NULL, 0, &reboot))
									{
										break;
									}
									else
									{
										USBERR("failed driver rollback for device %s\n", p);
									}
								}
								if (UninstallDevice)
								{
									if (UninstallDevice(NULL, dev_info, &dev_info_data, 0, &reboot))
									{
										usb_reset_required = TRUE;
										break;
									}
									else
									{
										USBERR("failed unnistalling device %s\n", p);
									}
								}

								if (SetupDiRemoveDevice(dev_info, &dev_info_data))
								{
									usb_reset_required = TRUE;
									break;
								}
								else
								{
									USBERR("failed removing device %s\n", p);
								}

							}
							else
							{
								cr = CM_Get_DevNode_Status(&status,
									&problem,
									dev_info_data.DevInst,
									0);

								/* is this device disconnected? */
								if (cr == CR_NO_SUCH_DEVINST)
								{
									/* found a device node that represents an unattached */
									/* device */
									if (SetupDiGetDeviceRegistryProperty(dev_info,
										&dev_info_data,
										SPDRP_CONFIGFLAGS,
										NULL,
										(BYTE *)&config_flags,
										sizeof(config_flags),
										NULL))
									{
										/* mark the device to be reinstalled the next time it is */
										/* plugged in */
										config_flags |= CONFIGFLAG_REINSTALL;

										/* write the property back to the registry */
										SetupDiSetDeviceRegistryProperty(dev_info,
											&dev_info_data,
											SPDRP_CONFIGFLAGS,
											(BYTE *)&config_flags,
											sizeof(config_flags));
									}
								}
							}
							/* a match was found, skip the rest */
							break;
						}
					}
				}
				/* check the next device node */
				dev_index++;
			}

			SetupDiDestroyDeviceInfoList(dev_info);

			/* get the next device ID from the .inf file */
		}
	}
	while (SetupFindNextLine(&inf_context, &inf_context));

	/* we are done, close the .inf file */
	SetupCloseInfFile(inf_handle);

	if (usb_reset_required)
	{
		usb_registry_restart_all_devices();
	}
	else if (!remove_mode)
	{
		usb_registry_stop_libusb_devices(); /* stop all libusb devices */
		usb_registry_start_libusb_devices(); /* restart all libusb devices */
	}


	return 0;
}

int usb_install_driver_np(const char *inf_file)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	INFCONTEXT inf_context;
	HINF inf_handle;
	DWORD config_flags, problem, status;
	BOOL reboot;
	char inf_path[MAX_PATH];
	char id[MAX_PATH];
	char tmp_id[MAX_PATH];
	char *p;
	int dev_index;
	HINSTANCE newdev_dll = NULL;
	HMODULE setupapi_dll = NULL;
	CONFIGRET cr;

	update_driver_for_plug_and_play_devices_t UpdateDriverForPlugAndPlayDevices;
	setup_copy_oem_inf_t SetupCopyOEMInf;
	newdev_dll = LoadLibrary("newdev.dll");

	if (!newdev_dll)
	{
		USBERR0("loading newdev.dll failed\n");
		return -1;
	}

	UpdateDriverForPlugAndPlayDevices =
		(update_driver_for_plug_and_play_devices_t)
		GetProcAddress(newdev_dll, "UpdateDriverForPlugAndPlayDevicesA");

	if (!UpdateDriverForPlugAndPlayDevices)
	{
		USBERR0("loading newdev.dll failed\n");
		return -1;
	}

	setupapi_dll = GetModuleHandle("setupapi.dll");

	if (!setupapi_dll)
	{
		USBERR0("loading setupapi.dll failed\n");
		return -1;
	}
	SetupCopyOEMInf = (setup_copy_oem_inf_t)
		GetProcAddress(setupapi_dll, "SetupCopyOEMInfA");

	if (!SetupCopyOEMInf)
	{
		USBERR0("loading setupapi.dll failed\n");
		return -1;
	}

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);


	/* retrieve the full .inf file path */
	if (!GetFullPathName(inf_file, MAX_PATH, inf_path, NULL))
	{
		USBERR(".inf file %s not found\n",
			inf_file);
		return -1;
	}

	/* open the .inf file */
	inf_handle = SetupOpenInfFile(inf_path, NULL, INF_STYLE_WIN4, NULL);

	if (inf_handle == INVALID_HANDLE_VALUE)
	{
		USBERR("unable to open .inf file %s\n",
			inf_file);
		return -1;
	}

	/* find the .inf file's device description section marked "Devices" */
	if (!SetupFindFirstLine(inf_handle, "Devices", NULL, &inf_context))
	{
		USBERR(".inf file %s does not contain "
			"any device descriptions\n", inf_file);
		SetupCloseInfFile(inf_handle);
		return -1;
	}


	do
	{
		/* get the device ID from the .inf file */
		if (!SetupGetStringField(&inf_context, 2, id, sizeof(id), NULL))
		{
			continue;
		}

		/* convert the string to lowercase */
		strlwr(id);

		reboot = FALSE;

		/* copy the .inf file to the system directory so that is will be found */
		/* when new devices are plugged in */
		SetupCopyOEMInf(inf_path, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL);

		/* update all connected devices matching this ID, but only if this */
		/* driver is better or newer */
		UpdateDriverForPlugAndPlayDevices(NULL, id, inf_path, INSTALLFLAG_FORCE,
			&reboot);


		/* now search the registry for device nodes representing currently  */
		/* unattached devices */


		/* get all USB device nodes from the registry, present and non-present */
		/* devices */
		dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, DIGCF_ALLCLASSES);

		if (dev_info == INVALID_HANDLE_VALUE)
		{
			SetupCloseInfFile(inf_handle);
			break;
		}

		dev_index = 0;

		/* enumerate the device list to find all attached and unattached */
		/* devices */
		while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
		{
			/* get the harware ID from the registry, this is a multi-zero string */
			if (SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
				SPDRP_HARDWAREID, NULL,
				(BYTE *)tmp_id,
				sizeof(tmp_id), NULL))
			{
				/* check all possible IDs contained in that multi-zero string */
				for (p = tmp_id; *p; p += (strlen(p) + 1))
				{
					/* convert the string to lowercase */
					strlwr(p);

					/* found a match? */
					if (strstr(p, id))
					{
						cr = CM_Get_DevNode_Status(&status,
							&problem,
							dev_info_data.DevInst,
							0);

						/* is this device disconnected? */
						if (cr == CR_NO_SUCH_DEVINST)
						{
							/* found a device node that represents an unattached */
							/* device */
							if (SetupDiGetDeviceRegistryProperty(dev_info,
								&dev_info_data,
								SPDRP_CONFIGFLAGS,
								NULL,
								(BYTE *)&config_flags,
								sizeof(config_flags),
								NULL))
							{
								/* mark the device to be reinstalled the next time it is */
								/* plugged in */
								config_flags |= CONFIGFLAG_REINSTALL;

								/* write the property back to the registry */
								SetupDiSetDeviceRegistryProperty(dev_info,
									&dev_info_data,
									SPDRP_CONFIGFLAGS,
									(BYTE *)&config_flags,
									sizeof(config_flags));
							}
						}
						/* a match was found, skip the rest */
						break;
					}
				}
			}
			/* check the next device node */
			dev_index++;
		}

		SetupDiDestroyDeviceInfoList(dev_info);

		/* get the next device ID from the .inf file */
	}
	while (SetupFindNextLine(&inf_context, &inf_context));

	/* we are done, close the .inf file */
	SetupCloseInfFile(inf_handle);

	usb_registry_stop_libusb_devices(); /* stop all libusb devices */
	usb_registry_start_libusb_devices(); /* restart all libusb devices */

	return 0;
}


bool_t usb_service_load_dll()
{
	if (usb_registry_is_nt())
	{
		advapi32_dll = LoadLibrary("advapi32.dll");

		if (!advapi32_dll)
		{
			USBERR0("loading DLL advapi32.dll failed\n");
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

		if (!open_sc_manager || !open_service || !change_service_config
			|| !close_service_handle || !create_service || !delete_service
			|| !start_service || !query_service_status || !control_service)
		{
			FreeLibrary(advapi32_dll);
			USBERR0("loading exported functions of advapi32.dll failed");

			return FALSE;
		}
	}
	return TRUE;
}

bool_t usb_service_free_dll()
{
	if (advapi32_dll)
	{
		FreeLibrary(advapi32_dll);
	}
	return TRUE;
}

bool_t usb_service_create(const char *name, const char *display_name,
						  const char *binary_path, unsigned long type,
						  unsigned long start_type)
{
	SC_HANDLE scm = NULL;
	SC_HANDLE service = NULL;
	bool_t ret = FALSE;

	if (!usb_service_load_dll())
	{
		return FALSE;
	}

	do
	{
		scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE,
			SC_MANAGER_ALL_ACCESS);

		if (!scm)
		{
			USBERR("opening service control "
				"manager failed: %s", usb_win_error_to_string());
			break;
		}

		service = open_service(scm, name, SERVICE_ALL_ACCESS);

		if (service)
		{
			if (!change_service_config(service,
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
				USBERR("changing config of "
					"service '%s' failed: %s",
					name, usb_win_error_to_string());
				break;
			}
			ret = TRUE;
			break;
		}

		if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
		{
			service = create_service(scm,
				name,
				display_name,
				GENERIC_EXECUTE,
				type,
				start_type,
				SERVICE_ERROR_NORMAL,
				binary_path,
				NULL, NULL, NULL, NULL, NULL);

			if (!service)
			{
				USBERR("creating "
					"service '%s' failed: %s",
					name, usb_win_error_to_string());
			}
			ret = TRUE;
		}
	}
	while (0);

	if (service)
	{
		close_service_handle(service);
	}

	if (scm)
	{
		close_service_handle(scm);
	}

	usb_service_free_dll();

	return ret;
}

bool_t usb_service_stop(const char *name)
{
	bool_t ret = FALSE;
	SC_HANDLE scm = NULL;
	SC_HANDLE service = NULL;
	SERVICE_STATUS status;

	if (!usb_service_load_dll())
	{
		return FALSE;
	}

	USBMSG("stopping %s service..\n",name);

	do
	{
		scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE,
			SC_MANAGER_ALL_ACCESS);

		if (!scm)
		{
			USBERR("opening service control "
				"manager failed: %s", usb_win_error_to_string());
			break;
		}

		service = open_service(scm, name, SERVICE_ALL_ACCESS);

		if (!service)
		{
			ret = TRUE;
			break;
		}

		if (!query_service_status(service, &status))
		{
			USBERR("getting status of "
				"service '%s' failed: %s",
				name, usb_win_error_to_string());
			break;
		}

		if (status.dwCurrentState == SERVICE_STOPPED)
		{
			ret = TRUE;
			break;
		}

		if (!control_service(service, SERVICE_CONTROL_STOP, &status))
		{
			USBERR("stopping service '%s' failed: %s",
				name, usb_win_error_to_string());
			break;
		}

		do
		{
			int wait = 0;

			if (!query_service_status(service, &status))
			{
				USBERR("getting status of "
					"service '%s' failed: %s",
					name, usb_win_error_to_string());
				break;
			}
			Sleep(500);
			wait += 500;

			if (wait > 20000)
			{
				USBERR("stopping "
					"service '%s' failed, timeout", name);
				ret = FALSE;
				break;
			}
			ret = TRUE;
		}
		while (status.dwCurrentState != SERVICE_STOPPED);
	}
	while (0);

	if (service)
	{
		close_service_handle(service);
	}

	if (scm)
	{
		close_service_handle(scm);
	}

	usb_service_free_dll();

	return ret;
}

bool_t usb_service_delete(const char *name)
{
	bool_t ret = FALSE;
	SC_HANDLE scm = NULL;
	SC_HANDLE service = NULL;

	if (!usb_service_load_dll())
	{
		return FALSE;
	}

	USBMSG("deleting %s service..\n",name);

	do
	{
		scm = open_sc_manager(NULL, SERVICES_ACTIVE_DATABASE,
			SC_MANAGER_ALL_ACCESS);

		if (!scm)
		{
			USBERR("opening service control "
				"manager failed: %s", usb_win_error_to_string());
			break;
		}

		service = open_service(scm, name, SERVICE_ALL_ACCESS);

		if (!service)
		{
			ret = TRUE;
			break;
		}


		if (!delete_service(service))
		{
			USBERR("deleting "
				"service '%s' failed: %s",
				name, usb_win_error_to_string());
			break;
		}
		ret = TRUE;
	}
	while (0);

	if (service)
	{
		close_service_handle(service);
	}

	if (scm)
	{
		close_service_handle(scm);
	}

	usb_service_free_dll();

	return ret;
}

void CALLBACK usb_touch_inf_file_np_rundll(HWND wnd, HINSTANCE instance,
										   LPSTR cmd_line, int cmd_show)
{
	usb_touch_inf_file_np(cmd_line);
}

int usb_touch_inf_file_np(const char *inf_file)
{
	const char inf_comment[] = ";added by libusb to break this file's digital "
		"signature";
	const wchar_t inf_comment_uni[] = L";added by libusb to break this file's "
		L"digital signature";

	char buf[1024];
	wchar_t wbuf[1024];
	int found = 0;
	OSVERSIONINFO version;
	FILE *f;

	version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (!GetVersionEx(&version))
		return -1;


	/* XP system */
	if ((version.dwMajorVersion == 5) && (version.dwMinorVersion >= 1))
	{
		f = fopen(inf_file, "rb");

		if (!f)
			return -1;

		while (fgetws(wbuf, sizeof(wbuf)/2, f))
		{
			if (wcsstr(wbuf, inf_comment_uni))
			{
				found = 1;
				break;
			}
		}

		fclose(f);

		if (!found)
		{
			f = fopen(inf_file, "ab");
			/*           fputwc(0x000d, f); */
			/*           fputwc(0x000d, f); */
			fputws(inf_comment_uni, f);
			fclose(f);
		}
	}
	else
	{
		f = fopen(inf_file, "r");

		if (!f)
			return -1;

		while (fgets(buf, sizeof(buf), f))
		{
			if (strstr(buf, inf_comment))
			{
				found = 1;
				break;
			}
		}

		fclose(f);

		if (!found)
		{
			f = fopen(inf_file, "a");
			fputs("\n", f);
			fputs(inf_comment, f);
			fputs("\n", f);
			fclose(f);
		}
	}

	return 0;
}

int usb_install_needs_restart_np(void)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	int dev_index = 0;
	SP_DEVINSTALL_PARAMS install_params;
	int ret = FALSE;

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	dev_info = SetupDiGetClassDevs(NULL, NULL, NULL,
		DIGCF_ALLCLASSES | DIGCF_PRESENT);

	SetEnvironmentVariable("LIBUSB_NEEDS_REBOOT", "1");

	if (dev_info == INVALID_HANDLE_VALUE)
	{
		USBERR0("getting device info set failed\n");
		return ret;
	}

	while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
	{
		memset(&install_params, 0, sizeof(SP_PROPCHANGE_PARAMS));
		install_params.cbSize = sizeof(SP_DEVINSTALL_PARAMS);

		if (SetupDiGetDeviceInstallParams(dev_info, &dev_info_data,
			&install_params))
		{
			if (install_params.Flags & (DI_NEEDRESTART | DI_NEEDREBOOT))
			{
				USBMSG0("restart needed\n");
				ret = TRUE;
			}
		}

		dev_index++;
	}

	SetupDiDestroyDeviceInfoList(dev_info);

	return ret;
}

bool_t usb_install_get_filter_context(filter_context_t* filter_context, 
									  LPCWSTR cmd_line, 
									  int arg_start, 
									  int* arg_cnt)
{
	LONGGUID class_guid;
	int arg_pos;
	LPWSTR* argv = NULL;
	LPCWSTR arg_param, arg_value;
	bool_t success = TRUE;
	size_t length;
	char tmp[MAX_PATH+1];
	char* next_wild_char;
	filter_device_t* found_device;
	filter_class_t* found_class;
	filter_file_t* found_inf;

	*arg_cnt = 0;
	if (!(argv = CommandLineToArgvW(cmd_line, arg_cnt)))
	{
		USBERR("failed CommandLineToArgvW:%X",GetLastError());
		success = FALSE;
		goto Done;
	}

	if (*arg_cnt <= arg_start)
	{
		success = FALSE;
		goto Done;
	}

	for(arg_pos=arg_start; arg_pos < *arg_cnt; arg_pos++)
	{
		if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_help))
		{
			filter_context->show_help_only = TRUE;
			break;
		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_list))
		{
			filter_context->filter_mode |= FM_LIST;
			filter_context->filter_mode_main = FM_LIST;

		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_install))
		{
			filter_context->filter_mode_main = FM_INSTALL;
			filter_context->filter_mode |= FM_REMOVE | FM_INSTALL;
		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_uninstall))
		{
			filter_context->filter_mode_main = FM_REMOVE;
			filter_context->filter_mode |= FM_REMOVE;
		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_all_classes))
		{
			filter_context->switches.add_all_classes = TRUE;
		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_classes))
		{
			filter_context->switches.add_device_classes = TRUE;
		}
		else if 
			(
			usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_upper) ||
			usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_lower)
			)
		{
			length = wcstombs(tmp, arg_value, MAX_PATH);
			if (length < 1)
			{
				success = FALSE;
				USBERR("invalid argument %ls\n",argv[arg_pos]);
				break;
			}
			tmp[length] = 0;

			// replace '.' with '&' for hardware ids
			next_wild_char = tmp;
			while(*next_wild_char)
			{
				if (*next_wild_char == '.')
					*next_wild_char = '&';
				next_wild_char++;
			}

			usb_registry_add_filter_device_keys(&filter_context->device_filters, tmp, "", "", &found_device);

			if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_upper))
			{
				// upper device filter
				if (!found_device)
				{
					success = FALSE;
					USBERR("failed adding device upper filter key %ls\n",argv[arg_pos]);
					break;
				}
				else
				{
					found_device->filter_type |= FT_DEVICE_UPPERFILTER;
				}
			}
			else
			{
				// lower device filter
				if (!found_device)
				{
					success = FALSE;
					USBERR("failed adding device lower filter key %ls\n",argv[arg_pos]);
					break;
				}
				else
				{
					found_device->filter_type |= FT_DEVICE_LOWERFILTER;
				}
			}
		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_class))
		{
			memset(tmp,0,sizeof(tmp));
			length = wcstombs(tmp, arg_value, MAX_PATH);
			if (length < 1)
			{
				success = FALSE;
				USBERR("invalid argument %ls\n",argv[arg_pos]);
				break;
			}
			tmp[length] = 0;

			if ((length != 38 || tmp[0] != '{' || tmp[length-1] != '}') ||
				sscanf(tmp, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
				&class_guid.Data1, &class_guid.Data2, &class_guid.Data3, 
				&class_guid.Data4[0], &class_guid.Data4[1], &class_guid.Data4[2], &class_guid.Data4[3], 
				&class_guid.Data4[4], &class_guid.Data4[5], &class_guid.Data4[6], &class_guid.Data4[7]) != 11)
			{
				// assume arg_value is a class name
				success = usb_registry_add_class_key(&filter_context->class_filters, 
					"", tmp, "", &found_class, FALSE);
				if (!success)
				{
					USBERR("failed adding class name at argument %ls\n",argv[arg_pos]);
					break;
				}
			}
			else 
			{
				sprintf(tmp,"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
					class_guid.Data1, class_guid.Data2, class_guid.Data3, 
					class_guid.Data4[0], class_guid.Data4[1], class_guid.Data4[2], class_guid.Data4[3], 
					class_guid.Data4[4], class_guid.Data4[5], class_guid.Data4[6], class_guid.Data4[7]);

				// assume arg_value is a class guid
				success = usb_registry_add_usb_class_key(filter_context, tmp);
				if (!success)
				{
					USBERR("failed adding class guid at argument %ls\n",argv[arg_pos]);
					break;
				}
			}
		}
		else if (usb_install_get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_inf))
		{
			memset(tmp,0,sizeof(tmp));
			length = wcstombs(tmp, arg_value, MAX_PATH);
			if (length < 1)
			{
				success = FALSE;
				USBERR("invalid argument %ls\n",argv[arg_pos]);
				break;
			}
			tmp[length] = 0;

			usb_registry_add_filter_file_keys(&filter_context->inf_files, tmp, &found_inf);
			if (!found_inf)
			{
				success = FALSE;
				USBERR("failed adding inf %ls\n",argv[arg_pos]);
				break;
			}
		}
		else
		{
			USBERR("invalid argument %ls\n",argv[arg_pos]);
			success = FALSE;
			break;
		}
	}

Done:
	if (success && filter_context->class_filters)
	{
		// find class keys (fill in the blanks) 
		usb_registry_lookup_class_keys_by_name(&filter_context->class_filters);
	}
	if (argv)
	{
		LocalFree(argv);
	}
	return success;
}

void CALLBACK usb_install_np_rundll(HWND wnd, HINSTANCE instance, LPSTR cmd_line, int cmd_show)
{
	WCHAR cmd_line_w[MAX_PATH+1];
	size_t length;

	memset(cmd_line_w,0,sizeof(cmd_line_w));

	if (cmd_line && strlen(cmd_line))
	{
		if ((length = mbstowcs(cmd_line_w, cmd_line, MAX_PATH)) < 1)
		{
			return;
		}
		cmd_line_w[length] = 0;
	}
	usb_install_np(wnd, instance, cmd_line_w, 0);
}

int usb_install_np(HWND hwnd, HINSTANCE instance, LPCWSTR cmd_line_w, int starg_arg)
{
	return usb_install(hwnd, instance, cmd_line_w, starg_arg, NULL);
}

void usb_install_destroy_filter_context(filter_context_t** filter_context)
{
	if ((filter_context) && *filter_context)
	{
		usb_registry_free_class_keys(&(*filter_context)->class_filters);
		usb_registry_free_filter_devices(&(*filter_context)->device_filters);
		usb_registry_free_filter_files(&(*filter_context)->inf_files);
		free(*filter_context);
		*filter_context = NULL;
	}
}

static void center_dialog(HWND hWndToCenterOn, HWND hWndSubDialog)
{
	RECT rectToCenterOn;
	RECT rectSubDialog;
	int xLeft, yTop;

	if (hWndToCenterOn == NULL || hWndSubDialog == NULL)
		return;

	GetWindowRect(hWndToCenterOn, &rectToCenterOn);
	GetWindowRect(hWndSubDialog, &rectSubDialog);

	if ((rectToCenterOn.right - rectToCenterOn.left) < (rectSubDialog.right - rectSubDialog.left) ||
		rectToCenterOn.left < 0 || 
		rectToCenterOn.top < 0 ||
		(rectToCenterOn.bottom - rectToCenterOn.top) < (rectSubDialog.bottom - rectSubDialog.top))
	{
		hWndToCenterOn = GetDesktopWindow();
		GetWindowRect(hWndToCenterOn, &rectToCenterOn);
	}

	xLeft = (rectToCenterOn.left + rectToCenterOn.right) / 2 - (rectSubDialog.right-rectSubDialog.left) / 2;
	yTop = (rectToCenterOn.top + rectToCenterOn.bottom) / 2 - (rectSubDialog.bottom-rectSubDialog.top) / 2;

	// Move the window to the correct coordinates with SetWindowPos()
	SetWindowPos(hWndSubDialog, HWND_TOP, xLeft, yTop, -1, -1, SWP_NOSIZE | SWP_SHOWWINDOW);
}

static BOOL usb_install_log_handler(enum USB_LOG_LEVEL level, 
									const char* app_name, 
									const char* prefix, 
									const char* func, 
									const int app_prefix_func_end,
									char* message,
									const int message_length)
{
	HANDLE std_handle;
	DWORD length;

	if (g_install_progress_context.progress_hwnd)
	{
		SendMessageA(g_install_progress_context.hwnd_progress_label,
			WM_SETTEXT,0,(LPARAM)(message+app_prefix_func_end));
		UpdateWindow(g_install_progress_context.progress_hwnd);
		if ((level & LOG_LEVEL_MASK)==LOG_ERROR)
		{
			Sleep(1500);
		}
		else
		{
			Sleep(1);
		}
	}
	else
	{
		if ((level & LOG_LEVEL_MASK)==LOG_ERROR)
			std_handle = GetStdHandle(STD_ERROR_HANDLE);
		else
			std_handle = GetStdHandle(STD_OUTPUT_HANDLE);

		if (std_handle && std_handle != INVALID_HANDLE_VALUE)
		{
			WriteConsoleA(std_handle,message,(DWORD)message_length,&length,NULL);
		}
	}
	return TRUE;
}

int usb_install_console(filter_context_t* filter_context)
{
	filter_file_t* filter_file;
	int ret = 0;

	if (!filter_context->filter_mode)
	{
		USBERR("command not specified. Use %ls, %ls, or %ls.\n",
			paramcmd_list[0], paramcmd_install[0], paramcmd_uninstall[0]);
		ret = -1;
		goto Done;

	}
	/* only add the default class keys if there is nothing else to do. */
	if (filter_context->class_filters || 
		filter_context->device_filters ||
		filter_context->inf_files ||
		filter_context->switches.add_all_classes || 
		filter_context->switches.add_device_classes)
	{
		filter_context->switches.add_default_classes = FALSE;
	}
	else
	{
		filter_context->switches.add_default_classes = TRUE;

	}

	while (filter_context->filter_mode)
	{
		bool_t refresh_only;
		if (filter_context->class_filters && (filter_context->switches.add_all_classes || filter_context->switches.add_device_classes))
		{
			usb_registry_free_class_keys(&filter_context->class_filters);
		}

		if (filter_context->filter_mode & FM_REMOVE)
		{
			filter_context->active_filter_mode = FM_REMOVE;

			if (filter_context->switches.switches_value || 
				filter_context->class_filters ||
				filter_context->device_filters)
			{

				if (filter_context->switches.add_all_classes || filter_context->switches.add_device_classes)
					refresh_only = FALSE;
				else
					refresh_only = TRUE;

				if (!usb_registry_get_usb_class_keys(filter_context, refresh_only))
				{
					ret = -1;
					break;
				}

				if (!usb_registry_get_all_class_keys(filter_context, refresh_only))
				{
					ret = -1;
					break;
				}
				ret = usb_uninstall_service(filter_context);
				if (ret < 0)
				{
					break;
				}
			}
			if (filter_context->filter_mode_main == FM_REMOVE)
			{
				filter_file = filter_context->inf_files;
				while (filter_file)
				{
					USBMSG("uninstalling inf %s..\n",filter_file->name);
					if (usb_install_inf_np(filter_file->name, TRUE, FALSE) < 0)
					{
						ret = -1;
						break;
					}
					filter_file = filter_file->next; 
				}
				if (ret == -1)
					break;
			}
		}
		else if (filter_context->filter_mode & FM_INSTALL)
		{
			filter_context->active_filter_mode = FM_INSTALL;

			if (filter_context->switches.switches_value || 
				filter_context->class_filters ||
				filter_context->device_filters)
			{

				if (filter_context->switches.add_all_classes || filter_context->switches.add_device_classes)
					refresh_only = FALSE;
				else
					refresh_only = TRUE;

				if (!usb_registry_get_usb_class_keys(filter_context, refresh_only))
				{
					ret = -1;
					break;
				}

				if (!usb_registry_get_all_class_keys(filter_context, refresh_only))
				{
					ret = -1;
					break;
				}

				ret = usb_install_service(filter_context);
				if (ret < 0)
				{
					break;
				}
			}
			if (filter_context->filter_mode_main == FM_INSTALL)
			{
				filter_file = filter_context->inf_files;
				while (filter_file)
				{
					USBMSG("installing inf %s..\n",filter_file->name);
					if (usb_install_inf_np(filter_file->name, FALSE, FALSE) < 0)
					{
						ret = -1;
						break;
					}
					filter_file = filter_file->next; 
				}
				if (ret == -1)
					break;
			}
		}
		else if (filter_context->filter_mode & FM_LIST)
		{
			filter_context->active_filter_mode = FM_LIST;

			if (!usb_registry_get_usb_class_keys(filter_context, TRUE))
			{
				ret = -1;
				break;
			}

			if (!usb_registry_get_all_class_keys(filter_context, TRUE))
			{
				ret = -1;
				break;
			}

			usb_install_report(filter_context);
		}

		filter_context->filter_mode ^= filter_context->active_filter_mode;
		filter_context->active_filter_mode = 0;

	}

Done:
	return ret;
}

void __cdecl usb_progress_thread(void* param)
{
	install_progress_context_t* context = (install_progress_context_t*)param;

	context->ret = usb_install_console(context->filter_context);
	PostMessage(context->progress_hwnd, UM_PROGRESS_STOP, (WPARAM)context->ret, 0);
	_endthread();
}

LRESULT CALLBACK usb_progress_wndproc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{

	case WM_CREATE:
		g_install_progress_context.progress_hwnd = hDlg;
		PostMessage(hDlg, UM_PROGRESS_START, 0, 0);
		return (INT_PTR)TRUE;

	case UM_PROGRESS_START:
		if (g_install_progress_context.thread_id == 0)
		{
			// Using a thread prevents application freezout on security warning
			g_install_progress_context.thread_id = _beginthread(usb_progress_thread, 0, &g_install_progress_context);
			if (g_install_progress_context.thread_id != -1L)
			{
				return (INT_PTR)TRUE;
			}
		}
		// Fall through and return an error
		wParam = (WPARAM)-1;

	case UM_PROGRESS_STOP:
		PostQuitMessage((int)wParam);
		DestroyWindow(g_install_progress_context.progress_hwnd);
		return (INT_PTR)TRUE;
	case WM_CLOSE:		// prevent closure using Alt-F4
		return (INT_PTR)TRUE;

	case WM_DESTROY:	// close application
		memset(&g_install_progress_context,0,sizeof(g_install_progress_context));
		return (INT_PTR)FALSE;

	}
	return DefWindowProc(hDlg, message, wParam, lParam);
}

int usb_install_window(HWND hWnd, HINSTANCE instance, filter_context_t* filter_context)
{
	MSG msg;
	WNDCLASSEX wc;
	BOOL bRet;
	HFONT labelFont;

	if (!hWnd || !instance)
		return -1;

	memset(&g_install_progress_context, 0, sizeof(g_install_progress_context));
	g_install_progress_context.instance = instance;
	g_install_progress_context.parent = hWnd;
	g_install_progress_context.filter_context = filter_context;

	memset(&wc,0,sizeof(wc));

	// First we create  Window class if it doesn't already exist
	if (!GetClassInfoExA(instance, "libusbwin32_progress_class", &wc)) 
	{
		wc.cbSize        = sizeof(WNDCLASSEX);
		wc.style         = CS_DBLCLKS | CS_SAVEBITS;
		wc.lpfnWndProc   = usb_progress_wndproc;
		wc.cbClsExtra    = wc.cbWndExtra = 0;
		wc.hInstance     = GetModuleHandle(NULL);
		wc.hIcon         = wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
		wc.lpszClassName = "libusbwin32_progress_class";
		wc.lpszMenuName  = NULL;
		wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);

		if (!RegisterClassExA(&wc)) 
		{
			USBERR0("can't register class\n");
			memset(&g_install_progress_context,0,sizeof(g_install_progress_context));
			return -1;
		}
	}

	// Then we create the dialog base
	g_install_progress_context.progress_hwnd = CreateWindowExA(WS_EX_WINDOWEDGE | WS_EX_CONTROLPARENT,
		"libusbwin32_progress_class", "libusb-win32 installer...",
		WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_CAPTION | WS_POPUP | WS_THICKFRAME,
		100, 100, 287, 102, hWnd, NULL, instance, NULL);
	if (g_install_progress_context.progress_hwnd == NULL) 
	{
		USBERR0("Unable to create progress dialog\n");
		memset(&g_install_progress_context,0,sizeof(g_install_progress_context));
		return -1;
	}

	g_install_progress_context.hwnd_progress_label = CreateWindowA("Static", "please wait..",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		5, 5, 277, 92,
		g_install_progress_context.progress_hwnd,
		(HMENU)((UINT_PTR)IDL_INSTALL_PROGRESS_TEXT),
		instance, 0);

	// Set the font to MS Dialog default
	INIT_CREATEFONT;
	if (IS_CREATEFONT_AVAILABLE) 
	{
		labelFont = pCreateFontA(-11, 0, 0, 0, FW_DONTCARE, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH, "MS Shell Dlg 2");
		SendMessage(g_install_progress_context.hwnd_progress_label, WM_SETFONT, (WPARAM)labelFont, (LPARAM)FALSE);
	}

	center_dialog(hWnd, g_install_progress_context.progress_hwnd);

	if (GetDesktopWindow() != g_install_progress_context.parent)
		EnableWindow(g_install_progress_context.parent, FALSE);	// Start modal (disable main Window)

	UpdateWindow(g_install_progress_context.progress_hwnd);

	// ...and handle the message processing loop
	while( (bRet = GetMessage(&msg, NULL, 0, 0)) != 0) 
	{
		if (bRet == -1) 
		{
			//wdi_err("GetMessage error");
		} else 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if (GetDesktopWindow() != g_install_progress_context.parent)
		EnableWindow(g_install_progress_context.parent, TRUE);	// Start modal (disable main Window)

	return g_install_progress_context.ret;
}

int usb_install(HWND hwnd, HINSTANCE instance, 
				LPCWSTR cmd_line_w, 
				int starg_arg, 
				filter_context_t** out_filter_context)
{

	filter_context_t* filter_context;
	int ret = ERROR_SUCCESS;
	HANDLE sem_handle = NULL;
	int arg_cnt;

	if (out_filter_context)
		*out_filter_context = NULL;

	if (usb_install_iswow64())
	{
		USBERR0("This is a 64bit operating system and requires the 64bit " LOG_APPNAME " application.\n");
		return -1;
	}

	if (!usb_install_admin_check())
	{
		USBERR0(LOG_APPNAME " requires adminstrator privileges.\n");
		return -1;
	}

	// create a named semaphore
	if (!sem_create_lock(&sem_handle, install_lock_sem_name, 1, 1))
		return -1;

	// lock the semaphore 
	if (!sem_try_lock(sem_handle, 0))
	{
		sem_destroy_lock(&sem_handle);
		return -1;
	}

	// use the log handler to report progress status
	if (!usb_log_get_handler())
	{
		usb_log_set_handler(usb_install_log_handler);
		usb_log_set_level(LOG_DEBUG);
	}
	else
	{
		ret = -1;
		goto Done;
	}

	// allocate the filter context
	filter_context = (filter_context_t*)malloc(sizeof(filter_context_t));
	if (!filter_context)
	{
		USBERR0("memory allocation failure\n");
		ret = -1;
		goto Done;
	}
	memset(filter_context, 0, sizeof(filter_context_t));

	// Fill the filter context from the command line arguments.
	if (!(usb_install_get_filter_context(filter_context, cmd_line_w, starg_arg, &arg_cnt)))
	{
		if (arg_cnt <= starg_arg)
		{
			usage();
		}
		ret = -1;
		goto Done;
	}

	if (filter_context->show_help_only)
	{
		usage();
		ret = -1;
		goto Done;
	}

	if (hwnd && instance)
	{
		// Using windowed install mode.
		ret = usb_install_window(hwnd, instance, filter_context);
	}
	else
	{
		// Using console install mode.
		ret = usb_install_console(filter_context);
	}

Done:
	if (out_filter_context && ret == ERROR_SUCCESS)
	{
		// If an out filter context was specified, return it.
		// The use is responsible for freeing.
		*out_filter_context = filter_context;
	}
	else
	{
		// Free the filter context.
		usb_install_destroy_filter_context(&filter_context);
		if (out_filter_context)
		{
			*out_filter_context = NULL;
		}
	}

	// Restore the default log handler.
	usb_log_set_handler(NULL);

	// Close (release) the semaphore lock.
	sem_destroy_lock(&sem_handle);

	return ret;

}

static bool_t usb_install_get_argument(LPWSTR param_value, LPCWSTR* out_param,  LPCWSTR* out_value, LPCWSTR* param_names)
{
	LPCWSTR param_name;
	int param_name_length;
	int param_name_pos = 0;
	int ret;

	if (!param_value || !out_param || !out_value)
		return FALSE;

	*out_param = 0;
	*out_value = 0;

	while((param_name=param_names[param_name_pos]))
	{
		if (!(param_name_length = (int)wcslen(param_name)))
			return FALSE;

		if (param_name[param_name_length-1]=='=')
		{
			ret = _wcsnicmp(param_value,param_name,param_name_length);
		}
		else
		{
			ret = _wcsicmp(param_value,param_name);
		}
		if (ret == 0)
		{
			*out_param = param_names[param_name_pos];
			*out_value = param_value + param_name_length;
			return TRUE;
		}

		param_name_pos++;
	}

	return FALSE;
}

void usb_install_report(filter_context_t* filter_context)
{
	filter_class_t* next_class;
	filter_device_t* next_device;

	next_class = filter_context->class_filters;
	while (next_class)
	{
		if (next_class->class_filter_devices)
		{
			fprintf(stdout, "\n");
		}

		fprintf(stdout, "%s (%s)\n",
			next_class->class_guid, next_class->class_name);

		next_device = next_class->class_filter_devices;
		while(next_device)
		{

			fprintf(stdout, "    %s - %s (%s)\n",
				next_device->device_hwid, next_device->device_name, next_device->device_mfg);

			next_device = next_device->next;
		}
		next_class = next_class->next;

	}
}

static bool_t usb_install_iswow64(void)
{
	HMODULE kernel_dll;
	is_wow64_process_t IsWow64Process;
	BOOL IsWow64 = FALSE;

	kernel_dll = GetModuleHandleA("kernel32.dll");
	if (!kernel_dll)
	{
		USBERR0("loading kernel32.dll failed\n");
		return FALSE;
	}

	IsWow64Process =(is_wow64_process_t) GetProcAddress(kernel_dll, "IsWow64Process");
	if (IsWow64Process)
	{
		if (!IsWow64Process(GetCurrentProcess(), &IsWow64))
		{
			// handle error
			IsWow64 = FALSE;
		}
	}
	return IsWow64;
}

static BOOL usb_install_admin_check(void)
/*++ 
Routine Description: This routine returns TRUE if the caller's
process is a member of the Administrators local group. Caller is NOT
expected to be impersonating anyone and is expected to be able to
open its own process and process token. 
Arguments: None. 
Return Value: 
TRUE - Caller has Administrators local group. 
FALSE - Caller does not have Administrators local group. --
*/ 
{
	BOOL b;
	SID_IDENTIFIER_AUTHORITY NtAuthority = {SECURITY_NT_AUTHORITY};
	PSID AdministratorsGroup; 

	GET_WINDOWS_VERSION;
	if (windows_version <= WINDOWS_XP)
	{
		return TRUE;
	}

	b = AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup); 
	if(b) 
	{
		if (!CheckTokenMembership( NULL, AdministratorsGroup, &b)) 
		{
			b = FALSE;
		} 
		FreeSid(AdministratorsGroup); 
	}

	return(b);
}

void usage(void)
{
#define ID_HELP_TEXT  10020
#define ID_DOS_TEXT   300

	CONST CHAR* src;
	DWORD src_count, charsWritten;
	HGLOBAL res_data;
	HANDLE handle;
	HRSRC hSrc;

	if ((handle=GetStdHandle(STD_ERROR_HANDLE)) == INVALID_HANDLE_VALUE)
		return;

	hSrc = FindResourceA(NULL, MAKEINTRESOURCEA(ID_HELP_TEXT),MAKEINTRESOURCEA(ID_DOS_TEXT));
	if (!hSrc)	return;

	src_count = SizeofResource(NULL, hSrc);

	res_data = LoadResource(NULL, hSrc);
	if (!res_data)	return;

	src = (char*) LockResource(res_data);
	if (!src) return;

	WriteConsoleA(handle, src, src_count, &charsWritten, NULL);
}
