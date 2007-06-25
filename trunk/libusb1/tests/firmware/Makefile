CC = sdcc
RM = rm -f

CFLAGS = -Wl-bUSBDESC=0x1000
HID_CFLAGS = -DHID -Wl-bUSBDESC=0x1000

all: main.hex hid_main.hex

main.hex: fw_main.c fw_descriptors.c config.h
	-rm *.rel
	$(CC) -c $(CFLAGS) fw_main.c 
	$(CC) -c $(CFLAGS) fw_descriptors.c 
	$(CC) -o $@ $(CFLAGS) fw_descriptors.rel fw_main.rel 

hid_main.hex: fw_main.c fw_descriptors.c config.h
	-rm *.rel
	$(CC) -c $(HID_CFLAGS) fw_main.c 
	$(CC) -c $(HID_CFLAGS) fw_descriptors.c 
	$(CC) -o $@ $(HID_CFLAGS) fw_descriptors.rel fw_main.rel 

clean:
	$(RM) *.rel *.lnk *.map *.lst *.sym *.cdb *.asm *.hex *.rst *~ *.mem
