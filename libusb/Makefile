

CC = gcc
MAKE = make
CP = cp -a
CD = cd
MV = mv
RM = -rm -fr
TAR = tar
INSTALL = install
STRIP = strip
WINDRES = windres

VERSION_MAJOR = 0
VERSION_MINOR = 1
VERSION_MICRO = 7
VERSION_NANO = 8

VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO).$(VERSION_NANO)
RC_VERSION = $(VERSION_MAJOR),$(VERSION_MINOR),$(VERSION_MICRO),$(VERSION_NANO)
INF_VERSION = $(shell date +"%m\/%d\/%Y"), $(VERSION)

TARGET = libusb
DLL_TARGET = $(TARGET)$(VERSION_MAJOR)
LIB_TARGET = $(TARGET)
DRIVER_TARGETS = $(TARGET)fl.sys $(TARGET)st.sys

INF_FILES = $(TARGET)_fl.inf $(TARGET)_sa.inf

INSTALL_DIR = /usr
OBJECTS = usb.o error.o descriptors.o windows.o resource.o

SRC_DIST_DIR = $(TARGET)-win32-src-$(VERSION)
BIN_DIST_DIR = $(TARGET)-win32-bin-$(VERSION)

DIST_SOURCE_FILES = ./src
DIST_MISC_FILES = COPYING_LGPL.txt COPYING_GPL.txt AUTHORS.txt NEWS.txt \
	ChangeLog.txt

SRC_DIR = ./src
FILTER_SRC_DIR = $(SRC_DIR)/drivers/filter
STUB_SRC_DIR = $(SRC_DIR)/drivers/stub

VPATH = .:./src:./src/drivers/filter:./src/drivers/stub:./tests

INCLUDES = -I./src -I./src/drivers/filter

CFLAGS = -O2 
CPPFLAGS = -D EOVERFLOW=139 

LDFLAGS = -shared -Wl,--output-def,$(DLL_TARGET).def \
	-Wl,--out-implib,$(LIB_TARGET).a -lsetupapi


TEST_FILES = testlibusb.exe

BUILD_MSVC_LIB = 0
BUILD_BCC_LIB = 0

ifndef DDK_ROOT_PATH
	DDK_ROOT_PATH = c:/WINDDK
endif

.PHONY: all
all: $(DLL_TARGET).dll $(TEST_FILES) $(DRIVER_TARGETS) $(INF_FILES) README.txt

$(DLL_TARGET).dll: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) 
	$(STRIP) $@

windows.o: windows.c
	$(CC) -c $< -o $@ -Wall $(CFLAGS) $(CPPFLAGS) $(INCLUDES) 

%.o:%.c
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS) $(INCLUDES) 

README.txt: README.in
	sed 's/@VERSION@/$(VERSION)/' $< > $@

resource.o: resource.rc.in
	sed 's/@RC_VERSION@/$(RC_VERSION)/' $< | $(WINDRES) -o $@

testlibusb.exe: testlibusb.o
	$(CC) $(CFLAGS) -o $@ -I./src  $^ -lusb -L.
	$(STRIP) $@

%.rc:%.rc.in
	sed 's/@RC_VERSION@/$(RC_VERSION)/' $< > $(^D)/$@

%.inf:%.inf.in
	sed 's/@INF_VERSION@/$(INF_VERSION)/' $< > $@


$(TARGET)fl.sys: $(TARGET)_stub.rc $(TARGET)_filter.rc $(INF_FILES)
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
	$(INSTALL) -d $(BIN_DIST_DIR)/include
	$(INSTALL) -d $(BIN_DIST_DIR)/bin
	$(INSTALL) $(DLL_TARGET).dll $(BIN_DIST_DIR)/bin
	$(INSTALL) $(TEST_FILES) $(BIN_DIST_DIR)/bin

	$(INSTALL) $(DRIVER_TARGETS) $(BIN_DIST_DIR)/bin

	$(INSTALL) $(INF_FILES) $(BIN_DIST_DIR)/bin

	$(INSTALL) $(SRC_DIR)/usb.h $(BIN_DIST_DIR)/include
	$(INSTALL) $(LIB_TARGET).a $(BIN_DIST_DIR)/lib/gcc

ifeq ($(BUILD_BCC_LIB), 1)
	$(MAKE) bcc_lib
	$(INSTALL) -d $(BIN_DIST_DIR)/lib/bcc
	$(INSTALL) $(LIB_TARGET).lib $(BIN_DIST_DIR)/lib/bcc
endif

ifeq ($(BUILD_MSVC_LIB), 1)
	$(MAKE) msvc_lib
	$(INSTALL) -d $(BIN_DIST_DIR)/lib/msvc
	$(INSTALL) $(LIB_TARGET).lib $(BIN_DIST_DIR)/lib/msvc
endif

	$(INSTALL) $(DIST_MISC_FILES) README.txt $(BIN_DIST_DIR)
	$(TAR) -czf $(BIN_DIST_DIR).tar.gz $(BIN_DIST_DIR)

.PHONY: src_dist
src_dist:
	$(INSTALL) -d $(SRC_DIST_DIR)/src
	$(INSTALL) -d $(SRC_DIST_DIR)/src/drivers/stub
	$(INSTALL) -d $(SRC_DIST_DIR)/src/drivers/filter
	$(INSTALL) -d $(SRC_DIST_DIR)/tests
	$(INSTALL) $(SRC_DIR)/*.c $(SRC_DIST_DIR)/src
	$(INSTALL) $(SRC_DIR)/*.h $(SRC_DIST_DIR)/src
	$(INSTALL) $(SRC_DIR)/*.rc.in $(SRC_DIST_DIR)/src
	$(INSTALL) ./tests/*.c $(SRC_DIST_DIR)/tests

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

	$(INSTALL) $(DIST_MISC_FILES) README.in Makefile build_drivers.bat \
	build_all.sh $(SRC_DIST_DIR)

	$(TAR) -czf $(TARGET)-win32-src-$(VERSION).tar.gz $(SRC_DIST_DIR) 

.PHONY: dist
dist: bin_dist src_dist


.PHONY: clean
clean:	
	$(RM) *.o *.dll *.a *.def *.exp *.lib *.exe *.tar.gz *.inf *~ \
	./src/*~ *.sys \
	$(SRC_DIR)/*.rc \
	$(SRC_DIR)/drivers/*.log $(SRC_DIR)/drivers/i386 \
	$(FILTER_SRC_DIR)/*~ $(FILTER_SRC_DIR)/*.rc $(FILTER_SRC_DIR)/*.log \
	$(FILTER_SRC_DIR)/*_wxp_x86 \
	$(STUB_SRC_DIR)/*~ $(STUB_SRC_DIR)/*.rc $(STUB_SRC_DIR)/*.log \
	$(STUB_SRC_DIR)/*_wxp_x86 README
	$(RM) $(SRC_DIST_DIR)
	$(RM) $(BIN_DIST_DIR)



.PHONY : install
install: $(DLL_TARGET).dll
	$(INSTALL) -d $(INSTALL_DIR)/lib
	$(INSTALL) -d $(INSTALL_DIR)/include
	$(INSTALL) -d $(INSTALL_DIR)/bin
	$(INSTALL) $(LIB_TARGET).a $(INSTALL_DIR)/lib
	$(INSTALL) $(SRC_DIR)/usb.h $(INSTALL_DIR)/include

