/* LIBUSB-WIN32, Generic Windows USB Library
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
  int offset;
  
  memset(tmp, 0, sizeof(tmp));

  offset = sprintf(tmp, "LIBUSB: error: ");
  va_start(args, s);
  _vsnprintf(tmp + offset, sizeof(tmp) - offset - 1, s, args);
  va_end(args);
  printf("%s\n",tmp);

  OutputDebugString(tmp);

}
