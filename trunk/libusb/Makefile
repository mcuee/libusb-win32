# LIBUSB-WIN32, Generic Windows USB Library
# Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
# Copyright (c) 2010 Travis Robinson <libusbdotnet@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


# Supported arugments: all, dll, filter, infwizard, test, testwin, driver
#
#

# If you're cross-compiling and your mingw32 tools are called
# i586-mingw32msvc-gcc and so on, then you can compile libusb-win32
# by running
#    make host_prefix=i586-mingw32msvc all
#
ifdef host_prefix
	override host_prefix := $(host_prefix)-
endif

CC = $(host_prefix)gcc
LD = $(host_prefix)ld
WINDRES = $(host_prefix)windres
DLLTOOL = $(host_prefix)dlltool

MAKE = make
CP = cp
CD = cd
MV = mv
RM = -rm -fr
TAR = tar
ISCC = iscc
INSTALL = install
LIB = lib
IMPLIB = implib
UNIX2DOS = unix2dos

MANIFEST_FILE = '\"manifest_x86.xml\"'
TARGET = libusb
DLL_TARGET = $(TARGET)0
LIB_TARGET = $(TARGET)
DRIVER_TARGET = $(TARGET)0.sys
INSTALL_DIR = /usr
VPATH = .:./src:./src/driver:./tests

DRIVER_OBJECTS = abort_endpoint.o claim_interface.o clear_feature.o \
	dispatch.o get_configuration.o \
	get_descriptor.o get_interface.o get_status.o \
	ioctl.o libusb_driver.o pnp.o release_interface.o reset_device.o \
	reset_endpoint.o set_configuration.o set_descriptor.o \
	set_feature.o set_interface.o transfer.o vendor_request.o \
	power.o driver_registry.o driver_debug.o libusb_driver_rc.o 


SRC_DIR = ./src
DRIVER_SRC_DIR = $(SRC_DIR)/driver

INCLUDES = -I./src -I./src/driver -I.

CFLAGS = -O2 -Wall -mno-cygwin
WIN_CFLAGS = $(CFLAGS) -mwindows

CPPFLAGS = -DMANIFEST_FILE=$(MANIFEST_FILE)

WINDRES_FLAGS = -I$(SRC_DIR)

LDFLAGS = -s -mno-cygwin -L. -lusb -lgdi32 -luser32 -lcfgmgr32 -lsetupapi -lcomctl32
WIN_LDFLAGS = $(LDFLAGS) -mwindows
DLL_LDFLAGS = -s -mdll -mno-cygwin \
	-Wl,--kill-at \
	-Wl,--out-implib,$(LIB_TARGET).a \
	-Wl,--enable-stdcall-fixup \
	-L. -lcfgmgr32 -lsetupapi 

DRIVER_LDFLAGS = -s -shared -Wl,--entry,_DriverEntry@8 \
	-nostartfiles -nostdlib -L. -lusbd -lntoskrnl -lhal


.PHONY: all
all: dll filter infwizard test testwin driver

.PHONY: dll
dll: DLL_CFLAGS = $(CFLAGS) -DLOG_APPNAME='"libusb-dll"'
dll: $(DLL_TARGET).dll

$(DLL_TARGET).dll: usb.2.o error.2.o descriptors.2.o windows.2.o install.2.o registry.2.o resource.2.o 
	$(CC) $(DLL_CFLAGS) -o $@ -I./src  $^ $(DLL_TARGET).def $(DLL_LDFLAGS)

%.2.o: %.c libusb_driver.h driver_api.h error.h
	$(CC) $(DLL_CFLAGS) -c $< -o $@ $(CPPFLAGS) $(INCLUDES) 

%.2.o: %.rc
	$(WINDRES) $(CPPFLAGS) $(WINDRES_FLAGS) $< -o $@

.PHONY: filter
filter: FILTER_CFLAGS = $(CFLAGS) -DLOG_APPNAME='"install-filter"'
filter: FILTER_LDFLAGS = -s -mno-cygwin -L. -lgdi32 -luser32 -lcfgmgr32 -lsetupapi -lcomctl32
filter: install-filter.exe

install-filter.exe: install_filter.1.o error.1.o install.1.o registry.1.o install_filter_rc.1.o
	$(CC) $(FILTER_CFLAGS) -o $@ -I./src  $^ $(FILTER_LDFLAGS)

%.1.o: %.c libusb_driver.h driver_api.h error.h
	$(CC) $(FILTER_CFLAGS) -c $< -o $@ $(CPPFLAGS) $(INCLUDES)

%.1.o: %.rc
	$(WINDRES) $(CPPFLAGS) $(WINDRES_FLAGS) $< -o $@

.PHONY: test
test: dll
test: TEST_CFLAGS = $(CFLAGS) -DLOG_APPNAME='"testlibusb"'
test: testlibusb.exe

testlibusb.exe: testlibusb.3.o
	$(CC) $(TEST_CFLAGS) -o $@ -I./src  $^ $(LDFLAGS)

%.3.o: %.c libusb_driver.h driver_api.h error.h
	$(CC) -c $< -o $@ $(TEST_CFLAGS) $(CPPFLAGS) $(INCLUDES) 

.PHONY: testwin
testwin: dll
testwin: TESTWIN_CFLAGS = $(CFLAGS) -DLOG_APPNAME='"testlibusb-win"'
testwin: testlibusb-win.exe

testlibusb-win.exe: testlibusb_win.4.o testlibusb_win_rc.4.o
	$(CC) $(TESTWIN_CFLAGS) -o $@ -I./src  $^ $(WIN_LDFLAGS)

%.4.o: %.c libusb_driver.h driver_api.h error.h
	$(CC) -c $< -o $@ $(TESTWIN_CFLAGS) $(CPPFLAGS) $(INCLUDES) 

%.4.o: %.rc
	$(WINDRES) $(CPPFLAGS) $(WINDRES_FLAGS) $< -o $@

.PHONY: infwizard
infwizard: INFWIZARD_CFLAGS = $(CFLAGS) -DLOG_APPNAME='"infwizard"'
infwizard: inf-wizard.exe

inf-wizard.exe: inf_wizard.5.o registry.5.o error.5.o tokenizer.5.o inf_wizard_rc.5.o 
	$(CC) $(WIN_CFLAGS) -o $@ -I./src  $^ $(WIN_LDFLAGS)

%.5.o: %.c libusb_driver.h driver_api.h error.h
	$(CC) -c $< -o $@ $(INFWIZARD_CFLAGS) $(CPPFLAGS) $(INCLUDES) 

%.5.o: %.rc
	$(WINDRES) $(CPPFLAGS) $(WINDRES_FLAGS) $< -o $@

.PHONY: driver
driver: INFWIZARD_CFLAGS = $(CFLAGS) -DLOG_APPNAME='"libusb"'
driver: $(DRIVER_TARGET)

$(DRIVER_TARGET): libusbd.a $(DRIVER_OBJECTS)
	$(CC) -o $@ $(DRIVER_OBJECTS) $(DLL_TARGET)_drv.def $(DRIVER_LDFLAGS)

libusbd.a:
	$(DLLTOOL) --dllname usbd.sys --add-underscore --def ./src/driver/usbd.def --output-lib libusbd.a

%.o: %.c libusb_driver.h driver_api.h error.h
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(INCLUDES) 

%.o: %.rc
	$(WINDRES) $(CPPFLAGS) $(WINDRES_FLAGS) $< -o $@
	
.PHONY: cleantemp
cleantemp:	
	$(RM) *.o *.a *.exp *.tar.gz *~ *.iss *.rc *.h
	$(RM) ./src/*~ *.log
	$(RM) $(DRIVER_SRC_DIR)/*~
	$(RM) README.txt
	
.PHONY: clean
clean: cleantemp	
	$(RM) *.dll *.lib *.exe *.sys

