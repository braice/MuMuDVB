/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) 2008-2010 Brice DUBOST <mumudvb@braice.net>
 *
 * Parts of this code come from libdvb, modified for mumudvb
 * by Brice DUBOST 
 * Libdvb part : Copyright (C) 2000 Klaus Schmidinger
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
 */

/** @file
 *  @brief This file contain the code related to the PMT reading for autoconfiguration
 *
 */


extern int Interrupted;
static char *log_module="Autoconf: ";

#include <errno.h>
#include <string.h>

#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"
#include "dvb.h"

mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id);
mumudvb_service_t *autoconf_find_service_for_modify(mumudvb_service_t *services,int service_id);
int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len, int *pos);
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len);
void autoconf_free_services(mumudvb_service_t *services);
int autoconf_read_psip(autoconf_parameters_t *parameters);


/****************************************************************************/
//Parts of this code (read of the pmt and read of the pat)
// from libdvb, strongly modified, with commentaries added
/****************************************************************************/

/** @brief Reads the program map table
 *
 * It's used to get the differents "useful" pids of the channel
 * @param pmt the pmt packet
 * @param channel the associated channel
 */
int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel, char *card_base_path, int tuner, uint8_t *asked_pid, uint8_t *number_chan_asked_pid,fds_t *fds)
{
  int section_len, descr_section_len, i,j;
  int pid,pcr_pid;
  int pid_type;
  int found=0;
  pmt_t *header;
  pmt_info_t *descr_header;

  int program_info_length;
  int channel_update;

  //For channel update
  int temp_pids[MAX_PIDS_PAR_CHAINE];
  int temp_pids_type[MAX_PIDS_PAR_CHAINE];
  char temp_pids_language[MAX_PIDS_PAR_CHAINE][4];
  //For channel update
  int temp_num_pids=0;

  pid_type=0;

  section_len=pmt->len;
  header=(pmt_t *)pmt->packet;

  if(header->table_id!=0x02)
  {
    log_message( log_module,  MSG_INFO,"Packet PID %d for channel \"%s\" is not a PMT PID. We remove the pmt pid for this channel\n", pmt->pid, channel->name);
    channel->pmt_pid=0; /** @todo : put a threshold, */
    return 1;
  }

  //We check if this PMT belongs to the current channel. (Only works with autoconfiguration full for the moment because it stores the service_id)
  if(channel->service_id && (channel->service_id != HILO(header->program_number)) )
  {
    log_message( log_module,  MSG_DETAIL,"The PMT %d does not belongs to channel \"%s\"\n", pmt->pid, channel->name);
    return 1;
  }

  /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
  if(header->current_next_indicator == 0)
  {
    log_message( log_module, MSG_DEBUG,"The current_next_indicator is set to 0, this PMT is not valid for the current stream\n");
    return 1;
  }


  log_message( log_module,  MSG_DEBUG,"PMT (PID %d) read for autoconfiguration of channel \"%s\"\n", pmt->pid, channel->name);

  channel_update=channel->num_pids>1?1:0;
  if(channel_update)
  {
    // Update
    log_message( log_module,  MSG_INFO,"Channel %s update\n",channel->name);
    temp_pids[0]=pmt->pid;
    temp_num_pids++;
#ifdef ENABLE_CAM_SUPPORT
    // Reset of the CA SYS saved for the chanel
    for (i=0; i<32; i++)
      channel->ca_sys_id[i]=0;
#endif
  }

  program_info_length=HILO(header->program_info_length); //program_info_length
  char language[4]="";
  int pos=0;

  //we read the different descriptors included in the pmt
  //for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818 table 2-34
  for (i=program_info_length+PMT_LEN; i<=section_len-(PMT_INFO_LEN+4); i+=descr_section_len+PMT_INFO_LEN)
  {
    //we parse the part after the descriptors
    //we map the descriptor header
    descr_header=(pmt_info_t *)(pmt->packet+i);
    //We get the length of the descriptor
    descr_section_len=HILO(descr_header->ES_info_length);        //ES_info_length

    // Default language value if not found
    snprintf(language,4,"%s","---");
    // Default, no position found
    pos=0;

    pid=HILO(descr_header->elementary_PID);
    //Depending of the stream type we'll take or not this pid
    switch(descr_header->stream_type)
    {
      case 0x01:
        pid_type=PID_VIDEO_MPEG1;
        log_message( log_module,  MSG_DEBUG,"  Video MPEG1 \tpid %d\n",pid);
        break;
      case 0x02:
        pid_type=PID_VIDEO_MPEG2;
        log_message( log_module,  MSG_DEBUG,"  Video MPEG2 \tpid %d\n",pid);
        break;
      case 0x10: /* ISO/IEC 14496-2 Visual - MPEG4 video */
        pid_type=PID_VIDEO_MPEG4_ASP;
        log_message( log_module,  MSG_DEBUG,"  Video MPEG4-ASP \tpid %d\n",pid);
        break;
      case 0x1b: /* AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video */
        pid_type=PID_VIDEO_MPEG4_AVC;
        log_message( log_module,  MSG_DEBUG,"  Video MPEG4-AVC \tpid %d\n",pid);
        break;
      case 0x03:
        pid_type=PID_AUDIO_MPEG1;
        log_message( log_module,  MSG_DEBUG,"  Audio MPEG1 \tpid %d\n",pid);
        break;
      case 0x04:
        pid_type=PID_AUDIO_MPEG2;
        log_message( log_module,  MSG_DEBUG,"  Audio MPEG2 \tpid %d\n",pid);
        break;
      case 0x11: /* ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 */
        pid_type=PID_AUDIO_AAC_LATM;
        log_message( log_module,  MSG_DEBUG,"  Audio AAC-LATM \tpid %d\n",pid);
        break;
      case 0x0f: /* ISO/IEC 13818-7 Audio with ADTS transport syntax - usually AAC */
        pid_type=PID_AUDIO_AAC_ADTS;
        log_message( log_module,  MSG_DEBUG,"  Audio AAC-ADTS \tpid %d\n",pid);
        break;
      case 0x81: /* Audio per ATSC A/53B [2] Annex B */
        pid_type=PID_AUDIO_ATSC;
        log_message( log_module,  MSG_DEBUG,"  Audio ATSC A/53B \tpid %d\n",pid);
        break;



      case 0x06: /* Descriptor defined in EN 300 468 */
        if(descr_section_len) //If we have an accociated descriptor, we'll search inforation in it
        {
          if(pmt_find_descriptor(0x45,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  VBI Data \tpid %d\n",pid);
            pid_type=PID_EXTRA_VBIDATA;
          }else if(pmt_find_descriptor(0x46,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  VBI Teletext \tpid %d\n",pid);
            pid_type=PID_EXTRA_VBITELETEXT;
          }else if(pmt_find_descriptor(0x56,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  Teletext \tpid %d\n",pid);
            pid_type=PID_EXTRA_TELETEXT;
          }else if(pmt_find_descriptor(0x59,pmt->packet+i+PMT_INFO_LEN,descr_section_len, &pos)){
            log_message( log_module,  MSG_DEBUG,"  Subtitling \tpid %d\n",pid);
            pid_type=PID_EXTRA_SUBTITLE;
            char * lng=(char *)(pmt->packet+i+PMT_INFO_LEN+pos+2);
            language[0]=lng[0];
            language[1]=lng[1];
            language[2]=lng[2];
            language[3]=0;
          }else if(pmt_find_descriptor(0x6a,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  AC3 (audio) \tpid %d\n",pid);
            pid_type=PID_AUDIO_AC3;
          }else if(pmt_find_descriptor(0x7a,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  Enhanced AC3 (audio) \tpid %d\n",pid);
            pid_type=PID_AUDIO_EAC3;
          }else if(pmt_find_descriptor(0x7b,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  DTS (audio) \tpid %d\n",pid);
            pid_type=PID_AUDIO_DTS;
          }else if(pmt_find_descriptor(0x7c,pmt->packet+i+PMT_INFO_LEN,descr_section_len, NULL)){
            log_message( log_module,  MSG_DEBUG,"  AAC (audio) \tpid %d\n",pid);
            pid_type=PID_AUDIO_AAC;
          }else
          {
            log_message( log_module,  MSG_DEBUG,"Unknown descriptor see EN 300 468 v1.9.1 table 12, pid %d descriptor tags : ", pid);
            pmt_print_descriptor_tags(pmt->packet+i+PMT_INFO_LEN,descr_section_len);
            log_message( log_module,  MSG_DEBUG,"\n");
            continue;
          }
        }
        else
        {
          log_message( log_module,  MSG_DEBUG,"PMT read : stream type 0x06 without descriptor\n");
          continue;
        }
        break;

      //Now, the list of what we drop
      case 0x05:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : 0x05, ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections \n",pid);
        continue;
      //Digital Storage Medium Command and Control (DSM-CC) cf H.222.0 | ISO/IEC 13818-1 annex B
      case 0x0a:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : 0x0A ISO/IEC 13818-6 type A (DSM-CC)\n",pid);
        continue;
      case 0x0b:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : 0x0B ISO/IEC 13818-6 type B (DSM-CC)\n",pid);
        continue;
      case 0x0c:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : 0x0C ISO/IEC 13818-6 type C (DSM-CC)\n",pid);
        continue;
      case 0x0D:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : ISO/IEC 13818-6 type D",pid);
        continue;
      case 0x0E:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary",pid);
        continue;
      case 0x12:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets",pid);
        continue;
      case 0x13:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections",pid);
        continue;
      case 0x14:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : ISO/IEC 13818-6 Synchronized Download Protocol",pid);
        continue;
      case 0x15:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : Metadata carried in PES packets",pid);
        continue;
      case 0x16:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : Metadata carried in metadata_sections",pid);
        continue;
      case 0x17:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : Metadata carried in ISO/IEC 13818-6 Data Carousel",pid);
        continue;
      case 0x18:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : Metadata carried in ISO/IEC 13818-6 Object Carousel",pid);
        continue;
      case 0x19:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol",pid);
        continue;
      case 0x1A:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)",pid);
        continue;
      case 0x7F:
        log_message( log_module,  MSG_DEBUG, "Dropped pid %d, type : IPMP stream",pid);
        continue;
      default:
        if(descr_header->stream_type >= 0x1C && descr_header->stream_type <= 0x7E)
          log_message( log_module,  MSG_DEBUG, "Dropped pid %d, stream type : 0x%02x : ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved",pid,descr_header->stream_type);
        else if(descr_header->stream_type >= 0x80)
          log_message( log_module,  MSG_DEBUG, "Dropped pid %d, stream type : 0x%02x : User Private",pid,descr_header->stream_type);
        else
          log_message( log_module,  MSG_INFO, "!!!!Unknown stream type : 0x%02x, PID : %d cf ITU-T Rec. H.222.0 | ISO/IEC 13818\n",descr_header->stream_type,pid);
        continue;
    }

    //We keep this pid

    // We try to find a 0x0a (ISO639) descriptor to have language information about the stream
    pos=0;
    if(pmt_find_descriptor(0x0a,pmt->packet+i+PMT_INFO_LEN,descr_section_len, &pos)){
      char * lng=(char *)(pmt->packet+i+PMT_INFO_LEN+pos+2);
      language[0]=lng[0];
      language[1]=lng[1];
      language[2]=lng[2];
      language[3]=0;
    }
    log_message( log_module,  MSG_DEBUG,"  PID Language Code = %s\n",language);

    //For cam debugging purposes, we look if we can find a ca descriptor to display ca system ids
    if(descr_section_len)
    {
      int pos;
      int casysid;
      pos=0;
      while(pmt_find_descriptor(0x09,pmt->packet+i+PMT_INFO_LEN,descr_section_len,&pos))
      {
        descr_ca_t *ca_descriptor;
        ca_descriptor=(descr_ca_t *)(pmt->packet+i+PMT_INFO_LEN+pos);
        casysid=0;
        while(channel->ca_sys_id[casysid] && channel->ca_sys_id[casysid]!=HILO(ca_descriptor->CA_type)&& casysid<32 )
          casysid++;
        if(!channel->ca_sys_id[casysid])
        {
          channel->ca_sys_id[casysid]=HILO(ca_descriptor->CA_type);
	  log_message( log_module,  MSG_DETAIL,"Ca system id 0x%04x : %s\n", HILO(ca_descriptor->CA_type), ca_sys_id_to_str(HILO(ca_descriptor->CA_type)));//we display it with the description
        }
        pos+=ca_descriptor->descriptor_length+2;
      }
    }

    if(channel_update)
    {
      temp_pids[temp_num_pids]=pid;
      temp_pids_type[temp_num_pids]=pid_type;
      snprintf(temp_pids_language[temp_num_pids],4,"%s",language);
      temp_num_pids++;
    }
    else
    {
      channel->pids[channel->num_pids]=pid;
      channel->pids_type[channel->num_pids]=pid_type;
      snprintf(channel->pids_language[channel->num_pids],4,"%s",language);
      channel->num_pids++;
    }
  }

  /**************************
  * PCR PID
  **************************/

  pcr_pid=HILO(header->PCR_PID); //The PCR pid.
  //we check if it's not already included (ie the pcr is carried with the video)
  found=0;
  for(i=0;i<channel->num_pids;i++)
  {
    if((channel_update && temp_pids[i]==pcr_pid) || (!channel_update && channel->pids[i]==pcr_pid))
      found=1;
  }
  if(!found)
  {
    if(channel_update)
    {
      temp_pids[temp_num_pids]=pcr_pid;
      temp_pids_type[temp_num_pids]=PID_PCR;
      snprintf(temp_pids_language[temp_num_pids],4,"%s","---");
      temp_num_pids++;
    }
    else
    {
      channel->pids[channel->num_pids]=pcr_pid;
      channel->pids_type[channel->num_pids]=PID_PCR;
      snprintf(channel->pids_language[channel->num_pids],4,"%s","---");
      channel->num_pids++;
    }
    log_message( log_module,  MSG_DEBUG, "Added PCR pid %d\n",pcr_pid);
  }

  /**************************
  * PCR PID - END
  **************************/
  //We store the PMT version useful to check for updates
  channel->pmt_version=header->version_number;

  /**************************
  * Channel update 
  **************************/
  //If it's a channel update we will have to update the filters
  if(channel_update)
  {
    log_message( log_module,  MSG_DEBUG,"Channel update new number of pids %d old %d we check for changes\n", temp_num_pids, channel->num_pids);

    //We search for added pids
    for(i=0;i<temp_num_pids;i++)
    {
      found=0;
      for(j=0;j<channel->num_pids;j++)
      {
        if(channel->pids[j]==temp_pids[i])
          found=1;
      }
      if(!found)
      {
        log_message( log_module,  MSG_DETAIL, "Update : pid %d added \n",temp_pids[i]);
	//If the pid is not on the list we add it for the filters
        if(asked_pid[temp_pids[i]]==PID_NOT_ASKED)
          asked_pid[temp_pids[i]]=PID_ASKED;

        number_chan_asked_pid[temp_pids[i]]++;
        channel->pids[channel->num_pids]=temp_pids[i];
        channel->pids_type[channel->num_pids]=temp_pids_type[i];
        snprintf(channel->pids_language[channel->num_pids],4,"%s",temp_pids_language[i]);
        channel->num_pids++;

        log_message( log_module, MSG_DETAIL,"Add the new filters\n");
	// we open the file descriptors
        if (create_card_fd (card_base_path, tuner, asked_pid, fds) < 0)
        {
          log_message( log_module, MSG_ERROR,"CANNOT open the new descriptors. Some channels will probably not work\n");
	  //return; //FIXME : what do we do here ?
        }
	//open the new filters
        set_filters(asked_pid, fds);

      }
    }
    //We search for suppressed pids
    for(i=0;i<channel->num_pids;i++)
    {
      found=0;
      for(j=0;j<temp_num_pids;j++)
      {
        if(channel->pids[i]==temp_pids[j])
          found=1;
      }
      if(!found)
      {
        log_message( log_module,  MSG_DETAIL, "Update : pid %d supressed \n",channel->pids[i]);

	//We check the number of channels on wich this pid is registered, if 0 it's strange we warn
        if((channel->pids[i]>MAX_MANDATORY_PID_NUMBER )&& (number_chan_asked_pid[channel->pids[i]]))
        {
	  //We decrease the number of channels with this pid
          number_chan_asked_pid[channel->pids[i]]--;
	  //If no channel need this pid anymore, we remove the filter (closing the file descriptor remove the filter associated)
          if(number_chan_asked_pid[channel->pids[i]]==0)
          {
            log_message( log_module,  MSG_DEBUG, "Update : pid %d does not belong to any channel anymore, we close the filter \n",channel->pids[i]);
            close(fds->fd_demuxer[channel->pids[i]]);
            fds->fd_demuxer[channel->pids[i]]=0;
            asked_pid[channel->pids[i]]=PID_NOT_ASKED;
          }
        }
        else
          log_message( log_module,  MSG_WARN, "Update : We tried to suppress pid %d in a strange way, please contact if you can reproduce\n",channel->pids[i]);
        //We remove the pid from this channel by swapping with the last one and decreasing the pid number
        channel->pids[i]=channel->pids[channel->num_pids-1];
        channel->num_pids--;
	i--; //we wan to check the pid just moved so we force the loop to reaxamine pid i

      }
    }
    log_message( log_module,  MSG_DETAIL, "        pids : \n");/**@todo Generate a strind and call log_message after, in syslog it generates one line per pid : use the toolbox unicast*/
    int curr_pid;
    for (curr_pid = 0; curr_pid < channel->num_pids; curr_pid++)
	log_message( log_module,  MSG_DETAIL, "              %d (%s) \n", channel->pids[curr_pid], pid_type_to_str(channel->pids_type[curr_pid]));

  }
  /** @todo : update generated conf file*/
  /**************************
  * Channel update END
  **************************/
  /*************************
  * Language template
  **************************/
  int found =0;
  int len=MAX_NAME_LEN;
  for(i=0;i<channel->num_pids && !found;i++)
  {
    if(channel->pids_language[i][0]!='-')
    {
      log_message( log_module,  MSG_FLOOD, "Primary language for channel: %s",channel->pids_language[i]);
      mumu_string_replace(channel->name,&len,0,"%lang",channel->pids_language[i]);
      found=1; //we exit the loop
    }
  }
  //If we don't find a lang we replace by our "usual" ---
  if(!found)
    mumu_string_replace(channel->name,&len,0,"%lang",channel->pids_language[0]);
  /*************************
  * Language template END
  **************************/

  log_message( log_module,  MSG_DEBUG,"Number of pids after autoconf %d\n", channel->num_pids);
  return 0; 
}


/** @brief Tells if the descriptor with tag in present in buf
 *
 * for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818
 *
 * @param tag the descriptor tag, cf EN 300 468
 * @param buf the decriptors buffer (part of the PMT)
 * @param descriptors_loop_len the length of the descriptors
 * @param pos the position in the buffer
 */
int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len, int *pos)
{
  if(pos!=NULL)
  {
    buf+=*pos;
    descriptors_loop_len -= *pos;
  }
  while (descriptors_loop_len > 0) 
  {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;

    if (tag == descriptor_tag)
      return 1;

    if(pos!=NULL)
      *pos += descriptor_len;
    buf += descriptor_len;
    descriptors_loop_len -= descriptor_len;
  }
  return 0;
}

/** @brief Debugging function, Print the tags present in the descriptor
 *
 * for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818
 * @param buf the decriptors buffer (part of the PMT)
 * @param descriptors_loop_len the length of the descriptors
 */
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len)
{
  while (descriptors_loop_len > 0) 
  {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;

    log_message( log_module,  MSG_DEBUG,"0x%02x - \n", descriptor_tag);
    buf += descriptor_len;
    descriptors_loop_len -= descriptor_len;
  }
}


/********************************************************************
 * Autoconfiguration auto update
 ********************************************************************/

/** @brief, tell if the pmt have a newer version than the one recorded actually
 * In the PMT pid there is a field to say if the PMT was updated
 * This function check if it has changed 
 *
 * @param channel the channel for which we have to check
 * @param buf : the received buffer
 * @param ts_header says if the packet contains a transport stream header
 */
int pmt_need_update(mumudvb_channel_t *channel, unsigned char *packet)
{
  pmt_t       *pmt;

  pmt=(pmt_t*)(packet);

  if(pmt == NULL)
    return 0;

  /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
  if(pmt->current_next_indicator == 0)
  {
    return 0;
  }

  if(pmt->table_id==0x02)
    if(pmt->version_number!=channel->pmt_version)
    {
      log_message( log_module, MSG_DEBUG,"PMT version changed, channel %s . stored version : %d, new: %d.\n",channel->name,channel->pmt_version,pmt->version_number);
      return 1;
    }
  return 0;

}


/** @brief update the version using the dowloaded pmt*/
void update_pmt_version(mumudvb_channel_t *channel)
{
  pmt_t       *pmt=(pmt_t*)(channel->pmt_packet->packet);
  if(channel->pmt_version!=pmt->version_number)
    log_message( log_module, MSG_INFO,"New PMT version for channel %s. Old : %d, new: %d\n",channel->name,channel->pmt_version,pmt->version_number);

  channel->pmt_version=pmt->version_number;
}



/** @brief This function is called when a new PMT packet is there and we asked to check if there is updates*/
void autoconf_pmt_follow(unsigned char *ts_packet, fds_t *fds, mumudvb_channel_t *actual_channel, char *card_base_path, int tuner, mumudvb_chan_and_pids_t *chan_and_pids)
{
  /*Note : the pmt version is initialised during autoconfiguration*/
  /*Check the version stored in the channel*/
  if(!actual_channel->pmt_needs_update)
  {
    //Checking without crc32, it there is a change we get the full packet for crc32 checking
    actual_channel->pmt_needs_update=pmt_need_update(actual_channel,get_ts_begin(ts_packet));

    if(actual_channel->pmt_needs_update && actual_channel->pmt_packet) //It needs update we mark the packet as empty
      actual_channel->pmt_packet->empty=1;
  }
  /*We need to update the full packet, we download it*/
  if(actual_channel->pmt_needs_update)
  {
    if(get_ts_packet(ts_packet,actual_channel->pmt_packet))
    {
      if(pmt_need_update(actual_channel,actual_channel->pmt_packet->packet))
      {
        log_message( log_module, MSG_DETAIL,"PMT packet updated, we have now to check if there is new things\n");
        /*We've got the FULL PMT packet*/
        if(autoconf_read_pmt(actual_channel->pmt_packet, actual_channel, card_base_path, tuner, chan_and_pids->asked_pid, chan_and_pids->number_chan_asked_pid, fds)==0)
        {
          if(actual_channel->need_cam_ask==CAM_ASKED)
            actual_channel->need_cam_ask=CAM_NEED_UPDATE; //We we resend this packet to the CAM
          update_pmt_version(actual_channel);
          actual_channel->pmt_needs_update=0;
        }
        else
          actual_channel->pmt_packet->empty=1;
      }
      else
      {
        log_message( log_module, MSG_DEBUG,"False alert, nothing to do\n");
        actual_channel->pmt_needs_update=0;
      }
    }
  }
}
