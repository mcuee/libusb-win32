

CC = gcc
LD = ld
MAKE = make
CP = cp -a
CD = cd
MV = mv
RM = -rm -fr
TAR = tar
WINDRES = windres
NSIS = makensis
INSTALL = install

VERSION_MAJOR = 0
VERSION_MINOR = 1
VERSION_MICRO = 8
VERSION_NANO = 1

VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO).$(VERSION_NANO)
RC_VERSION = $(VERSION_MAJOR),$(VERSION_MINOR),$(VERSION_MICRO),$(VERSION_NANO)
INF_VERSION = $(shell date +"%m\/%d\/%Y"), $(VERSION)
DATE = $(shell date +"%Y%m%d")

TARGET = libusb
DLL_TARGET = $(TARGET)$(VERSION_MAJOR)
LIB_TARGET = $(TARGET)
DRIVER_TARGETS = $(TARGET)fl.sys $(TARGET)st.sys
INSTALLER_TARGET = libusb-win32-filter-bin-$(VERSION).exe

INSTALL_DIR = /usr
OBJECTS = usb.o error.o descriptors.o windows.o resource.o

SRC_DIST_DIR = $(TARGET)-win32-src-$(VERSION)
BIN_DIST_DIR = $(TARGET)-win32-device-bin-$(VERSION)


DIST_SOURCE_FILES = ./src
DIST_MISC_FILES = COPYING_LGPL.txt COPYING_GPL.txt AUTHORS.txt ChangeLog.txt 

SRC_DIR = ./src
FILTER_SRC_DIR = $(SRC_DIR)/drivers/filter
STUB_SRC_DIR = $(SRC_DIR)/drivers/stub

VPATH = .:./src:./src/drivers/filter:./src/drivers/stub:./tests:./src/service

INCLUDES = -I./src -I./src/drivers/filter -I./src/service

CFLAGS = -O2 -s -mno-cygwin

CPPFLAGS = -DVERSION_MAJOR=$(VERSION_MAJOR) \
	-DVERSION_MINOR=$(VERSION_MINOR) \
	-DVERSION_MICRO=$(VERSION_MICRO) \
	-DVERSION_NANO=$(VERSION_NANO)

LDFLAGS = -s -shared -Wl,--output-def,$(DLL_TARGET).def  -mno-cygwin


TEST_FILES = testlibusb.exe testlibusb-win.exe


ifndef DDK_ROOT_PATH
	DDK_ROOT_PATH = C:/WINDDK/2600.1106
endif

.PHONY: all
all: $(DLL_TARGET).dll $(TEST_FILES) libusbd-nt.exe \
	libusbd-9x.exe libusbis.exe $(DRIVER_TARGETS) \
	$(TARGET).inf README.txt
	unix2dos *.txt *.inf

$(DLL_TARGET).dll: filter_api.h $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) -Wl,--out-implib,$(LIB_TARGET).a

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) -Wall $(CPPFLAGS) $(INCLUDES) 

testlibusb.exe: testlibusb.o
	$(CC) $(CFLAGS) -o $@ -I./src  $^ -lusb -L.

testlibusb-win.exe: testlibusb_win.o
	$(CC) $(CFLAGS) -mwindows -o $@ -I./src  $^ -lusb -lgdi32 -L.

libusbd-nt.exe: service_nt.o service.o registry.o resource.o win_debug.o
	$(CC) $(CPPFLAGS) -mwindows -Wall $(CFLAGS) -o $@ $^ -lsetupapi

libusbd-9x.exe: service_9x.o service.o registry.o resource.o win_debug.o
	$(CC) $(CPPFLAGS) -mwindows -Wall $(CFLAGS) -o $@ $^ -lsetupapi

libusbis.exe: nsis.o service.o registry.o resource.o win_debug.o
	$(CC) $(CPPFLAGS) -Wall $(CFLAGS) -o $@ $^ -lsetupapi

README.txt: README.in
	sed -e 's/@VERSION@/$(VERSION)/' $< > $@

%.o: %.rc.in
	sed -e 's/@RC_VERSION@/$(RC_VERSION)/' \
	-e 's/@VERSION@/$(VERSION)/' $< | $(WINDRES) -o $@

%.rc: %.rc.in
	sed -e 's/@RC_VERSION@/$(RC_VERSION)/' \
	-e 's/@VERSION@/$(VERSION)/' $< > $(^D)/$@

%.h: %.h.in
	sed -e 's/@VERSION_MAJOR@/$(VERSION_MAJOR)/' \
	-e 's/@VERSION_MINOR@/$(VERSION_MINOR)/' \
	-e 's/@VERSION_MICRO@/$(VERSION_MICRO)/' \
	-e 's/@VERSION_NANO@/$(VERSION_NANO)/' \
	$< > $(^D)/$@

%.inf: %.inf.in
	sed -e 's/@INF_VERSION@/$(INF_VERSION)/' $< > $@

$(TARGET)fl.sys: $(TARGET)_stub.rc $(TARGET)_filter.rc filter_api.h
	$(MAKE) --win32 build_drivers

.PHONY: build_drivers
build_drivers:
	call build_drivers.bat $(DDK_ROOT_PATH)

.PHONY: bcc_implib
bcc_lib:
	implib -a $(LIB_TARGET).lib $(DLL_TARGET).dll

.PHONY: msvc_lib
msvc_lib:
	$(DDK_ROOT_PATH)/bin/x86/lib.exe /machine:i386 /def:$(DLL_TARGET).def 
	$(MV) $(DLL_TARGET).lib $(LIB_TARGET).lib

.PHONY: bin_dist
bin_dist: all
	$(INSTALL) -d $(BIN_DIST_DIR)/lib/gcc
	$(INSTALL) -d $(BIN_DIST_DIR)/lib/bcc
	$(INSTALL) -d $(BIN_DIST_DIR)/lib/msvc
	$(INSTALL) -d $(BIN_DIST_DIR)/include
	$(INSTALL) -d $(BIN_DIST_DIR)/bin
	$(INSTALL) $(TEST_FILES) $(BIN_DIST_DIR)/bin
	$(INSTALL) *.manifest $(BIN_DIST_DIR)/bin

	$(INSTALL) $(DRIVER_TARGETS) $(BIN_DIST_DIR)/bin
	$(INSTALL) $(TARGET).inf $(BIN_DIST_DIR)/bin
	$(INSTALL) $(TARGET)is.exe $(BIN_DIST_DIR)/bin
	$(INSTALL) $(DLL_TARGET).dll $(BIN_DIST_DIR)/bin

	$(INSTALL) $(SRC_DIR)/usb.h $(BIN_DIST_DIR)/include
	$(INSTALL) $(LIB_TARGET).a $(BIN_DIST_DIR)/lib/gcc
	$(MAKE) bcc_lib 
	$(INSTALL) $(LIB_TARGET).lib $(BIN_DIST_DIR)/lib/bcc
	$(MAKE) msvc_lib
	$(INSTALL) $(LIB_TARGET).lib $(BIN_DIST_DIR)/lib/msvc
	$(INSTALL) $(DIST_MISC_FILES) README.txt $(BIN_DIST_DIR)


.PHONY: src_dist
src_dist:
	$(INSTALL) -d $(SRC_DIST_DIR)/src
	$(INSTALL) -d $(SRC_DIST_DIR)/src/service
	$(INSTALL) -d $(SRC_DIST_DIR)/src/drivers/stub
	$(INSTALL) -d $(SRC_DIST_DIR)/src/drivers/filter
	$(INSTALL) -d $(SRC_DIST_DIR)/tests

	$(INSTALL) $(SRC_DIR)/*.c $(SRC_DIST_DIR)/src
	$(INSTALL) $(SRC_DIR)/*.h $(SRC_DIST_DIR)/src
	$(INSTALL) $(SRC_DIR)/*.rc.in $(SRC_DIST_DIR)/src
	$(INSTALL) ./tests/*.c $(SRC_DIST_DIR)/tests

	$(INSTALL) $(SRC_DIR)/service/*.c $(SRC_DIST_DIR)/src/service
	$(INSTALL) $(SRC_DIR)/service/*.h $(SRC_DIST_DIR)/src/service

	$(INSTALL) $(SRC_DIR)/drivers/dirs $(SRC_DIST_DIR)/src/drivers
	$(INSTALL) $(SRC_DIR)/drivers/usbd.lib $(SRC_DIST_DIR)/src/drivers

	$(INSTALL) $(STUB_SRC_DIR)/*.c $(SRC_DIST_DIR)/src/drivers/stub
	$(INSTALL) $(STUB_SRC_DIR)/*.in $(SRC_DIST_DIR)/src/drivers/stub
	$(INSTALL) $(STUB_SRC_DIR)/sources $(SRC_DIST_DIR)/src/drivers/stub
	$(INSTALL) $(STUB_SRC_DIR)/makefile $(SRC_DIST_DIR)/src/drivers/stub

	$(INSTALL) $(FILTER_SRC_DIR)/*.h $(SRC_DIST_DIR)/src/drivers/filter
	$(INSTALL) $(FILTER_SRC_DIR)/*.c $(SRC_DIST_DIR)/src/drivers/filter
	$(INSTALL) $(FILTER_SRC_DIR)/*.in $(SRC_DIST_DIR)/src/drivers/filter
	$(INSTALL) $(FILTER_SRC_DIR)/sources $(SRC_DIST_DIR)/src/drivers/filter
	$(INSTALL) $(FILTER_SRC_DIR)/makefile $(SRC_DIST_DIR)/src/drivers/filter

	$(INSTALL) $(DIST_MISC_FILES) *.in Makefile *.bat *.sh *.manifest \
		license_nsis.txt $(SRC_DIST_DIR)


.PHONY: dist
dist: bin_dist src_dist
	sed -e 's/@VERSION@/$(VERSION)/' \
	-e 's/@BIN_DIST_DIR@/$(BIN_DIST_DIR)/' \
	-e 's/@SRC_DIST_DIR@/$(SRC_DIST_DIR)/' \
	-e 's/@INSTALLER_TARGET@/$(INSTALLER_TARGET)/' \
	install.nsi.in > install.nsi
	$(TAR) -czf $(SRC_DIST_DIR).tar.gz $(SRC_DIST_DIR) 
	$(TAR) -czf $(BIN_DIST_DIR).tar.gz $(BIN_DIST_DIR)
	$(NSIS) install.nsi
	$(RM) $(SRC_DIST_DIR)
	$(RM) $(BIN_DIST_DIR)

.PHONY: snapshot
snapshot: VERSION = $(DATE)
snapshot: dist

.PHONY: clean
clean:	
	$(RM) *.o *.dll *.a *.def *.exp *.lib *.exe *.tar.gz *.inf *~ *.nsi
	$(RM) ./src/*~ *.sys *.log
	$(RM) $(SRC_DIR)/*.rc
	$(RM) $(SRC_DIR)/drivers/*.log $(SRC_DIR)/drivers/i386
	$(RM) $(FILTER_SRC_DIR)/*~ $(FILTER_SRC_DIR)/*.rc
	$(RM) $(FILTER_SRC_DIR)/*.log
	$(RM) $(FILTER_SRC_DIR)/*_wxp_x86
	$(RM) $(FILTER_SRC_DIR)/filter_api.h
	$(RM) $(STUB_SRC_DIR)/*~ $(STUB_SRC_DIR)/*.rc 
	$(RM) $(STUB_SRC_DIR)/*.log
	$(RM) $(STUB_SRC_DIR)/*_wxp_x86 
	$(RM) README.txt

