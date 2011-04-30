/* Error & Logging functions

 Copyright (C) 2010 Travis Robinson. <libusbdotnet@gmail.com>
 website: http://sourceforge.net/projects/libusb-win32
 
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU Lesser General Public License as published by 
 the Free Software Foundation; either version 2 of the License, or 
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, please visit www.gnu.org.
*/
 
#include "error.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

#if IS_DRIVER
	#ifdef __GNUC__
		#define OBJ_KERNEL_HANDLE       0x00000200L
		#include <ddk/usb100.h>
		#include <ddk/usbdi.h>
		#include <ddk/winddk.h>
		#include "usbdlib_gcc.h"
	#else
		#include <ntddk.h>
	#endif
#else
	#include <windows.h>
#endif

#define USB_ERROR_BEGIN			500000

#ifndef LOG_APPNAME
#define LOG_APPNAME "LOG_APPNAME define missing"
#endif

#define GetLogLevel(UsbLogLevel) ((UsbLogLevel & LOG_LEVEL_MASK)>LOG_LEVEL_MAX?LOG_LEVEL_MAX:UsbLogLevel & LOG_LEVEL_MASK)
#define GetLogOuput(LogOutputType) (LogOutputType>0?(_LOG_OUTPUT_TYPE & LogOutputType):1)

void usb_err_v	(const char* function, const char* format, va_list args);
void usb_wrn_v	(const char* function, const char* format, va_list args);
void usb_msg_v	(const char* function, const char* format, va_list args);
void usb_dbg_v	(const char* function, const char* format, va_list args);

void usb_log_v	(enum USB_LOG_LEVEL level, const char* function, const char* format, va_list args);
void _usb_log	(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, ...);
void _usb_log_v	(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, va_list args);

static int usb_log_def_handler(enum USB_LOG_LEVEL level, 
								const char* app_name, 
								const char* prefix, 
								const char* func, 
								int app_prefix_func_end,
								char* message,
								int message_length);

#define STRIP_PREFIX(stringSrc, stringPrefix) \
	(strstr(stringSrc,stringPrefix)==stringSrc?stringSrc+strlen(stringPrefix):stringSrc)

static const char *log_level_string[LOG_LEVEL_MAX+1] =
{
    "off",
    "err",
    "wrn",
    "",
    "dbg",

    "unknown",
};

static const char *skipped_function_prefix_list[] =
{
    "usb_registry_",
    "usb_",
	NULL
};

int usb_error_errno = 0;
log_hander_t user_log_hander = NULL;

#if (defined(_DEBUG) || defined(DEBUG) || defined(DBG))
int __usb_log_level = LOG_LEVEL_MAX;
#else
int __usb_log_level = LOG_OFF;
#endif

usb_error_type_t usb_error_type = USB_ERROR_TYPE_NONE;

const char** skipped_function_prefix = skipped_function_prefix_list;

#if !IS_DRIVER

char usb_error_str[LOGBUF_SIZE] = "";

char *usb_strerror(void)
{
    switch (usb_error_type)
    {
    case USB_ERROR_TYPE_NONE:
        return "No error";
    case USB_ERROR_TYPE_STRING:
        return usb_error_str;
    case USB_ERROR_TYPE_ERRNO:
        if (usb_error_errno > -USB_ERROR_BEGIN)
            return strerror(usb_error_errno);
        else
            /* Any error we don't know falls under here */
            return "Unknown error";
    }

    return "Unknown error";
}

/* returns Windows' last error in a human readable form */
const char *usb_win_error_to_string(void)
{
    static char tmp[LOGBUF_SIZE];

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                  LANG_USER_DEFAULT, tmp, sizeof(tmp) - 1, NULL);

    return tmp;
}


int usb_win_error_to_errno(void)
{
    switch (GetLastError())
    {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_INVALID_PARAMETER:
        return EINVAL;
    case ERROR_SEM_TIMEOUT:
    case ERROR_OPERATION_ABORTED:
        return ETRANSFER_TIMEDOUT;
    case ERROR_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    default:
        return EIO;
    }
}

#endif

void usb_err(const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    usb_err_v(function, format, args);
    va_end(args);
}
void usb_wrn(const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    usb_wrn_v(function, format, args);
    va_end(args);
}

void usb_msg(const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    usb_msg_v(function, format, args);
    va_end(args);
}

void usb_dbg(const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    usb_dbg_v(function, format, args);
    va_end(args);
}

void usb_log(enum USB_LOG_LEVEL level, const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    usb_log_v(level, function, format, args);
    va_end(args);
}

void usb_err_v(const char* function, const char* format, va_list args)
{
    usb_log_v(LOG_ERROR, function, format, args);
}

void usb_wrn_v(const char* function, const char* format, va_list args)
{
    usb_log_v(LOG_WARNING, function, format, args);
}

void usb_msg_v(const char* function, const char* format, va_list args)
{
    usb_log_v(LOG_INFO, function, format, args);
}

void usb_dbg_v(const char* function, const char* format, va_list args)
{
    usb_log_v(LOG_DEBUG, function, format, args);
}

void usb_log_v(enum USB_LOG_LEVEL level, const char* function, const char* format, va_list args)
{
    _usb_log_v(level, LOG_APPNAME, function, format, args);
}

void _usb_log(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    _usb_log_v(level, app_name, function, format, args);
    va_end(args);
}

void _usb_log_v(enum USB_LOG_LEVEL level,
                const char* app_name,
                const char* function,
                const char* format,
                va_list args)
{

    char local_buffer[LOGBUF_SIZE];
    int totalCount, count;
    const char* prefix;
    const char* func;
    char* buffer;
    int masked_level;
	int app_prefix_func_end;
#ifndef LOG_STYLE_SHORT
	const char** skip_list = NULL;
#endif

	masked_level = GetLogLevel(level);

    if (__usb_log_level < masked_level && masked_level != LOG_ERROR) return;
    buffer = local_buffer;
    totalCount = 0;
    count = 0;
    prefix = log_level_string[masked_level];
	func = function;
	app_prefix_func_end = 0;

    if (masked_level > LOG_LEVEL_MAX) masked_level = LOG_LEVEL_MAX;

    if ((level & LOG_RAW) == LOG_RAW)
    {
        count = _vsnprintf(buffer, LOGBUF_SIZE-1, format, args);
        if (count > 0)
        {
            buffer += count;
            totalCount += count;
        }
    }
    else
    {
#ifdef LOG_STYLE_SHORT
        if ((prefix) && strlen(prefix))
        {
		    count = _snprintf(buffer, (LOGBUF_SIZE-1), "%s: ",  prefix);
        }
        else
        {
		    count = 0;
        }
		func = "";
#else
		func = function;

		if (func)
		{
			// strip some prefixes to shorten function names
			skip_list=skipped_function_prefix;
			while(*skip_list && ((func)) && func[0])
			{
				func = STRIP_PREFIX(func,skip_list[0]);
				skip_list++;
			}
		}

		if(!func) func="none";

        // print app name, level string and short function name
        if ((prefix) && strlen(prefix))
        {
            count = _snprintf(buffer, (LOGBUF_SIZE-1), "%s:%s [%s] ", app_name, prefix, func);
        }
        else
        {
            count = _snprintf(buffer, (LOGBUF_SIZE-1), "%s:[%s] ", app_name, func);
        }
#endif

        if (count >= 0)
        {
			app_prefix_func_end = count;
            buffer += count;
            totalCount += count;
            count = _vsnprintf(buffer, (LOGBUF_SIZE-1) - totalCount, format, args);
            if (count > 0)
            {
                buffer += count;
                totalCount += count;
            }
        }
    }

	if (count < 0)
        totalCount = LOGBUF_SIZE - 1;

    // make sure its null terminated
    local_buffer[totalCount] = 0;

#if (!IS_DRIVER)
    if (masked_level == LOG_ERROR)
    {
        // if this is an error message then store it
        strncpy(usb_error_str, local_buffer, totalCount);
        usb_error_str[totalCount] = '\0';
        usb_error_type = USB_ERROR_TYPE_STRING;
    }
#endif

	if (user_log_hander)
	{
		if (user_log_hander(level, app_name, prefix, func, app_prefix_func_end, local_buffer, totalCount))
			return;
	}
	if (__usb_log_level >= masked_level)
	{
		usb_log_def_handler(level, app_name, prefix, func, app_prefix_func_end, local_buffer, totalCount);
	}
}

void usb_log_set_level(enum USB_LOG_LEVEL level)
{
	// Debug builds of the driver force all messages on; all the time;
	// Application can no longer change this.
	//
#if (defined(_DEBUG) || defined(DEBUG) || defined(DBG))
	__usb_log_level = LOG_LEVEL_MAX;
#else
    __usb_log_level = level > LOG_LEVEL_MAX ? LOG_LEVEL_MAX : level;
#endif
}

int usb_log_get_level()
{
    return __usb_log_level;
}

/* Default log handler
*/
static int usb_log_def_handler(enum USB_LOG_LEVEL level, 
								const char* app_name, 
								const char* prefix, 
								const char* func, 
								int app_prefix_func_end,
								char* message,
								int message_length)
{
#if IS_DRIVER
	DbgPrint("%s",message);
#else
	#if GetLogOuput(LOG_OUTPUT_TYPE_FILE)
		FILE* file;
		file = fopen(LOG_FILE_PATH,"a");
		if (file)
		{
			fwrite(message,1,strlen(message),file);
			fflush(file);
			fclose(file);
		}
	#endif

	#if GetLogOuput(LOG_OUTPUT_TYPE_STDERR)
		fprintf(stderr, "%s", message);
	#endif

	#if GetLogOuput(LOG_OUTPUT_TYPE_DEBUGWINDOW)
		OutputDebugStringA(message);
	#endif


	#if GetLogOuput(LOG_OUTPUT_TYPE_MSGBOX)
		if (GetLogLevel(level)==LOG_ERROR)
		{
			message[app_prefix_func_end-1]='\0';
			MessageBoxA(NULL,message+strlen(message),message,MB_OK|MB_ICONERROR);
		}
	#endif

#endif // IS_DRIVER

	return 1;
}

void usb_log_set_handler(log_hander_t log_hander)
{
	user_log_hander = log_hander;
}

log_hander_t usb_log_get_handler(void)
{
	return user_log_hander;
}
