#include "section.h"

/* shamelessly stolen from dvbsnoop, but modified */
u32 getBits (const u8 *buf, int startbit, int bitlen)
{
	const u8 *b;
	u32 mask,tmp_long;
	int bitHigh,i;

	b = &buf[startbit / 8];
	startbit %= 8;

	bitHigh = 8;
	tmp_long = b[0];
	for (i = 0; i < ((bitlen-1) >> 3); i++) {
		tmp_long <<= 8;
		tmp_long  |= b[i+1];
		bitHigh   += 8;
	}

	startbit = bitHigh - startbit - bitlen;
	tmp_long = tmp_long >> startbit;
	mask     = (1ULL << bitlen) - 1;
	return tmp_long & mask;
}
