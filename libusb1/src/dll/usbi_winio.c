#include "usbi_winio.h"
#include "usbi.h"

#include <setupapi.h>
#include <string.h>
#include <stdio.h>
#include <winioctl.h>
#include <initguid.h>



#define WINIO_DEBUG_ASSERT_DEV(dev) \
  USBI_DEBUG_ASSERT_PARAM((dev) != INVALID_HANDLE_VALUE, \
                          dev, USBI_STATUS_PARAM)

winio_io_t winio_create_io(void)
{
  winio_io_t io = malloc(sizeof(*io));

  if(!io)
    return NULL;
  memset(io, 0, sizeof(*io));
  io->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if(!io->hEvent) {
    free(io);
    return NULL;
  }
  return io;
}

void winio_free_io(winio_io_t io)
{
  if(io->hEvent)
    CloseHandle(io->hEvent);
  free(io);
}

int winio_open_name(winio_device_t *dev, const char *name)
{
  USBI_DEBUG_ASSERT_PARAM(name, name, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(dev, dev, USBI_STATUS_PARAM);

  *dev = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 
                    FILE_SHARE_READ | FILE_SHARE_WRITE, 
                    NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  
  if(*dev == INVALID_HANDLE_VALUE)
    *dev = CreateFile(name, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  
  if(*dev == INVALID_HANDLE_VALUE) {
    return USBI_STATUS_BUSY;
  }
  return USBI_STATUS_SUCCESS;
}

int winio_open_guid(winio_device_t *dev, const GUID *guid, int index)
{
  int ret;
  char name[1024];

  USBI_DEBUG_ASSERT_PARAM(guid, guid, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(dev, dev, USBI_STATUS_PARAM);

  if((ret = winio_name_by_guid(guid, index, name, sizeof(name)) >= 0))
    ret = winio_open_name(dev, name);
  return ret;
}


int winio_name_by_guid(const GUID *guid, int index, char *name, int size)

{
  HDEVINFO *dev_info = NULL;
	SP_DEVICE_INTERFACE_DATA dev_data;
  SP_DEVICE_INTERFACE_DETAIL_DATA *dev_detail_data = NULL;
  DWORD length = 0;
  int ret = USBI_STATUS_UNKNOWN;

  USBI_DEBUG_ASSERT_PARAM(guid, guid, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(name, name, USBI_STATUS_PARAM);

  dev_info = SetupDiGetClassDevs(guid, NULL, NULL,
                                 DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
  if(dev_info == INVALID_HANDLE_VALUE)
    return USBI_STATUS_UNKNOWN;

	dev_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  do {
    if(!SetupDiEnumDeviceInterfaces(dev_info, 0, guid, index, &dev_data))
      break;
    
    SetupDiGetDeviceInterfaceDetail(dev_info, &dev_data, 
                                    NULL, 0, &length, NULL);
    
    if(!length)
      break;
    
    if(!(dev_detail_data = malloc(length)))
      break;
      
    dev_detail_data->cbSize = sizeof(*dev_detail_data);
    
    if(!SetupDiGetDeviceInterfaceDetail(dev_info, &dev_data, 
                                        dev_detail_data, length, 
                                        &length, NULL))
      break;
    
    snprintf(name, size - 1, dev_detail_data->DevicePath);
    ret = USBI_STATUS_SUCCESS;
  } while(0);

  if(dev_detail_data)
    free(dev_detail_data);
	
  SetupDiDestroyDeviceInfoList(dev_info);
  return ret;
}

int winio_close(winio_device_t dev)
{
  if(dev != INVALID_HANDLE_VALUE)
    CloseHandle(dev);
  return USBI_STATUS_SUCCESS;
}

int winio_ioctl_async(winio_device_t dev, unsigned int code, 
                      void *write, int write_size,
                      void *read, int read_size, winio_io_t *io)
{
  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(io, io, USBI_STATUS_PARAM);

  if(!(*io = winio_create_io()))
    return USBI_STATUS_NOMEM;
    
  if(!DeviceIoControl(dev, code, write, write_size,
                      read, read_size, NULL, *io)) {
    if(GetLastError() != ERROR_IO_PENDING) {
      winio_free_io(*io);
      return USBI_STATUS_UNKNOWN;
    }
  }
  return USBI_STATUS_SUCCESS;
}

int winio_write_async(winio_device_t dev, void *data, int size, winio_io_t *io)
{
  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(io, io, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(data, data, USBI_STATUS_PARAM);

  if(!(*io = winio_create_io()))
    return USBI_STATUS_NOMEM;
    
  if(!WriteFile(dev, data, size, NULL, *io)) {
    if(GetLastError() != ERROR_IO_PENDING) {
      winio_free_io(*io);
      return USBI_STATUS_UNKNOWN;
    }
  }
  return USBI_STATUS_SUCCESS;
}

int winio_read_async(winio_device_t dev, void *data, int size, winio_io_t *io)
{
  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(io, io, USBI_STATUS_PARAM);
  USBI_DEBUG_ASSERT_PARAM(data, data, USBI_STATUS_PARAM);

  if(!(*io = winio_create_io()))
    return USBI_STATUS_NOMEM;

  if(!ReadFile(dev, data, size, NULL, *io)) {
    if(GetLastError() != ERROR_IO_PENDING) {
      winio_free_io(*io);
      return USBI_STATUS_UNKNOWN;
    }
  }
  return USBI_STATUS_SUCCESS;
}


int winio_ioctl_sync(winio_device_t dev, unsigned int code,
                     void *write, int write_size, void *read, int read_size, 
                     int timeout)
{
  winio_io_t io;
  int ret;

  WINIO_DEBUG_ASSERT_DEV(dev);

  ret = winio_ioctl_async(dev, code, write, write_size, read, read_size, &io);
  if(!USBI_SUCCESS(ret))
    return ret;
  return winio_wait(dev, io, timeout);
}


int winio_write_sync(winio_device_t dev, void *data, int size, int timeout)
{
  winio_io_t io;
  int ret;

  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(data, data, USBI_STATUS_PARAM);

  ret = winio_write_async(dev, data, size, &io);
  if(!USBI_SUCCESS(ret))
    return ret;
  return winio_wait(dev, io, timeout);
}

int winio_read_sync(winio_device_t dev, void *data, int size, int timeout)
{
  winio_io_t io;
  int ret;

  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(data, data, USBI_STATUS_PARAM);

  ret = winio_read_async(dev, data, size, &io);
  if(!USBI_SUCCESS(ret))
    return ret;
  return winio_wait(dev, io, timeout);
}

int winio_wait(winio_device_t dev, winio_io_t io, int timeout)
{
  DWORD t = 0;
  int ret;

  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(io, io, USBI_STATUS_PARAM);

  switch(WaitForSingleObject(io->hEvent, timeout)) {
  case WAIT_OBJECT_0:
    if(GetOverlappedResult(dev, io, &t, FALSE))
      ret = (int)t;
    else
      ret = USBI_STATUS_UNKNOWN;
    winio_free_io(io);
    return ret;
  case WAIT_TIMEOUT:
    winio_cancel(dev, io);
    return USBI_STATUS_TIMEOUT;
  default:
    winio_free_io(io);
    return USBI_STATUS_UNKNOWN;
  }
}

int winio_poll(winio_device_t dev, winio_io_t io)
{
  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(io, io, USBI_STATUS_PARAM);

  if(HasOverlappedIoCompleted(io))
    return winio_wait(dev, io, 0);
  return USBI_STATUS_PENDING;
}

int winio_cancel(winio_device_t dev, winio_io_t io)
{
  WINIO_DEBUG_ASSERT_DEV(dev);
  USBI_DEBUG_ASSERT_PARAM(io, io, USBI_STATUS_PARAM);

  CancelIo(dev);
  /* wait until the request is completed */
  WaitForSingleObject(io->hEvent, INFINITE);
  winio_free_io(io);
  
  return USBI_STATUS_SUCCESS;
}
