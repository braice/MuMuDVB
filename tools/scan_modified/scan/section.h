#ifndef __SECTION_H__
#define __SECTION_H__

#include <stdio.h>

#define u8  unsigned char
#define u16 unsigned short
#define u32 unsigned int

#define PACKED __attribute((packed))

u32 getBits (const u8 *buf, int startbit, int bitlen);

#endif
