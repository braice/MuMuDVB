/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Conditionnal Access Modules support
 * 
 * (C) Brice DUBOST
 * 
 * The latest version can be found at http://mumudvb.braice.net
 * 
 * Parts of this code is from the VLC project, modified  for mumudvb
 * by Brice DUBOST 
 * 
 * Copyright (C) 1998-2005 the VideoLAN team
 * Authors of the VLC part : Johan Bilien <jobi@via.ecp.fr>
 *                           Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *                           Christopher Ross <chris@tebibyte.org>
 *                           Christophe Massiot <massiot@via.ecp.fr>
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


#ifndef _CAM_H
#define _CAM_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "ts.h"
#include "mumudvb.h"

struct ca_info {
  int initialized; //are the cai complete ?
  int ready; //We wait a pool between each channel sending
  int sys_num;
  uint16_t sys_id[256];
  char app_name[256];
};


int cam_send_ca_pmt( mumudvb_ts_packet_t *pmt, struct ca_info *cai);
int convert_desc(struct ca_info *cai, uint8_t *out, uint8_t *buf, int dslen, uint8_t cmd, int quiet);
int convert_pmt(struct ca_info *cai, mumudvb_ts_packet_t *pmt, uint8_t list, uint8_t cmd,int quiet);

/*****************************************************************************
 * VLC PART
 *****************************************************************************/

#define CA_DEV       "/dev/dvb/adapter%d/ca%d"

typedef int64_t mtime_t;

typedef struct access_sys_t access_sys_t;

/*****************************************************************************
 * Local structures
 *****************************************************************************/

typedef struct en50221_session_t
{
    int i_slot;
    int i_resource_id;
    void (* pf_handle)( access_sys_t *, int, uint8_t *, int );
    void (* pf_close)( access_sys_t *, int );
    void (* pf_manage)( access_sys_t *, int );
    void *p_sys;
} en50221_session_t;

#define EN50221_MMI_NONE 0
#define EN50221_MMI_ENQ 1
#define EN50221_MMI_ANSW 2
#define EN50221_MMI_MENU 3
#define EN50221_MMI_MENU_ANSW 4
#define EN50221_MMI_LIST 5

typedef struct en50221_mmi_object_t
{
    int i_object_type;

    union
    {
        struct
        {
            int b_blind;
            char *psz_text;
        } enq;

        struct
        {
            int b_ok;
            char *psz_answ;
        } answ;

        struct
        {
            char *psz_title, *psz_subtitle, *psz_bottom;
            char **ppsz_choices;
            int i_choices;
        } menu; /* menu and list are the same */

        struct
        {
            int i_choice;
        } menu_answ;
    } u;
} en50221_mmi_object_t;

static __inline__ void en50221_MMIFree( en50221_mmi_object_t *p_object )
{
    int i;

#define FREE( x )                                                           \
    if ( x != NULL )                                                        \
        free( x );

    switch ( p_object->i_object_type )
    {
    case EN50221_MMI_ENQ:
        FREE( p_object->u.enq.psz_text );
        break;

    case EN50221_MMI_ANSW:
        if ( p_object->u.answ.b_ok )
        {
            FREE( p_object->u.answ.psz_answ );
        }
        break;

    case EN50221_MMI_MENU:
    case EN50221_MMI_LIST:
        FREE( p_object->u.menu.psz_title );
        FREE( p_object->u.menu.psz_subtitle );
        FREE( p_object->u.menu.psz_bottom );
        for ( i = 0; i < p_object->u.menu.i_choices; i++ )
        {
            FREE( p_object->u.menu.ppsz_choices[i] );
        }
        FREE( p_object->u.menu.ppsz_choices );
        break;

    default:
        break;
    }
#undef FREE
}

#define MAX_DEMUX 256
#define MAX_CI_SLOTS 16
#define MAX_SESSIONS 32
#define MAX_PROGRAMS 24

struct access_sys_t
{
  /* CA management */
  int i_ca_handle;
  int i_ca_type;
  int i_nb_slots;
  int pb_active_slot[MAX_CI_SLOTS];
  int pb_tc_has_data[MAX_CI_SLOTS];
  int pb_slot_mmi_expected[MAX_CI_SLOTS];
  int pb_slot_mmi_undisplayed[MAX_CI_SLOTS];
  en50221_session_t p_sessions[MAX_SESSIONS];
  mtime_t i_ca_timeout, i_ca_next_event, i_frontend_timeout;
  mumudvb_ts_packet_t *pp_selected_programs[MAX_PROGRAMS]; //braice
  int i_selected_programs;

    /* */
  int i_read_once;
  struct ca_info cai[1];

};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  CAMOpen( access_sys_t * , int, int);
int  CAMPoll( access_sys_t * );
int  CAMSet( access_sys_t *, mumudvb_ts_packet_t * );
void CAMClose( access_sys_t * );

int en50221_Init( access_sys_t * );
int en50221_Poll( access_sys_t * );
int en50221_SetCAPMT( access_sys_t * , mumudvb_ts_packet_t * , mumudvb_channel_t *);
int en50221_OpenMMI( access_sys_t * p_sys, int i_slot );
int en50221_CloseMMI( access_sys_t * p_sys, int i_slot );
en50221_mmi_object_t *en50221_GetMMIObject( access_sys_t * p_sys,
                                                int i_slot );
void en50221_SendMMIObject( access_sys_t * p_sys, int i_slot,
                                en50221_mmi_object_t *p_object );
void en50221_End( access_sys_t * );


#endif
