#include <stdio.h>

#include "ezusb.h"


void usage(void)
{
  printf("usage: ezload <vid> <pid> <hex-file>\n");
}

int main(int argc, char **argv)
{
  int vid = 0, pid = 0;
  struct usb_bus *bus;
  struct usb_device *dev;
  usb_dev_handle *hdev = NULL;

  if(argc != 4) {
    usage();
    return 0;
  }

  if(argv[1][1] == 'x')
    sscanf(argv[1], "0x%x", &vid);
  else
    sscanf(argv[1], "%d", &vid);
  if(argv[2][1] == 'x')
    sscanf(argv[2], "0x%x", &pid);
  else
    sscanf(argv[2], "%d", &pid);

  if(!vid || !pid) {
    usage();
    return 0;
  }

  usb_init();
  usb_set_debug(0);
  usb_find_busses();
  usb_find_devices();

  for(bus = usb_get_busses(); bus; bus = bus->next) 
    {
      for(dev = bus->devices; dev; dev = dev->next) 
        {
          if(dev->descriptor.idVendor == vid
             && dev->descriptor.idProduct == pid)
            {
              hdev = usb_open(dev);
              if(hdev)
                break;
            }
        }
      if(hdev)
        break;
    }

  if(!hdev) {
    printf("unable to open device 0x%04x-0x%04x\n", vid, pid);
    return 1;
  }

  if(!ezusb_load_file(hdev, argv[3]))
    printf("loading firmware failed\n");
  
  usb_close(hdev);

  return 0;
}

