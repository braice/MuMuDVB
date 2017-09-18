/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) 2017-2017 Frederik Kriewitz <frederik @t kriewitz.eu>
 * (C) 2008-2013 Brice DUBOST <mumudvb@braice.net>
 *
 * Parts of this code come from libdvb, modified for mumudvb
 * by Brice DUBOST 
 * Libdvb part : Copyright (C) 2000 Klaus Schmidinger
 * 
 * The latest version can be found at http://mumudvb.net
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
 *  @brief This file contain the code related to the CAT reading for autoconfiguration
 *
 */


static char *log_module="Autoconf: ";

#include <errno.h>
#include <string.h>

#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"
#include "dvb.h"

int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len, int *pos);

void autoconf_cat_need_update(auto_p_t *auto_p, unsigned char *buf)
{
	cat_t       *cat=(cat_t*)(get_ts_begin(buf));
	if(cat) //It's the beginning of a new packet
	{
		if(cat->version_number!=auto_p->cat_version)
		{
			/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
        sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
        and shall be the next table to become valid.*/
			if(cat->current_next_indicator == 0)
			{
				return;
			}
			log_message( log_module, MSG_DEBUG,"CAT Need update. stored version : %d, new: %d\n",auto_p->cat_version,cat->version_number);
			auto_p->cat_need_update=1;
		}
		else if(auto_p->cat_all_sections_seen && auto_p->cat_need_update==1) //We can have a wrong need update if the packet was broken (the CRC32 is checked only if we think it's good)
		{
			log_message( log_module, MSG_DEBUG,"CAT Not needing update anymore (wrong CRC ?)");
			auto_p->cat_need_update=0;
		}
	}
}

/** @brief clears the ca_system_list
 *
 * @param auto_p The autoconfiguration structure
 */
void autoconf_clear_ca_system_list(auto_p_t *auto_p)
{
    for(int i = 0; i < MAX_CA_SYSTEMS; i++)
    {
        mumudvb_ca_system_t *ca_system;
        ca_system = &auto_p->ca_system_list[i];
        ca_system->id = -1;
    }
}

/** @brief finds the CA_system entry for a given CA system ID
 *
 * It's used to get the different "useful" pids of the channel
 * @param auto_p The autoconfiguration structure, containing all we need
 * @param ca_system_id the CA system id we're looking for
 */
mumudvb_ca_system_t* autoconf_get_ca_system(auto_p_t *auto_p, int ca_system_id)
{
    for(int i = 0; i < MAX_CA_SYSTEMS; i++)
    {
        mumudvb_ca_system_t *ca_system;
        ca_system = &auto_p->ca_system_list[i];
        if(ca_system->id == ca_system_id)
            return ca_system;
        else if(ca_system->id == -1) // we reached the last entry
            return NULL;
    }
    return NULL;
}

/** @brief Reads the conditional access table
 *
 * It's used to get the different "useful" pids of the channel
 * @param auto_p The autoconfiguration structure, containing all we need
 * @param channel the associated channel
 */
int autoconf_read_cat(auto_p_t *auto_p,mumu_chan_p_t *chan_p)
{
    mumudvb_ts_packet_t *cat_mumu;
    unsigned char *buf=NULL;
    cat_mumu=auto_p->autoconf_temp_cat;
    buf=cat_mumu->data_full;
    cat_t       *cat=(cat_t*)(buf);
    int delta=CAT_LEN;
    int section_length=0;
    int i;

    if(cat->version_number==auto_p->cat_version)
    {
        //check if we saw this section
        if(auto_p->cat_sections_seen[cat->section_number])
            return 0;
    }
    else
    {
        //New CAT version so we didn't got all CAT
        auto_p->cat_all_sections_seen=0;
        //We also force a re read of the SDT
        auto_p->sdt_version=-1;

        //New version, no section seen
        for(i=0;i<256;i++)
            auto_p->cat_sections_seen[i]=0;
        auto_p->cat_version=cat->version_number;
        if(auto_p->cat_version!=-1)
        {
            log_message( log_module, MSG_INFO,"The CAT version changed");
        }
        log_message( log_module, MSG_INFO,"New CAT we force SDT update after all sections seen");
    }
    //we store the section
    auto_p->cat_sections_seen[cat->section_number]=1;

    log_message( log_module, MSG_DEBUG,"---- New CAT version %d section %d ----\n",cat->version_number, cat->section_number);
    //CAT reading
    section_length=HILO(cat->section_length);

    /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
    sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
    and shall be the next table to become valid.*/
    if(cat->current_next_indicator == 0)
    {
        log_message( log_module, MSG_DEBUG,"The current_next_indicator is set to 0, this CAT is not valid for the current stream\n");
        return 0;
    }

    // clear old ca system cache
    autoconf_clear_ca_system_list(auto_p);

    // parse the CA descriptors and store them in the ca_system_list
    int ca_system_list_index = 0;
    while((delta+2)<(section_length)) {
        unsigned char *descriptor_buf = buf+delta;
        unsigned char descriptor_tag = descriptor_buf[0];
        unsigned char descriptor_len = descriptor_buf[1] + 2;

        if(descriptor_buf+descriptor_len >= buf+cat_mumu->len_full)
        {
            log_message( log_module, MSG_ERROR, "Invalid CAT descriptor length, canceling parsing\n");
            break;
        }

        if(descriptor_tag == 0x09)
        {
            if(descriptor_buf+DESCR_CA_LEN >= buf+cat_mumu->len_full)
            {
                log_message( log_module, MSG_ERROR, "Not enough data for CAT CA descriptor left, canceling parsing\n");
                break;
            }

            // parse ca descriptor
            descr_ca_t *ca_descriptor;
            ca_descriptor = (descr_ca_t *)descriptor_buf;
            int ca_type = HILO(ca_descriptor->CA_type);
            int ca_pid = HILO(ca_descriptor->CA_PID);
            log_message( log_module, MSG_DETAIL, "Found CAT CA system id 0x%04x: %s, EMM PID: %i\n", ca_type, ca_sys_id_to_str(ca_type), ca_pid);

            // store in the ca_system_list
            if(ca_system_list_index >= MAX_CA_SYSTEMS)
            {
                log_message( log_module, MSG_ERROR, "Reached MAX_CA_SYSTEMS limit, canceling parsing\n");
                break;
            }
            mumudvb_ca_system_t *ca_system;
            ca_system = &auto_p->ca_system_list[ca_system_list_index];
            ca_system->id = ca_type;
            ca_system->emm_pid = ca_pid;
            ca_system_list_index++;
        }
        else
        {
            log_message( log_module, MSG_DEBUG, "Unsupported CAT tag received: 0x%02x - \n", descriptor_tag);
        }

        delta += descriptor_len;
    }

    int sections_missing=0;
    //We check if we saw all sections
    for(i=0;i<=cat->last_section_number;i++)
        if(auto_p->cat_sections_seen[i]==0)
            sections_missing++;
    if(sections_missing)
    {
        log_message( log_module, MSG_DETAIL, "CAT  %d sections on %d are missing",
                     sections_missing,cat->last_section_number);
        return 0;
    }
    else
    {
        auto_p->cat_all_sections_seen=1;
        auto_p->cat_need_update=0;
        log_message( log_module, MSG_DEBUG, "It seems that we have finished to get the CAT");

        // force PMT update for channels with CA systems
        for(i=0; i < chan_p->number_of_channels && i < MAX_CHANNELS; i++)
        {
            if(chan_p->channels[i].ca_sys_id[0] != 0) // channels with at least on CA system ID
            {
                // Force PMT update
                log_message( log_module, MSG_INFO, "Channel %d SID %d: force PMT update due to CAT update",
                             i,
                             chan_p->channels[i].service_id);
                chan_p->channels[i].pmt_version=-1;
            }
        }
    }


    return 0;
}

