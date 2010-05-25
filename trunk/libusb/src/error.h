/* Error & Logging functions

 Copyright © 2010 Travis Robinson. <libusbdotnet@gmail.com>
 website: http://sourceforge.net/projects/libusb-win32
 
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU (LGPL) General Public License as published by 
 the Free Software Foundation; either version 2 of the License, or 
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU (LGPL) General Public
 License for more details.
 
 You should have received a copy of the GNU (LGPL) General Public License
 along with this program; if not, please visit www.gnu.org.
*/

#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdarg.h>

/* Connection timed out */
#define ETIMEDOUT 116

#define LOGBUF_SIZE 512

#define PROGRAMconsole 0
#define PROGRAMwindows 1
#define DYNLINK 2
#define DRIVER 3

#ifndef TARGETTYPE
#define TARGETTYPE PROGRAMconsole
#endif
#define IsDebugMode()		(defined(_DEBUG) || defined(DEBUG) || defined(DBG))

#define IsDriver()			(TARGETTYPE==DRIVER)
#define IsConsoleApp()		(TARGETTYPE==PROGRAMconsole)
#define IsWindowsApp()		(TARGETTYPE==PROGRAMwindows)
#define IsApplication()		(IsConsoleApp() || IsWindowsApp())
#define IsDll()				(TARGETTYPE==DYNLINK)

#define LOG_OUTPUT_TYPE_REMOVE		0x00
#define LOG_OUTPUT_TYPE_STDERR		0x01
#define LOG_OUTPUT_TYPE_DEBUGWINDOW	0x02
#define LOG_OUTPUT_TYPE_MSGBOX		0x04
#define LOG_OUTPUT_TYPE_FILE		0x20
#define LOG_OUTPUT_TYPE_DBGPRINT	0x10

// Default logging output
#ifndef LOG_OUTPUT_TYPE

	#if IsDriver()
		#define LOG_OUTPUT_TYPE LOG_OUTPUT_TYPE_DBGPRINT
	#endif

	#if IsDll()
		#define LOG_OUTPUT_TYPE LOG_OUTPUT_TYPE_DEBUGWINDOW
	#endif

	#ifndef LOG_OUTPUT_TYPE
		#define LOG_OUTPUT_TYPE LOG_OUTPUT_TYPE_STDERR
	#endif

#endif

// Log messages can be completely stripped be defining LOG_OUTPUT_TYPE=LOG_OUTPUT_TYPE_REMOVE
#if (LOG_OUTPUT_TYPE==LOG_OUTPUT_TYPE_REMOVE && !IsDebugMode())
#define USBMSG(format,...)
#define USBERR(format,...)
#define USBWRN(format,...)
#define USBDBG(format,...)
#define USBRAWMSG(format,...)

#define USBMSG0(format)
#define USBERR0(format)
#define USBWRN0(format)
#define USBDBG0(format)
#define USBRAWMSG0(format)
#endif

// These are the actually logging macros that are used by the application
#ifndef USBMSG
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
#endif

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

typedef void (*usb_log_handler_t)(enum USB_LOG_LEVEL, const char*);

#if (!IsDriver())
const char *usb_win_error_to_string(void);
int usb_win_error_to_errno(void);
#endif

void usb_log_set_level(enum USB_LOG_LEVEL level);
int usb_log_get_level();

void usb_err	(const char* function, const char* format, ...);
void usb_wrn	(const char* function, const char* format, ...);
void usb_msg	(const char* function, const char* format, ...);
void usb_dbg	(const char* function, const char* format, ...);
void usb_log	(enum USB_LOG_LEVEL level, const char* function, const char* format, ...);

#endif /* _ERROR_H_ */

