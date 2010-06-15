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

#include <stdio.h>
#include "usb.h"
#include "error.h"
#include "libusb_version.h"

void usage(void);
const char* get_argument(char **argv, int argc, const char* arg);

void usage(void)
{
    fprintf(stderr, "Usage: install-filter.exe [option]\n"
            "Options:\n"
            "-h  prints this help message\n"
            "-i  installs the filter driver\n"
            "-u  uninstalls the filter driver\n"
            "-v  verbose mode\n");
}
const char* get_argument(char **argv, int argc, const char* arg)
{
	int pos=0;
	while(pos < argc)
	{
		if (strstr(argv[pos],arg)==argv[pos])
			return argv[pos];
		pos++;
	}
	return NULL;
}

int main(int argc, char **argv)
{
#ifdef _DEBUG
	usb_log_set_level(LOG_DEBUG);
#else
	usb_log_set_level(LOG_INFO);
#endif
	USBRAWMSG("\nLIBUSB-WIN32 (v%u.%u.%u.%u)\n",VERSION_MAJOR,VERSION_MINOR,VERSION_MICRO,VERSION_NANO);

    if (argc >= 2)
    {

		if (get_argument(argv,argc,"-v"))
			usb_log_set_level(LOG_DEBUG);

        if (get_argument(argv,argc, "-i"))
        {
			USBRAWMSG0("This could take up to 20 seconds to complete.\n");
			USBRAWMSG0("During this time USB devices may stop responding.\n");
 			USBRAWMSG0("Installing..\n\n");
            usb_install_service_np();
            return 0;
        }

        if (get_argument(argv,argc, "-u"))
        {
			USBRAWMSG0("This could take up to 20 seconds to complete.\n");
			USBRAWMSG0("During this time USB devices may stop responding.\n");
 			USBRAWMSG0("Uninstalling..\n\n");
            usb_uninstall_service_np();
            return 0;
        }
    }

    usage();

    return 0;
}
