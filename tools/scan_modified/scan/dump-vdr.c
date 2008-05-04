#include <stdio.h>
#include "dump-vdr.h"
#include <linux/dvb/frontend.h>


static const char *inv_name [] = {
	"0",
	"1",
	"999"
};

static const char *fec_name [] = {
	"0",
	"12",
	"23",
	"34",
	"45",
	"56",
	"67",
	"78",
	"89",
	"999"
};

static const char *qam_name [] = {
	"0",
	"16",
	"32",
	"64",
	"128",
	"256",
	"999"
};


static const char *bw_name [] = {
	"8",
	"7",
	"6",
	"999"
};


static const char *mode_name [] = {
	"2",
	"8",
	"999"
};

static const char *guard_name [] = {
	"32",
	"16",
	"8",
	"4",
	"999"
};


static const char *hierarchy_name [] = {
	"0",
	"1",
	"2",
	"4",
	"999"
};

static const char *west_east_flag_name [] = {
	"W",
	"E"
};

void vdr_dump_dvb_parameters (FILE *f, fe_type_t type,
		struct dvb_frontend_parameters *p,
		char polarity, int orbital_pos, int we_flag)
{
	switch (type) {
	case FE_QPSK:
		fprintf (f, "%i:", p->frequency / 1000);
		fprintf (f, "%c:", polarity);
		fprintf (f, "S%i.%i%s:", orbital_pos/10,
			 orbital_pos % 10, west_east_flag_name[we_flag]);
		fprintf (f, "%i:", p->u.qpsk.symbol_rate / 1000);
		break;

	case FE_QAM:
		fprintf (f, "%i:", p->frequency / 1000000);
		fprintf (f, "M%s:C:", qam_name[p->u.qam.modulation]);
		fprintf (f, "%i:", p->u.qam.symbol_rate / 1000);
		break;

	case FE_OFDM:
		fprintf (f, "%i:", p->frequency / 1000);
		fprintf (f, "I%s", inv_name[p->inversion]);
		fprintf (f, "B%s", bw_name[p->u.ofdm.bandwidth]);
		fprintf (f, "C%s", fec_name[p->u.ofdm.code_rate_HP]);
		fprintf (f, "D%s", fec_name[p->u.ofdm.code_rate_LP]);
		fprintf (f, "M%s", qam_name[p->u.ofdm.constellation]);
		fprintf (f, "T%s", mode_name[p->u.ofdm.transmission_mode]);
		fprintf (f, "G%s", guard_name[p->u.ofdm.guard_interval]);
		fprintf (f, "Y%s", hierarchy_name[p->u.ofdm.hierarchy_information]);
		fprintf (f, ":T:27500:");
		break;

	case FE_ATSC:
		fprintf (f, "%i:", p->frequency / 1000);
		fprintf (f, "VDR does not support ATSC at this time");
		break;

	default:
		;
	};
}

void vdr_dump_service_parameter_set (FILE *f,
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
				 int dump_provider,
				 int ca_select,
				 int vdr_version,
				 int dump_channum,
				 int channel_num)
{
        int i;

	if ((video_pid || audio_pid[0]) && ((ca_select > 0) || ((ca_select == 0) && (scrambled == 0)))) {
		if (vdr_version <= 2) {
			audio_lang = NULL;
			network_id = 0;
			transport_stream_id = 0;
		}
		if ((dump_channum == 1) && (channel_num > 0))
			fprintf(f, ":@%i\n", channel_num);
		if (vdr_version >= 3)
			fprintf (f, "%s;%s:", service_name, provider_name);
		else
		  {
		    if (dump_provider == 1)
		      fprintf (f, "%s - ", provider_name);
		    fprintf (f, "%s:", service_name);
		  }
		vdr_dump_dvb_parameters (f, type, p, polarity, orbital_pos, we_flag);
		if ((pcr_pid != video_pid) && (video_pid > 0))
			fprintf (f, "%i+%i:", video_pid, pcr_pid);
		else
			fprintf (f, "%i:", video_pid);
		fprintf (f, "%i", audio_pid[0]);
		if (audio_lang && audio_lang[0][0])
			fprintf (f, "=%.4s", audio_lang[0]);
	        for (i = 1; i < audio_num; i++)
	        {
			fprintf (f, ",%i", audio_pid[i]);
			if (audio_lang && audio_lang[i][0])
				fprintf (f, "=%.4s", audio_lang[i]);
		}
		if (ac3_pid)
	        {
			fprintf (f, ";%i", ac3_pid);
			if (audio_lang && audio_lang[0][0])
				fprintf (f, "=%.4s", audio_lang[0]);
 		}
		if (scrambled == 1) scrambled = ca_select;
		fprintf (f, ":%d:%d:%d:%d:%d:0", teletext_pid, scrambled,
				service_id, network_id, transport_stream_id);
		fprintf (f, "\n");
	}
}

