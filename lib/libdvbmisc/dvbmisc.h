/*
	libdvbmisc - DVB miscellaneous library

	Copyright (C) 2005 Manu Abraham <abraham.manu@gmail.com>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.
	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#ifndef DVB_MISC_H
#define DVB_MISC_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#define ERROR		0
#define NOTICE		1
#define INFO		2
#define DEBUG		3

#define print(x, y, z, fmt, arg...) do {				\
	if (z) {							\
		if	((x > ERROR) && (x > y))			\
			vprint("%s: " fmt "\n", __func__ , ##arg);	\
		else if	((x > NOTICE) && (x > y))			\
			vprint("%s: " fmt "\n",__func__ , ##arg);	\
		else if ((x > INFO) && (x > y))				\
			vprint("%s: " fmt "\n", __func__ , ##arg);	\
		else if ((x > DEBUG) && (x > y))			\
			vprint("%s: " fmt "\n", __func__ , ##arg);	\
	} else {							\
		if (x > y)						\
			vprint(fmt, ##arg);				\
	}								\
} while(0)

static inline void vprint(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

static inline int time_after(struct timeval oldtime, uint32_t delta_ms)
{
	// calculate the oldtime + add on the delta
	uint64_t oldtime_ms = (oldtime.tv_sec * 1000) + (oldtime.tv_usec / 1000);
	oldtime_ms += delta_ms;

	// calculate the nowtime
	struct timeval nowtime;
	gettimeofday(&nowtime, 0);
	uint64_t nowtime_ms = (nowtime.tv_sec * 1000) + (nowtime.tv_usec / 1000);

	// check
	return nowtime_ms > oldtime_ms;
}

#endif
