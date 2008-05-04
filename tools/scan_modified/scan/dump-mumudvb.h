#ifndef __DUMP_MUMUDVB_H__
#define __DUMP_MUMUDVB_H__

#include <stdint.h>
#include <linux/dvb/frontend.h>

extern
void mumudvb_dump_dvb_parameters (FILE *f, fe_type_t type,
		struct dvb_frontend_parameters *p,
		char polarity, int orbital_pos, int we_flag);

extern
void mumudvb_dump_service_parameter_set (FILE *f,
				 const char *service_name,
				 const char *provider_name,
				 fe_type_t type,
				 struct dvb_frontend_parameters *p,
				 char polarity,
				 int video_pid,
				 int pcr_pid,
				 uint16_t *audio_pid,
				 char audio_lang[][4],
                                 int audio_num,
				 int teletext_pid,
				 int scrambled,
				 int ac3_pid,
                                 int service_id,
				 int network_id,
				 int transport_stream_id,
				 int orbital_pos,
				 int we_flag,
				 int ca_select,
				 int channel_num,
			         int channel_num_mumudvb,
				 int pmt_pid,
				 int subtitling_pid);

#endif

