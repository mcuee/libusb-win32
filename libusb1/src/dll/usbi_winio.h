
#ifndef __USBI_WINIO_H__
#define __USBI_WINIO_H__

#include <windows.h>


typedef HANDLE winio_device_t;
typedef OVERLAPPED *winio_io_t;

int winio_open_name(winio_device_t *dev, const char *name);
int winio_open_guid(winio_device_t *dev, const GUID *guid, int index);
int winio_name_by_guid(const GUID *guid, int index, char *name, int size);
int winio_close(winio_device_t dev);
int winio_ioctl_sync(winio_device_t dev, unsigned int code, 
                     void *write, int write_size, void *read, int read_size, 
                     int timeout);
int winio_write_sync(winio_device_t dev, void *data, int size, int timeout);
int winio_read_sync(winio_device_t dev, void *data, int size, int timeout);
int winio_ioctl_async(winio_device_t dev, unsigned int code, 
                      void *write, int write_size, void *read, int read_size,
                      winio_io_t *io);
int winio_write_async(winio_device_t dev, void *data, int size,
                      winio_io_t *io);                      
int winio_read_async(winio_device_t dev, void *data, int size,
                     winio_io_t *io);
int winio_wait(winio_device_t dev, winio_io_t io, int timeout);
int winio_poll(winio_device_t dev, winio_io_t io);
int winio_cancel(winio_device_t dev, winio_io_t io);
winio_io_t winio_create_io(void);
void winio_free_io(winio_io_t io);

#endif
