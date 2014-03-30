
#ifndef _MUMUDVB_MON_H
#define _MUMUDVB_MON_H
#include "mumudvb.h"
#include "tune.h"
#include "network.h"
#include "dvb.h"
#ifdef ENABLE_CAM_SUPPORT
#include "cam.h"
#endif
#ifdef ENABLE_SCAM_SUPPORT
#include "scam_capmt.h"
#include "scam_common.h"
#include "scam_getcw.h"
#include "scam_decsa.h"
#endif
#include "ts.h"
#include "errors.h"
#include "autoconf.h"
#include "sap.h"
#include "rewrite.h"
#include "unicast_http.h"
#include "rtp.h"
#include "log.h"

void *monitor_func(void* arg);
int mumudvb_close(int no_daemon,
		monitor_parameters_t *monitor_thread_params,
		rewrite_parameters_t *rewrite_vars,
		auto_p_t *auto_p,
		unicast_parameters_t *unicast_vars,
		volatile int *strengththreadshutdown,
		void *cam_p_v,
		void *scam_vars_v,
		char *filename_channels_not_streamed,
		char *filename_channels_streamed,
		char *filename_pid,
		int Interrupted,
		mumu_chan_p_t *chan_p,
		pthread_t *signalpowerthread,
		pthread_t *monitorthread,
		card_thread_parameters_t *cardthreadparams,
		fds_t *fds);


void parse_cmd_line(int argc, char **argv,
		char **conf_filename,
		tune_p_t *tune_p,
		stats_infos_t *stats_infos,
		int *server_id,
		int *no_daemon,
		char **dump_filename,
		int *listingcards);











#endif
