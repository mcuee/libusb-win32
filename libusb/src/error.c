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
#warning The LOG_APPNAME preprocessor not defined
#define LOG_APPNAME "unknown"
#endif


void usb_err_v	(const char* function, const char* format, va_list args);
void usb_wrn_v	(const char* function, const char* format, va_list args);
void usb_msg_v	(const char* function, const char* format, va_list args);
void usb_dbg_v	(const char* function, const char* format, va_list args);

void usb_log_v	(enum USB_LOG_LEVEL level, const char* function, const char* format, va_list args);
void _usb_log	(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, ...);
void _usb_log_v	(enum USB_LOG_LEVEL level, const char* app_name, const char* function, const char* format, va_list args);

static void output_debug_string(const char *s, ...);
static void WINAPI usb_log_def_handler(enum USB_LOG_LEVEL level, const char* message);

#define STRIP_PREFIX(stringSrc, stringPrefix) \
	(strstr(stringSrc,stringPrefix)==stringSrc?stringSrc+strlen(stringPrefix):stringSrc)

static const char *log_level_string[LOG_LEVEL_MAX+1] =
{
    "off",
    "error",
    "warning",
    "info",
    "debug",

    "unknown",
};

static const char *skipped_function_prefix_list[] =
{
    "usb_registry_",
    "usb_",
	NULL
};

char usb_error_str[LOGBUF_SIZE] = "";
int usb_error_errno = 0;

#ifdef _DEBUG
int __usb_log_level = LOG_DEBUG;
#else
int __usb_log_level = LOG_OFF;
#endif

usb_error_type_t usb_error_type = USB_ERROR_TYPE_NONE;
usb_log_handler_t log_handler = NULL;

const char** skipped_function_prefix = skipped_function_prefix_list;

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

    char local_buffer[LOGBUF_SIZE];
    int totalCount, count;
    const char* prefix;
    const char* func;
    char* buffer;
    int masked_level = level & LOG_LEVEL_MASK;
	const char** skip_list;

    if (__usb_log_level < masked_level && masked_level != LOG_ERROR) return;
    buffer = local_buffer;
    totalCount = 0;
    count = 0;

    if (masked_level > LOG_LEVEL_MAX) masked_level = LOG_LEVEL_MAX;

    if ((level & LOG_RAW) == LOG_RAW)
    {
        count = _vsnprintf(buffer, LOGBUF_SIZE, format, args);
        if (count > 0)
        {
            buffer += count;
            totalCount += count;
        }
    }
    else
    {
        prefix = log_level_string[masked_level];
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
        count = _snprintf(buffer, (LOGBUF_SIZE-1), "%s:%s [%s] ", app_name, prefix, func);
        if (count > 0)
        {
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

    if (masked_level == LOG_ERROR)
    {
        // if this is an error message then store it
        strncpy(usb_error_str, local_buffer, totalCount);
        usb_error_str[totalCount] = '\0';
        usb_error_type = USB_ERROR_TYPE_STRING;
    }

    if (__usb_log_level >= masked_level)
    {
        // if a custom log handler has been set; use it.
        if (log_handler)
            log_handler(level, local_buffer);
        else
            usb_log_def_handler(level, local_buffer);
    }
}

void usb_log_set_level(enum USB_LOG_LEVEL level)
{
    __usb_log_level = level > LOG_LEVEL_MAX ? LOG_LEVEL_MAX : level;
}

int usb_log_get_level()
{
    return __usb_log_level;
}

/* Sets the custom log handler that is called what a new log entry arrives.
*  NOTE: Pass NULL to use the default handler.
*/
void usb_log_set_handler(usb_log_handler_t handler)
{
    log_handler = handler;
}

/* Gets the custom log handler that was set with usb_log_set_handler().
*  NOTE: this function will not return the default handler.
*/
usb_log_handler_t usb_log_get_handler()
{
    return log_handler;
}

/* Default log handler routine.
*/
static void WINAPI usb_log_def_handler(enum USB_LOG_LEVEL level, const char* message)
{
    FILE* stream = stderr;

    fprintf(stream, message);
    fflush(stream);
#ifdef _DEBUG
    OutputDebugStringA(message);
#endif

}
