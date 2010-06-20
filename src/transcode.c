/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Code for transcoding
 * 
 * Code written by Utelisys Communications B.V.
 * Copyright (C) 2009 Utelisys Communications B.V.
 * Copyright (C) 2009 Brice DUBOST
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
#include "transcode.h"
#include "transcode_queues.h"
#include "transcode_avlib.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#endif

#define TRANSCODE_QUEUE_SIZE (5 * 1024 * 1024)

static char *log_module="Transcode : ";

void* transcode_thread_routine(void *p)
{
    transcode_thread_data_t *transcode_thread_data = p;

    while (!transcode_thread_data->terminate_thread_flag) {
        
        if (0 >= transcode_thread_data->data_queue.data_size) {
            log_message( log_module, MSG_DEBUG, "No data for transcoding.\n");

            /* Sleep 100 times for 0.1 sec with checking transcode_thread_data->terminate_thread_flag */
            int i;
            for (i = 0; i < 100 && !transcode_thread_data->terminate_thread_flag; i++) {
                usleep(100000);
            }

            continue; /* No data - go to beginning of this loop */
        }

        /* Initialize transcode */

        void *transcode_handle = initialize_transcode(transcode_thread_data);
        
        if (NULL == transcode_handle) {
            log_message( log_module, MSG_ERROR, "Failed to initialize transcoding.\n");
            
            /* Sleep 100 times for 0.1 sec with checking transcode_thread_data->terminate_thread_flag */
            int i;
            for (i = 0; i < 100 && !transcode_thread_data->terminate_thread_flag; i++) {
                usleep(100000);
            }

            continue;
        }

        log_message( log_module, MSG_INFO, "Transcoding sarted.\n");
        transcode_thread_data->is_initialized = 1;
        
        /* Transcode */
        transcode(transcode_handle, transcode_thread_data);

        transcode_thread_data->is_initialized = 0;        
        log_message( log_module, MSG_INFO, "Transcoding finished.\n");
 
        /* Free transcode data - requires threadsafety */
        free_transcode(transcode_handle, transcode_thread_data);
    }

    return NULL;
}

void* transcode_start_thread(int socket, struct sockaddr_in *socket_addr,
    transcode_options_t *options)
{
    /* Create transcode thread data  */

    transcode_thread_data_t *transcode_thread_data = malloc(sizeof(transcode_thread_data_t));

    if (NULL == transcode_thread_data) {
        return NULL;
    }

    /* Initialize properties and data queue */

    transcode_thread_data->socket = socket;
    transcode_thread_data->socket_addr = socket_addr;
    transcode_thread_data->options = options;
    transcode_thread_data->is_initialized = 0;

    data_queue_init(&transcode_thread_data->data_queue, TRANSCODE_QUEUE_SIZE);

    /* Initialize and create transcode thread and mutexes */

    transcode_thread_data->terminate_thread_flag = 0;

    if (pthread_mutex_init(&transcode_thread_data->queue_mutex, NULL) != 0) {
        /* Mutext initialization failed */
        log_message( log_module, MSG_ERROR, "pthread_mutex_init failed.\n");
        free(transcode_thread_data);
        return NULL;
    }

    if (pthread_create(&transcode_thread_data->thread, NULL,
            transcode_thread_routine, transcode_thread_data) != 0) {
        /* Thread initialization failed */
        log_message( log_module, MSG_ERROR, "pthread_create failed.\n");
        pthread_mutex_destroy(&transcode_thread_data->queue_mutex);
        free(transcode_thread_data);
        return NULL;
    }

    return (void*)transcode_thread_data;
}

void transcode_request_thread_end(void *transcode_handle)
{
    if (NULL == transcode_handle) {
        return;
    }

    transcode_thread_data_t *transcode_thread_data = transcode_handle;

    transcode_thread_data->terminate_thread_flag = 1;
}

void transcode_wait_thread_end(void *transcode_handle)
{
    if (NULL == transcode_handle) {
        return;
    }

    transcode_thread_data_t *transcode_thread_data = transcode_handle;

    /* Finish thread, destroy mutexes */

    pthread_join(transcode_thread_data->thread, NULL);
    
    pthread_mutex_destroy(&transcode_thread_data->queue_mutex);

    /* Free thread data */

    data_queue_free(&transcode_thread_data->data_queue);

    free(transcode_thread_data);
}

int transcode_enqueue_data(void *transcode_handle, void *data, int data_size)
{    
    if (NULL == transcode_handle) {
        return -1;
    }

    transcode_thread_data_t *transcode_thread_data = transcode_handle;

    /* Lock mutext and add data into the queue */

    pthread_mutex_lock(&transcode_thread_data->queue_mutex);

    int result = data_queue_enqueue(&transcode_thread_data->data_queue, data, data_size);

    if (-1 == result) {
        log_message( log_module, MSG_ERROR, "Failed to enqueue data.\n");
    }
    else if (0 == result) {
        if (transcode_thread_data->is_initialized) {
            log_message( log_module, MSG_INFO, "Data queue is full.\n");
        }
    }

    pthread_mutex_unlock(&transcode_thread_data->queue_mutex);

    return result;
}

#define SET_OPTION_INT(config_option_name, struct_option_name)\
else if (!strcmp(*substring, config_option_name)) {\
    if (NULL == struct_option_name) {\
        struct_option_name = malloc(sizeof(int));\
    }\
    *(struct_option_name) = atoi(strtok(NULL, delimiteurs));\
}

#define SET_OPTION_FLT(config_option_name, struct_option_name)\
else if (!strcmp(*substring, config_option_name)) {\
    if (NULL == struct_option_name) {\
        struct_option_name = malloc(sizeof(float));\
    }\
    *(struct_option_name) = atof(strtok(NULL, delimiteurs));\
}

#define SET_OPTION_STR(config_option_name, struct_option_name, max_length)\
else if (!strcmp(*substring, config_option_name)) {\
    *substring = strtok(NULL, delimiteurs);\
    int length = strlen(*substring);\
    if (length <= max_length) {\
        if (NULL != struct_option_name) {\
            free(struct_option_name);\
        }\
        struct_option_name = malloc(length + 1);\
        strcpy(struct_option_name, *substring);\
        trim(struct_option_name);\
    }\
}

// Trim spaces, tabs and '\n'
void trim(char *s)
{
    size_t i = 0;

    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n') {
        i++;
    }

    if (i > 0) {
        size_t j;
        for (j = 0; j < strlen(s); j++) {
            s[j] = s[j+i];
        }
        s[j] = '\0';
    }

    i = strlen(s) - 1;

    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n') {
        i--;
    }

    if (i < strlen(s) - 1) {
        s[i+1] = '\0';
    }
}

int transcode_read_option(struct transcode_options_t *transcode_options, char *delimiteurs, char **substring)
{
    if (!strcmp(*substring, "transcode_streaming_type")) {

        *substring = strtok(NULL, delimiteurs);

        if (strlen(*substring) <= TRANSCODE_STREAMING_TYPE_MAX) {

            int streaming_type;
            int streaming_type_set = 0;

            char *stremaing_type_string = malloc(strlen(*substring) + 1);
            strcpy(stremaing_type_string, *substring);
            trim(stremaing_type_string);

            if (0 == strcmp(stremaing_type_string, "mpegts")) {
                streaming_type_set = 1;
                streaming_type = STREAMING_TYPE_MPEGTS;
            }
            else if (0 == strcmp(stremaing_type_string, "rtp")) {
                streaming_type_set = 1;
                streaming_type = STREAMING_TYPE_RTP;
            }
            else if (0 == strcmp(stremaing_type_string, "ffm")) {
                streaming_type_set = 1;
                streaming_type = STREAMING_TYPE_FFM;
            }

            if (streaming_type_set) {
                if (NULL == transcode_options->streaming_type) {
                    transcode_options->streaming_type = malloc(sizeof(int));
                }

                *(transcode_options->streaming_type) = streaming_type;
            }

            free(stremaing_type_string);
        }
    }
    else if (!strcmp(*substring, "transcode_x264_profile")) {

        *substring = strtok(NULL, delimiteurs);

        if (strlen(*substring) <= TRANSCODE_PROFILE_MAX) {
            int profile_set = 0;

            char *profile_string = malloc(strlen(*substring) + 1);
            strcpy(profile_string, *substring);
            trim(profile_string);

            if (strcmp(profile_string, "baseline") == 0) {

                transcode_options->coder_type = (NULL == transcode_options->coder_type ?
                    malloc(sizeof(int)) : transcode_options->coder_type);
                transcode_options->enable_8x8dct = (NULL == transcode_options->enable_8x8dct ?
                    malloc(sizeof(int)) : transcode_options->enable_8x8dct);

                *(transcode_options->coder_type) = 0;
                *(transcode_options->enable_8x8dct) = 0;

                profile_set = 1;
            }
            else if (strcmp(profile_string, "main") == 0) {
                transcode_options->coder_type = (NULL == transcode_options->coder_type ?
                    malloc(sizeof(int)) : transcode_options->coder_type);
                transcode_options->enable_8x8dct = (NULL == transcode_options->enable_8x8dct ?
                    malloc(sizeof(int)) : transcode_options->enable_8x8dct);

                *(transcode_options->coder_type) = 1;
                *(transcode_options->enable_8x8dct) = 0;

                profile_set = 1;
            }
            else if (strcmp(profile_string, "high") == 0) {
                transcode_options->coder_type = (NULL == transcode_options->coder_type ?
                    malloc(sizeof(int)) : transcode_options->coder_type);
                transcode_options->enable_8x8dct = (NULL == transcode_options->enable_8x8dct ?
                    malloc(sizeof(int)) : transcode_options->enable_8x8dct);

                *(transcode_options->coder_type) = 1;
                *(transcode_options->enable_8x8dct) = 1;

                profile_set = 1;
            }

            free(profile_string);

            if (profile_set) {
                transcode_options->loop_filter = (NULL == transcode_options->loop_filter ?
                    malloc(sizeof(int)) : transcode_options->loop_filter);
                transcode_options->sc_threshold = (NULL == transcode_options->sc_threshold ?
                    malloc(sizeof(int)) : transcode_options->sc_threshold);
                transcode_options->x264_partitions = (NULL == transcode_options->x264_partitions ?
                    malloc(sizeof(int)) : transcode_options->x264_partitions);

                *(transcode_options->loop_filter) = 1;
                *(transcode_options->sc_threshold) = 1;
                *(transcode_options->x264_partitions) = 1;

                if (NULL != transcode_options->video_codec) {
                    free(transcode_options->video_codec);
                }

                char *codec_str = "libx264";

                transcode_options->video_codec = malloc(strlen(codec_str) + 1);
                strcpy(transcode_options->video_codec, codec_str);
            }
        }
    }
    else if (!strcmp(*substring, "transcode_aac_profile")) {

        *substring = strtok(NULL, delimiteurs);

        if (strlen(*substring) <= TRANSCODE_AAC_PROFILE_MAX) {

            int aac_profile_set = 0;
            int aac_profile;

            char *aac_profile_string = malloc(strlen(*substring) + 1);
            strcpy(aac_profile_string, *substring);
            trim(aac_profile_string);

            if (0 == strcmp(aac_profile_string, "low")) {
                aac_profile_set = 1;
                aac_profile = FF_PROFILE_AAC_LOW;
            }
            else if (0 == strcmp(aac_profile_string, "main")) {
                aac_profile_set = 1;
                aac_profile = FF_PROFILE_AAC_MAIN;
            }
            else if (0 == strcmp(aac_profile_string, "ssr")) {
                aac_profile_set = 1;
                aac_profile = FF_PROFILE_AAC_SSR;
            }
            else if (0 == strcmp(aac_profile_string, "ltp")) {
                aac_profile_set = 1;
                aac_profile = FF_PROFILE_AAC_LTP;
            }

            if (aac_profile_set) {
                if (NULL == transcode_options->aac_profile) {
                    transcode_options->aac_profile = malloc(sizeof(int));
                }

                *(transcode_options->aac_profile) = aac_profile;
            }

            free(aac_profile_string);
        }
    }

    SET_OPTION_INT("transcode_enable", transcode_options->enable)
    SET_OPTION_INT("transcode_video_bitrate", transcode_options->video_bitrate)
    SET_OPTION_INT("transcode_audio_bitrate", transcode_options->audio_bitrate)
    SET_OPTION_INT("transcode_gop", transcode_options->gop)
    SET_OPTION_INT("transcode_b_frames", transcode_options->b_frames)
    SET_OPTION_INT("transcode_mbd", transcode_options->mbd)
    SET_OPTION_INT("transcode_cmp", transcode_options->cmp)
    SET_OPTION_INT("transcode_subcmp", transcode_options->subcmp)
    SET_OPTION_STR("transcode_video_codec", transcode_options->video_codec, TRANSCODE_CODEC_MAX)
    SET_OPTION_STR("transcode_audio_codec", transcode_options->audio_codec, TRANSCODE_CODEC_MAX)
    SET_OPTION_FLT("transcode_crf", transcode_options->crf)
    SET_OPTION_INT("transcode_refs", transcode_options->refs)
    SET_OPTION_INT("transcode_b_strategy", transcode_options->b_strategy)
    SET_OPTION_INT("transcode_coder_type", transcode_options->coder_type)
    SET_OPTION_INT("transcode_me_method", transcode_options->me_method)
    SET_OPTION_INT("transcode_me_range", transcode_options->me_range)
    SET_OPTION_INT("transcode_subq", transcode_options->subq)
    SET_OPTION_INT("transcode_trellis", transcode_options->trellis)
    SET_OPTION_INT("transcode_sc_threshold", transcode_options->sc_threshold)
    SET_OPTION_STR("transcode_rc_eq", transcode_options->rc_eq, TRANSCODE_RC_EQ_MAX)
    SET_OPTION_FLT("transcode_qcomp", transcode_options->qcomp)
    SET_OPTION_INT("transcode_qmin", transcode_options->qmin)
    SET_OPTION_INT("transcode_qmax", transcode_options->qmax)
    SET_OPTION_INT("transcode_qdiff", transcode_options->qdiff)
    SET_OPTION_INT("transcode_loop_filter", transcode_options->loop_filter)
    SET_OPTION_INT("transcode_mixed_refs", transcode_options->mixed_refs)
    SET_OPTION_INT("transcode_enable_8x8dct", transcode_options->enable_8x8dct)
    SET_OPTION_INT("transcode_x264_partitions", transcode_options->x264_partitions)
    SET_OPTION_INT("transcode_level", transcode_options->level)
    SET_OPTION_STR("transcode_sdp_filename", transcode_options->sdp_filename, TRANSCODE_SDP_FILENAME_MAX)
    SET_OPTION_FLT("transcode_video_scale", transcode_options->video_scale)
    SET_OPTION_INT("transcode_aac_latm", transcode_options->aac_latm)
    SET_OPTION_STR("transcode_ffm_url", transcode_options->ffm_url, TRANSCODE_FFM_URL_MAX)
    SET_OPTION_INT("transcode_audio_channels", transcode_options->audio_channels)
    SET_OPTION_INT("transcode_audio_sample_rate", transcode_options->audio_sample_rate)
    SET_OPTION_INT("transcode_video_frames_per_second", transcode_options->video_frames_per_second)
    SET_OPTION_STR("transcode_rtp_port", transcode_options->s_rtp_port,TRANSCODE_RTP_PORT_MAX)
    SET_OPTION_INT("transcode_keyint_min", transcode_options->keyint_min)
    SET_OPTION_INT("transcode_send_transcoded_only", transcode_options->send_transcoded_only)
    else {
        return 0;
    }
    return 1;
}


/********** Functions to copy transcode options *********************/

#define COPY_OPTION_INT(option_name, struct_source, struct_dest)\
  if(NULL != struct_source->option_name){\
    if (NULL == struct_dest->option_name) {\
      struct_dest->option_name = malloc(sizeof(int));\
    }\
    *(struct_dest->option_name) = *(struct_source->option_name);\
  }


#define COPY_OPTION_FLT(option_name, struct_source, struct_dest)\
  if(NULL != struct_source->option_name){\
    if (NULL == struct_dest->option_name) {\
      struct_dest->option_name = malloc(sizeof(float));\
    }\
    *(struct_dest->option_name) = *(struct_source->option_name);\
  }



#define COPY_OPTION_STR(option_name, struct_source, struct_dest)\
  if(NULL != struct_source->option_name){\
    length = strlen(struct_source->option_name);\
    if (NULL != struct_dest->option_name) {\
      free(struct_dest->option_name);\
    }\
    struct_dest->option_name = malloc(length + 1);\
    strcpy(struct_dest->option_name, struct_source->option_name);\
  }



/******* End of functions to copy transcode options *****************/



void transcode_copy_options(struct transcode_options_t *src, struct transcode_options_t *dst)
{
  int length=0;
  COPY_OPTION_INT(enable,src,dst)
  COPY_OPTION_INT(video_bitrate,src,dst)
  COPY_OPTION_INT(audio_bitrate,src,dst)
  COPY_OPTION_INT(gop,src,dst)
  COPY_OPTION_INT(b_frames,src,dst)
  COPY_OPTION_INT(mbd,src,dst)
  COPY_OPTION_INT(cmp,src,dst)
  COPY_OPTION_INT(subcmp,src,dst)
  COPY_OPTION_STR(video_codec,src,dst)
  COPY_OPTION_STR(audio_codec,src,dst)
  COPY_OPTION_FLT(crf,src,dst)
  COPY_OPTION_INT(refs,src,dst)
  COPY_OPTION_INT(b_strategy,src,dst)
  COPY_OPTION_INT(coder_type,src,dst)
  COPY_OPTION_INT(me_method,src,dst)
  COPY_OPTION_INT(me_range,src,dst)
  COPY_OPTION_INT(subq,src,dst)
  COPY_OPTION_INT(trellis,src,dst)
  COPY_OPTION_INT(sc_threshold,src,dst)
  COPY_OPTION_STR(rc_eq,src,dst)
  COPY_OPTION_FLT(qcomp,src,dst)
  COPY_OPTION_INT(qmin,src,dst)
  COPY_OPTION_INT(qmax,src,dst)
  COPY_OPTION_INT(qdiff,src,dst)
  COPY_OPTION_INT(loop_filter,src,dst)
  COPY_OPTION_INT(mixed_refs,src,dst)
  COPY_OPTION_INT(enable_8x8dct,src,dst)
  COPY_OPTION_INT(x264_partitions,src,dst)
  COPY_OPTION_INT(level,src,dst)
  COPY_OPTION_INT(streaming_type,src,dst)
  COPY_OPTION_STR(sdp_filename,src,dst)
  COPY_OPTION_INT(aac_profile,src,dst)
  COPY_OPTION_INT(aac_latm,src,dst)
  COPY_OPTION_FLT(video_scale,src,dst)
  COPY_OPTION_STR(ffm_url,src,dst)
  COPY_OPTION_INT(audio_channels,src,dst)
  COPY_OPTION_INT(audio_sample_rate,src,dst)
  COPY_OPTION_INT(video_frames_per_second,src,dst)
  COPY_OPTION_STR(s_rtp_port,src,dst)
  COPY_OPTION_INT(keyint_min,src,dst)
  COPY_OPTION_INT(send_transcoded_only,src,dst)
}



void transcode_options_apply_templates(struct transcode_options_t *opt, int card, int tuner, int server, int channel_number)
{
  char c_number[10];
  char c_card[10];
  char c_tuner[10];
  char c_server[10];
  int len;
  sprintf(c_number,"%d",channel_number+1);
  sprintf(c_card,"%d",card);
  sprintf(c_tuner,"%d",tuner);
  sprintf(c_server,"%d",server);
  if(opt->sdp_filename)
  {
    len=strlen(opt->sdp_filename)+1;
    opt->sdp_filename=mumu_string_replace(opt->sdp_filename,&len,1,"%number",c_number);
    opt->sdp_filename=mumu_string_replace(opt->sdp_filename,&len,1,"%card",c_card);
    opt->sdp_filename=mumu_string_replace(opt->sdp_filename,&len,1,"%tuner",c_tuner);
    opt->sdp_filename=mumu_string_replace(opt->sdp_filename,&len,1,"%server",c_server);
    log_message(log_module,MSG_DETAIL,"Channel %d, sdp_filename %s\n",channel_number,opt->sdp_filename);
  }
  if(opt->ffm_url)
  {
    len=strlen(opt->ffm_url)+1;
    opt->ffm_url=mumu_string_replace(opt->ffm_url,&len,1,"%number",c_number);
    opt->ffm_url=mumu_string_replace(opt->ffm_url,&len,1,"%card",c_card);
    opt->ffm_url=mumu_string_replace(opt->ffm_url,&len,1,"%tuner",c_tuner);
    opt->ffm_url=mumu_string_replace(opt->ffm_url,&len,1,"%server",c_server);
    log_message(log_module,MSG_DETAIL,"Channel %d, ffm_url %s\n",channel_number,opt->ffm_url);
  }
  if(opt->s_rtp_port)
  {
    len=strlen(opt->s_rtp_port)+1;
    opt->s_rtp_port=mumu_string_replace(opt->s_rtp_port,&len,1,"%number",c_number);
    opt->s_rtp_port=mumu_string_replace(opt->s_rtp_port,&len,1,"%card",c_card);
    opt->s_rtp_port=mumu_string_replace(opt->s_rtp_port,&len,1,"%tuner",c_tuner);
    opt->s_rtp_port=mumu_string_replace(opt->s_rtp_port,&len,1,"%server",c_server);
    opt->rtp_port=malloc(sizeof(int));
    *opt->rtp_port=string_comput(opt->s_rtp_port);
    log_message(log_module,MSG_DETAIL,"Channel %d, computed RTP port %d\n",channel_number,*opt->rtp_port);
  }

}




