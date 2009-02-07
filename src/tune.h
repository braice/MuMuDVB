/* dvbtune - tune.c

   part of mumudvb

   last version availaible from http://mumudvb.braice.net/

   Copyright (C) 2004-2009 Brice DUBOST
   Copyright (C) Dave Chapman 2001,2002
  
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

#ifndef _TUNE_H
#define _TUNE_H

#include <linux/dvb/frontend.h>

#include "dvb_defaults.h"

//The different parameters used for autoconfiguration
typedef struct tuning_parameters_t{
  unsigned long freq;
  unsigned int srate;
  char pol;
  //int tone;
  fe_spectral_inversion_t specInv;
  unsigned char sat_number;
  fe_modulation_t modulation;
  fe_code_rate_t HP_CodeRate;
  fe_code_rate_t LP_CodeRate;
  fe_transmit_mode_t TransmissionMode;
  fe_guard_interval_t guardInterval;
  fe_bandwidth_t bandwidth;
  fe_hierarchy_t hier;
  int display_strenght;
}tuning_parameters_t;



int tune_it(int, tuning_parameters_t);

#endif
