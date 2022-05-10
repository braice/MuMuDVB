#pragma once

struct timezone
{
    int  tz_minuteswest; /* minutes W of Greenwich */
    int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
int asprintf(char **strp, const char *fmt, ...);
void usleep(unsigned int usec);
void sleep(unsigned int sec);
int mkpath(char *file_path, unsigned int mode);
