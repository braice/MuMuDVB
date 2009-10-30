#include "transcode.h"
#include "transcode_queues.h"
#include "transcode_avlib.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <libavcodec/avcodec.h>

#define TRANSCODE_QUEUE_SIZE (5 * 1024 * 1024)

void* transcode_thread_routine(void *p)
{
    transcode_thread_data_t *transcode_thread_data = p;

    while (!transcode_thread_data->terminate_thread_flag) {
        
        if (0 >= transcode_thread_data->data_queue.data_size) {
            log_message(MSG_DEBUG, "[Transcode] No data for transcoding.\n");

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
            log_message(MSG_ERROR, "[Transcode] Failed to initialize transcoding.\n");
            
            /* Sleep 100 times for 0.1 sec with checking transcode_thread_data->terminate_thread_flag */
            int i;
            for (i = 0; i < 100 && !transcode_thread_data->terminate_thread_flag; i++) {
                usleep(100000);
            }

            continue;
        }

        log_message(MSG_INFO, "[Transcode] Transcoding sarted.\n");
        transcode_thread_data->is_initialized = 1;
        
        /* Transcode */
        transcode(transcode_handle, transcode_thread_data);

        transcode_thread_data->is_initialized = 0;        
        log_message(MSG_INFO, "[Transcode] Transcoding finished.\n");
 
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
        log_message(MSG_ERROR, "[Transcode] pthread_mutex_init failed.\n");
        free(transcode_thread_data);
        return NULL;
    }

    if (pthread_create(&transcode_thread_data->thread, NULL,
            transcode_thread_routine, transcode_thread_data) != 0) {
        /* Thread initialization failed */
        log_message(MSG_ERROR, "[Transcode] pthread_create failed.\n");
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
        log_message(MSG_ERROR, "[Transcode] Failed to enqueue data.\n");
    }
    else if (0 == result) {
        if (transcode_thread_data->is_initialized) {
            log_message(MSG_INFO, "[Transcode] Data queue is full.\n");
        }
    }

    pthread_mutex_unlock(&transcode_thread_data->queue_mutex);

    return result;
}

#define SET_OPTION_INT(config_option_name, struct_option_name)\
else if (!strcmp(*substring, config_option_name)) {\
    if (0 == ip_ok) {\
        log_message( MSG_ERROR,\
            config_option_name" : You must precise ip first\n");\
        exit(ERROR_CONF);\
    }\
    if (NULL == channel->struct_option_name) {\
        channel->struct_option_name = malloc(sizeof(int));\
    }\
    *(channel->struct_option_name) = atoi(strtok(NULL, delimiteurs));\
}

#define SET_OPTION_FLT(config_option_name, struct_option_name)\
else if (!strcmp(*substring, config_option_name)) {\
    if (0 == ip_ok) {\
        log_message( MSG_ERROR,\
            config_option_name" : You must precise ip first\n");\
        exit(ERROR_CONF);\
    }\
    if (NULL == channel->struct_option_name) {\
        channel->struct_option_name = malloc(sizeof(float));\
    }\
    *(channel->struct_option_name) = atof(strtok(NULL, delimiteurs));\
}

#define SET_OPTION_STR(config_option_name, struct_option_name, max_length)\
else if (!strcmp(*substring, config_option_name)) {\
    if (0 == ip_ok) {\
        log_message( MSG_ERROR,\
            config_option_name" : You must precise ip first\n");\
        exit(ERROR_CONF);\
    }\
    *substring = strtok(NULL, delimiteurs);\
    int length = strlen(*substring);\
    if (length <= max_length) {\
        if (NULL != channel->struct_option_name) {\
            free(channel->struct_option_name);\
        }\
        channel->struct_option_name = malloc(length + 1);\
        strcpy(channel->struct_option_name, *substring);\
        trim(channel->struct_option_name);\
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

int transcode_read_option(mumudvb_channel_t *channel, int ip_ok, char *delimiteurs, char **substring)
{
    if (!strcmp(*substring, "transcode_streaming_type")) {
        if (0 == ip_ok) {
            log_message( MSG_ERROR,
                "transcode_streaming_type : You must precise ip first\n");
            exit(ERROR_CONF);
        }

        *substring = strtok(NULL, delimiteurs);

        if (strlen(*substring) <= TRANSCODE_STREAMING_TYPE_MAX) {
            transcode_options_t *transcode_options = &channel->transcode_options;

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
        if (0 == ip_ok) {
            log_message( MSG_ERROR,
                "transcode_x264_profile : You must precise ip first\n");
            exit(ERROR_CONF);
        }

        *substring = strtok(NULL, delimiteurs);

        if (strlen(*substring) <= TRANSCODE_PROFILE_MAX) {
            transcode_options_t *transcode_options = &channel->transcode_options;
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
        if (0 == ip_ok) {
            log_message( MSG_ERROR,
                "transcode_aac_profile : You must precise ip first\n");
            exit(ERROR_CONF);
        }

        *substring = strtok(NULL, delimiteurs);

        if (strlen(*substring) <= TRANSCODE_AAC_PROFILE_MAX) {
            transcode_options_t *transcode_options = &channel->transcode_options;

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

    SET_OPTION_INT("transcode_enable", transcode_options.enable)
    SET_OPTION_INT("transcode_video_bitrate", transcode_options.video_bitrate)
    SET_OPTION_INT("transcode_audio_bitrate", transcode_options.audio_bitrate)
    SET_OPTION_INT("transcode_gop", transcode_options.gop)
    SET_OPTION_INT("transcode_b_frames", transcode_options.b_frames)
    SET_OPTION_INT("transcode_mbd", transcode_options.mbd)
    SET_OPTION_INT("transcode_cmp", transcode_options.cmp)
    SET_OPTION_INT("transcode_subcmp", transcode_options.subcmp)
    SET_OPTION_STR("transcode_video_codec", transcode_options.video_codec, TRANSCODE_CODEC_MAX)
    SET_OPTION_STR("transcode_audio_codec", transcode_options.audio_codec, TRANSCODE_CODEC_MAX)
    SET_OPTION_FLT("transcode_crf", transcode_options.crf)
    SET_OPTION_INT("transcode_refs", transcode_options.refs)
    SET_OPTION_INT("transcode_b_strategy", transcode_options.b_strategy)
    SET_OPTION_INT("transcode_coder_type", transcode_options.coder_type)
    SET_OPTION_INT("transcode_me_method", transcode_options.me_method)
    SET_OPTION_INT("transcode_me_range", transcode_options.me_range)
    SET_OPTION_INT("transcode_subq", transcode_options.subq)
    SET_OPTION_INT("transcode_trellis", transcode_options.trellis)
    SET_OPTION_INT("transcode_sc_threshold", transcode_options.sc_threshold)
    SET_OPTION_STR("transcode_rc_eq", transcode_options.rc_eq, TRANSCODE_RC_EQ_MAX)
    SET_OPTION_FLT("transcode_qcomp", transcode_options.qcomp)
    SET_OPTION_INT("transcode_qmin", transcode_options.qmin)
    SET_OPTION_INT("transcode_qmax", transcode_options.qmax)
    SET_OPTION_INT("transcode_qdiff", transcode_options.qdiff)
    SET_OPTION_INT("transcode_loop_filter", transcode_options.loop_filter)
    SET_OPTION_INT("transcode_mixed_refs", transcode_options.mixed_refs)
    SET_OPTION_INT("transcode_enable_8x8dct", transcode_options.enable_8x8dct)
    SET_OPTION_INT("transcode_x264_partitions", transcode_options.x264_partitions)
    SET_OPTION_INT("transcode_level", transcode_options.level)
    SET_OPTION_STR("transcode_sdp_filename", transcode_options.sdp_filename, TRANSCODE_SDP_FILENAME_MAX)
    SET_OPTION_FLT("transcode_video_scale", transcode_options.video_scale)
    SET_OPTION_INT("transcode_aac_latm", transcode_options.aac_latm)
    SET_OPTION_STR("transcode_ffm_url", transcode_options.ffm_url, TRANSCODE_FFM_URL_MAX)
    SET_OPTION_INT("transcode_audio_channels", transcode_options.audio_channels)
    SET_OPTION_INT("transcode_audio_sample_rate", transcode_options.audio_sample_rate)
    SET_OPTION_INT("transcode_video_frames_per_second", transcode_options.video_frames_per_second)
    SET_OPTION_INT("transcode_rtp_port", transcode_options.rtp_port)
    SET_OPTION_INT("transcode_keyint_min", transcode_options.keyint_min)

    else {
        return 0;
    }
    
    return 1;
}
