#include <stdio.h>
#include "dump-mumudvb.h"
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

void mumudvb_dump_dvb_parameters (FILE *f, fe_type_t type,
		struct dvb_frontend_parameters *p,
		char polarity, int orbital_pos, int we_flag)
{

	switch (type) {
	case FE_QPSK:
		fprintf (f, "#QPSK Modulation : DVB-S. Satellite : S%i.%i%s\n", orbital_pos/10,
			 orbital_pos % 10, west_east_flag_name[we_flag]);
		fprintf (f, "freq=%i\n", p->frequency / 1000);
		fprintf (f, "pol=%c\n", polarity);
		fprintf (f, "srate=%i\n", p->u.qpsk.symbol_rate / 1000);
		break;

	case FE_QAM:
	  fprintf (f, "#QAM Modulation : Probably DVB-C ---------- Not Tested ----------- \n");
		fprintf (f, "%i:", p->frequency / 1000000);
		fprintf (f, "M%s:C:", qam_name[p->u.qam.modulation]);
		fprintf (f, "%i:", p->u.qam.symbol_rate / 1000);
		break;

	case FE_OFDM:
	  fprintf (f, "#OFDM Modulation : DVB-T\n");
	  fprintf (f, "#QAM : %s ", qam_name[p->u.ofdm.constellation]);
	  fprintf (f, "Inversion : %s ", inv_name[p->inversion]);
	  fprintf (f, "Coderate HP : %s ", fec_name[p->u.ofdm.code_rate_HP]);
	  fprintf (f, "Coderate LP : %s ", fec_name[p->u.ofdm.code_rate_LP]);
	  fprintf (f, "Guard interval : %s ", guard_name[p->u.ofdm.guard_interval]);
	  fprintf (f, "Transmission mode : %s ", mode_name[p->u.ofdm.transmission_mode]);
	  fprintf (f, "Hierarchy : %s ", hierarchy_name[p->u.ofdm.hierarchy_information]);
	  fprintf (f, "\n");
	  if(p->frequency>=0xfffffff)
	    fprintf (f, "#==========WARNING=============\n#The network provider probably returned the wrong frequency\n#You have to set manually the freq= option\n\n");
	  else
	    fprintf (f, "freq=%i\n", p->frequency );
	  fprintf (f, "bandwith=%sMhz\n", bw_name[p->u.ofdm.bandwidth]);
	  fprintf (f, "qam=auto\n");
	  fprintf (f, "trans_mode=auto\n");
	  fprintf (f, "guardinterval=auto\n");
	  fprintf (f, "coderate=auto\n");
		//		fprintf (f, ":T:27500:");
		break;

	case FE_ATSC:
		fprintf (f, "%i:", p->frequency / 1000);
		fprintf (f, "VDR does not support ATSC at this time");
		break;

	default:
		fprintf (f, "Modulation : %d\n", type);
		;
	};

}

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
				 int subtitling_pid)
{
        int i;

	if (channel_num_mumudvb == 0)
	  {
	    fprintf (f, "#This is an automatically generated config file for mumudvb\n#Check if the ip adresses are good for you\n#You might also have to ad the card=n parameter with n the number of your DVB adapter\n\n");
	    mumudvb_dump_dvb_parameters (f, type, p, polarity, orbital_pos, we_flag);
	  }
	if ((video_pid || audio_pid[0]) && ((ca_select > 0) || ((ca_select == 0) && (scrambled == 0)))) {
	  fprintf (f, "\n#Channel : \"%s\" Provider : \"%s\" Number : %d\n", service_name, provider_name, channel_num);
	  fprintf (f, "ip=239.200.200.2%02i\n",channel_num_mumudvb);
	  fprintf (f, "port=1234\n");
	  fprintf (f, "name=");
	  if (audio_lang && audio_lang[0][0])
		fprintf (f, "%.4s ", audio_lang[0]);
	  fprintf (f, "%s\n", service_name);

	  fprintf (f, "#Pids are the following : PMT ");
	  if ((pcr_pid != video_pid) && (video_pid > 0)) fprintf (f, "Video PCR ");
	  else if (video_pid > 0) fprintf (f, "Video ");
	  for (i = 0; i < audio_num; i++)
	    {
	      if(audio_pid[i]) fprintf (f, "Audio ");
	    }
	  if (ac3_pid) fprintf (f, "AC3 ");
	  if (teletext_pid) fprintf (f, "Text ");
	  if (subtitling_pid) fprintf (f, "SUB ");
	  fprintf (f, "\n");
	  fprintf (f, "pids=");
	  fprintf (f, "%i ", pmt_pid);
	  if ((pcr_pid != video_pid) && (video_pid > 0))
	    fprintf (f, "%i %i", video_pid, pcr_pid);
	  else if (video_pid > 0)
	    fprintf (f, "%i", video_pid);
	  if(audio_pid[0])
	    fprintf (f, " %i", audio_pid[0]);
	  for (i = 1; i < audio_num; i++)
	    {
	      if(audio_pid[i])
		fprintf (f, " %i", audio_pid[i]);
	    }
	  if (ac3_pid)
	    {
	      fprintf (f, " %i", ac3_pid);
	    }
	  if (scrambled == 1) scrambled = ca_select;
	  if (teletext_pid)
	    fprintf (f, " %d", teletext_pid);
	  if (subtitling_pid)
	    fprintf (f, " %d", subtitling_pid);
	  //	    fprintf (f, ":%d:%d:%d:%d:%d:0", teletext_pid, scrambled,
	  //		   service_id, network_id, transport_stream_id);
	  fprintf (f, "\n");
	}
}

