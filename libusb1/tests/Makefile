
CC = gcc
CFLAGS = -g -I./src -I../src/dll -I../src/driver \
	-DVERSION_MAJOR=1	-DVERSION_MINOR=0 \
	-DVERSION_MICRO=0 -DVERSION_NANO=0

LDFLAGS = -lsetupapi -lcfgmgr32 -lusb -L..
RM = -rm -f

TARGET = unit_tests.exe

TEST_OBJECTS = $(notdir $(patsubst %.c,%.o,$(wildcard ./src/test_*.c))) \
	ezusb.o fw_descriptors.o

VPATH = ./src:./firmware:../src/dll

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY : all
all: unit-tests.exe ezload.exe main.ihx 

unit-tests.exe: $(TEST_OBJECTS) unit.h test_main.h fw_descriptors.h
	$(CC) -o $@ $(TEST_OBJECTS) $(LDFLAGS) 

ezload.exe: ezload.o ezusb.o
	$(CC) -o $@ $^ $(LDFLAGS) 

main.ihx:
	cd ./firmware; make all; cd ..

.PHONY : clean
clean:	
	cd ./firmware; make clean; cd ..
	$(RM) *.o *.exe *~

