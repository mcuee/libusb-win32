
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
#include <stdio.h>

static char __error_buf[512];

const char *win_error_to_string(void)
{
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 
		LANG_USER_DEFAULT, __error_buf, 
		sizeof (__error_buf) - 1, 
		NULL);
  return __error_buf;
}

void usb_debug_error(const char *s, ...)
{
  char tmp[512];
  va_list args;
  
  memset(tmp, 0, sizeof(tmp));

  int offset = sprintf(tmp, "LIBUSB: error: ");
  va_start(args, s);
  vsnprintf(tmp + offset, sizeof(tmp) - offset - 1, s, args);
  va_end(args);
  printf("%s\n",tmp);

  OutputDebugString(tmp);

}
