/*
 * USB Error messages
 *
 * Copyright (c) 2000-2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "usb.h"
#include "error.h"

#ifndef LOG_APPNAME
#warning The LOG_APPNAME preprocessor must be defined to use error.c in an application.
#define LOG_APPNAME "unknown"
#endif

static void output_debug_string(const char *s, ...);
static void WINAPI usb_log_def_handler(enum USB_LOG_LEVEL level, const char* message);

char usb_error_str[LOGBUF_SIZE] = "";
int usb_error_errno = 0;
int __usb_log_level = LOG_OFF;
usb_error_type_t usb_error_type = USB_ERROR_TYPE_NONE;
usb_log_handler_t log_handler = NULL;

/* prints a message to the Windows debug system */
static void output_debug_string(const char *s, ...)
{
    char tmp[512];
    va_list args;
    va_start(args, s);
    _vsnprintf(tmp, sizeof(tmp) - 1, s, args);
    va_end(args);
    OutputDebugStringA(tmp);
}

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

void usb_error(char *format, ...)
{
    va_list args;

    usb_error_type = USB_ERROR_TYPE_STRING;

    va_start(args, format);
    _vsnprintf(usb_error_str, sizeof(usb_error_str) - 1, format, args);
    va_end(args);

    if (__usb_log_level >= LOG_ERROR)
    {
        fprintf(stderr, "LIBUSB_DLL: error: %s\n", usb_error_str);
        fflush(stderr);
        output_debug_string("LIBUSB_DLL: error: %s\n", usb_error_str);
    }
}

void usb_message(char *format, ...)
{
    char tmp[512];
    va_list args;

    if (__usb_log_level >= LOG_WARNING)
    {
        va_start(args, format);
        _vsnprintf(tmp, sizeof(tmp) - 1, format, args);
        va_end(args);

        fprintf(stderr, "LIBUSB_DLL: info: %s\n", tmp);
        fflush(stderr);
        output_debug_string("LIBUSB_DLL: info: %s\n", tmp);
    }
}

/* returns Windows' last error in a human readable form */
const char *usb_win_error_to_string(void)
{
    static char tmp[512];

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
        return ETIMEDOUT;
    case ERROR_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    default:
        return EIO;
    }
}

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

    char buffer[LOGBUF_SIZE];
    int size1, size2;
    const char* prefix;

    if (__usb_log_level < level && level != LOG_ERROR) return;

    switch (level & LOG_LEVEL_MASK)
    {
    case LOG_INFO:
        prefix = "info";
        break;
    case LOG_WARNING:
        prefix = "warning";
        break;
    case LOG_ERROR:
        prefix = "error";
        break;
    case LOG_DEBUG:
        prefix = "debug";
        break;
    default:
        prefix = "unknown";
        break;
    }

    size1 = _snprintf(buffer, LOGBUF_SIZE, "%s:%s [%s] ", app_name, prefix, function);
    size2 = 0;
    if (size1 < 0)
    {
        buffer[LOGBUF_SIZE-1] = 0;
        size1 = LOGBUF_SIZE - 1;
    }
    else
    {
        size2 = _vsnprintf(buffer + size1, LOGBUF_SIZE - size1, format, args);
        if (size2 < 0)
        {
            buffer[LOGBUF_SIZE-1] = 0;
            size2 = LOGBUF_SIZE - 1 - size1;
        }
    }

	if (level == LOG_ERROR)
		memcpy(usb_error_str,buffer,size1+size2);

    if (__usb_log_level >= level)
    {
        if (log_handler)
            log_handler(level, buffer);
        else
            usb_log_def_handler(level, buffer);
    }
}

void usb_log_set_level(enum USB_LOG_LEVEL level)
{
    __usb_log_level = level;
}

int usb_log_get_level()
{
    return __usb_log_level;
}

void usb_log_set_handler(usb_log_handler_t handler)
{
    log_handler = handler;
}

usb_log_handler_t usb_log_get_handler()
{
    return log_handler;
}

static void WINAPI usb_log_def_handler(enum USB_LOG_LEVEL level, const char* message)
{
    FILE* stream = stdout;

    if (level == LOG_ERROR)
        stream = stderr;

    fprintf(stream, message);
    fflush(stream);
#if DEBUG
	OutputDebugStringA(message)
#endif

}
