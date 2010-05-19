#ifndef __ERROR_H__
#define __ERROR_H__

#include <windows.h>

/* Connection timed out */
#define ETIMEDOUT 116

#define LOGBUF_SIZE 1024

#define USBMSG(format,...) usb_msg(__FUNCTION__,format,__VA_ARGS__)
#define USBERR(format,...) usb_err(__FUNCTION__,format,__VA_ARGS__)
#define USBWRN(format,...) usb_wrn(__FUNCTION__,format,__VA_ARGS__)
#define USBDBG(format,...) usb_dbg(__FUNCTION__,format,__VA_ARGS__)
#define USBRAWMSG(format,...) usb_log(LOG_INFO|LOG_RAW,__FUNCTION__,format,__VA_ARGS__)

#define USBMSG0(format) usb_msg(__FUNCTION__,format,NULL)
#define USBERR0(format) usb_err(__FUNCTION__,format,NULL)
#define USBWRN0(format) usb_wrn(__FUNCTION__,format,NULL)
#define USBDBG0(format) usb_dbg(__FUNCTION__,format,NULL)
#define USBRAWMSG0(format) usb_log(LOG_INFO|LOG_RAW,__FUNCTION__,format,NULL)

typedef enum
{
    USB_ERROR_TYPE_NONE = 0,
    USB_ERROR_TYPE_STRING,
    USB_ERROR_TYPE_ERRNO,
} usb_error_type_t;


enum USB_LOG_LEVEL
{
	LOG_OFF,
	LOG_ERROR,
	LOG_WARNING,
	LOG_INFO,
	LOG_DEBUG,

	LOG_LEVEL_MAX,
	LOG_LEVEL_MASK=0xff,
	LOG_RAW=0x100

};

typedef void (WINAPI * usb_log_handler_t)(enum USB_LOG_LEVEL, const char*);

const char *usb_win_error_to_string(void);
int usb_win_error_to_errno(void);

void usb_log_set_level(enum USB_LOG_LEVEL level);
int usb_log_get_level();

void usb_log_set_handler(usb_log_handler_t handler);
usb_log_handler_t usb_log_get_handler();

void usb_err	(const char* function, const char* format, ...);
void usb_wrn	(const char* function, const char* format, ...);
void usb_msg	(const char* function, const char* format, ...);
void usb_dbg	(const char* function, const char* format, ...);
void usb_log	(enum USB_LOG_LEVEL level, const char* function, const char* format, ...);

#endif /* _ERROR_H_ */

