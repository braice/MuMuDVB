/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Code for transcoding
 * 
 * Code written by Utelisys Communications B.V.
 * Copyright (C) 2009 Utelisys Communications B.V.
 *
 * The latest version can be found at http://mumudvb.braice.net
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
#ifndef _TRANSCODE_COMMON_H
#define _TRANSCODE_COMMON_H

#define TRANSCODE_CODEC_MAX 32
#define TRANSCODE_RC_EQ_MAX 64
#define TRANSCODE_PROFILE_MAX 32
#define TRANSCODE_STREAMING_TYPE_MAX 32
#define TRANSCODE_SDP_FILENAME_MAX 1024
#define TRANSCODE_AAC_PROFILE_MAX 32
#define TRANSCODE_FFM_URL_MAX 1024

#define STREAMING_TYPE_MPEGTS 1
#define STREAMING_TYPE_RTP 2
#define STREAMING_TYPE_FFM 3

#include "transcode_queues.h"

#include <pthread.h>

typedef struct transcode_options_t
{
    int *enable;
    int *video_bitrate;
    int *audio_bitrate;
    int *gop;
    int *b_frames;
    int *mbd;
    int *cmp;
    int *subcmp;
    char *video_codec;
    char *audio_codec;
    float *crf;
    int *refs;
    int *b_strategy;
    int *coder_type;
    int *me_method;
    int *me_range;
    int *subq;
    int *trellis;
    int *sc_threshold;
    char *rc_eq;
    float *qcomp;
    int *qmin;
    int *qmax;
    int *qdiff;
    int *loop_filter;
    int *mixed_refs;
    int *enable_8x8dct;
    int *x264_partitions;
    int *level;
    int *streaming_type;
    char *sdp_filename;
    int *aac_profile;
    int *aac_latm;
    float *video_scale;
    char *ffm_url;
    int *audio_channels;
    int *audio_sample_rate;
    int *video_frames_per_second;
    int *rtp_port;
    int *keyint_min;
    char ip[20]; //for rtp streaming
} transcode_options_t;

typedef struct transcode_thread_data_t
{
    /* Threading data */
    pthread_t thread;
    pthread_mutex_t queue_mutex;
    int terminate_thread_flag;

    /* Other data */
    int is_initialized;

    /* Comunications data */
    int socket;
    struct sockaddr_in *socket_addr;
    
    /* Transcode data queue */
    struct data_queue_t data_queue;

    /* Transcode parameters */
    struct transcode_options_t *options;
} transcode_thread_data_t;

void free_transcode_options(transcode_options_t *transcode_options);

#endif