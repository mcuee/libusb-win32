/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2002-2006 Stephan Meyer <ste_meyer@web.de>
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

#ifdef __GNUC__
#define _WIN32_IE 0x0400
#define WINVER 0x0500
#endif

#define INITGUID

#include <windows.h>
#include <commdlg.h>
#include <string.h>
#include <dbt.h>
#include <initguid.h>
#include <commctrl.h>

#include "usb.h"

#define __INF_INSTALL_C__
#include "inf_install_rc.rc"

BOOL CALLBACK dialog_proc(HWND dialog, UINT message, 
                          WPARAM w_param, LPARAM l_param);

static int get_file_name(HWND dialog, char *name, int size);

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                     LPSTR cmd_line, int cmd_show)
{
  LoadLibrary("comctl32.dll");
  InitCommonControls();

  DialogBox(instance, MAKEINTRESOURCE(ID_DIALOG), NULL, 
            dialog_proc);
  return 0;
}


BOOL CALLBACK dialog_proc(HWND dialog, UINT message, 
                          WPARAM w_param, LPARAM l_param)
{
  char inf_file[MAX_PATH];

  switch(message) {
  case WM_INITDIALOG:
    EnableWindow(GetDlgItem(dialog, ID_BUTTON_INSTALL), FALSE);   
    return TRUE;
  case WM_COMMAND:
    switch(LOWORD(w_param))
      {
      case ID_BUTTON_FILE:
        if(get_file_name(dialog, inf_file, sizeof(inf_file))) {
          SetWindowText(GetDlgItem(dialog, ID_TEXT_FILE), inf_file);
          EnableWindow(GetDlgItem(dialog, ID_BUTTON_INSTALL), TRUE);
        }
        return TRUE;
      case ID_BUTTON_INSTALL:
        GetWindowText(GetDlgItem(dialog, ID_TEXT_FILE), 
                      inf_file, sizeof(inf_file));
        if(usb_install_driver_np(inf_file) < 0) {
          MessageBox(dialog, "Installing .inf file failed.", "Error",
                     MB_ICONERROR);
          return TRUE;
        }
        MessageBox(dialog, ".inf file installed successfully.", "Info",
                   MB_OK);
        EndDialog(dialog, ID_DIALOG);
        return TRUE;
      case ID_BUTTON_CANCEL:
      case IDCANCEL:
        EndDialog(dialog, 0);
        return TRUE;
      }
  }
  return FALSE;
}

static int get_file_name(HWND dialog, char *name, int size)
{
  OPENFILENAME open_file;

  memset(&open_file, 0, sizeof(open_file));
  memset(name, 0, size);
  open_file.lStructSize = sizeof(OPENFILENAME);
  open_file.hwndOwner = dialog;
  open_file.lpstrFile = name;
  open_file.nMaxFile = size;
  open_file.lpstrFilter = "*.inf\0*.inf\0";
  open_file.nFilterIndex = 1;
  open_file.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  
  return GetOpenFileName(&open_file);
}
