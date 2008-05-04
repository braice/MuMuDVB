#include <stdio.h>
#include <linux/dvb/frontend.h>
#include "dump-zap.h"

static const char *inv_name [] = {
	"INVERSION_OFF",
	"INVERSION_ON",
	"INVERSION_AUTO"
};

static const char *fec_name [] = {
	"FEC_NONE",
	"FEC_1_2",
	"FEC_2_3",
	"FEC_3_4",
	"FEC_4_5",
	"FEC_5_6",
	"FEC_6_7",
	"FEC_7_8",
	"FEC_8_9",
	"FEC_AUTO"
};


static const char *qam_name [] = {
	"QPSK",
	"QAM_16",
	"QAM_32",
	"QAM_64",
	"QAM_128",
	"QAM_256",
	"QAM_AUTO",
	"8VSB",
	"16VSB",
};


static const char *bw_name [] = {
	"BANDWIDTH_8_MHZ",
	"BANDWIDTH_7_MHZ",
	"BANDWIDTH_6_MHZ",
	"BANDWIDTH_AUTO"
};


static const char *mode_name [] = {
	"TRANSMISSION_MODE_2K",
	"TRANSMISSION_MODE_8K",
	"TRANSMISSION_MODE_AUTO"
};

static const char *guard_name [] = {
	"GUARD_INTERVAL_1_32",
	"GUARD_INTERVAL_1_16",
	"GUARD_INTERVAL_1_8",
	"GUARD_INTERVAL_1_4",
	"GUARD_INTERVAL_AUTO"
};


static const char *hierarchy_name [] = {
	"HIERARCHY_NONE",
	"HIERARCHY_1",
	"HIERARCHY_2",
	"HIERARCHY_4",
	"HIERARCHY_AUTO"
};


void zap_dump_dvb_parameters (FILE *f, fe_type_t type, struct dvb_frontend_parameters *p, char polarity, int sat_number)
{
	switch (type) {
	case FE_QPSK:
		fprintf (f, "%i:", p->frequency / 1000);	/* channels.conf wants MHz */
		fprintf (f, "%c:", polarity);
		fprintf (f, "%d:", sat_number);
		fprintf (f, "%i", p->u.qpsk.symbol_rate / 1000); /* channels.conf wants kBaud */
		/*fprintf (f, "%s", fec_name[p->u.qpsk.fec_inner]);*/
		break;

	case FE_QAM:
		fprintf (f, "%i:", p->frequency);
		fprintf (f, "%s:", inv_name[p->inversion]);
		fprintf (f, "%i:", p->u.qpsk.symbol_rate);
		fprintf (f, "%s:", fec_name[p->u.qpsk.fec_inner]);
		fprintf (f, "%s", qam_name[p->u.qam.modulation]);
		break;

	case FE_OFDM:
		fprintf (f, "%i:", p->frequency);
		fprintf (f, "%s:", inv_name[p->inversion]);
		fprintf (f, "%s:", bw_name[p->u.ofdm.bandwidth]);
		fprintf (f, "%s:", fec_name[p->u.ofdm.code_rate_HP]);
		fprintf (f, "%s:", fec_name[p->u.ofdm.code_rate_LP]);
		fprintf (f, "%s:", qam_name[p->u.ofdm.constellation]);
		fprintf (f, "%s:", mode_name[p->u.ofdm.transmission_mode]);
		fprintf (f, "%s:", guard_name[p->u.ofdm.guard_interval]);
		fprintf (f, "%s", hierarchy_name[p->u.ofdm.hierarchy_information]);
		break;

	case FE_ATSC:
		fprintf (f, "%i:", p->frequency);
		fprintf (f, "%s", qam_name[p->u.vsb.modulation]);
		break;

	default:
		;
	};
}

void zap_dump_service_parameter_set (FILE *f,
				 const char *service_name,
				 fe_type_t type,
				 struct dvb_frontend_parameters *p,
				 char polarity,
				 int sat_number,
				 uint16_t video_pid,
				 uint16_t *audio_pid,
				 uint16_t service_id)
{
	fprintf (f, "%s:", service_name);
	zap_dump_dvb_parameters (f, type, p, polarity, sat_number);
	fprintf (f, ":%i:%i:%i", video_pid, audio_pid[0], service_id);
	fprintf (f, "\n");
}

