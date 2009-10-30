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
#include "transcode_avlib.h"
#include "transcode_queues.h"
#include "network.h"
#include "mumudvb.h"
#include "log.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <sys/time.h>
#include <libswscale/swscale.h>

#define TRANSCODE_BUF_SIZE (5 * 1024 * 1024)
#define TRANSCODE_INPUT_BUFFER_SIZE (5 * 1024 * 1024)
#define TRANSCODE_OUTPUT_BUFFER_SIZE (TS_PACKET_SIZE * 7)
#define PACKETS_QUEUE_LENGTH 500
#define DEFAULT_RTP_PORT 6643

#if LIBAVFORMAT_VERSION_INT >= ((52<<16)+(21<<8)+0)
    #define STREAM_SAMPLE_ASPECT_RATIO
#endif

#if LIBAVFORMAT_VERSION_INT >= ((52<<16)+(26<<8)+0)
    #define av_alloc_format_context avformat_alloc_context
#endif

#define AAC_LATM_CODEC_ID 0x20032 /* Some random ID */

#define SET_CODEC_OPTION_INT(codec_option, config_option) \
if (NULL != options->config_option) {\
    /*printf(#config_option"=%d\n", out_stream->codec->codec_option);*/\
    out_stream->codec->codec_option = *options->config_option;\
}

#define SET_CODEC_OPTION_FLT(codec_option, config_option) \
if (NULL != options->config_option) {\
    /*printf(#config_option"=%f\n", out_stream->codec->codec_option);*/\
    out_stream->codec->codec_option = *options->config_option;\
}

/* FIXME: don't know another way to include this ffmpeg function */
void url_split(char *proto, int proto_size,
           char *authorization, int authorization_size,
           char *hostname, int hostname_size,
           int *port_ptr,
           char *path, int path_size,
           const char *url);

typedef struct output_stream_transcode_data_t
{
    /* Output stream and format context */
    AVStream *out_stream;
    AVFormatContext *out_context;

    /* Stream's frame counters */
    int64_t frame_no;
    int64_t written_frames;

    /* Video stream transcoding data */
    struct SwsContext *sws_context;
    AVFrame temp_picture;

    /* Other transcoding data */
    double frame_rate_factor;

    /* Audio stream transcoding data */
    int frame_buf_data_length;
    void *frame_buf;
    int frame_size;
    int sample_size;
    struct ReSampleContext *resample_context;

    /* Packet queue */
    ref_queue_t packets;
} output_stream_transcode_data_t;

typedef struct input_stream_transcode_data_t
{
    /* Stream start PTS */
    int64_t stream_start_pts;

    /* Factor for synchronization */
    int pts_duration_factor;

    int sample_size;

    output_stream_transcode_data_t *output_streams_transcode_data;
    int output_streams_count;

//    int x;
} input_stream_transcode_data_t;

typedef struct transcode_data_t
{
    AVFormatContext *in_context;
    ref_queue_t out_contexts;

    input_stream_transcode_data_t *input_streams_transcode_data;
    int input_streams_count;

    struct timeval start_time;
} transcode_data_t;

typedef struct input_context_data_t
{
    pthread_mutex_t *queue_mutex;
    data_queue_t *queue;
    int *terminate_thread_flag;
    int no_data_counter;
    int data_counter;
} input_context_data_t;

typedef struct output_context_data_t
{
    int socket;
    struct sockaddr_in *socket_addr;
} output_context_data_t;

pthread_mutex_t avlib_mutex = PTHREAD_MUTEX_INITIALIZER;

AVCodec aac_latm_encoder;
AVCodec *aac_encoder = NULL;

int aac_latm_encode(AVCodecContext *c, uint8_t *buf, int buf_size, void *data)
{
    /* Encode data */
    int out_size = aac_encoder->encode(c, buf, buf_size, data);

    if (0 > out_size) {
        return out_size; /* Error while encoding */
    }

    /* Skip ADTS header (7 bytes), if present - approach from libavformat */
    int adts_size = ((0 == c->extradata_size && 0 != out_size) ? 7 : 0);
    out_size -= adts_size;

    /* Calculate LATM header size */
    int latm_header_size = out_size / 0xff + 1;

    if (out_size + latm_header_size > buf_size) {
        return -1; /* Buffer is too small */
    }

    /* Move data to insert LATM header */
    memmove(buf + latm_header_size, buf + adts_size, out_size);

    /* Create LATM header */

    int cur_size = out_size;
    uint8_t *p_header = buf;

    while (cur_size > 0xfe) {
        *p_header = 0xff;
        p_header++;
        cur_size -= 0xff;
    }

    *p_header = cur_size;

    /* Return size of output data */
    return out_size + latm_header_size;
}

AVCodec* get_latm_encoder()
{
    if (NULL == aac_encoder) { /* Encoder is not initialized */
        /* Find AAC encoder */
        aac_encoder = avcodec_find_encoder(CODEC_ID_AAC);

        if (NULL == aac_encoder) {
            return NULL; /* Failed to find AAC encoder */
        }

        /* AAC LATM is almost copy of built in AAC encoder */
        aac_latm_encoder = *aac_encoder;

        /* Change CODEC_ID_AAC to something else so it will be not defined as AAC encoder */
        /* RTP muxer will not enable AAC specific routines for such codec */
        aac_latm_encoder.id = AAC_LATM_CODEC_ID;

        /* Override encode function */
        aac_latm_encoder.encode = (NULL == aac_encoder->encode ? NULL : aac_latm_encode);
    }

    return &aac_latm_encoder;
}

/* Copy of ffmpeg function */
char *ff_data_to_hex(char *buff, const uint8_t *src, int s)
{
    int i;
    static const char hex_table[16] = { '0', '1', '2', '3',
                                        '4', '5', '6', '7',
                                        '8', '9', 'A', 'B',
                                        'C', 'D', 'E', 'F' };

    for (i = 0; i < s; i++) {
        buff[i * 2]     = hex_table[src[i] >> 4];
        buff[i * 2 + 1] = hex_table[src[i] & 0xF];
    }

    return buff;
}

int create_sdp(AVFormatContext *ac[], int n_files, char *buff, int size)
{
    /* Create SDP 'header' using default ffmpeg routine */

    if (0 != avf_sdp_create(ac, n_files, buff, size)) {
        return -1;
    }

    /* Define SDP 'header' */

    char *p = strstr(buff, "m=");

    if (NULL == p) {
        return -1;
    }

    int buff_len = p - buff;

    /* Copy "c=..." line to output */

    if (NULL == (p = strstr(buff, "c="))) {
        return -1;
    }
    else {
         char *p2 = strstr(p, "\r\n");
         
         if (NULL == p2) {
              return -1;
         }

         if (buff_len + (p2 - p) + 2 > size) {
              return -1;
         }

         strncpy(buff + buff_len, p, p2 - p + 2);
         buff_len += (p2 - p) + 2;
    }

    /* Create SDP lines for each stream */

    char *buff2 = (char*)malloc(size);
    int i;

    for (i = 0; i < n_files; i++) {
        int len;
        AVCodecContext *codec = ac[i]->streams[0]->codec;

        switch (codec->codec_id) {

            case AAC_LATM_CODEC_ID: /* Create SDP lines using specific AAC LATM config */
            {
                /* Create hex config */

                int aacsrates[15] = {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                    16000, 12000, 11025, 8000, 7350, 0, 0 };

                int j;
                for (j = 0; j < 15; j++) {
                    if (codec->sample_rate == aacsrates[j]) {
                        break;
                    }
                }

                uint8_t config[6] = { 0x40, 0, 0x20 | j, codec->channels << 4, 0x3f, 0xc0 };

                char config_hexa[13];

                ff_data_to_hex(config_hexa, config, 6);
                config_hexa[12] = 0;

                /* Get port */

                int port;
                url_split(NULL, 0, NULL, 0, NULL, 0, &port, NULL, 0, ac[i]->filename);

                /* Generate SDP lines for MP4A-LATM */

                len = snprintf(buff2, size,
                    "m=audio %d RTP/AVP 96\r\n"
                    "b=AS:%d\r\n"
                    "a=rtpmap:96 MP4A-LATM/%d/%d\r\n"
                    "a=fmtp:96 nobject=2; cpresent=0; config=%s\r\n",
                    port, codec->bit_rate / 1000, codec->sample_rate, codec->channels, config_hexa);

                if (len + buff_len >= size) {
                    break; /* Buffer is too small */
                }

                /* Copy stream lines to output */
                strncpy(buff + buff_len, buff2, len);
                buff_len += len;

                break;
            }

            default: /* Create SDP lines using default ffmpeg routine */
                if (0 == avf_sdp_create(ac + i, 1, buff2, size) &&
                    NULL != (p = strstr(buff2, "m="))) {

                    len = strlen(p);

                    if (len != 0 && len + buff_len < size) {
                        /* Copy stream lines to output */
                        strncpy(buff + buff_len, p, len);
                        buff_len += len;
                    }
                }
                break;
        }
    }

    free(buff2);

    /* End SDP creation */

    buff[buff_len] = 0;

    return 0;
}

int read_input_packet(void *opaque, uint8_t *buf, int buf_size)
{
    input_context_data_t *context_data = opaque;

    context_data->no_data_counter = 0;

    while (0 == *context_data->terminate_thread_flag) { /* Terminate if it requested */

        /* Lock mutext and dequeue data from queue */
        pthread_mutex_lock(context_data->queue_mutex);
    
        int dequeued_size = data_queue_dequeue(context_data->queue, buf, buf_size);

        pthread_mutex_unlock(context_data->queue_mutex);

        if (0 < dequeued_size) {
            context_data->no_data_counter = 0;
            context_data->data_counter += dequeued_size;
            return dequeued_size; /* We have data */
        }
        else {
            if (6 == context_data->no_data_counter) { /* 3 Sec */
                log_message(MSG_INFO, "[Transcode] No data for transcoding.\n");
                return -1;
            }

            context_data->no_data_counter++;
            usleep(500000); /* No data yet. Sleep for 0.5 sec */
        }
    }

    return -1;
}

int write_output_packet(void *opaque, uint8_t *buf, int buf_size)
{
    /*log_message(MSG_INFO, "[Transcode] Sending data %d.\n", buf_size);*/
    output_context_data_t *context_data = opaque;
    sendudp(context_data->socket, context_data->socket_addr, buf, buf_size);
    return buf_size;
}

ByteIOContext* create_input_byte_context(data_queue_t *queue, pthread_mutex_t *queue_mutex,
    int *terminate_thread_flag, int input_buffer_size)
{
    /* Allocate input buffer */

    unsigned char *input_buffer = (unsigned char*)av_malloc(input_buffer_size);

    if (NULL == input_buffer) {
        log_message(MSG_ERROR, "[Transcode] Couldn't allocate input buffer.\n");
        return NULL;
    }

    /* Creating input context data */

    input_context_data_t *context_data = malloc(sizeof(input_context_data_t));

    if (NULL == context_data) {
        log_message(MSG_ERROR, "[Transcode] Couldn't allocate input context data.\n");
        av_free(input_buffer);
        return NULL;
    }
    
    context_data->queue = queue;
    context_data->queue_mutex = queue_mutex;
    context_data->terminate_thread_flag = terminate_thread_flag;
    context_data->no_data_counter = 0;
    context_data->data_counter = 0;
    
    /* Creating ByteIOContext using input_buffer */

    ByteIOContext *input_io = av_alloc_put_byte(input_buffer, input_buffer_size, 0, context_data,
        read_input_packet,
        NULL,
        NULL);
    
    if (NULL == input_io) {
        log_message(MSG_ERROR, "[Transcode] Couldn't create input IO context.\n");
        free(context_data);
        av_free(input_buffer);
        return NULL;
    }

    input_io->is_streamed = 1; /* Disallow seek operations */

    return input_io;
}

ByteIOContext* create_output_byte_context(int socket, struct sockaddr_in *socket_addr, int output_buffer_size)
{
    /* Allocate output buffer */

    unsigned char *output_buffer = (unsigned char*)av_malloc(output_buffer_size);

    if (NULL == output_buffer) {
        log_message(MSG_ERROR, "[Transcode] Couldn't allocate output buffer.\n");
        return NULL;
    }

    /* Creating output context data */

    output_context_data_t *context_data = malloc(sizeof(output_context_data_t));

    if (NULL == context_data) {
        log_message(MSG_ERROR, "[Transcode] Couldn't allocate output context data.\n");
        av_free(output_buffer);
        return NULL;
    }
    
    context_data->socket = socket;
    context_data->socket_addr = socket_addr;
    
    /* Creating ByteIOContext using input_buffer */

    ByteIOContext *output_io = av_alloc_put_byte(output_buffer, output_buffer_size, 1, context_data,
        NULL,
        write_output_packet,
        NULL);
    
    if (NULL == output_io) {
        log_message(MSG_ERROR, "[Transcode] Couldn't create output IO context.\n");
        free(context_data);
        av_free(output_buffer);
        return NULL;
    }

    output_io->is_streamed = 1; /* Disallow seek operations */
    output_io->max_packet_size = output_buffer_size; /* TODO: am I right? */

    return output_io;
}

void free_byte_context(ByteIOContext *input_output_context)
{
    av_free(input_output_context->buffer);
    free(input_output_context->opaque);
    av_free(input_output_context);
}

void free_format_context(AVFormatContext *context, int input, int streaming_type)
{
    if (input || STREAMING_TYPE_MPEGTS == streaming_type) {
        free_byte_context(context->pb);
    }
    else if (NULL != context->pb &&
            (STREAMING_TYPE_RTP == streaming_type || STREAMING_TYPE_FFM == streaming_type)) {
        url_fclose(context->pb);
    }

    unsigned int i;

    /* Clear input/output codecs */

    pthread_mutex_lock(&avlib_mutex);

    for (i = 0; i < context->nb_streams; i++) {
        if (NULL != context->streams[i]->codec->codec) {
            avcodec_close(context->streams[i]->codec);
        }
    }

    pthread_mutex_unlock(&avlib_mutex);
    
    if (input) {        
        av_close_input_stream(context);
    }
    else {
        /* Free output streams */

        for (i = 0; i < context->nb_streams; i++) {
            av_freep(&context->streams[i]->codec);
            av_freep(&context->streams[i]->priv_data);
            av_freep(&context->streams[i]);
        }
        
        av_free(context->priv_data);
        av_free(context);
    }
}

AVFormatContext *create_input_format_context(data_queue_t *queue, pthread_mutex_t *queue_mutex,
    int *terminate_thread_flag, int input_buffer_size)
{
    ByteIOContext *input_io = create_input_byte_context(queue, queue_mutex,
        terminate_thread_flag, input_buffer_size);

    if (NULL == input_io) {
        return NULL;
    }

    /* Setting input format as MPEG-2 TS */

    AVInputFormat *input_format = av_find_input_format("mpegts");

    if (NULL == input_format) {
        log_message(MSG_ERROR, "[Transcode] Couldn't find input format.\n");
        free_byte_context(input_io);
        return NULL;
    }

    /* Creating input context */

    AVFormatContext *in_context;

    if (0 != av_open_input_stream(&in_context, input_io, "stream", input_format, NULL)) {
        log_message(MSG_ERROR, "[Transcode] Couldn't open input stream.\n");
        free_byte_context(input_io);
        return NULL;
    }

    /* Retrieve stream information */
    
    pthread_mutex_lock(&avlib_mutex); /*avcodec_open\close require thread safety */
    
    if (0 > av_find_stream_info(in_context)) {
        log_message(MSG_ERROR, "[Transcode] Couldn't find stream information.\n");
        pthread_mutex_unlock(&avlib_mutex);
        free_format_context(in_context, 1, 0);
        return NULL;
    }
    
    pthread_mutex_unlock(&avlib_mutex);

    return in_context;
}

AVFormatContext *create_output_format_context(int socket, struct sockaddr_in *socket_addr,
    int output_buffer_size, char *url, int streaming_type, int open_byte_context)
{
    /* Select output format */

    AVOutputFormat *fmt = NULL;

    switch (streaming_type) {
        case STREAMING_TYPE_MPEGTS:
            fmt = guess_format("mpegts", NULL, NULL);
            break;

        case STREAMING_TYPE_RTP:
            fmt = guess_format("rtp", NULL, NULL);
            break;

        case STREAMING_TYPE_FFM:
            fmt = guess_format("ffm", NULL, NULL);
            break;
    }

    if (NULL == fmt) {
        log_message(MSG_ERROR, "[Transcode] Couldn't define output format.\n");
        return NULL;
    }

    /* Create output context */

    AVFormatContext *out_context = av_alloc_format_context();

    if (NULL == out_context) {
        log_message(MSG_ERROR, "[Transcode] Couldn't create output format context.\n");
        return NULL;
    }

    out_context->pb = NULL;

    if (NULL != url) {
        if (open_byte_context) {
            url_fopen(&out_context->pb, url, URL_WRONLY);
        }
        strcpy(out_context->filename, url);
    }
    else {
        if (open_byte_context) {
            out_context->pb = create_output_byte_context(socket, socket_addr, output_buffer_size);
        }
    }

    if (open_byte_context && NULL == out_context->pb) {
        log_message(MSG_ERROR, "[Transcode] Couldn't create output format context.\n");
        av_free(out_context);
        return NULL;
    }

    out_context->oformat = fmt;

    return out_context;
}

int initialize_out_video_stream(AVStream *out_stream,  AVStream *in_stream, transcode_options_t *options)
{
    /* Setting codec */

    AVCodec *codec = avcodec_find_encoder_by_name(NULL != options->video_codec ? options->video_codec : "mpeg4");

    if (NULL == codec || CODEC_TYPE_VIDEO != codec->type) {
        log_message(MSG_ERROR, "[Transcode] Couldn't find video encoder.\n");
        return 0;
    }

    avcodec_get_context_defaults2(out_stream->codec, CODEC_TYPE_VIDEO);

    out_stream->codec->codec_type = CODEC_TYPE_VIDEO;
    out_stream->codec->codec_id = codec->id;

    /* Other configurable parameters */

    SET_CODEC_OPTION_INT(gop_size, gop)
    SET_CODEC_OPTION_INT(max_b_frames, b_frames)
    SET_CODEC_OPTION_INT(bit_rate, video_bitrate)
    SET_CODEC_OPTION_INT(me_cmp, cmp)
    SET_CODEC_OPTION_INT(me_sub_cmp, subcmp)
    SET_CODEC_OPTION_INT(mb_decision, mbd)
    SET_CODEC_OPTION_FLT(crf, crf)
    SET_CODEC_OPTION_INT(refs, refs)
    SET_CODEC_OPTION_INT(b_frame_strategy, b_strategy)
    SET_CODEC_OPTION_INT(coder_type, coder_type)
    SET_CODEC_OPTION_INT(me_method, me_method)
    SET_CODEC_OPTION_INT(me_range, me_range)
    SET_CODEC_OPTION_INT(me_subpel_quality, subq)
    SET_CODEC_OPTION_INT(trellis, trellis)
    SET_CODEC_OPTION_INT(scenechange_threshold, sc_threshold)
    SET_CODEC_OPTION_INT(keyint_min, keyint_min)

    if (NULL != options->rc_eq) {
        out_stream->codec->rc_eq = options->rc_eq;
    }

    SET_CODEC_OPTION_FLT(qcompress, qcomp)
    SET_CODEC_OPTION_INT(qmin, qmin)
    SET_CODEC_OPTION_INT(qmax, qmax)
    SET_CODEC_OPTION_INT(max_qdiff, qdiff)

    if (NULL != options->loop_filter && 1 == *options->loop_filter) {
        out_stream->codec->flags |= CODEC_FLAG_LOOP_FILTER;
    }

    if (NULL != options->mixed_refs && 1 == *options->mixed_refs) {
        out_stream->codec->flags2 |= CODEC_FLAG2_MIXED_REFS;
    }

    if (NULL != options->enable_8x8dct && 1 == *options->enable_8x8dct) {
        out_stream->codec->flags2 |= CODEC_FLAG2_8X8DCT;
    }

    if (NULL != options->x264_partitions && 1 == *options->x264_partitions) {
        out_stream->codec->partitions =
            X264_PART_I4X4 |
            X264_PART_I8X8 |
            X264_PART_P8X8 |
            X264_PART_B8X8;
    }

    if (NULL != options->video_scale && 1 != *options->video_scale &&
        *options->video_scale > 0) {

        out_stream->codec->width = (int)((double)in_stream->codec->width *
            *options->video_scale + 0.5);

        out_stream->codec->height = (int)((double)in_stream->codec->height *
            *options->video_scale + 0.5);

        out_stream->codec->width = out_stream->codec->width == 0 ? 1 : out_stream->codec->width;
        out_stream->codec->height = out_stream->codec->width == 0 ? 1 : out_stream->codec->height;
    }
    else {
        out_stream->codec->width = in_stream->codec->width;
        out_stream->codec->height = in_stream->codec->height;
    }

    SET_CODEC_OPTION_INT(level, level)
    
    /* Copy some input stream parameters */

    if (NULL != options->video_frames_per_second && *(options->video_frames_per_second) > 0) {
        out_stream->codec->time_base.num = 1;
        out_stream->codec->time_base.den = *(options->video_frames_per_second);
    }
    else if (25 == in_stream->r_frame_rate.num && 1 == in_stream->r_frame_rate.den) {
        /* We have estimated framerate 25 FPS */
        out_stream->codec->time_base.num = in_stream->r_frame_rate.den;
        out_stream->codec->time_base.den = in_stream->r_frame_rate.num;
    }
    else {
        out_stream->codec->time_base = in_stream->codec->time_base;
    }

    out_stream->codec->pix_fmt = in_stream->codec->pix_fmt; /* TODO: own pixel format??? yuv420p = 0 */

#ifdef STREAM_SAMPLE_ASPECT_RATIO
    if (0 != in_stream->sample_aspect_ratio.num) {
        out_stream->sample_aspect_ratio = in_stream->sample_aspect_ratio;
        out_stream->codec->sample_aspect_ratio = in_stream->sample_aspect_ratio;
    }
    else {
        out_stream->sample_aspect_ratio = in_stream->codec->sample_aspect_ratio;
        out_stream->codec->sample_aspect_ratio = in_stream->codec->sample_aspect_ratio;
    }
#else
    out_stream->codec->sample_aspect_ratio = in_stream->codec->sample_aspect_ratio;
#endif

    /*if (out_context->oformat->flags & AVFMT_GLOBALHEADER) {*/
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    /*}*/

    if (0 > avcodec_open(out_stream->codec, codec)) {
        log_message(MSG_ERROR, "[Transcode] Couldn't open codec for encoding.\n");
        return 0;
    }

    return 1;
}

int initialize_out_audio_stream(AVStream *out_stream,  AVStream *in_stream,
                                AVFormatContext *out_context, transcode_options_t *options)
{
    /* Setting codec */

    AVCodec *codec = avcodec_find_encoder_by_name(
        NULL != options->audio_codec ? options->audio_codec : "libmp3lame");

    if (NULL == codec || codec->type != CODEC_TYPE_AUDIO) {
        log_message(MSG_ERROR, "[Transcode] Couldn't find audio encoder.\n");
        return 0;
    }

    if (CODEC_ID_AAC == codec->id && NULL != options->streaming_type &&
        STREAMING_TYPE_RTP == *options->streaming_type &&
        1 == (options->aac_latm == NULL ? 0 : *options->aac_latm)) {

        codec = get_latm_encoder();
    }

    avcodec_get_context_defaults2(out_stream->codec, CODEC_TYPE_AUDIO);

    out_stream->codec->codec_type = CODEC_TYPE_AUDIO;
    out_stream->codec->codec_id = codec->id;

    /* Other configurable parameters */
    
    SET_CODEC_OPTION_INT(bit_rate, audio_bitrate)
    SET_CODEC_OPTION_INT(profile, aac_profile)

    /* Copy some input stream parameters */

    out_stream->time_base = in_stream->time_base;
    
    if (NULL != options->audio_channels && *(options->audio_channels) > 0) {
        out_stream->codec->channels = *(options->audio_channels);
    }
    else {
        out_stream->codec->channels = in_stream->codec->channels;
    }

    if (NULL != options->audio_sample_rate && *(options->audio_sample_rate) > 0) {
        out_stream->codec->sample_rate = *(options->audio_sample_rate);
    }
    else {
        out_stream->codec->sample_rate = in_stream->codec->sample_rate;
    }

    out_stream->codec->sample_fmt = in_stream->codec->sample_fmt;

    out_context = out_context; /* He-he. Just hiding warning. This line can be safely removed. */

    /* FIXME: is it good for audio stream to set always CODEC_FLAG_GLOBAL_HEADER?
       RTP output format is not CODEC_FLAG_GLOBAL_HEADER but AAC codec requires it. Strange. */
    /*if (out_context->oformat->flags & AVFMT_GLOBALHEADER) { */
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    /*}*/

    //out_stream->pts.v-al = 100;

    if (0 > avcodec_open(out_stream->codec, codec)) {
        log_message(MSG_ERROR, "[Transcode] Couldn't open codec for encoding.\n");
        return 0;
    }

    return 1;
}

void initialize_transcode_common_free(AVCodecContext *in_stream_codec, int is_avlib_mutex_locked,
    AVFormatContext *out_context, int *streaming_type,
    struct SwsContext *sws_context, AVFrame *temp_picture,
    struct ReSampleContext *resample_context, void *frame_buf)
{
    if (!is_avlib_mutex_locked) {
        pthread_mutex_lock(&avlib_mutex);
    }

    if (NULL != in_stream_codec) {
        avcodec_close(in_stream_codec);
    }

    pthread_mutex_unlock(&avlib_mutex);
    
    if (NULL != out_context && NULL != streaming_type &&
        STREAMING_TYPE_RTP == *streaming_type) {

        free_format_context(out_context, 0, *streaming_type);
    }

    if (NULL != sws_context) {
        sws_freeContext(sws_context);
    }

    if (NULL != temp_picture) {
        avpicture_free((AVPicture*)temp_picture);
    }

    if (NULL != resample_context) {
        audio_resample_close(resample_context);
    }

    if (NULL != frame_buf) {
        free(frame_buf);
    }
}

transcode_data_t* initialize_transcode_data(transcode_thread_data_t *transcode_thread_data)
{
    /* Create input format context */

    AVFormatContext *in_context = NULL;

    log_message(MSG_INFO, "[Transcode] Initializing transcoding.\n");

    in_context = create_input_format_context(&transcode_thread_data->data_queue,
        &transcode_thread_data->queue_mutex, &transcode_thread_data->terminate_thread_flag,
        TRANSCODE_INPUT_BUFFER_SIZE);

    if (NULL == in_context) { /* CAM not initialized or wrong stream */
        log_message(MSG_ERROR, "[Transcode] Failed to initialize input format context.\n");
        return NULL;
    }

    /* Input format initialized */
    dump_format(in_context, 0, "stream", 0);

    transcode_data_t *transcode_data = malloc(sizeof(transcode_data_t));

    if (NULL == transcode_data) {
        log_message(MSG_ERROR, "[Transcode] Failed to initialize transcode data.\n");

        free_format_context(in_context, 1, 0);

        return NULL;
    }

    transcode_data->in_context = in_context;
    transcode_data->start_time.tv_sec = transcode_data->start_time.tv_usec = 0;
    ref_queue_init(&transcode_data->out_contexts, 0);
    
    transcode_data->input_streams_transcode_data =
        calloc(in_context->nb_streams, sizeof(input_stream_transcode_data_t));
    transcode_data->input_streams_count = in_context->nb_streams;

    if (NULL == transcode_data->input_streams_transcode_data) {
        log_message(MSG_ERROR, "[Transcode] Failed to initialize transcode data.\n");

        free_format_context(in_context, 1, 0);
        free(transcode_data);

        return NULL;
    }

    return transcode_data;
}

int initialize_video_parameters(input_stream_transcode_data_t *input_stream_transcode_data,
    output_stream_transcode_data_t *output_stream_transcode_data,
    AVStream *in_stream, AVStream *out_stream)
{
    input_stream_transcode_data->pts_duration_factor =
        (int)((double)in_stream->time_base.den * in_stream->codec->time_base.num /
        in_stream->codec->time_base.den / in_stream->time_base.num + 0.5);

    output_stream_transcode_data->frame_rate_factor =
        (double)out_stream->codec->time_base.num * in_stream->codec->time_base.den /
        out_stream->codec->time_base.den / in_stream->codec->time_base.num;

    /* Initialize image scaling */
    if (in_stream->codec->width != out_stream->codec->width ||
        in_stream->codec->height != out_stream->codec->height) {
        
        output_stream_transcode_data->sws_context =
            sws_getContext(
                in_stream->codec->width, in_stream->codec->height, 
                in_stream->codec->pix_fmt,
                out_stream->codec->width, out_stream->codec->height, 
                out_stream->codec->pix_fmt,
                SWS_BICUBIC, NULL, NULL, NULL);

        if (NULL == output_stream_transcode_data->sws_context) {
            log_message(MSG_ERROR, "[Transcode] Image scaling initialization failed.\n");
            return 0;
        }

        avcodec_get_frame_defaults(&output_stream_transcode_data->temp_picture);

        if (avpicture_alloc((AVPicture*)&output_stream_transcode_data->temp_picture,
            out_stream->codec->pix_fmt, out_stream->codec->width, out_stream->codec->height) != 0) {

            log_message(MSG_ERROR, "[Transcode] Image scaling initialization failed.\n");
            sws_freeContext(output_stream_transcode_data->sws_context);
            return 0;
        }
    }

    return 1;
}

int initialize_audio_parameters(input_stream_transcode_data_t *input_stream_transcode_data, 
    output_stream_transcode_data_t *output_stream_transcode_data,
    AVStream *in_stream, AVStream *out_stream) 
{
    if (out_stream->codec->frame_size < 1) {
        log_message(MSG_ERROR, "[Transcode] Initialize audio frame buffer failed: frame size < 1.\n");
        return 0;
    }

    /* Initialize audio frame buffer */

    input_stream_transcode_data->sample_size =
        av_get_bits_per_sample_format(in_stream->codec->sample_fmt) *
        in_stream->codec->channels / 8;

    output_stream_transcode_data->sample_size =
        av_get_bits_per_sample_format(out_stream->codec->sample_fmt) *
        out_stream->codec->channels / 8;

    output_stream_transcode_data->frame_size =
        output_stream_transcode_data->sample_size * out_stream->codec->frame_size;

    output_stream_transcode_data->frame_buf =
        malloc(output_stream_transcode_data->frame_size);

    if (NULL == output_stream_transcode_data->frame_buf) {
        log_message(MSG_ERROR, "[Transcode] Initialize audio frame buffer failed.\n");
        return 0;
    }

    output_stream_transcode_data->frame_buf_data_length = 0;

    input_stream_transcode_data->pts_duration_factor =
        (int)((double)in_stream->time_base.den * in_stream->codec->frame_size /
        in_stream->time_base.num / in_stream->codec->sample_rate + 0.5);

    output_stream_transcode_data->frame_rate_factor =
        (double)out_stream->codec->frame_size * in_stream->codec->sample_rate /
        in_stream->codec->frame_size / out_stream->codec->sample_rate;

    /* Initialize audio resampling */

    if (in_stream->codec->sample_rate != out_stream->codec->sample_rate ||
        in_stream->codec->channels    != out_stream->codec->channels ||
        in_stream->codec->sample_fmt  != out_stream->codec->sample_fmt) {

        output_stream_transcode_data->resample_context =
            av_audio_resample_init(
                out_stream->codec->channels,    in_stream->codec->channels,
                out_stream->codec->sample_rate, in_stream->codec->sample_rate,
                out_stream->codec->sample_fmt,  in_stream->codec->sample_fmt,
                16, 10, 0, 0.8);

        if (NULL == output_stream_transcode_data->resample_context) {
            log_message(MSG_ERROR, "[Transcode] Can not initialize audio resampling.\n");
            free(output_stream_transcode_data->frame_buf);
            return 0;
        }
    }

    return 1;
}

void* initialize_transcode_common(transcode_thread_data_t *transcode_thread_data)
{
    /* Initialize transcode data and open input context */
    transcode_data_t *transcode_data = initialize_transcode_data(transcode_thread_data);

    if (NULL == transcode_data) {
        return NULL;
    }

    AVFormatContext *in_context = transcode_data->in_context;

    AVFormatContext **out_contexts = calloc(in_context->nb_streams, sizeof(AVFormatContext*));

    if (NULL == out_contexts) {
        log_message(MSG_ERROR, "[Transcode] Failed to initialize output contexts buffer.\n");

        free_format_context(in_context, 1, 0);
        free(transcode_data->input_streams_transcode_data);
        free(transcode_data);

        return NULL;
    }

    AVFormatContext *out_context = NULL;

    if (NULL == transcode_thread_data->options->streaming_type ||
            STREAMING_TYPE_MPEGTS == *transcode_thread_data->options->streaming_type) {
        
        /* Need single output format context for all streams */

        out_context = create_output_format_context(transcode_thread_data->socket,
            transcode_thread_data->socket_addr, TRANSCODE_OUTPUT_BUFFER_SIZE, NULL,
            NULL == transcode_thread_data->options->streaming_type ?
                STREAMING_TYPE_MPEGTS : *transcode_thread_data->options->streaming_type, 1);

        if (NULL == out_context) {
            log_message(MSG_ERROR, "[Transcode] Failed to create output context.\n");
            
            free_format_context(in_context, 1, 0);
            free(transcode_data->input_streams_transcode_data);
            free(transcode_data);
            free(out_contexts);

            return NULL;
        }

        ref_queue_enqueue(&transcode_data->out_contexts, out_context);
    }
 
    int output_streams_counter = 0;
    int output_contexts_counter = 0;
    unsigned int i;

    /* Loop through all input streams and duplicate
    video/audio/subtitles in output streams */

    int64_t smallest_start_time = 0;
    int rtp_port = NULL == transcode_thread_data->options->rtp_port ? DEFAULT_RTP_PORT : *transcode_thread_data->options->rtp_port;
    rtp_port = (rtp_port + 1) / 2 * 2; /* get even port */
 
    for (i = 0; i < in_context->nb_streams; i++) {

        AVStream *in_stream = transcode_data->in_context->streams[i];

        output_stream_transcode_data_t output_stream_transcode_data;
         
        output_stream_transcode_data.out_stream = NULL;
        output_stream_transcode_data.frame_no = 0;
        output_stream_transcode_data.written_frames = -1;
        output_stream_transcode_data.sws_context = NULL;
        output_stream_transcode_data.resample_context = NULL;
        output_stream_transcode_data.frame_buf = NULL;
        ref_queue_init(&output_stream_transcode_data.packets, PACKETS_QUEUE_LENGTH);

        transcode_data->input_streams_transcode_data[i].output_streams_transcode_data = NULL;
        transcode_data->input_streams_transcode_data[i].output_streams_count = 0;
//        transcode_data->input_streams_transcode_data[i].x = 0;
 
        if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type ||
            CODEC_TYPE_AUDIO == in_stream->codec->codec_type /*||
            CODEC_TYPE_SUBTITLE == in_stream->codec->codec_type*/) {

            /* Initialize decoder for input stream */
                
            AVCodec *codec = avcodec_find_decoder(in_stream->codec->codec_id);

            if (NULL == codec) {
                log_message(MSG_ERROR, "[Transcode] Couldn't find codec for decoding.\n");
                continue;
            }

            pthread_mutex_lock(&avlib_mutex);

            if (0 > avcodec_open(in_stream->codec, codec)) {
                log_message(MSG_ERROR, "[Transcode] Couldn't open codec for decoding.\n");
                pthread_mutex_unlock(&avlib_mutex);
                continue;
            }

            /* WORKAROUND: specific bug - empty subtitle stream detected as audio one.
                Skip it. */
            if (0 == in_stream->codec->sample_rate &&
                CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {
                
                initialize_transcode_common_free(in_stream->codec, 1, NULL, NULL,
                    NULL, NULL, NULL, NULL);
                
                /* Fix memory leak in av_read_paket when reading this stream */
                in_stream->need_parsing = AVSTREAM_PARSE_NONE;
                
                continue;
            }

            if (NULL != transcode_thread_data->options->streaming_type &&
                STREAMING_TYPE_RTP == *transcode_thread_data->options->streaming_type) {

                /* Creating separate output format context for each stream */

                out_context = NULL;
                
                /* URL for RTP sreaming */
                char url[30];
                sprintf(url, "rtp://%s:%d", transcode_thread_data->options->ip, rtp_port);

                out_context = create_output_format_context(transcode_thread_data->socket,
                    transcode_thread_data->socket_addr, TRANSCODE_OUTPUT_BUFFER_SIZE,
                    url, *transcode_thread_data->options->streaming_type, 1);

                if (NULL == out_context) {
                    initialize_transcode_common_free(in_stream->codec, 1, NULL, NULL,
                        NULL, NULL, NULL, NULL);
                    continue;
                }
            }

            /* Create output stream - analog of input stream, but with another codec */

            AVStream *out_stream = av_new_stream(out_context, i);

            if (!out_stream) {

                log_message(MSG_ERROR, "[Transcode] Couldn't create new output stream.\n");

                initialize_transcode_common_free(in_stream->codec, 1,
                    out_context, transcode_thread_data->options->streaming_type,
                    NULL, NULL, NULL, NULL);

                continue;
            }

            /* Initialize encoder for output stream */ 

            if ((CODEC_TYPE_VIDEO == in_stream->codec->codec_type &&
                    !initialize_out_video_stream(out_stream, in_stream,
                        transcode_thread_data->options)) ||
                (CODEC_TYPE_AUDIO == in_stream->codec->codec_type &&
                    !initialize_out_audio_stream(out_stream, in_stream, out_context,
                        transcode_thread_data->options))) {

                initialize_transcode_common_free(in_stream->codec, 1,
                    out_context, transcode_thread_data->options->streaming_type,
                    NULL, NULL, NULL, NULL);

                continue;
            }

            pthread_mutex_unlock(&avlib_mutex);

            /* Add stream info to transcoding data */
                
            /* Calculating start time for all streams */
            if (in_stream->start_time < smallest_start_time || 0 == smallest_start_time) {
                smallest_start_time = in_stream->start_time;
            }

            /* Calculating various options */

            if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type) {
                if (!initialize_video_parameters(transcode_data->input_streams_transcode_data + i, 
                    &output_stream_transcode_data, in_stream, out_stream)) {

                    initialize_transcode_common_free(in_stream->codec, 0,
                        out_context, transcode_thread_data->options->streaming_type,
                        NULL, NULL, NULL, NULL);

                    continue;
                }
            }
            else if (CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {
                if (!initialize_audio_parameters(transcode_data->input_streams_transcode_data + i, 
                    &output_stream_transcode_data, in_stream, out_stream)) {

                    initialize_transcode_common_free(in_stream->codec, 0,
                        out_context, transcode_thread_data->options->streaming_type,
                        NULL, NULL, NULL, NULL);

                    continue;
                }
            }
            else if (CODEC_TYPE_SUBTITLE == in_stream->codec->codec_type) {
                /* Add subtitles support here */
            }

            if (NULL != transcode_thread_data->options->streaming_type &&
                STREAMING_TYPE_RTP == *transcode_thread_data->options->streaming_type) {

                /* Set output context parameters */

                if (0 > av_set_parameters(out_context, NULL)) {

                    log_message(MSG_ERROR, "[Transcode] Couldn't set output context parameters.\n");

                    initialize_transcode_common_free(in_stream->codec, 0,
                        out_context, transcode_thread_data->options->streaming_type,
                        output_stream_transcode_data.sws_context,
                        &output_stream_transcode_data.temp_picture,
                        output_stream_transcode_data.resample_context,
                        output_stream_transcode_data.frame_buf);

                    continue;
                }

                /* Write output context header */

                if (0 != av_write_header(out_context)) {
                    
                    log_message(MSG_ERROR, "[Transcode] Writing header failed.\n");

                    initialize_transcode_common_free(in_stream->codec, 0,
                        out_context, transcode_thread_data->options->streaming_type,
                        output_stream_transcode_data.sws_context,
                        &output_stream_transcode_data.temp_picture,
                        output_stream_transcode_data.resample_context,
                        output_stream_transcode_data.frame_buf);

                    continue;
                }

                dump_format(out_context, 0, out_context->filename, 1);

                ref_queue_enqueue(&transcode_data->out_contexts, out_context);

                out_contexts[output_contexts_counter] = out_context;
                output_contexts_counter++;

                rtp_port += 2;
            }

            transcode_data->input_streams_transcode_data[i].output_streams_transcode_data =
                malloc(sizeof(output_stream_transcode_data_t));
         
            if (NULL == transcode_data->input_streams_transcode_data[i].output_streams_transcode_data) {
                log_message(MSG_ERROR, "[Transcode] Failed to allocate memory for output_streams_transcode_data.\n");

                initialize_transcode_common_free(in_stream->codec, 0,
                    out_context, transcode_thread_data->options->streaming_type,
                    output_stream_transcode_data.sws_context,
                    &output_stream_transcode_data.temp_picture,
                    output_stream_transcode_data.resample_context,
                    output_stream_transcode_data.frame_buf);

                continue;
            }

            output_stream_transcode_data.out_context = out_context;
            output_stream_transcode_data.out_stream = out_stream;

            transcode_data->input_streams_transcode_data[i].output_streams_transcode_data[0] = output_stream_transcode_data;
            transcode_data->input_streams_transcode_data[i].output_streams_count = 1;     

            output_streams_counter++;
        }
    }

    if (0 == output_streams_counter) {
        goto INITIALIZE_TRANSCODING_ERROR;
    }

    /* Creating SDP file if needed */

    if (NULL != transcode_thread_data->options->streaming_type &&
        STREAMING_TYPE_RTP == *transcode_thread_data->options->streaming_type &&
        NULL != transcode_thread_data->options->sdp_filename) {

        FILE *sdp_file = fopen(transcode_thread_data->options->sdp_filename, "w+");

        if (NULL != sdp_file) {
            char *buff = malloc(10240);

            if (NULL != buff) {
                /*avf_sdp_create(out_contexts, output_contexts_counter, buff, 10240);*/
                if (0 == create_sdp(out_contexts, output_contexts_counter, buff, 10240)) {
                    fprintf(sdp_file, "%s", buff);
                }

                free(buff);
            }
            
            fclose(sdp_file);
        }
    }
 
    /* Synchronizing start time to latest one */

    for (i = 0; i < in_context->nb_streams; i++) {
        transcode_data->input_streams_transcode_data[i].stream_start_pts = smallest_start_time;
    }

    //transcode_data->input_streams_transcode_data[0].stream_start_pts += 3600 * 100;

    if (NULL == transcode_thread_data->options->streaming_type ||
            STREAMING_TYPE_MPEGTS == *transcode_thread_data->options->streaming_type) {

        /* Set output context parameters */

        if (0 > av_set_parameters(out_context, NULL)) {
            log_message(MSG_ERROR, "[Transcode] Couldn't set output context parameters.\n");
            goto INITIALIZE_TRANSCODING_ERROR;
        }

        /* Write output context header */

        if (0 != av_write_header(out_context)) {
            log_message(MSG_ERROR, "[Transcode] Writing header failed.\n");
            goto INITIALIZE_TRANSCODING_ERROR;
        }

        dump_format(out_context, 0, out_context->filename, 1);
    }

    free(out_contexts);
    return transcode_data;

INITIALIZE_TRANSCODING_ERROR:

    free(out_contexts);
    free_transcode(transcode_data, transcode_thread_data);
    return NULL;
}

void* initialize_transcode_ffm(transcode_thread_data_t *transcode_thread_data)
{
    if (NULL == transcode_thread_data->options->ffm_url) {
    log_message(MSG_ERROR, "[Transcode] URL of FFM feed was not specified (transcode_ffm_url).\n");
        return NULL;
    }

    int streaming_type = *transcode_thread_data->options->streaming_type;

    /* Initialize transcode data and open input context */
    transcode_data_t *transcode_data = initialize_transcode_data(transcode_thread_data);

    if (NULL == transcode_data) {
        return NULL;
    }

    AVFormatContext *in_context = transcode_data->in_context;

    /* Create output format context */

    AVFormatContext *out_context = create_output_format_context(transcode_thread_data->socket,
        transcode_thread_data->socket_addr, TRANSCODE_OUTPUT_BUFFER_SIZE,
        transcode_thread_data->options->ffm_url, streaming_type, 0);

    if (NULL == out_context) {
        log_message(MSG_ERROR, "[Transcode] Failed to initialize output format context.\n");

        free_format_context(in_context, 1, 0);
        free(transcode_data->input_streams_transcode_data);
        free(transcode_data);

        return NULL;
    }

    ref_queue_enqueue(&transcode_data->out_contexts, out_context);

    /* Read FFM streams */

    pthread_mutex_lock(&avlib_mutex);

    AVFormatContext *ffm_format_context;

    if (0 > av_open_input_file(&ffm_format_context, transcode_thread_data->options->ffm_url, NULL, FFM_PACKET_SIZE, NULL)) {
        log_message(MSG_ERROR, "[Transcode] Failed to read FFM feed.\n");
        pthread_mutex_unlock(&avlib_mutex);
        free_format_context(out_context, 0, streaming_type);
        free_format_context(in_context, 1, 0);
        free(transcode_data->input_streams_transcode_data);
        free(transcode_data);
        return NULL;
    }

    pthread_mutex_unlock(&avlib_mutex);

    dump_format(ffm_format_context, 0, transcode_thread_data->options->ffm_url, 0);

    /* Copy FFM streams to output context (approach from ffmpeg sources) */

    out_context->nb_streams = ffm_format_context->nb_streams;

    unsigned int i;
    for(i = 0; i < ffm_format_context->nb_streams; i++) {
        /* TODO: Add checks and free routines for this loop */

        AVStream *st;
        
        st = av_mallocz(sizeof(AVStream));
        memcpy(st, ffm_format_context->streams[i], sizeof(AVStream));
        st->codec = avcodec_alloc_context();
        memcpy(st->codec, ffm_format_context->streams[i]->codec, sizeof(AVCodecContext));
        st->codec->thread_count = 1;
        out_context->streams[i] = st;
    }

    av_close_input_file(ffm_format_context);

    url_fopen(&out_context->pb, out_context->filename, URL_WRONLY);

    if (NULL == out_context->pb) {
        log_message(MSG_ERROR, "[Transcode] Failed to open FFM feed.\n");

        free_format_context(out_context, 0, streaming_type);
        free_format_context(in_context, 1, 0);
        free(transcode_data->input_streams_transcode_data);
        free(transcode_data);

        return NULL;
    }

    dump_format(out_context, 0, out_context->filename, 1);

    /* Open output streams for transcoding */

    int output_video_streams = 0;
    int output_audio_streams = 0;

    for (i = 0; i < out_context->nb_streams; i++) {
        AVCodec *codec = avcodec_find_encoder(out_context->streams[i]->codec->codec_id);

        if (NULL == codec || (CODEC_TYPE_VIDEO != codec->type && CODEC_TYPE_AUDIO != codec->type)) {
            continue;
        }

        pthread_mutex_lock(&avlib_mutex);

        if (0 > avcodec_open(out_context->streams[i]->codec, codec)) {
            log_message(MSG_ERROR, "[Transcode] Couldn't open codec for encoding.\n");
            pthread_mutex_unlock(&avlib_mutex);
            continue;
        }

        pthread_mutex_unlock(&avlib_mutex);

        if (CODEC_TYPE_VIDEO == codec->type) {
            output_video_streams++;
        }
        else if (CODEC_TYPE_AUDIO == codec->type) {
            output_audio_streams++;
        }
    }

    if (0 == output_video_streams && 0 == output_audio_streams) {
        log_message(MSG_ERROR, "[Transcode] No output streams for transcoding.\n");
        free_transcode(transcode_data, transcode_thread_data);
        return NULL;
    }

    /* Open input streams for transcoding */

    int is_audio_initialized = 0;
    int is_video_initialized = 0;
    int64_t smallest_start_time = 0;

    for (i = 0; i < in_context->nb_streams; i++) {
        AVStream *in_stream = transcode_data->in_context->streams[i];

        transcode_data->input_streams_transcode_data[i].output_streams_transcode_data = NULL;
        transcode_data->input_streams_transcode_data[i].output_streams_count = 0;

        if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type ||
            CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {

            /* Initialize decoder for input stream */
                
            AVCodec *codec = avcodec_find_decoder(in_stream->codec->codec_id);

            if (NULL == codec) {
                log_message(MSG_ERROR, "[Transcode] Couldn't find codec for decoding.\n");
                continue;
            }

            pthread_mutex_lock(&avlib_mutex);

            if (0 > avcodec_open(in_stream->codec, codec)) {
                log_message(MSG_ERROR, "[Transcode] Couldn't open codec for decoding.\n");
                pthread_mutex_unlock(&avlib_mutex);
                continue;
            }

            /* WORKAROUND: specific bug - empty subtitle stream detected as audio one.
                Skip it. */
            if (0 == in_stream->codec->sample_rate &&
                CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {
                
                initialize_transcode_common_free(in_stream->codec, 1, NULL, NULL,
                    NULL, NULL, NULL, NULL);
                
                /* Fix memory leak in av_read_paket when reading this stream */
                in_stream->need_parsing = AVSTREAM_PARSE_NONE;
                
                continue;
            }

            if ((CODEC_TYPE_VIDEO == in_stream->codec->codec_type && is_video_initialized) ||
                (CODEC_TYPE_AUDIO == in_stream->codec->codec_type && is_audio_initialized)) {
                /* Audio or video already initialized - skipping */
                initialize_transcode_common_free(in_stream->codec, 1, NULL, NULL,
                    NULL, NULL, NULL, NULL);
                continue;
            }

            pthread_mutex_unlock(&avlib_mutex);

            if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type) {
                transcode_data->input_streams_transcode_data[i].output_streams_count = output_video_streams;
            }
            else if (CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {
                transcode_data->input_streams_transcode_data[i].output_streams_count = output_audio_streams;
            }

            if (0 != transcode_data->input_streams_transcode_data[i].output_streams_count) {
                transcode_data->input_streams_transcode_data[i].output_streams_transcode_data =
                    calloc(sizeof(output_stream_transcode_data_t),
                        transcode_data->input_streams_transcode_data[i].output_streams_count);
            }
            else {
                initialize_transcode_common_free(in_stream->codec, 0, NULL, NULL,
                    NULL, NULL, NULL, NULL);
                continue;
            }

            unsigned int j;
            int good_streams_counter = 0;

            for (j = 0; j < out_context->nb_streams; j++) {
                if (out_context->streams[j]->codec->codec_type == in_stream->codec->codec_type) {
                    output_stream_transcode_data_t *output_stream_transcode_data =
                        transcode_data->input_streams_transcode_data[i].output_streams_transcode_data + good_streams_counter;

                    if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type) {
                        if (!initialize_video_parameters(transcode_data->input_streams_transcode_data + i, 
                            output_stream_transcode_data, in_stream, out_context->streams[j])) {
                            continue;
                        }
                    }
                    else if (CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {
                        if (!initialize_audio_parameters(transcode_data->input_streams_transcode_data + i, 
                            output_stream_transcode_data, in_stream, out_context->streams[j])) {
                            continue;
                        }
                    }

                    output_stream_transcode_data->out_context = out_context;
                    output_stream_transcode_data->out_stream = out_context->streams[j];
                    output_stream_transcode_data->frame_no = 0;
                    output_stream_transcode_data->written_frames = -1;
                    ref_queue_init(&output_stream_transcode_data->packets, PACKETS_QUEUE_LENGTH);

                    good_streams_counter++;
                }
            }

            transcode_data->input_streams_transcode_data[i].output_streams_count = good_streams_counter;

            if (0 != good_streams_counter) {
                /* Calculating start time for all streams */
                if (in_stream->start_time < smallest_start_time || 0 == smallest_start_time) {
                    smallest_start_time = in_stream->start_time;
                }

                if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type) {
                    is_video_initialized = 1;
                }
                else if (CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {
                    is_audio_initialized = 1;
                }
            }
        }
    }

    if (!is_video_initialized && !is_audio_initialized) {
        log_message(MSG_ERROR, "[Transcode] No input streams for transcoding.\n");
        free_transcode(transcode_data, transcode_thread_data);
        return NULL;
    }

    /* Synchronizing start time */

    for (i = 0; i < in_context->nb_streams; i++) {
        transcode_data->input_streams_transcode_data[i].stream_start_pts = smallest_start_time;
    }

    /* Set output context pameters */

    if (0 > av_set_parameters(out_context, NULL)) {
        log_message(MSG_ERROR, "[Transcode] Couldn't set output context parameters.\n");
                free_transcode(transcode_data, transcode_thread_data);
        return NULL;
    }

    /* Write output context header */

    if (0 != av_write_header(out_context)) {
        log_message(MSG_ERROR, "[Transcode] Writing header failed.\n");
        free_transcode(transcode_data, transcode_thread_data);
        return NULL;
    }

    //dump_format(out_context, 0, out_context->filename, 1);

    return transcode_data;
}

void* initialize_transcode(transcode_thread_data_t *transcode_thread_data)
{
    /* Initialize all codecs */

    pthread_mutex_lock(&avlib_mutex);

    static int avlib_initialized = 0;

    if (!avlib_initialized) {
        log_message(MSG_ERROR, "[Transcode] Initializing avlibs.\n");
        avlib_initialized = 1;
        avcodec_register_all();
        av_register_all();
    }

    pthread_mutex_unlock(&avlib_mutex);

    if (NULL != transcode_thread_data->options->streaming_type &&
        STREAMING_TYPE_FFM == *(transcode_thread_data->options->streaming_type)) {

        return initialize_transcode_ffm(transcode_thread_data);
    }

    return initialize_transcode_common(transcode_thread_data);
}

void free_transcode(void *transcode_avlib_handle, transcode_thread_data_t *transcode_thread_data)
{
    transcode_data_t *transcode_data = transcode_avlib_handle;

    /* Close output format context(s) */

    AVFormatContext *out_context;

    while (NULL != (out_context = ref_queue_dequeue(&transcode_data->out_contexts))) {
        /* Writing trailer */
        if (0 != av_write_trailer(out_context)) {
            log_message(MSG_ERROR, "[Transcode] Couldn't write trailer.\n");
        }
        
        free_format_context(out_context, 0,
            NULL == transcode_thread_data->options->streaming_type ?
            STREAMING_TYPE_MPEGTS : *transcode_thread_data->options->streaming_type);
    }

    /* Free steams transcoding data */

    int i;
    for (i = 0; i < transcode_data->input_streams_count; i++) {

        if (NULL != transcode_data->input_streams_transcode_data[i].output_streams_transcode_data) {
           int j;
           for (j = 0; j < transcode_data->input_streams_transcode_data[i].output_streams_count; j++) {
                
                output_stream_transcode_data_t *output_stream_transcode_data =
                    transcode_data->input_streams_transcode_data[i].output_streams_transcode_data + j;
                
                /* Free packets queue */
                
                ref_queue_t *queue = &(output_stream_transcode_data->packets);
                
                AVPacket *packet;

                while (NULL != (packet = ref_queue_dequeue(queue))) {
                    av_free(packet->data);
                    free(packet);
                }

                /* Free video scale data */
                
                if (NULL != output_stream_transcode_data->sws_context) {
                    sws_freeContext(output_stream_transcode_data->sws_context);
                    avpicture_free((AVPicture*)&output_stream_transcode_data->temp_picture);
                }

                if (NULL != output_stream_transcode_data->resample_context) {
                    audio_resample_close(output_stream_transcode_data->resample_context);
                }

                /* Free audio stream transcode data */
                if (NULL != output_stream_transcode_data->frame_buf) {
                    free(output_stream_transcode_data->frame_buf);
                }
           }

           free(transcode_data->input_streams_transcode_data[i].output_streams_transcode_data);
        }
    }
    
    free(transcode_data->input_streams_transcode_data);

    /* Close input format context */
    free_format_context(transcode_data->in_context, 1, 0);

    free(transcode_data);
}

int copy_packet_to_queue(ref_queue_t *queue, AVPacket *packet)
{
    AVPacket *copy = malloc(sizeof(AVPacket));

    if (NULL == copy) {
        return 0;
    }

    av_init_packet(copy);

    copy->stream_index = packet->stream_index;
    copy->pts = packet->pts;
    copy->dts = packet->dts;
    copy->flags = packet->flags;
    copy->size = packet->size;

    copy->data = (uint8_t*)av_malloc(packet->size);

    if (NULL == copy->data) {
        free(copy);
        return 0;
    }

    int result = ref_queue_enqueue(queue, copy);

    if (1 != result) {
        if (0 == result) {
            log_message(MSG_INFO, "[Transcode] Frames queue is full.\n");
        }
        else if (-1 == result) {
            log_message(MSG_ERROR, "[Transcode] Failed to add video frame to queue.\n");
        }

        av_free(copy->data);
        free(copy);

        return 0;
    }

    memcpy(copy->data, packet->data, packet->size);

    return 1;
}

int write_frames_from_queue(AVFormatContext *out_context, ref_queue_t *queue, int count)
{
    int counter = 0;

    AVPacket *packet;
    while (NULL != (packet= ref_queue_dequeue(queue))) {

        /*if (out_context->streams[packet->stream_index]->codec->codec_type == CODEC_TYPE_VIDEO) {
            printf("video\n");
        }
        else {
            printf("audio\n");
        }*/

        if (0 > av_interleaved_write_frame(out_context, packet)) {
            log_message(MSG_ERROR, "[Transcode] Couldn't write frame.\n");
            av_free(packet->data);
            free(packet);
            /* Error writing frame */
            return -1;
        }

        av_free(packet->data);
        free(packet);

        counter++;

        if (count != -1) {
            count--;
            if (count ==  0) {
                break;
            }
        }
    }

    return counter;
}

int write_succeeded_frame(transcode_data_t *transcode_data, int input_stream_index, int output_stream_index,
    uint8_t *outbuf, int out_size)
{
    input_stream_transcode_data_t *input_stream_data =
        transcode_data->input_streams_transcode_data + input_stream_index;

    output_stream_transcode_data_t *output_stream_data =
        input_stream_data->output_streams_transcode_data + output_stream_index;

    /* Create output packet */

    AVPacket out_packet;
    AVStream *out_stream = output_stream_data->out_stream;
    AVFrame *coded_frame = out_stream->codec->coded_frame;

    av_init_packet(&out_packet);

    out_packet.stream_index = out_stream->index;
    
    if ((coded_frame && coded_frame->key_frame) || CODEC_TYPE_AUDIO == out_stream->codec->codec_type) {
        out_packet.flags |= PKT_FLAG_KEY;
    }

    out_packet.data = outbuf;
    out_packet.size = out_size;

    /*if (0 > av_interleaved_write_frame(output_stream_data->out_context, &out_packet)) {
        log_message(MSG_ERROR, "[Transcode] Couldn't write frame.\n");
        return -1;
    }

    return 0;*/

    /* Enqueue packet */
    
    copy_packet_to_queue(&output_stream_data->packets, &out_packet);

    /* Output packets from queues */

    /* Get elapsed time since transcoding start */

    if (0 == transcode_data->start_time.tv_sec) {
        gettimeofday(&transcode_data->start_time, NULL);
    }
    
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    double time_elapsed_sec =
        (double)(current_time.tv_usec - transcode_data->start_time.tv_usec) / 1000000 + 
        current_time.tv_sec - transcode_data->start_time.tv_sec;
 
    while (1) {
        int is_frames_written = 0;
        int i, j;
 
        for (i = 0; i < transcode_data->input_streams_count; i++) {
            for (j = 0; j < transcode_data->input_streams_transcode_data[i].output_streams_count; j++) {
                output_stream_transcode_data_t *stream_data =
                    transcode_data->input_streams_transcode_data[i].output_streams_transcode_data + j;

                /* Do we need to output packet?*/
                int do_packet_output = 0;

                if (CODEC_TYPE_VIDEO == stream_data->out_stream->codec->codec_type) {
                    do_packet_output =
                        (double)stream_data->written_frames *
                        stream_data->out_stream->codec->time_base.num /
                        stream_data->out_stream->codec->time_base.den
                            <=
                        time_elapsed_sec;

                    /*if (do_packet_output) {
                        printf("------------- Video\n");
                    }*/
                }
                else if (CODEC_TYPE_AUDIO == stream_data->out_stream->codec->codec_type) {
                    do_packet_output =
                        (double)stream_data->written_frames *
                        stream_data->out_stream->codec->frame_size /
                        stream_data->out_stream->codec->sample_rate
                            <=
                        time_elapsed_sec;

                    /*if (do_packet_output) {
                        printf("-------------- Audio\n");
                    }*/
                }

                if (!do_packet_output) {
                    continue;
                }

                /* Output packet */

                int written_frames = write_frames_from_queue(
                        stream_data->out_context, &stream_data->packets, 2);

                if (0 == written_frames) {
                    continue;
                }
                else if (0 > written_frames) {
                    log_message(MSG_ERROR, "[Transcode] Error writing frame\n");
                    return -1; /* Error writing frame */
                }

                is_frames_written = 1;
                stream_data->written_frames += written_frames;
            }
        }

        if (!is_frames_written) {
            break;
        }
    }

    return 0;
}

void transcode(void *transcode_avlib_handle, transcode_thread_data_t *transcode_thread_data)
{
     transcode_data_t *transcode_data = transcode_avlib_handle;
 
     AVFormatContext *in_context = transcode_data->in_context;
     input_stream_transcode_data_t *input_streams_transcode_data = transcode_data->input_streams_transcode_data;
 
     /* Initializing data for transcoding */
 
     AVPacket packet;
     
     AVFrame *frame = avcodec_alloc_frame();
 
     if (NULL == frame) {
         log_message(MSG_ERROR, "[Transcode] Couldn't allocate frame.\n");
         return;
     }
 
     uint8_t *outbuf = (uint8_t*)av_malloc(TRANSCODE_BUF_SIZE);
 
     if (NULL == outbuf) {
         log_message(MSG_ERROR, "[Transcode] Couldn't allocate buffer.\n");
         av_free(frame);
         return;
     }
 
     int16_t *inbuf = (int16_t*)av_malloc(TRANSCODE_BUF_SIZE);
 
     if (NULL == inbuf) {
         log_message(MSG_ERROR, "[Transcode] Couldn't allocate buffer.\n");
         av_free(frame);
         av_free(outbuf);
         return;
     }

    int16_t *inbuf2 = (int16_t*)av_malloc(TRANSCODE_BUF_SIZE);
 
    if (NULL == inbuf2) {
        log_message(MSG_ERROR, "[Transcode] Couldn't allocate buffer.\n");
        av_free(frame);
        av_free(inbuf);
        av_free(outbuf);
        return;
    }

    int bytes_decoded;
    int is_frame_finished;
    int out_size;
    int64_t input_frame_no;
    int terminate_loop_flag = 0;
 
     /* Reading -> decoding -> encoding -> writing all frames */
 
     while (!transcode_thread_data->terminate_thread_flag && !terminate_loop_flag) {
 
        if (0 > av_read_frame(in_context, &packet)) {
            log_message(MSG_ERROR, "[Transcode] Ending transcoding - av_read_frame returned error.\n");
            break;
        }

        input_stream_transcode_data_t *cur_input_stream_transcode_data =
            input_streams_transcode_data + packet.stream_index;
        AVStream *in_stream = in_context->streams[packet.stream_index];

//        cur_input_stream_transcode_data->x++;

        /* Get PTS and rescale it to duration_factor */

        input_frame_no = ((int64_t)AV_NOPTS_VALUE != packet.dts ?
            packet.dts : ((int64_t)AV_NOPTS_VALUE != packet.pts ? packet.pts : (int64_t)AV_NOPTS_VALUE));

        if ((int64_t)AV_NOPTS_VALUE != input_frame_no) {
            /* Input frame number in format 1, 2, 3, ... */
            input_frame_no = (int64_t)((double)(input_frame_no - cur_input_stream_transcode_data->stream_start_pts) /
            cur_input_stream_transcode_data->pts_duration_factor + 0.5);
        }

        if (0 == cur_input_stream_transcode_data->output_streams_count ||
                NULL == cur_input_stream_transcode_data->output_streams_transcode_data) {

            goto FREE_PACKET_AND_CONTINUE;
        }
 
        if (CODEC_TYPE_VIDEO == in_stream->codec->codec_type) {

            /* Decode video */

            bytes_decoded = avcodec_decode_video(in_stream->codec,
                frame, &is_frame_finished, packet.data, packet.size);

            if (0 >= bytes_decoded) {
                /* Video was not decoded */
                log_message(MSG_ERROR, "[Transcode] Video was not decoded.\n");
            }
            else if (!is_frame_finished) {
                /* Video was not decoded */
                /* log_message(MSG_ERROR, "[Transcode] Frame not finished.\n"); */
            }
            else {
                /* Video was decoded. Encode video for each output stream */

                int i;
                for (i = 0; i < cur_input_stream_transcode_data->output_streams_count; i++) {

                    output_stream_transcode_data_t *cur_output_stream_transcode_data = 
                        cur_input_stream_transcode_data->output_streams_transcode_data + i;
                    
                    /* Skip frames with wrong PTS or late PTS */
                    if ((int64_t)AV_NOPTS_VALUE == input_frame_no || input_frame_no < 0 ||
                            input_frame_no < cur_output_stream_transcode_data->frame_no *
                            cur_output_stream_transcode_data->frame_rate_factor) {

                        if (input_frame_no < 0) {
                            log_message(MSG_INFO, "[Transcode] Reset transcoding: input frame number < 0.\n");
                            terminate_loop_flag = 1;
                        }

                        goto FREE_PACKET_AND_CONTINUE;
                    }

                    AVFrame encode_frame;

                    if (cur_output_stream_transcode_data->sws_context) {
                        /* Rescale video */
                        sws_scale(cur_output_stream_transcode_data->sws_context,
                            frame->data, frame->linesize,
                            0, in_stream->codec->height, 
                            cur_output_stream_transcode_data->temp_picture.data,
                            cur_output_stream_transcode_data->temp_picture.linesize);

                        encode_frame = cur_output_stream_transcode_data->temp_picture;
                    }
                    else {
                        encode_frame = *frame;
                    }

                    encode_frame.interlaced_frame = frame->interlaced_frame;
                    encode_frame.pict_type = 0;
                    encode_frame.quality = cur_output_stream_transcode_data->out_stream->quality;
                    encode_frame.pts = AV_NOPTS_VALUE;


                    //printf("Entering loop\n");

                    do {
                        /* Encode frame and write it to output */

                        out_size = avcodec_encode_video(cur_output_stream_transcode_data->out_stream->codec, outbuf,
                            TRANSCODE_BUF_SIZE, &encode_frame);

                        if (0 > out_size) {
                            /* Error encoding video frame */
                            log_message(MSG_ERROR, "[Transcode] Error encoding video frame.\n");
                            break;
                        }
                        else {
                            /* Video was encoded - saving it */

                            cur_output_stream_transcode_data->frame_no++;

                            if (0 != out_size) {

                                //printf("Frame out\n");

                                if (0 > write_succeeded_frame(transcode_data, packet.stream_index, i,
                                            outbuf, out_size)) {

                                    terminate_loop_flag = 1;
                                    goto FREE_PACKET_AND_CONTINUE;
                                }
                            }
                            else {
                                //printf("Frame is buffered\n");
                            }
                            /* else frame is buffered (happens when b-frames present in output stream) */
                        }
                    } while (input_frame_no >= (cur_output_stream_transcode_data->frame_no) *
                                    cur_output_stream_transcode_data->frame_rate_factor); /* Duplicating */
                }
            }
        }
        else if (CODEC_TYPE_AUDIO == in_stream->codec->codec_type) {            

            /* Decode audio */
 
            int result_bytes = TRANSCODE_BUF_SIZE;
            
            /* Decoding audio to input buffer */
            bytes_decoded = avcodec_decode_audio2(in_stream->codec, inbuf,
                &result_bytes, packet.data, packet.size);

            if (0 > bytes_decoded) {
                log_message(MSG_ERROR, "[Transcode] Error decoding audio frame.\n");
            }
            else if (0 == bytes_decoded || 0 == result_bytes) {
                log_message(MSG_ERROR, "[Transcode] Frame could not be decoded.\n");
            }
            else {
                if (bytes_decoded != packet.size) {
                    log_message(MSG_ERROR, "[Transcode] bytes_decoded != packet.size.\n");
                }

                /* Audio was decoded. Encode audio */
                int i;
                for (i = 0; i < cur_input_stream_transcode_data->output_streams_count; i++) {
                    
                    output_stream_transcode_data_t *cur_output_stream_transcode_data = 
                        cur_input_stream_transcode_data->output_streams_transcode_data + i;

                    /* Skip frames with wrong pts */
                    if ((int64_t)AV_NOPTS_VALUE == input_frame_no || input_frame_no < 0 ||
                            input_frame_no < cur_output_stream_transcode_data->frame_no *
                            cur_output_stream_transcode_data->frame_rate_factor) {

                        if (input_frame_no < 0) {
                            log_message(MSG_INFO, "[Transcode] Reset transcoding: input frame number < 0.\n");
                            terminate_loop_flag = 1;
                        }

                        goto FREE_PACKET_AND_CONTINUE;
                    }

                    int16_t *cur_buf;

                    if (NULL != cur_output_stream_transcode_data->resample_context) {

                        int out_samples = audio_resample(cur_output_stream_transcode_data->resample_context,
                            inbuf2, inbuf,
                            result_bytes / cur_input_stream_transcode_data->sample_size);

                        cur_buf = inbuf2;
                        result_bytes = out_samples * cur_output_stream_transcode_data->sample_size;
                    }
                    else {
                        cur_buf = inbuf;
                    }

                    int data_was_encoded;

                    do {
                        data_was_encoded = 0;

                        char* buf_pos = (char*)cur_buf;
                        
                        while (buf_pos < (char*)cur_buf + result_bytes) {
                            /* Copy cur_buf data to frame_buf */

                            int data_size_to_copy = cur_output_stream_transcode_data->frame_size -
                                cur_output_stream_transcode_data->frame_buf_data_length;
                            
                            data_size_to_copy = data_size_to_copy < result_bytes - (buf_pos - (char*)cur_buf) ?
                                data_size_to_copy : result_bytes - (buf_pos - (char*)cur_buf);

                            memcpy(
                                (char*)cur_output_stream_transcode_data->frame_buf +
                                        cur_output_stream_transcode_data->frame_buf_data_length,
                                        buf_pos, data_size_to_copy);
  
                            buf_pos += data_size_to_copy;
                            cur_output_stream_transcode_data->frame_buf_data_length += data_size_to_copy;

                            if (cur_output_stream_transcode_data->frame_buf_data_length !=
                                    cur_output_stream_transcode_data->frame_size) {
                                break; /* Not enough data for encoding */
                            }

                            out_size = avcodec_encode_audio(
                                cur_output_stream_transcode_data->out_stream->codec, outbuf,
                                TRANSCODE_BUF_SIZE, cur_output_stream_transcode_data->frame_buf);

                            cur_output_stream_transcode_data->frame_buf_data_length = 0;

                            if (0 > out_size) {
                                log_message(MSG_ERROR, "[Transcode] Error encoding audio.\n");
                                continue;
                            }
                            if (0 == out_size) {
                                //log_message(MSG_ERROR, "[Transcode] Error not enough data for encoding audio frame.\n");
                                continue;
                            }
                            else {
                                data_was_encoded = 1;

                                /* Audio was encoded - saving it */

                                cur_output_stream_transcode_data->frame_no++;
                                if (0 > write_succeeded_frame(transcode_data, packet.stream_index, i,
                                            outbuf, out_size)) {

                                    terminate_loop_flag = 1;
                                    goto FREE_PACKET_AND_CONTINUE;
                                }
                            }
                        }
                    } while (data_was_encoded && input_frame_no >= (cur_output_stream_transcode_data->frame_no + 1) *
                                cur_output_stream_transcode_data->frame_rate_factor);
                }
            }                    
         }
         else if (CODEC_TYPE_SUBTITLE == in_stream->codec->codec_type) {
         }

FREE_PACKET_AND_CONTINUE:
         av_free_packet(&packet);
     }
 
     /* Free allocated data */
     
     av_free(outbuf);
     av_free(inbuf);
     av_free(inbuf2);
     av_free(frame);
 
     return;
}
