#ifndef _HLS_H
#define _HLS_H

#define LEN_MAX	64

typedef struct hls_open_fds {
    int service_id;
    int has_traffic;
    unsigned int initialized;
    unsigned int access_time;
    unsigned int rotate_time;
    unsigned int sequence;	// HLS media sequence
    char name[LEN_MAX];		// channel name for master playlist
    char path[LEN_MAX];		// path for all files
    char name_playlist[LEN_MAX];	// playlist filename
    char name_stream[LEN_MAX];	// file writing right now
    char name_newest[LEN_MAX];
    char name_oldest[LEN_MAX];
    char name_delete[LEN_MAX];	// filename to delete
    FILE* stream;
} hls_open_fds_t;

void hls_data_send(mumudvb_channel_t *actual_channel, struct unicast_parameters_t *unicast_vars, uint64_t now_time);
void hls_prepare_files();

#endif
