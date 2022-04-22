/* dvb.c
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2004-2013 Brice DUBOST
 * (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 * The latest version can be found at http://mumudvb.net
 *
 * Copyright notice:
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

 /** @file
  * @brief dvb part (except tune) of mumudvb
  * Ie : setting the filters, openning the file descriptors etc...
  */

#define _GNU_SOURCE
#define _CRT_SECURE_NO_WARNINGS

#include "dvb.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "log.h"
#include <sys/stat.h>

static char *log_module = "DVBW32: ";

/**
 * @brief Open the frontend associated with card
 * Return 1 in case of succes, -1 otherwise
 *
 * @param fd_frontend the file descriptor for the frontend
 * @param card the card number
 */
int open_fe(HANDLE *fd_frontend, char *base_path, int tuner, int rw, int full_path)
{
    char device_path[MAX_PATH] = { 0, };
    HANDLE pipe = INVALID_HANDLE_VALUE;
    int retries = 5;

    /* we only support files or named pipes */
    if (!full_path)
        return -1;

    /* if we have a local file to try and open */
    if (base_path) {
        pipe = CreateFile(base_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) {
            log_message(log_module, MSG_ERROR, "Cannot open file %s (%s)\n", base_path, strerror(GetLastError()));
            return -1;
        }

        *fd_frontend = pipe;
        return 1;
    }

    /* generate named pipe based on tuner number */
    snprintf(device_path, sizeof(device_path), "\\\\.\\pipe\\DVB_TUNER%d", tuner);
    do {
        pipe = CreateFile(device_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE)
            break;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            log_message(log_module, MSG_ERROR, "Cannot connect to tuner %d pipe (%s)\n", tuner, strerror(GetLastError()));
            return -1;
        }

        if (!WaitNamedPipe(device_path, 2)) {
            log_message(log_module, MSG_ERROR, "Cannot connect to tuner %d pipe (%s)\n", tuner, strerror(GetLastError()));
            return -1;
        }
    } while (retries--);

    /* pipe opened */

    *fd_frontend = pipe;

    return 1;
}

int dvb_poll(HANDLE fd_dvr, int timeout)
{
    DWORD avail = 0;
    static DWORD type = 0;

    /* do this once */
    if (!type)
        type = GetFileType(fd_dvr);

    if (type == FILE_TYPE_PIPE) {
        if (PeekNamedPipe(fd_dvr, NULL, 0, NULL, &avail, NULL)) {
            if (avail > TS_PACKET_SIZE * 20)
                return 1;

            return 0;
        }
    } else {
        /* assume file will always have some data to read */
        return 1;
    }

    return -1;
}

void set_ts_filt(int fd, uint16_t pid)
{
    (void)fd;
    (void)pid;
}

void *show_power_func(void *arg)
{
    (void)arg;

    return 0;
}

int create_card_fd(char *base_path, int tuner, uint8_t *asked_pid, fds_t *fds)
{
    (void)base_path;
    (void)tuner;
    (void)asked_pid;
    (void)fds;

    return 0;
}

/**
 * @brief Open filters for the pids in asked_pid. This function update the asked_pid array and
 * can be called more than one time if new pids are added (typical case autoconf)
 * @param asked_pid the array of asked pids
 */
void set_filters(uint8_t *asked_pid, fds_t *fds)
{
    for (int curr_pid = 0; curr_pid < 8193; curr_pid++) {
        if (asked_pid[curr_pid] == PID_ASKED) {
            asked_pid[curr_pid] = PID_FILTERED;
        }
    }
}

void close_card_fd(fds_t *fds)
{
    if (fds->fd_dvr)
        CloseHandle(fds->fd_dvr);
    fds->fd_dvr = 0;
}

/* unused */
void *read_card_thread_func(void *arg)
{

    (void)arg;

    return 0;
}

int card_read(HANDLE fd_dvr, unsigned char *dest_buffer, card_buffer_t *card_buffer)
{
    DWORD bread = 0;

    if (!ReadFile(fd_dvr, dest_buffer, TS_PACKET_SIZE * card_buffer->dvr_buffer_size, &bread, NULL)) {
        log_message(log_module, MSG_ERROR, "Error reading tuner pipe, aborting (%s)\n", strerror(GetLastError()));
        return -1;
    }

    return bread;
}

/* unused */
void list_dvb_cards(void)
{

}
