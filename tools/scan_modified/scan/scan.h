#ifndef __SCAN_H__
#define __SCAN_H__

#include <stdio.h>
#include <errno.h>

extern int verbosity;

#define dprintf(level, fmt...)			\
	do {					\
		if (level <= verbosity)		\
			fprintf(stderr, fmt);	\
	} while (0)

#define dpprintf(level, fmt, args...) \
	dprintf(level, "%s:%d: " fmt, __FUNCTION__, __LINE__ , ##args)

#define fatal(fmt, args...) do { dpprintf(-1, "FATAL: " fmt , ##args); exit(1); } while(0)
#define error(msg...) dprintf(0, "ERROR: " msg)
#define errorn(msg) dprintf(0, "%s:%d: ERROR: " msg ": %d %m\n", __FUNCTION__, __LINE__, errno)
#define warning(msg...) dprintf(1, "WARNING: " msg)
#define info(msg...) dprintf(2, msg)
#define verbose(msg...) dprintf(3, msg)
#define moreverbose(msg...) dprintf(4, msg)
#define debug(msg...) dpprintf(5, msg)
#define verbosedebug(msg...) dpprintf(6, msg)

#endif

