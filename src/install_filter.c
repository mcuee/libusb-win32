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

#include <stdio.h>
#include "registry.h"
#include "usb.h"
#include "error.h"
#include "libusb-win32_version.h"


int main()
{
    int ret = -1;
    LPWSTR command_line_w;


    #ifdef _DEBUG
	    usb_log_set_level(LOG_DEBUG);
    #else
	    usb_log_set_level(LOG_INFO);
    #endif

	USBRAWMSG("\nlibusb-win32 installer (v%u.%u.%u.%u)\n",VERSION_MAJOR,VERSION_MINOR,VERSION_MICRO,VERSION_NANO);

    if (!(command_line_w = GetCommandLineW()))
    {
        USBERR("failed GetCommandLineW:%X",GetLastError());
        goto Done;
    }

    usb_install_np(NULL, command_line_w, 1);

Done:
    return 0;
}
