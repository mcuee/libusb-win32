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


#include "libusb_driver.h"

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>



#define DEBUG_BUFFER_SIZE 512

static int debug_level = LIBUSB_DEBUG_ERR;

void debug_print_nl(void)
{
  if(debug_level >= LIBUSB_DEBUG_MSG) 
    KdPrint((" "));
}

void debug_set_level(int level)
{
  debug_level = level;
}

void debug_printf(int level, char *format, ...)
{
  if(level <= debug_level)
    {
      char tmp[DEBUG_BUFFER_SIZE];
      va_list args;

      va_start(args, format);
      _vsnprintf(tmp, DEBUG_BUFFER_SIZE - 1, format, args);
      va_end(args);
      KdPrint(("LIBUSB-DRIVER - %s\n", tmp));
    }
}
