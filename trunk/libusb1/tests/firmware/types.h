
#ifndef __TYPES_H__
#define __TYPES_H__

#include <stddef.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef uint8_t bool_t;

#define FALSE 0
#define TRUE (!FALSE)

#define MSB(a) ((unsigned char)(((unsigned int)(a)) >> 8))
#define LSB(a) ((unsigned char)(((unsigned int)(a)) & 0x000000ff))

#define SPLIT_16(a) LSB(a), MSB(a)

#endif
