#ifndef __ERROR_H__
#define __ERROR_H__

#include <windows.h>

/* Connection timed out */
#define ETIMEDOUT 116

#define LOGBUF_SIZE 1024

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
	LOG_LEVEL_MASK=0xff
};

typedef void (WINAPI * usb_log_handler_t)(enum USB_LOG_LEVEL, const char*);

void usb_error(char *format, ...);
void usb_message(char *format, ...);
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
void usb_err_v	(const char* function, const char* format, va_list args);
void usb_wrn_v	(const char* function, const char* format, va_list args);
void usb_msg_v	(const char* function, const char* format, va_list args);
void usb_dbg_v	(const char* function, const char* format, va_list args);

void usb_log	(enum USB_LOG_LEVEL level, const char* function, const char* format, ...);
void usb_log_v	(enum USB_LOG_LEVEL level, const char* function, const char* format, va_list args);
void _usb_log	(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, ...);
void _usb_log_v	(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, va_list args);

#endif /* _ERROR_H_ */

