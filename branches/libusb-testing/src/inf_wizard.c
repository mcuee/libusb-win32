/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2006 Stephan Meyer <ste_meyer@web.de>
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

#ifdef __GNUC__
#define _WIN32_IE 0x0400
#define WINVER 0x0500
#endif

#define INITGUID
#include "libusb_version.h"
#include "tokenizer.h"
#include "error.h"

#include <windows.h>
#include <commdlg.h>
#include <dbt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initguid.h>
#include <commctrl.h>
#include <setupapi.h>
#include <time.h>
#include "registry.h"

#define __INF_WIZARD_C__
#include "inf_wizard_rc.rc"

enum LIBUSB_INF_TAGS
{
	INF_FILENAME,
	CAT_FILENAME,
	BASE_FILENAME,
	HARDWAREID,
	DRIVER_DATE,
	DRIVER_VERSION,
	DEVICE_MANUFACTURER,
	DEVICE_INTERFACE_GUID,
	DEVICE_DESCRIPTION,
};
/* AUTOGEN MSVC REGEXPS (use against LIBUSB_INF_TAGS)
Find:
{[A-Za-z0-9_]+},
Repl:
{"\1",""},
*/
token_entity_t libusb_inf_entities[]=
{
	{"INF_FILENAME",""},
	{"CAT_FILENAME",""},
	{"BASE_FILENAME",""},
	{"HARDWAREID",""},
	{"DRIVER_DATE",""},
	{"DRIVER_VERSION",""},
	{"DEVICE_MANUFACTURER",""},
	{"DEVICE_INTERFACE_GUID",""},
	{"DEVICE_DESCRIPTION",""},

	{NULL} // DO NOT REMOVE!
};

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

static char installnow_inf[MAX_PATH]={0};

DEFINE_GUID(GUID_DEVINTERFACE_USB_HUB, 0xf18a0e88, 0xc30c, 0x11d0, 0x88, \
            0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8);

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, \
            0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

const char cat_file_content[] =
    "This file will contain the digital signature of the files to be installed\n"
    "on the system.\n"
    "This file will be provided by Microsoft upon certification of your "
    "drivers.\n";

const char info_text_0[] =
    "This program will create an .inf file for your device.\n\n"
    "Before clicking \"Next\" make sure that your device is connected to the "
    "system.\n";

#ifdef EMBEDDED_LIBUSB_WIN32
const char info_text_1[] =
    "A driver install package has been created successfully for the following "
	"device. This package contains an .inf file, a .cat file, and the libusb-win32 v"
	RC_VERSION_STR " binaries:\n\n";
#else
const char info_text_1[] =
    "An .inf and .cat file has been created successfully for the following "
    "device:\n\n";
#endif

const char list_header_text[] =
    "Select your device from the list of detected devices below.\n"
    "If your device isn't listed then either connect it or just click \"Next\"\n"
    "and enter your device description manually\n";

const char strings_header[] =
    "\n"
    ";--------------------------------------------------------------------------\n"
    "; Strings\n"
    ";--------------------------------------------------------------------------\n"
    "\n"
    "[Strings]\n";

typedef struct
{
    int vid;
    int pid;
	int rev;
    int mi;
    char description[MAX_PATH];
    char manufacturer[MAX_PATH];
} device_context_t;


BOOL CALLBACK dialog_proc_0(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param);
BOOL CALLBACK dialog_proc_1(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param);
BOOL CALLBACK dialog_proc_2(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param);
BOOL CALLBACK dialog_proc_3(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param);

static void device_list_init(HWND list);
static void device_list_refresh(HWND list);
static void device_list_add(HWND list, device_context_t *device);
static void device_list_clean(HWND list);

static int save_file(HWND dialog, device_context_t *device);

int write_driver_binary(LPCSTR resource_name, 
					 LPCSTR resource_type,
					 const char* dst);

static int write_driver_resource(const char* inf_dir, 
								 const char* file_dir,
								 const char* file_ext,
								 int id_file,
								 int id_file_type);
void close_file(FILE** file);

int usb_install_driver_np(const char *inf_file);
char *usb_strerror(void);

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                     LPSTR cmd_line, int cmd_show)
{
    int next_dialog;
    device_context_t device;

    LoadLibrary("comctl32.dll");
    InitCommonControls();

    memset(&device, 0, sizeof(device));

    next_dialog = ID_DIALOG_0;

    while (next_dialog)
    {
        switch (next_dialog)
        {
        case ID_DIALOG_0:
            next_dialog = (int)DialogBoxParam(instance,
                                              MAKEINTRESOURCE(next_dialog),
                                              NULL, (DLGPROC)dialog_proc_0,
                                              (LPARAM)&device);

            break;
        case ID_DIALOG_1:
            next_dialog = (int)DialogBoxParam(instance,
                                              MAKEINTRESOURCE(next_dialog),
                                              NULL, (DLGPROC)dialog_proc_1,
                                              (LPARAM)&device);
            break;
        case ID_DIALOG_2:
            next_dialog = (int)DialogBoxParam(instance,
                                              MAKEINTRESOURCE(next_dialog),
                                              NULL, (DLGPROC)dialog_proc_2,
                                              (LPARAM)&device);
            break;
        case ID_DIALOG_3:
            next_dialog = (int)DialogBoxParam(instance,
                                              MAKEINTRESOURCE(next_dialog),
                                              NULL, (DLGPROC)dialog_proc_3,
                                              (LPARAM)&device);
            break;
        default:
            ;
        }
    }

    return 0;
}


BOOL CALLBACK dialog_proc_0(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SetWindowText(GetDlgItem(dialog, ID_INFO_TEXT), info_text_0);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(w_param))
        {
        case ID_BUTTON_NEXT:
            EndDialog(dialog, ID_DIALOG_1);
            return TRUE ;
        case ID_BUTTON_CANCEL:
        case IDCANCEL:
            EndDialog(dialog, 0);
            return TRUE ;
        }
    }

    return FALSE;
}

BOOL CALLBACK dialog_proc_1(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param)
{
    static HDEVNOTIFY notification_handle_hub = NULL;
    static HDEVNOTIFY notification_handle_dev = NULL;
    DEV_BROADCAST_HDR *hdr = (DEV_BROADCAST_HDR *) l_param;
    DEV_BROADCAST_DEVICEINTERFACE dev_if;
    static device_context_t *device = NULL;
    HWND list = GetDlgItem(dialog, ID_LIST);
    LVITEM item;

    switch (message)
    {
    case WM_INITDIALOG:
        device = (device_context_t *)l_param;
        memset(device, 0, sizeof(*device));

        SetWindowText(GetDlgItem(dialog, ID_LIST_HEADER_TEXT), list_header_text);
        device_list_init(list);
        device_list_refresh(list);

        dev_if.dbcc_size = sizeof(dev_if);
        dev_if.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

        dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_HUB;
        notification_handle_hub = RegisterDeviceNotification(dialog, &dev_if, 0);

        dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;
        notification_handle_dev = RegisterDeviceNotification(dialog, &dev_if, 0);

        return TRUE;

    case WM_DEVICECHANGE:
        switch (w_param)
        {
        case DBT_DEVICEREMOVECOMPLETE:
            if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                device_list_refresh(list);
            break;
        case DBT_DEVICEARRIVAL:
            if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                device_list_refresh(list);
            break;
        default:
            ;
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(w_param))
        {
        case ID_BUTTON_NEXT:
            if (notification_handle_hub)
                UnregisterDeviceNotification(notification_handle_hub);
            if (notification_handle_dev)
                UnregisterDeviceNotification(notification_handle_dev);

            memset(&item, 0, sizeof(item));
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);

            memset(device, 0, sizeof(*device));

            if (item.iItem >= 0)
            {
                if (ListView_GetItem(list, &item))
                {
                    if (item.lParam)
                    {
                        memcpy(device, (void *)item.lParam, sizeof(*device));
                    }
                }
            }

            if (!device->vid)
            {
                device->vid = 0x12AB;
                device->pid = 0x12AB;
            }

            if (!device->manufacturer[0])
                strcpy(device->manufacturer, "Insert manufacturer name");
            if (!device->description[0])
                strcpy(device->description,  "Insert device description");

            if (notification_handle_hub)
                UnregisterDeviceNotification(notification_handle_hub);
            if (notification_handle_dev)
                UnregisterDeviceNotification(notification_handle_dev);

            device_list_clean(list);

            EndDialog(dialog, ID_DIALOG_2);
            return TRUE;

        case ID_BUTTON_BACK:
            device_list_clean(list);
            if (notification_handle_hub)
                UnregisterDeviceNotification(notification_handle_hub);
            if (notification_handle_dev)
                UnregisterDeviceNotification(notification_handle_dev);
            EndDialog(dialog, ID_DIALOG_0);
            return TRUE ;

        case ID_BUTTON_CANCEL:
        case IDCANCEL:
            device_list_clean(list);
            if (notification_handle_hub)
                UnregisterDeviceNotification(notification_handle_hub);
            if (notification_handle_dev)
                UnregisterDeviceNotification(notification_handle_dev);
            EndDialog(dialog, 0);
            return TRUE ;
        }
    }

    return FALSE;
}

BOOL CALLBACK dialog_proc_2(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param)
{
    static device_context_t *device = NULL;
    char tmp[MAX_PATH];
	int i;

    switch (message)
    {
    case WM_INITDIALOG:
        device = (device_context_t *)l_param;

        if (device)
        {
            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "0x%04X", device->vid);
            SetWindowText(GetDlgItem(dialog, ID_TEXT_VID), tmp);

            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "0x%04X", device->pid);
            SetWindowText(GetDlgItem(dialog, ID_TEXT_PID), tmp);

            memset(tmp, 0, sizeof(tmp));
			if (device->mi != -1)
			{
				sprintf(tmp, "0x%02X", device->mi);
			}
            SetWindowText(GetDlgItem(dialog, ID_TEXT_MI), tmp);

			SetWindowText(GetDlgItem(dialog, ID_TEXT_MANUFACTURER),
                          device->manufacturer);

            SetWindowText(GetDlgItem(dialog, ID_TEXT_DEV_NAME),
                          device->description);
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(w_param))
        {
        case ID_BUTTON_NEXT:
            memset(device, 0, sizeof(*device));
			device->mi = -1;

            GetWindowText(GetDlgItem(dialog, ID_TEXT_MANUFACTURER),
                          device->manufacturer, sizeof(tmp));
            GetWindowText(GetDlgItem(dialog, ID_TEXT_DEV_NAME),
                          device->description, sizeof(tmp));

            GetWindowText(GetDlgItem(dialog, ID_TEXT_VID), tmp, sizeof(tmp));
            sscanf(tmp, "0x%04x", &device->vid);

            GetWindowText(GetDlgItem(dialog, ID_TEXT_PID), tmp, sizeof(tmp));
            sscanf(tmp, "0x%04x", &device->pid);

            GetWindowText(GetDlgItem(dialog, ID_TEXT_MI), tmp, sizeof(tmp));

            if (sscanf(tmp, "0x%02x", &i) == 1)
				if (i > -1 && i < 256)
					device->mi = i;

			if (save_file(dialog, device))
                EndDialog(dialog, ID_DIALOG_3);
            return TRUE ;
        case ID_BUTTON_BACK:
            EndDialog(dialog, ID_DIALOG_1);
            return TRUE ;
        case ID_BUTTON_CANCEL:
        case IDCANCEL:
            EndDialog(dialog, 0);
            return TRUE ;
        }
    }

    return FALSE;
}
#define MAX_TEXT_LENGTH 1024

BOOL CALLBACK dialog_proc_3(HWND dialog, UINT message,
                            WPARAM w_param, LPARAM l_param)
{
    static device_context_t *device = NULL;
    char* buffer = NULL;
    char* bufferTemp = NULL;
	int ret;

    switch (message)
    {
    case WM_INITDIALOG:
        device = (device_context_t *)l_param;

		buffer = malloc(MAX_TEXT_LENGTH * 2);
		if (buffer)
		{
			memset(buffer,0,MAX_TEXT_LENGTH * 2);
			bufferTemp = buffer + MAX_TEXT_LENGTH;

			sprintf_s(buffer,(MAX_TEXT_LENGTH - 1), "%s\n",info_text_1);

			sprintf_s(bufferTemp,(MAX_TEXT_LENGTH - 1), "Vendor ID:\t\t 0x%04X\n",device->vid);
			strcat_s(buffer,(MAX_TEXT_LENGTH - 1),bufferTemp);

			sprintf_s(bufferTemp,(MAX_TEXT_LENGTH - 1), "Product ID:\t\t 0x%04X\n",device->pid);
			strcat_s(buffer,(MAX_TEXT_LENGTH - 1),bufferTemp);
			if (device->mi!=-1)
			{
				sprintf_s(bufferTemp,(MAX_TEXT_LENGTH - 1), "Interface # (MI):\t\t 0x%02X\n",device->mi);
				strcat_s(buffer,(MAX_TEXT_LENGTH - 1),bufferTemp);
			}
					
			sprintf_s(bufferTemp,(MAX_TEXT_LENGTH - 1), "Device description:\t %s\n",device->description);
			strcat_s(buffer,(MAX_TEXT_LENGTH - 1),bufferTemp);
					
			sprintf_s(bufferTemp,(MAX_TEXT_LENGTH - 1), "Manufacturer:\t\t %s\n",device->manufacturer);
			strcat_s(buffer,(MAX_TEXT_LENGTH - 1),bufferTemp);

			SetWindowText(GetDlgItem(dialog, ID_INFO_TEXT), buffer);
			free(buffer);
		}
		if (GetFileAttributesA(installnow_inf)!=INVALID_FILE_ATTRIBUTES)
		{
			EnableWindow(GetDlgItem(dialog, ID_BUTTON_INSTALLNOW), TRUE);
		}
		else
		{
			EnableWindow(GetDlgItem(dialog, ID_BUTTON_INSTALLNOW), FALSE);
		}
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(w_param))
        {
        case ID_BUTTON_INSTALLNOW:
			SetCursor(LoadCursor(NULL,IDC_WAIT));
			SetWindowText(GetDlgItem(dialog, IDL_INSTALLING_TEXT), "Installing driver, please wait..");
			ret = usb_install_driver_np(installnow_inf);
			SetWindowText(GetDlgItem(dialog, IDL_INSTALLING_TEXT), "");
			SetCursor(LoadCursor(NULL,IDC_ARROW));

			if (ret == ERROR_SUCCESS)
			{
				MessageBoxA(dialog,"Driver installation successful.", "Install Driver Done", MB_OK | MB_APPLMODAL);
				EndDialog(dialog, 0);
			}
			else
			{
				MessageBoxA(dialog, usb_strerror(), "Install Driver Error", MB_OK | MB_APPLMODAL | MB_ICONWARNING);

			}
            return TRUE;
        case ID_BUTTON_NEXT:
        case IDCANCEL:
            EndDialog(dialog, 0);
            return TRUE ;
        }
    }

    return FALSE;
}

static void device_list_init(HWND list)
{
    LVCOLUMN lvc;

    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT);

    memset(&lvc, 0, sizeof(lvc));

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    lvc.fmt = LVCFMT_LEFT;

    lvc.cx = 70;
    lvc.iSubItem = 0;
    lvc.pszText = "Vendor ID";
    ListView_InsertColumn(list, 1, &lvc);

    lvc.cx = 70;
    lvc.iSubItem = 1;
    lvc.pszText = "Product ID";
    ListView_InsertColumn(list, 2, &lvc);

	lvc.cx = 250;
    lvc.iSubItem = 2;
    lvc.pszText = "Description";
    ListView_InsertColumn(list, 3, &lvc);

	lvc.cx = 70;
    lvc.iSubItem = 3;
	lvc.pszText = "MI";
    ListView_InsertColumn(list, 4, &lvc);
}

static void device_list_refresh(HWND list)
{
    HDEVINFO dev_info;
    SP_DEVINFO_DATA dev_info_data;
    int dev_index = 0;
    device_context_t *device;
    char tmp[MAX_PATH];
	int ret;

    device_list_clean(list);

    dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
    dev_index = 0;

    dev_info = SetupDiGetClassDevs(NULL, "USB", NULL,
                                   DIGCF_ALLCLASSES | DIGCF_PRESENT);

    if (dev_info == INVALID_HANDLE_VALUE)
    {
        return;
    }

    while (SetupDiEnumDeviceInfo(dev_info, dev_index++, &dev_info_data))
    {
        if (usb_registry_match(dev_info, &dev_info_data))
        {
            if (usb_registry_get_property(SPDRP_HARDWAREID, dev_info,
                                      &dev_info_data,
                                      tmp, sizeof(tmp) - 1))
			{

				device = (device_context_t *) malloc(sizeof(device_context_t));
				memset(device, 0, sizeof(*device));
				device->mi = -1;

				strlwr(tmp);
				ret = sscanf(tmp, "usb\\vid_%04x&pid_%04x&rev_%04d&mi_%02x", &device->vid, &device->pid, &device->rev, &device->mi);
				if (ret < 3)
				{
					free(device);
					continue;
				}

				//sscanf(tmp + sizeof("USB\\VID_") - 1, "%04x", &device->vid);
				//sscanf(tmp + sizeof("USB\\VID_XXXX&PID_") - 1, "%04x", &device->pid);

				usb_registry_get_property(SPDRP_DEVICEDESC, dev_info,
										  &dev_info_data,
										  tmp, sizeof(tmp) - 1);
				strcpy(device->description, tmp);

				usb_registry_get_property(SPDRP_MFG, dev_info,
										  &dev_info_data,
										  tmp, sizeof(tmp) - 1);
				strcpy(device->manufacturer, tmp);

				device_list_add(list, device);
			}
        }
    }

    SetupDiDestroyDeviceInfoList(dev_info);
}

static void device_list_add(HWND list, device_context_t *device)
{
    LVITEM item;
    char vid[32];
    char pid[32];
    char mi[32];

    memset(&item, 0, sizeof(item));
    memset(vid, 0, sizeof(vid));
    memset(pid, 0, sizeof(pid));
    memset(mi, 0, sizeof(mi));

    sprintf(vid, "0x%04X", device->vid);
    sprintf(pid, "0x%04X", device->pid);
	if (device->mi > -1)
	{
		sprintf(mi, "0x%02X", device->mi);
	}

    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.lParam = (LPARAM)device;

    ListView_InsertItem(list, &item);

    ListView_SetItemText(list, 0, 0, vid);
    ListView_SetItemText(list, 0, 1, pid);
    ListView_SetItemText(list, 0, 2, device->description);
    ListView_SetItemText(list, 0, 3, mi);
}

static void device_list_clean(HWND list)
{
    LVITEM item;

    memset(&item, 0, sizeof(LVITEM));

    while (ListView_GetItem(list, &item))
    {
        if (item.lParam)
            free((void *)item.lParam);

        ListView_DeleteItem(list, 0);
        memset(&item, 0, sizeof(LVITEM));
    }
}

static int save_file(HWND dialog, device_context_t *device)
{
    OPENFILENAME open_file;
    char inf_name[MAX_PATH];
    char inf_path[MAX_PATH];
    char inf_dir[MAX_PATH];

    char cat_name[MAX_PATH];
    char cat_path[MAX_PATH];

    char error[MAX_PATH];
    FILE *file = NULL;

	long inf_file_size;
	char *dst=NULL;
	char* c;
	int length;

    memset(&open_file, 0, sizeof(open_file));
    memset(inf_path, 0, sizeof(inf_path));
	memset(installnow_inf,0,sizeof(installnow_inf));

	if (strlen(device->description))
	{
		if (stricmp(device->description,"Insert device description")!=0)
		{
			strcpy(inf_path, device->description);
			c=inf_path;
			while(c[0])
			{
				if (c[0]>='A' && c[0]<='Z') { c++; continue;}
				if (c[0]>='a' && c[0]<='z') { c++; continue;}
				if (c[0]>='0' && c[0]<='9') { c++; continue;}

				switch(c[0])
				{
				case '_':
				case ' ':
				case '.':
					c[0]='_';
					break;
				default: // remove
					if (!c[1])
						c[0]='\0';
					else
						memmove(c,c+1,strlen(c+1)+1);
					break;
				}

				c++;
			}
		}
	}
	if (!strlen(inf_path))
		strcpy(inf_path, "your_file.inf");

    open_file.lStructSize = sizeof(OPENFILENAME);
    open_file.hwndOwner = dialog;
    open_file.lpstrFile = inf_path;
    open_file.nMaxFile = sizeof(inf_path);
    open_file.lpstrFilter = "*.inf\0*.inf\0";
    open_file.nFilterIndex = 1;
    open_file.lpstrFileTitle = inf_name;
    open_file.nMaxFileTitle = sizeof(inf_name);
    open_file.lpstrInitialDir = NULL;
    open_file.Flags = OFN_PATHMUSTEXIST;
    open_file.lpstrDefExt = "inf";

	dst=NULL;

    if (GetSaveFileName(&open_file))
    {
        strcpy(cat_path, inf_path);
        strcpy(cat_name, inf_name);

		strcpy(strstr(cat_path, ".inf"), ".cat");
        strcpy(strstr(cat_name, ".inf"), ".cat");

        file = fopen(inf_path, "wb");

        if (file)
        {

			safe_strcpy(libusb_inf_entities[INF_FILENAME].replace,inf_name);
			safe_strcpy(libusb_inf_entities[CAT_FILENAME].replace,cat_name);
			safe_strcpy(libusb_inf_entities[BASE_FILENAME].replace,"LIBUSB_WIN32");
			
			if (device->mi == -1)
				sprintf(libusb_inf_entities[HARDWAREID].replace,"USB\\VID_%04x&PID_%04x",
				device->vid, device->pid);
			else
				sprintf(libusb_inf_entities[HARDWAREID].replace,"USB\\VID_%04x&PID_%04x&MI_%02x",
				device->vid, device->pid, device->mi);

			safe_strcpy(libusb_inf_entities[DRIVER_DATE].replace,STRINGIFY(INF_DATE));
			safe_strcpy(libusb_inf_entities[DRIVER_VERSION].replace,STRINGIFY(INF_VERSION));
			safe_strcpy(libusb_inf_entities[DEVICE_MANUFACTURER].replace,device->manufacturer);
			safe_strcpy(libusb_inf_entities[DEVICE_INTERFACE_GUID].replace,"00000000-0000-0000-0000-000000000000");
			safe_strcpy(libusb_inf_entities[DEVICE_DESCRIPTION].replace,device->description);

			if ((inf_file_size = tokenize_resource(MAKEINTRESOURCEA(ID_LIBUSB_INF),MAKEINTRESOURCEA(ID_INF_TEXT),
				&dst,libusb_inf_entities,"#","#",0)) > 0)
			{
				fwrite(dst,sizeof(char),inf_file_size,file);
				fflush(file);
				free(dst);

#ifdef EMBEDDED_LIBUSB_WIN32
				safe_strcpy(inf_dir,inf_path);
				while ((length = strlen(inf_dir)))
				{
					if (inf_dir[length-1]=='\\' || inf_dir[length-1]=='/')
					{
						break;
					}
					inf_dir[length-1]='\0';
				}

				// libusb-win32 x86 binaries. 
				if (write_driver_resource(inf_dir, "x86", "_x86.dll", ID_LIBUSB_DLL, ID_X86) != ERROR_SUCCESS)
					goto Done;
				if (write_driver_resource(inf_dir, "x86", ".sys", ID_LIBUSB_SYS, ID_X86) != ERROR_SUCCESS)
					goto Done;

				// libusb-win32 amd64 binaries. 
				if (write_driver_resource(inf_dir, "amd64", ".dll", ID_LIBUSB_DLL, ID_AMD64) != ERROR_SUCCESS)
					goto Done;
				if (write_driver_resource(inf_dir, "amd64", ".sys", ID_LIBUSB_SYS, ID_AMD64) != ERROR_SUCCESS)
					goto Done;

				// libusb-win32 ia64 binaries. 
				if (write_driver_resource(inf_dir, "ia64", ".dll", ID_LIBUSB_DLL, ID_IA64) != ERROR_SUCCESS)
					goto Done;
				if (write_driver_resource(inf_dir, "ia64", ".sys", ID_LIBUSB_SYS, ID_IA64) != ERROR_SUCCESS)
					goto Done;
#endif

			}
			else
			{
				sprintf(error, "Error: unable to tokenize file: %s", inf_name);
				MessageBox(dialog, error, "Error",
						   MB_OK | MB_APPLMODAL | MB_ICONWARNING);
			}

			close_file(&file);
        }
        else
        {
            sprintf(error, "Error: unable to open file: %s", inf_name);
            MessageBox(dialog, error, "Error",
                       MB_OK | MB_APPLMODAL | MB_ICONWARNING);
        }


        file = fopen(cat_path, "w");

        if (file)
        {
            fprintf(file, "%s", cat_file_content);
			close_file(&file);
        }
        else
        {
            sprintf(error, "Error: unable to open file: %s", cat_name);
            MessageBox(dialog, error, "Error",
                       MB_OK | MB_APPLMODAL | MB_ICONWARNING);
        }


		safe_strcpy(installnow_inf, inf_path);
		return TRUE;
    }
Done:
	close_file(&file);
    return FALSE;
}

int write_driver_binary(LPCSTR resource_name, 
					 LPCSTR resource_type,
					 const char* dst)
{
	void* src;
	long src_count;
	HGLOBAL res_data;
	FILE* driver_file;
	int ret;

	HRSRC hSrc = FindResourceA(NULL, resource_name, resource_type);

	if (!hSrc)
		return -ERROR_RESOURCE_DATA_NOT_FOUND;

	src_count = SizeofResource(NULL, hSrc);

	res_data = LoadResource(NULL,hSrc);
	
	if (!res_data)
		return -ERROR_RESOURCE_DATA_NOT_FOUND;

	src = LockResource(res_data);

	if (!src)
		return -ERROR_RESOURCE_DATA_NOT_FOUND;

	if (!(driver_file = fopen(dst,"wb")))
		return -1;

	ret = fwrite(src,1,src_count,driver_file);
	fflush(driver_file);
	close_file(&driver_file);

	return (ret == src_count) ? src_count : -1;
}

void close_file(FILE** file)
{
	if (*file)
	{
		fclose(*file);
		*file=NULL;
	}
}

static int write_driver_resource(const char* inf_dir, 
								 const char* file_dir,
								 const char* file_ext,
								 int id_file,
								 int id_file_type)
{
	HANDLE hFindFile = NULL;
	WIN32_FIND_DATA findFileData;
	char driver_file[MAX_PATH];
	char error[256];

	safe_strcpy(driver_file,inf_dir);
	strcat(driver_file, file_dir);
	if ((hFindFile = FindFirstFileA(driver_file,&findFileData)) == INVALID_HANDLE_VALUE)
	{
		if (!CreateDirectoryA(driver_file,NULL))
		{
			sprintf(error, "Error: unable to create directory file: %s", driver_file);
			MessageBox(NULL, error, "Error",
					   MB_OK | MB_APPLMODAL | MB_ICONWARNING);
			goto DoneWithErrors;
		}
	}
	else
	{
		FindClose(hFindFile);
		hFindFile = NULL;
	}

	sprintf(driver_file,"%s%s%c%s%s",inf_dir, file_dir, inf_dir[strlen(inf_dir)-1], "libusb0", file_ext);
	if (write_driver_binary(MAKEINTRESOURCEA(id_file),MAKEINTRESOURCEA(id_file_type),driver_file) < 0)
	{
		sprintf(error, "Error: unable to write file: %s", driver_file);
		MessageBox(NULL, error, "Error",
				   MB_OK | MB_APPLMODAL | MB_ICONWARNING);
		goto DoneWithErrors;
	}
	
	return ERROR_SUCCESS;

DoneWithErrors:

	return -ERROR_PATH_NOT_FOUND;
}