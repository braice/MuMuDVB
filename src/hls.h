#ifndef _HLS_H
#define _HLS_H

#include "dvb.h"	// for strength_parameters_t

#define LEN_MAX	64

typedef struct hls_file {
    char name[LEN_MAX];
} hls_file_t;

typedef struct hls_open_fds {
    int service_id;
    int has_traffic;
    unsigned int initialized;
    unsigned int access_time;
    unsigned int rotate_time;
    unsigned int sequence;	// HLS media sequence
    unsigned int need_rotate;
    char name[LEN_MAX];		// channel name for master playlist
    char path[LEN_MAX];		// path for all files
    char name_playlist[LEN_MAX];	// playlist filename
    hls_file_t *filenames;	// pointer to array of filenames: stream, newest ... oldest, delete
    unsigned int filenames_num;
    FILE* stream;
} hls_open_fds_t;

typedef struct hls_thread_params {
    volatile int threadshutdown;
    unicast_parameters_t *unicast_vars;
    strength_parameters_t *strengthparams;
} hls_thread_parameters_t;

void hls_data_send(mumudvb_channel_t *actual_channel, struct unicast_parameters_t *unicast_vars, uint64_t now_time);
int hls_start(unicast_parameters_t *unicast_vars);
void hls_stop(unicast_parameters_t *unicast_vars);
void *hls_periodic_task(void* arg);

#endif
