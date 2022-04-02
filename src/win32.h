#pragma once

int gettimeofday(struct timeval *tv, struct timezone *tz);
int asprintf(char **strp, const char *fmt, ...);
void usleep(unsigned int usec);
