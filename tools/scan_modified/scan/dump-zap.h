#ifndef __DUMP_ZAP_H__
#define __DUMP_ZAP_H__

#include <stdint.h>
#include <linux/dvb/frontend.h>

extern void zap_dump_dvb_parameters (FILE *f, fe_type_t type,
		struct dvb_frontend_parameters *t, char polarity, int sat);

extern void zap_dump_service_parameter_set (FILE *f,
				 const char *service_name,
				 fe_type_t type,
				 struct dvb_frontend_parameters *t,
				 char polarity, int sat,
				 uint16_t video_pid,
				 uint16_t *audio_pid,
				 uint16_t service_id);

#endif

