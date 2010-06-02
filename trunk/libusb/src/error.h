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

// TARGETTYPEs
#define PROGRAMconsole 0
#define PROGRAMwindows 1
#define DYNLINK 2
#define DRIVER 3

// default TARGETTYPE
#ifndef TARGETTYPE
#define TARGETTYPE PROGRAMconsole
#endif

#define IS_DEBUG_MODE		(defined(_DEBUG) || defined(DEBUG) || defined(DBG))

#define IS_DRIVER			(TARGETTYPE==DRIVER)
#define IS_CONSOLE_APP		(TARGETTYPE==PROGRAMconsole)
#define IS_WINDOW_APP		(TARGETTYPE==PROGRAMwindows)
#define IS_APP				(IS_CONSOLE_APP || IS_WINDOW_APP)
#define IS_DLL				(TARGETTYPE==DYNLINK)

// NOTE: LOG_OUTPUT_TYPEs can be combined
// writes log messages to standard error output
#define LOG_OUTPUT_TYPE_STDERR		0x001

// writes log messages to Win32 OutputDebugString
#define LOG_OUTPUT_TYPE_DEBUGWINDOW	0x0002

// displays error log messages to a messagebox (not recommended)
#define LOG_OUTPUT_TYPE_MSGBOX		0x0004

// writes log messages to Kernel-mode DbgPrint
#define LOG_OUTPUT_TYPE_DBGPRINT	0x0008

// writes log messages directly to a file
#define LOG_OUTPUT_TYPE_FILE		0x0010

// strips all log messages except errors
#define LOG_OUTPUT_TYPE_REMOVE		0x0020

#define LOG_OUTPUT_TYPE_DEFAULT		0x0100

// File logging is never enabled by default.
// The LOG_OUTPUT_TYPE define must be manually
// set to enable file logging.
#if IS_DRIVER
	#ifndef LOG_DIRECTORY
		#define LOG_FILE_PATH "\\DosDevices\\C:\\Log\\" LOG_APPNAME	".log"
	#else
		#define LOG_FILE_PATH "\\DosDevices\\" LOG_DIRECTORY LOG_APPNAME ".log"
	#endif
#else
	#ifndef LOG_DIRECTORY
		#define LOG_FILE_PATH LOG_APPNAME ".log"
	#else
		#define LOG_FILE_PATH LOG_DIRECTORY LOG_APPNAME ".log"
	#endif
#endif

#if IS_DRIVER
	// default logging for a driver
	#define DEF_LOG_OUTPUT_TYPE LOG_OUTPUT_TYPE_DBGPRINT
#elif IS_DLL
	// default logging for a dll
	#define DEF_LOG_OUTPUT_TYPE LOG_OUTPUT_TYPE_DEBUGWINDOW
#else
	// default logging for applications and everything else
	#define DEF_LOG_OUTPUT_TYPE LOG_OUTPUT_TYPE_STDERR
#endif

// Default logging output
#ifdef LOG_OUTPUT_TYPE
	// all log messages (except errors) are stripped
	#if (LOG_OUTPUT_TYPE & LOG_OUTPUT_TYPE_REMOVE)
		#define USBMSG(format,...)
		#define USBWRN(format,...)
		#define USBDBG(format,...)
		#define USBRAWMSG(format,...)

		#define USBMSG0(format)
		#define USBWRN0(format)
		#define USBDBG0(format)
		#define USBRAWMSG0(format)
	#endif

	#if (LOG_OUTPUT_TYPE & LOG_OUTPUT_TYPE_DEFAULT)
		#define _LOG_OUTPUT_TYPE ((LOG_OUTPUT_TYPE & 0xff)|DEF_LOG_OUTPUT_TYPE)
	#else
		#define _LOG_OUTPUT_TYPE (LOG_OUTPUT_TYPE)
	#endif

#else
	// if the LOG_OUTPUT_TYPE has not been manually set use
	// the as defaults.
	#define _LOG_OUTPUT_TYPE DEF_LOG_OUTPUT_TYPE
#endif

// always keep error messages
#define USBERR(format,...) usb_err(__FUNCTION__,format,__VA_ARGS__)
#define USBERR0(format) usb_err(__FUNCTION__,format,NULL)

// only keep debug log messages in debug builds
#if !IS_DEBUG_MODE && !defined(USBDBG)
	#define USBDBG(format,...)
	#define USBDBG0(format)
#endif

// if USBMSG has not been defined as empty (see above)
// then keep all the info and warning log messages
#ifndef USBMSG
	#define USBMSG(format,...) usb_msg(__FUNCTION__,format,__VA_ARGS__)
	#define USBWRN(format,...) usb_wrn(__FUNCTION__,format,__VA_ARGS__)
	#define USBRAWMSG(format,...) usb_log(LOG_INFO|LOG_RAW,__FUNCTION__,format,__VA_ARGS__)

	#define USBMSG0(format) usb_msg(__FUNCTION__,format,NULL)
	#define USBWRN0(format) usb_wrn(__FUNCTION__,format,NULL)
	#define USBRAWMSG0(format) usb_log(LOG_INFO|LOG_RAW,__FUNCTION__,format,NULL)
#endif

// if USBDBG has not been defined as empty (see above)
// then keep all the debug log messages
#ifndef USBDBG
	#define USBDBG(format,...) usb_dbg(__FUNCTION__,format,__VA_ARGS__)
	#define USBDBG0(format) usb_dbg(__FUNCTION__,format,NULL)
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

#if (!IS_DRIVER)
	const char *usb_win_error_to_string(void);
	int usb_win_error_to_errno(void);
#endif

void usb_log_set_level(enum USB_LOG_LEVEL level);
int usb_log_get_level();

// these are the core logging functions used by the logging macros
// (not used directly)
void usb_err	(const char* function, const char* format, ...);
void usb_wrn	(const char* function, const char* format, ...);
void usb_msg	(const char* function, const char* format, ...);
void usb_dbg	(const char* function, const char* format, ...);
void usb_log	(enum USB_LOG_LEVEL level, const char* function, const char* format, ...);

#endif /* _ERROR_H_ */

