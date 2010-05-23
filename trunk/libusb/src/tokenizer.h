/* LIBUSB-WIN32, Generic Windows USB Library
 * Copyright (c) 2010 Travis Robinson <libusbdotnet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <windows.h>
#include <stdlib.h>

#ifndef _TOKENIZER_H
#define _TOKENIZER_H


#if __STDC_WANT_SECURE_LIB__
#define safe_strncpy(dest,src,count) strncpy_s(dest,count+1,src,count)
#define safe_strcpy(dest,src) strcpy_s(dest,src?strlen(src)+1:0,src)
#else
#define safe_strncpy(dest,src,count) strncpy(dest,src,count)
#define safe_strcpy(dest,src) strcpy(dest,src)
#endif

typedef struct _token_entity_t
{
	const char* match;
	char replace[1024];
}token_entity_t;

static long tokenize_string(const char* src,
						 long src_count, 
						 char** dst,
						 token_entity_t token_entities[],
						 const char* tok_prefix,
						 const char* tok_suffix,
						 int recursive);

long tokenize_resource(LPCSTR resource_name, 
					 LPCSTR resource_type,
					 char** dst,
					 token_entity_t token_entities[],
					 const char* tok_prefix,
					 const char* tok_suffix,
					 int recursive);
#endif
