/*****************************************************************************
 * linux_dvb.c : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christopher Ross <chris@tebibyte.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/poll.h>

/* DVB Card Drivers */
#include <linux/dvb/version.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/ca.h>

#include "vlc_dvb.h"


/*
 * CAM device
 */

/*****************************************************************************
 * CAMOpen :
 *****************************************************************************/
int CAMOpen( access_t *p_access , int card)
{
    access_sys_t *p_sys = p_access->p_sys;
    char ca[128];
    int i_adapter, i_device;
    ca_caps_t caps;

    //    i_adapter = var_GetInteger( p_access, "dvb-adapter" );
    i_adapter = card;
    //    i_device = var_GetInteger( p_access, "dvb-device" );
    i_device = 0;

    if( snprintf( ca, sizeof(ca), CA, i_adapter, i_device ) >= (int)sizeof(ca) )
    {
        fprintf(stderr,"CAM : snprintf() truncated string for CA" );
        ca[sizeof(ca) - 1] = '\0';
    }
    memset( &caps, 0, sizeof( ca_caps_t ));

    fprintf(stderr,"CAM : Opening device %s\n", ca );
    if( (p_sys->i_ca_handle = open(ca, O_RDWR | O_NONBLOCK)) < 0 )
    {
        fprintf(stderr, "CAMInit: opening CAM device failed (%s)\n",
                  strerror(errno) );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    if ( ioctl( p_sys->i_ca_handle, CA_GET_CAP, &caps ) != 0 )
    {
        fprintf(stderr, "CAMInit: ioctl() error getting CAM capabilities\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    /* Output CA capabilities */
    fprintf(stderr, "CAMInit: CA interface with %d %s\n", caps.slot_num, 
        caps.slot_num == 1 ? "slot" : "slots" );
    if ( caps.slot_type & CA_CI )
        fprintf(stderr, "CAMInit: CI high level interface type\n" );
    if ( caps.slot_type & CA_CI_LINK )
        fprintf(stderr, "CAMInit: CI link layer level interface type\n" );
    if ( caps.slot_type & CA_CI_PHYS )
        fprintf(stderr, "CAMInit: CI physical layer level interface type (not supported) \n" );
    if ( caps.slot_type & CA_DESCR )
        fprintf(stderr, "CAMInit: built-in descrambler detected\n" );
    if ( caps.slot_type & CA_SC )
        fprintf(stderr, "CAMInit: simple smart card interface\n" );

    fprintf(stderr, "CAMInit: %d available %s\n", caps.descr_num,
        caps.descr_num == 1 ? "descrambler (key)" : "descramblers (keys)" );
    if ( caps.descr_type & CA_ECD )
        fprintf(stderr, "CAMInit: ECD scrambling system supported\n" );
    if ( caps.descr_type & CA_NDS )
        fprintf(stderr, "CAMInit: NDS scrambling system supported\n" );
    if ( caps.descr_type & CA_DSS )
        fprintf(stderr, "CAMInit: DSS scrambling system supported\n" );

    if ( caps.slot_num == 0 )
    {
        fprintf(stderr, "CAMInit: CAM module with no slots\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    if( caps.slot_type & CA_CI_LINK )
    {
        p_sys->i_ca_type = CA_CI_LINK;
    }
    else if( caps.slot_type & CA_CI )
    {
        p_sys->i_ca_type = CA_CI;
    }
    else {
        p_sys->i_ca_type = -1;
        fprintf(stderr, "CAMInit: incompatible CAM interface\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    p_sys->i_nb_slots = caps.slot_num;
    memset( p_sys->pb_active_slot, 0, sizeof(int) * MAX_CI_SLOTS );
    memset( p_sys->pb_slot_mmi_expected, 0, sizeof(int) * MAX_CI_SLOTS );
    memset( p_sys->pb_slot_mmi_undisplayed, 0,
            sizeof(int) * MAX_CI_SLOTS );

    return en50221_Init( p_access );
}

/*****************************************************************************
 * CAMPoll :
 *****************************************************************************/
int CAMPoll( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret = -666;

    if ( p_sys->i_ca_handle == 0 )
    {
        return -666;
    }

    switch( p_sys->i_ca_type )
    {
    case CA_CI_LINK:
        i_ret = en50221_Poll( p_access );
        break;
    case CA_CI:
        i_ret = 0;
        break;
    default:
        fprintf(stderr, "CAMPoll: This should not happen" );
        break;
    }

    return i_ret;
}

/*****************************************************************************
 * CAMSet :
 *****************************************************************************/
int CAMSet( access_t * p_access, mumudvb_pmt_t *p_pmt )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->i_ca_handle == 0 )
    {
      //dvbpsi_DeletePMT( p_pmt );
        return -666;
    }

    en50221_SetCAPMT( p_access, p_pmt );

    return 0;
}

/*****************************************************************************
 * CAMClose :
 *****************************************************************************/
void CAMClose( access_t * p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    en50221_End( p_access );

    if ( p_sys->i_ca_handle )
    {
        close( p_sys->i_ca_handle );
    }
}

