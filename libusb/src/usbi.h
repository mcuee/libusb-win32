#ifndef _USBI_H_
#define _USBI_H_

#include "usb.h"

#include "error.h"

extern int usb_debug;

/* Some quick and generic macros for the simple kind of lists we use */
#define LIST_ADD(begin, ent) \
	do { \
	  if (begin) { \
	    ent->next = begin; \
	    ent->next->prev = ent; \
	  } else \
	    ent->next = NULL; \
	  ent->prev = NULL; \
	  begin = ent; \
	} while(0)

#define LIST_DEL(begin, ent) \
	do { \
	  if (ent->prev) \
	    ent->prev->next = ent->next; \
	  else \
	    begin = ent->next; \
	  if (ent->next) \
	    ent->next->prev = ent->prev; \
	  ent->prev = NULL; \
	  ent->next = NULL; \
	} while (0)

struct usb_dev_handle {
  int fd;

  struct usb_bus *bus;
  struct usb_device *device;

  int config;
  int interface;
  int altsetting;

  /* Added by RMT so implementations can store other per-open-device data */
  void *impl_info;
};

void usb_fetch_descriptors(usb_dev_handle *udev);

/* OS specific routines */
int usb_os_find_busses(struct usb_bus **busses);
int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices);
void usb_os_init(void);
int usb_os_open(usb_dev_handle *dev);
int usb_os_close(usb_dev_handle *dev);

void usb_free_dev(struct usb_device *dev);
void usb_free_bus(struct usb_bus *bus);

#endif /* _USBI_H_ */

