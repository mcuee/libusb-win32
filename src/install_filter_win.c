/* libusb-win32, Generic Windows USB Library
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

#define INITGUID

#include <windows.h>

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

#include <commdlg.h>
#include <dbt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initguid.h>
#include <commctrl.h>
#include <setupapi.h>
#include <time.h>

#include "libusb-win32_version.h"
#include "registry.h"

#define __INSTALL_FILTER_WIN_C__
#include "install_filter_win_rc.rc"

#define MAX_TEXT_LENGTH 256

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)


// Used for device notification
DEFINE_GUID(GUID_DEVINTERFACE_USB_HUB, 0xf18a0e88, 0xc30c, 0x11d0, 0x88, \
			0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8);

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, \
			0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

typedef struct
{
	UINT controlID;
	const char* message;
}create_tooltip_t;

typedef struct
{
	filter_mode_e mode;
	filter_type_e type;

	struct
	{
		// device plugged notification
		HDEVNOTIFY handle_hub;
		HDEVNOTIFY handle_dev;
	}notify;

}install_filter_win_t,*pinstall_filter_win_t;

const char info_text_0[] =
"This program adds/removes libusb-win32 as a driver to an existing device installation.\r\n\r\n"
"The libusb-win32 filter driver allows access to usb devices using the libusb-win32 api while maintaining "
"compatibility with software which uses the original driver.";

const char info_text_1[] =
"A windows driver installation package has been created for the following "
"device:";

const char package_contents_fmt_0[] =
"This package contains %s v%d.%d.%d.%d drivers and support for the following platforms: %s.";

const char package_contents_fmt_1[] =
"This package contains an inf file only.";


const char list_header_text_install[] =
"Connect your device and select it from the list of unfiltered devices below. "
"If your device isn't listed, it may already be filtered, be in a \"driverless\" state, "
"or incompatible with the libusb-win32 filter driver.";

const char list_header_text_remove[] =
"Select your device from the list of filtered devices below. "
"If your device isn't listed, it does not have a device filter.";

create_tooltip_t tooltips_dlg0[]=
{
	{IDC_INSTALL_DEVICE_FILTER,
	"Install libusb-win32 as a device upper filter for a single device. This device is selected on the next wizard page."},

	{IDC_REMOVE_DEVICE_FILTER,
	"Remove a libusb-win32 device upper filter for a single device. This device is selected on the next wizard page."},
	
	{IDC_REMOVE_DEVICE_FILTERS,
	"Remove all libusb-win32 device upper filters."},

	{0,NULL}
};

create_tooltip_t tooltips_dlg1[]=
{
	{IDC_SHOW_CONNECTED_DEVICES,
	"A VID is a 16-bit vendor number (Vendor ID). A vendor ID is "
	"necessary for developing a USB product. The USB-IF is responsible "
	"for issuing USB vendor ID's to product manufacturers."},

	{IDC_SHOW_ALL_DEVICES,
	"A PID is a 16-bit product number (Product ID)."},
	
	{ID_LIST,
	LPSTR_TEXTCALLBACK},

	{0,NULL}
};

HICON mIcon;
HINSTANCE g_hInst = NULL;

// see install.c 
extern int usb_install_npA(HWND hwnd, HINSTANCE instance, LPCSTR cmd_line, int starg_arg);

HWND create_tooltip(HWND hMain, HINSTANCE hInstance, UINT max_tip_width, create_tooltip_t tool_tips[]);
BOOL CALLBACK dialog_proc_0(HWND dialog, UINT message,
							WPARAM wParam, LPARAM lParam);
BOOL CALLBACK dialog_proc_1(HWND dialog, UINT message,
							WPARAM wParam, LPARAM lParam);

static void device_list_init(pinstall_filter_win_t context, HWND list);
static bool_t device_list_refresh(pinstall_filter_win_t context, HWND list);
static void device_list_add(pinstall_filter_win_t context, HWND list, filter_device_t* device);
static void device_list_clean(pinstall_filter_win_t context, HWND list);

void output_debug(char* format,...)
{
	va_list args;
	char msg[256];

	va_start (args, format);
	vsprintf(msg, format, args);
	va_end (args);

	OutputDebugStringA(msg);
}

void init_install_filter_context(install_filter_win_t* context)
{
	memset(context, 0, sizeof(*context));
	context->mode = FM_INSTALL;
	context->type = FT_DEVICE_UPPERFILTER;
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev_instance,
					 LPSTR cmd_line, int cmd_show)
{
	install_filter_win_t context;
	int next_dialog;


	LoadLibrary("comctl32.dll");
	InitCommonControls();
	if (cmd_line && strlen(cmd_line))
	{
		HWND hwnd = GetDesktopWindow();
		return usb_install_npA(hwnd, instance, cmd_line, 0);
	}
	init_install_filter_context(&context);

	next_dialog = ID_DIALOG_0;

	mIcon = LoadIcon(instance, MAKEINTRESOURCE(IDR_MAIN_ICON));

	g_hInst = instance;

	while (next_dialog)
	{
		switch (next_dialog)
		{
		case ID_DIALOG_0:
			next_dialog = (int)DialogBoxParam(instance,
				MAKEINTRESOURCE(next_dialog),
				NULL, (DLGPROC)dialog_proc_0,
				(LPARAM)&context);

			break;
		case ID_DIALOG_1:
			next_dialog = (int)DialogBoxParam(instance,
				MAKEINTRESOURCE(next_dialog),
				NULL, (DLGPROC)dialog_proc_1,
				(LPARAM)&context);
			break;
		default:
			;
		}
	}

	if (mIcon)
	{
		DestroyIcon(mIcon);
		mIcon = NULL;
	}
	return 0;
}

static void device_notification_register(HWND dialog, pinstall_filter_win_t context)
{
	DEV_BROADCAST_DEVICEINTERFACE dev_if;

	dev_if.dbcc_size = sizeof(dev_if);
	dev_if.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

	// register for device hub hot-plugs
	if (!context->notify.handle_hub)
	{
		dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_HUB;
		context->notify.handle_hub = RegisterDeviceNotification(dialog, &dev_if, 0);
	}

	// register for device hot-plugs
	if (!context->notify.handle_dev)
	{
		dev_if.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;
		context->notify.handle_dev = RegisterDeviceNotification(dialog, &dev_if, 0);
	}
}

static void device_notification_unregister(pinstall_filter_win_t context)
{
	if (context->notify.handle_hub)
	{
		UnregisterDeviceNotification(context->notify.handle_hub);
		context->notify.handle_hub = NULL;
	}
	if (context->notify.handle_dev)
	{
		UnregisterDeviceNotification(context->notify.handle_dev);
		context->notify.handle_dev = NULL;
	}
}

BOOL CALLBACK dialog_proc_0(HWND dialog, UINT message,
							WPARAM wParam, LPARAM lParam)
{
	static install_filter_win_t* context = NULL;

	switch (message)
	{
	case WM_INITDIALOG:
		SendMessage(dialog,WM_SETICON,ICON_SMALL, (LPARAM)mIcon);
		SendMessage(dialog,WM_SETICON,ICON_BIG,   (LPARAM)mIcon);
		context = (install_filter_win_t*)lParam;

		SetWindowText(GetDlgItem(dialog, ID_INFO_TEXT), info_text_0);
		create_tooltip(dialog, g_hInst, 300, tooltips_dlg0);
		
		switch(context->mode)
		{
		case FM_INSTALL:
			CheckRadioButton(dialog, IDC_INSTALL_DEVICE_FILTER, IDC_REMOVE_DEVICE_FILTERS, IDC_INSTALL_DEVICE_FILTER);
			break;
		case FM_REMOVE:
			CheckRadioButton(dialog, IDC_INSTALL_DEVICE_FILTER, IDC_REMOVE_DEVICE_FILTERS, IDC_REMOVE_DEVICE_FILTER);
			break;
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_BUTTON_NEXT:
			context->type = FT_DEVICE_UPPERFILTER;
			if (IsDlgButtonChecked(dialog, IDC_INSTALL_DEVICE_FILTER))
			{
				context->mode = FM_INSTALL;
			}
			else if (IsDlgButtonChecked(dialog, IDC_REMOVE_DEVICE_FILTER))
			{
				context->mode = FM_REMOVE;
			}
			else if (IsDlgButtonChecked(dialog, IDC_REMOVE_DEVICE_FILTERS))
			{
				if (MessageBox(dialog,
					"This will remove libusb-win32 as a device filter for all known devices. Are you sure you wich to continue?",
					"Remove all device filters",
					MB_OKCANCEL|MB_ICONWARNING) == IDOK)
				{
					if (usb_install_npA(dialog, g_hInst, "uninstall -ad", 0) == 0)
					{
						MessageBox(dialog,
							"All libusb-win32 device filters removed successfully.",
							"Remove all device filters",
							MB_OK|MB_ICONINFORMATION);

						// reset to wizard defaults
						init_install_filter_context(context);
						EndDialog(dialog, ID_DIALOG_0);
					}
					else
					{
						// if an error occurs, usb_install_np() displays the error message.
					}
				}
				return TRUE ;
			}
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
							WPARAM wParam, LPARAM lParam)
{
	static install_filter_win_t* context = NULL;
	DEV_BROADCAST_HDR *hdr = (DEV_BROADCAST_HDR *) lParam;
	filter_device_t* selected_device;

	HWND list = GetDlgItem(dialog, ID_LIST);
	LVITEM item;

	switch (message)
	{
	case WM_DESTROY:
		if (list)
		{
			device_list_clean(context, list);
		}
		break;
	case WM_INITDIALOG:
		SendMessage(dialog,WM_SETICON,ICON_SMALL, (LPARAM)mIcon);
		SendMessage(dialog,WM_SETICON,ICON_BIG,   (LPARAM)mIcon);

		context = (install_filter_win_t *)lParam;

		device_list_init(context, list);
		switch (context->mode)
		{
		case FM_INSTALL:
			SetWindowText(GetDlgItem(dialog, ID_LIST_HEADER_TEXT), list_header_text_install);
			SetWindowText(GetDlgItem(dialog, ID_BUTTON_NEXT), "Install");
			break;
		case FM_REMOVE:
			SetWindowText(GetDlgItem(dialog, ID_LIST_HEADER_TEXT), list_header_text_remove);
			SetWindowText(GetDlgItem(dialog, ID_BUTTON_NEXT), "Remove");
			break;
		}
		EnableWindow(GetDlgItem(dialog, ID_BUTTON_NEXT), FALSE);
		device_list_refresh(context, list);

		device_notification_register(dialog, context);

		return TRUE;

	case WM_DEVICECHANGE:
		switch (wParam)
		{
		case DBT_DEVICEREMOVECOMPLETE:
			if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			{
				EnableWindow(GetDlgItem(dialog, ID_BUTTON_NEXT), FALSE);
				device_list_refresh(context, list);
			}
			break;
		case DBT_DEVICEARRIVAL:
			if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			{
				EnableWindow(GetDlgItem(dialog, ID_BUTTON_NEXT), FALSE);
				device_list_refresh(context, list);
			}
			break;
		default:;
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_BUTTON_NEXT:
			device_notification_unregister(context);

			memset(&item, 0, sizeof(item));
			item.mask = LVIF_TEXT | LVIF_PARAM;
			item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);

			if (item.iItem >= 0)
			{
				if (ListView_GetItem(list, &item))
				{
					selected_device = (filter_device_t*)item.lParam;
					if (selected_device)
					{
						char tmp[MAX_PATH];
						sprintf(tmp,"%s \"-di=%s\"",
							(context->mode == FM_INSTALL) ? "i":"u",
							selected_device->device_id);

						if (usb_install_npA(dialog, g_hInst, tmp, 0) == 0)
						{
							char msg_title[MAX_PATH];
							char msg_txt[MAX_PATH*3];

							sprintf(msg_title,
								"%s device filter",
								(context->mode == FM_INSTALL) ? "Install":"Remove");

							sprintf(msg_txt,
								"libusb-win32 device filter successfully %s for %s (%s)",
								(context->mode == FM_INSTALL) ? "installed":"removed",
								selected_device->device_name,
								selected_device->device_hwid);

							MessageBox(dialog, msg_txt, msg_title, MB_OK|MB_ICONINFORMATION);
							device_list_clean(context, list);
							EndDialog(dialog, ID_DIALOG_1);
							return TRUE;
						}
					}
				}
			}

			EndDialog(dialog, 0);
			return TRUE;

		case ID_BUTTON_BACK:
			device_notification_unregister(context);
			device_list_clean(context, list);
			EndDialog(dialog, ID_DIALOG_0);
			return TRUE ;

		case ID_BUTTON_CANCEL:
		case IDCANCEL:
			device_notification_unregister(context);
			device_list_clean(context, list);
			EndDialog(dialog, 0);
			return TRUE ;
		}
		break;
    case WM_NOTIFY:
		if ( (lParam) && (((LPNMHDR)lParam)->idFrom == ID_LIST) )
		{
			LPNMLISTVIEW pnmv;
			HWND hwnd;
			switch (((LPNMHDR)lParam)->code)
			{
			case LVN_ITEMCHANGED:
				pnmv = (LPNMLISTVIEW) lParam;
				if (pnmv->uNewState & LVIS_SELECTED)
				{
					hwnd = GetDlgItem(dialog, ID_BUTTON_NEXT);
					if (!IsWindowEnabled(hwnd))
					{
						EnableWindow(hwnd, TRUE);
					}
				}
				break;
			}
		}
		break;
	}

	return FALSE;
}

static void device_list_init(pinstall_filter_win_t context, HWND list)
{
	LVCOLUMN lvc;
	int ignored, width;
	RECT list_rect;

	GetClientRect(list, &list_rect);
	width = list_rect.right - GetSystemMetrics(SM_CXVSCROLL);

	ignored = (int)ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT);

	memset(&lvc, 0, sizeof(lvc));

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
	lvc.fmt = LVCFMT_LEFT;

	lvc.cx = width / 2;
	lvc.iSubItem = 0;
	lvc.pszText = "Hardware ID";
	ignored = ListView_InsertColumn(list, 1, &lvc);

	lvc.iSubItem = 1;
	lvc.pszText = "Description";
	ignored = ListView_InsertColumn(list, 2, &lvc);

	lvc.iSubItem = 2;
	lvc.pszText = "Manufacturer";
	ignored = ListView_InsertColumn(list, 3, &lvc);

}

static bool_t device_list_refresh(pinstall_filter_win_t context, HWND list)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	filter_device_t* device = NULL;
	filter_device_t* prev = NULL;
	filter_device_t temp;
	int dev_index = -1;
	bool_t is_service_libusb;
	filter_type_e filter_type;
	DWORD flags;
	bool_t remove;

	if (!context) return FALSE;

	device_list_clean(context, list);

	flags = context->mode == FM_INSTALL ? (DIGCF_ALLCLASSES | DIGCF_PRESENT) : DIGCF_ALLCLASSES;
	remove = context->mode == FM_INSTALL ? FALSE : TRUE;

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	dev_info = SetupDiGetClassDevs(NULL, "USB", NULL, flags);
	if (dev_info == INVALID_HANDLE_VALUE)
	{
		MessageBox(GetParent(list),
			"failed enumerating devices",
			"install-filter error",
			MB_OK|MB_ICONERROR); 
		return FALSE;
	}

	while (SetupDiEnumDeviceInfo(dev_info, ++dev_index, &dev_info_data))
	{
		if (!usb_registry_is_service_libusb(dev_info, &dev_info_data, &is_service_libusb))
			continue;

		// don't list libusb devices
		if (is_service_libusb)
			continue;

		memset(&temp, 0, sizeof(temp));

		// get the compatible ids
		if (!usb_registry_get_property(SPDRP_COMPATIBLEIDS, dev_info, &dev_info_data, 
			temp.device_hwid, sizeof(temp.device_hwid)))
			continue;

		usb_registry_mz_string_lower(temp.device_hwid);

		// don't list usb hubs
		if (usb_registry_mz_string_find_sub(temp.device_hwid, "class_09"))
			continue;

		// get the hardware ids
		if (!usb_registry_get_hardware_id(dev_info, &dev_info_data, temp.device_hwid))
			continue;
		usb_registry_mz_string_lower(temp.device_hwid);

		// don't list root hubs
		if (usb_registry_mz_string_find_sub(temp.device_hwid, "root_hub"))
			continue;

		// get the device instance id
		if (CM_Get_Device_ID(dev_info_data.DevInst, temp.device_id, sizeof(temp.device_id), 0) != CR_SUCCESS)
			continue;

		// get the libusb0 device upper/lower filter type
		if (!usb_registry_get_device_filter_type(dev_info, &dev_info_data, &filter_type))
			continue;

		// list devices for removal that currently have a device filter 
		if (remove && ((filter_type & (FT_DEVICE_UPPERFILTER | FT_DEVICE_LOWERFILTER)) == FT_NONE))
			continue;

		// don't list devices for install that are already filtered
		if (!remove && ((filter_type & (FT_DEVICE_UPPERFILTER | FT_DEVICE_LOWERFILTER)) != FT_NONE))
			continue;

		// get the manufacturer
		usb_registry_get_property(SPDRP_MFG, dev_info, &dev_info_data, 
			temp.device_mfg, sizeof(temp.device_mfg));

		// get the description
		usb_registry_get_property(SPDRP_DEVICEDESC, dev_info, &dev_info_data, 
			temp.device_name, sizeof(temp.device_name));

		device = (filter_device_t*) malloc(sizeof(filter_device_t));
		memcpy(device, &temp, sizeof(filter_device_t));

		device_list_add(context, list, device);
		if (prev)
			prev->next = device;

		prev = device;
	}
	SetupDiDestroyDeviceInfoList(dev_info);

	return TRUE;
}

static void device_list_add(pinstall_filter_win_t context, HWND list, filter_device_t *device)
{
	LVITEM item;
	int ignored;
	char hwid[MAX_PATH];
	char* hwid_ch;
	memset(&item, 0, sizeof(item));

	// strip "usb\"
	strcpy(hwid, device->device_hwid+4);

	// replace '&' with ' '
	while( (hwid_ch = strchr(hwid,'&')) )
		*hwid_ch = ' ';

	// replace '_' with ':'
	while( (hwid_ch = strchr(hwid,'_')) )
		*hwid_ch = ':';

	item.mask = LVIF_TEXT | LVIF_PARAM;
	item.lParam = (LPARAM)device;

	ignored = ListView_InsertItem(list, &item);

	ListView_SetItemText(list, 0, 0, hwid);
	ListView_SetItemText(list, 0, 1, device->device_name);
	ListView_SetItemText(list, 0, 2, device->device_mfg);
}

static void device_list_clean(pinstall_filter_win_t context, HWND list)
{
	LVITEM item;
	BOOL ignored;

	memset(&item, 0, sizeof(LVITEM));

	while (ListView_GetItem(list, &item))
	{
		if (item.lParam)
			free((void *)item.lParam);

		ignored = ListView_DeleteItem(list, 0);
		memset(&item, 0, sizeof(LVITEM));
	}
}

int device_filter_install(pinstall_filter_win_t context, HWND dialog, filter_device_t *device)
{

	// TODO
	MessageBoxA(dialog, "error","Error Installing Driver", MB_OK|MB_ICONWARNING);

	return 0;
}

/*
* Create a tooltip for the controls in tool_tips
*/
HWND create_tooltip(HWND hMain, HINSTANCE hInstance, UINT max_tip_width, create_tooltip_t tool_tips[])
{
	HWND hTip;
	TOOLINFO toolInfo = {0};
	int i;

	// Create the tooltip window
	hTip = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,	hMain, NULL,
		hInstance, NULL);

	if (hTip == NULL) {
		return (HWND)NULL;
	}

	// Associate the tooltip to the control
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.hwnd = hMain;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;

	for (i=0; tool_tips[i].controlID != 0 && tool_tips[i].message != NULL; i++)
	{
		toolInfo.uId =(UINT_PTR)GetDlgItem(hMain,tool_tips[i].controlID);
		toolInfo.lpszText = (LPSTR)tool_tips[i].message;
		SendMessage(hTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	}

	SendMessage(hTip, TTM_SETMAXTIPWIDTH, 0, max_tip_width);

	return hTip;
}
