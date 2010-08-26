/* libusb-win32, Generic Windows USB Library
* Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
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

static bool_t get_argument(LPWSTR param_value, LPCWSTR* out_param,  LPCWSTR* out_value, LPCWSTR* param_names);
void usb_install_report(filter_context_t* filter_context);
int usb_install(HWND hwnd, LPCWSTR cmd_line_w, int starg_arg, filter_context_t** out_filter_context);
void usage(void);

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


static bool_t usb_service_load_dll(void);
static bool_t usb_service_free_dll(void);

static bool_t usb_service_create(const char *name, const char *display_name,
                                 const char *binary_path, unsigned long type,
                                 unsigned long start_type);
static bool_t usb_service_stop(const char *name);
static bool_t usb_service_delete(const char *name);

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
    return usb_install_np(NULL,L"install", 0);
}

int usb_uninstall_service(filter_context_t* filter_context)
{
    HKEY reg_key = NULL;

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
    return usb_install_np(NULL,L"uninstall",0);
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

    UninstallDevice =
        (uninstall_device_t)
        GetProcAddress(newdev_dll, "DiUninstallDevice");

    RollBackDriver =
        (rollback_driver_t)
        GetProcAddress(newdev_dll, "DiRollbackDriver");

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

        if (!remove_mode)
        {
            if (copy_oem_inf_mode)
            {
                /* copy the .inf file to the system directory so that is will be found */
                /* when new devices are plugged in */
                SetupCopyOEMInf(inf_path, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL);
            }
            /* update all connected devices matching this ID, but only if this */
            /* driver is better or newer */
            UpdateDriverForPlugAndPlayDevices(NULL, id, inf_path, INSTALLFLAG_FORCE,
                &reboot);
        }

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

                        if (remove_mode)
                        {
                            if (RollBackDriver)
                            {
                                if (RollBackDriver(dev_info, &dev_info_data, NULL, 0, &reboot))
                                {
                                    usb_registry_restart_all_devices();
                                    break;
                                }
                                else
                                {
                                    USBERR("failed driver rollback for device %s\n",id);
                                }
                            }
                            if (UninstallDevice)
                            {
                                if (UninstallDevice(NULL, dev_info, &dev_info_data, 0, &reboot))
                                {
                                    usb_registry_restart_all_devices();
                                    break;
                                }
                                else
                                {
                                    USBERR("failed unnistalling device %s\n",id);
                                }
                            }
                            
                            if (SetupDiRemoveDevice(dev_info, &dev_info_data))
                            {
                                usb_registry_restart_all_devices();
                                break;
                            }
                            else
                            {
                                USBERR("failed removing device %s\n",id);
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
    while (SetupFindNextLine(&inf_context, &inf_context));

    /* we are done, close the .inf file */
    SetupCloseInfFile(inf_handle);

    usb_registry_stop_libusb_devices(); /* stop all libusb devices */
    usb_registry_start_libusb_devices(); /* restart all libusb devices */

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

bool_t usb_filter_context_from_cmdline(filter_context_t* filter_context, 
                                      LPCWSTR cmd_line, 
                                      int arg_start, 
                                      int* arg_cnt)
{
    int arg_pos;
    LPWSTR* argv = NULL;
    LPCWSTR arg_param, arg_value;
    bool_t success = TRUE;
    int length;
    char tmp[MAX_PATH+1];
    char* next_wild_char;
    filter_device_t* found_device;
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
        if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_list))
        {
            filter_context->filter_mode |= FM_LIST;
            filter_context->filter_mode_main = FM_LIST;

        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_install))
        {
            filter_context->filter_mode_main = FM_INSTALL;
            filter_context->filter_mode |= FM_REMOVE | FM_INSTALL;
        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramcmd_uninstall))
        {
            filter_context->filter_mode_main = FM_REMOVE;
            filter_context->filter_mode |= FM_REMOVE;
        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_all_classes))
        {
            filter_context->switches.add_all_classes = TRUE;
        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_classes))
        {
            filter_context->switches.add_device_classes = TRUE;
        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_upper))
        {
            memset(tmp,0,sizeof(tmp));
            length = WideCharToMultiByte(CP_ACP,0,
                arg_value,(int)wcslen(arg_value),
                tmp,MAX_PATH,
                NULL,NULL);

            if (length != wcslen(arg_value))
            {
                USBERR("failed WideCharToMultiByte %ls\n",argv[arg_pos]);
                success = FALSE;
                break;
            }

            next_wild_char = tmp;
            while(*next_wild_char)
            {
                if (*next_wild_char == '.')
                    *next_wild_char = '&';
                next_wild_char++;
            }
            usb_registry_add_filter_device_keys(&filter_context->device_filters, tmp, "", "", &found_device);
            if (!found_device)
            {
                success = FALSE;
                USBERR("failed adding device upper filter %ls\n",argv[arg_pos]);
                break;
            }
            found_device->filter_type|=FT_DEVICE_UPPERFILTER;
        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_device_lower))
        {
            memset(tmp,0,sizeof(tmp));
            length = WideCharToMultiByte(CP_ACP,0,
                arg_value,(int)wcslen(arg_value),
                tmp,MAX_PATH,
                NULL,NULL);
            if (length != wcslen(arg_value))
            {
                success = FALSE;
                USBERR("invalid argument %ls\n",argv[arg_pos]);
                break;
            }

            usb_registry_add_filter_device_keys(&filter_context->device_filters, tmp, "", "", &found_device);
            if (!found_device)
            {
                success = FALSE;
                USBERR("invalid argument %ls\n",argv[arg_pos]);
                break;
            }
            found_device->filter_type|=FT_DEVICE_LOWERFILTER;
        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_class))
        {

            if ((!arg_value) || wcslen(arg_value) != 38)
            {
               USBERR("invalid guid length %ls\n",argv[arg_pos]);
               success = FALSE;
            }
            if (arg_value[0]!='{' || arg_value[wcslen(arg_value)-1]!='}')
            {
                USBERR("invalid guid terminators. use '{' and '}'. %ls\n",argv[arg_pos]);
                success = FALSE;
            }
            if (!success)
            {
                break;
            }
            memset(tmp,0,sizeof(tmp));
            length = WideCharToMultiByte(CP_ACP,0,
                arg_value,(int)wcslen(arg_value),
                tmp,MAX_PATH,
                NULL,NULL);

            if (length != wcslen(arg_value))
            {
                USBERR("failed WideCharToMultiByte %ls\n",argv[arg_pos]);
                success = FALSE;
                break;
            }
            usb_registry_add_usb_class_key(filter_context,tmp);

        }
        else if (get_argument(argv[arg_pos], &arg_param, &arg_value, paramsw_inf))
        {
            memset(tmp,0,sizeof(tmp));
            length = WideCharToMultiByte(CP_ACP,0,
                arg_value,(int)wcslen(arg_value),
                tmp,MAX_PATH,
                NULL,NULL);

            if (length != wcslen(arg_value))
            {
                USBERR("failed WideCharToMultiByte %ls\n",argv[arg_pos]);
                success = FALSE;
                break;
            }

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
    if (argv)
    {
        LocalFree(argv);
    }
    return success;
}

void CALLBACK usb_install_np_rundll(HWND wnd, HINSTANCE instance, LPSTR cmd_line, int cmd_show)
{
    WCHAR cmd_line_w[MAX_PATH+1];
    int length;

    if (!cmd_line)
    {
        if (wnd)
        {
            MessageBoxA(wnd, "invalid arguments.", "install-filter", MB_OK|MB_ICONERROR);
        }
        return;
    }

    memset(cmd_line_w,0,sizeof(cmd_line_w));
    length = MultiByteToWideChar(CP_ACP,0,cmd_line,(int)strlen(cmd_line),cmd_line_w,MAX_PATH);
    if (length <= 0) return;

    usb_install_np(wnd, cmd_line_w, 0);
}

int usb_install_np(HWND hwnd, LPCWSTR cmd_line_w, int starg_arg)
{
    return usb_install(hwnd, cmd_line_w, starg_arg, NULL);
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

int usb_install(HWND hwnd, LPCWSTR cmd_line_w, int starg_arg, filter_context_t** out_filter_context)
{

    filter_context_t* filter_context;
    filter_file_t* filter_file;

    int ret = ERROR_SUCCESS;
    int arg_cnt;

    if (out_filter_context)
        *out_filter_context = NULL;

    filter_context = (filter_context_t*)malloc(sizeof(filter_context_t));
    if (!filter_context)
    {
        USBERR0("memory allocation failure\n");
        return -1;
    }
    memset(filter_context, 0, sizeof(filter_context_t));

    if (!(usb_filter_context_from_cmdline(filter_context, cmd_line_w, starg_arg, &arg_cnt)))
    {
        if (arg_cnt <= starg_arg)
        {
            usage();
        }
        return -1;
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

    if (out_filter_context && ret == ERROR_SUCCESS)
    {
        *out_filter_context = filter_context;
    }
    else
    {
        usb_install_destroy_filter_context(&filter_context);
    }

    return ret;
}

static bool_t get_argument(LPWSTR param_value, LPCWSTR* out_param,  LPCWSTR* out_value, LPCWSTR* param_names)
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
