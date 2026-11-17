#ifndef _CRC32_H
#define _CRC32_H

#include <stdint.h>

extern uint32_t crc32_table[256];

unsigned long calculateCRC32(const unsigned char *data, int len);

void setCRC32(unsigned char *data);

#endif //_CRC32_H
