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
#define LOG_APPNAME "install-filter"

#include <stdio.h>
#include "usb.h"
#include "error.h"
#include "libusb_version.h"

void usage(void);

void usage(void)
{
    fprintf(stderr, "Usage: install-filter.exe [option]\n"
            "Options:\n"
            "-h  prints this help message\n"
            "-i  installs the filter driver\n"
            "-u  uninstalls the filter driver\n");
}

void print_user_status(char* install_type)
{
    fprintf(stdout, "\n[LIBUSB-WIN32 (v%u.%u.%u.%u) %s filter]\n\n",
            VERSION_MAJOR,VERSION_MINOR,VERSION_MICRO,VERSION_NANO,
            install_type);

    fprintf(stdout, "This could take up to 20 seconds to complete.\n"
            "During this time USB devices may stop responding.\n"
            "Please wait while the filter is %sed..\n",install_type);

}

int main(int argc, char **argv)
{
	usb_log_set_level(LOG_INFO);

    if (argc == 2)
    {
        if (!strcmp(argv[1], "-i"))
        {
            print_user_status("install");
            usb_install_service_np();
            return 0;
        }

        if (!strcmp(argv[1], "-u"))
        {
            print_user_status("uninstall");
            usb_uninstall_service_np();
            return 0;
        }
    }

    usage();

    return 0;
}
