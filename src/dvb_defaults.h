/* dvb_defaults.h
   part of mumudvb

   last version availaible from http://mumudvb.braice.net/

   Copyright (C) Dave Chapman 2002

   Copyright (C) 2004-2009 Brice DUBOST

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

/**@file
 * @brief defaults for the dvb parameters
 */

#ifndef _DVB_DEFAULTS_H
#define _DVB_DEFAULTS_H

/* DVB-S */
// With a diseqc system you may need different values per LNB.  I hope
// no-one ever asks for that :-)
/** lnb_slof: switch frequency of LNB */
#define SLOF (11700*1000UL)
/** lnb_lof1: local frequency of lower LNB band */
#define LOF1 (9750*1000UL)
/** lnb_lof2: local frequency of upper LNB band */
#define LOF2 (10600*1000UL)

/* DVB-T */
/* default option : full auto except bandwith = 8MHz*/
/* AUTO settings */
#define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
#define HP_CODERATE_DEFAULT         FEC_AUTO
#define MODULATION_DEFAULT          QAM_AUTO
#define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_AUTO
#define GUARD_INTERVAL_DEFAULT      GUARD_INTERVAL_AUTO
#define HIERARCHY_DEFAULT           HIERARCHY_NONE

#if HIERARCHY_DEFAULT == HIERARCHY_NONE && !defined (LP_CODERATE_DEFAULT)
#define LP_CODERATE_DEFAULT (FEC_NONE) /* unused if HIERARCHY_NONE */
#endif

#endif
