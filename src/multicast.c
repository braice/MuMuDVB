/*
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 * (C) 2009 Brice DUBOST
 *
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
 *
 */

#define _CRT_SECURE_NO_WARNINGS

#include "mumudvb.h"
#include "log.h"
#include <string.h>
#ifndef _WIN32
#include <net/if.h>
#endif

static char *log_module = "Multicast: ";

/** Initialize multicast variables*/
void init_multicast_v(multi_p_t *multi_p)
{
    memset(multi_p, 0, sizeof(multi_p_t));
    *multi_p = (multi_p_t) {
        .multicast = 1,
        .multicast_ipv6 = 0,
        .multicast_ipv4 = 1,
        .ttl = DEFAULT_TTL,
        .common_port = 1234,
        .auto_join = 0,
        .rtp_header = 0,
        .iface4 = "\0",
        .iface6 = "\0",
    };

}

/** @brief Read a line of the configuration file to check if there is a cam parameter
*
* @param multi_p the multicast parameters
* @param substring The currrent line
 */
int read_multicast_configuration(multi_p_t *multi_p, mumudvb_channel_t *c_chan, char *substring)
{
    char delimiteurs[] = CONFIG_FILE_SEPARATOR;

    if (!strcmp(substring, "common_port")) {
        substring = strtok(NULL, delimiteurs);
        multi_p->common_port = atoi(substring);
    } else if (!strcmp(substring, "multicast_ttl")) {
        substring = strtok(NULL, delimiteurs);
        multi_p->ttl = atoi(substring);
    } else if (!strcmp(substring, "multicast_ipv4")) {
        substring = strtok(NULL, delimiteurs);
        multi_p->multicast_ipv4 = atoi(substring);
    } else if (!strcmp(substring, "multicast_ipv6")) {
        substring = strtok(NULL, delimiteurs);
        multi_p->multicast_ipv6 = atoi(substring);
    } else if (!strcmp(substring, "multicast_auto_join")) {
        substring = strtok(NULL, delimiteurs);
        multi_p->auto_join = atoi(substring);
    } else if (!strcmp(substring, "ip")) {
        if (c_chan == NULL) {
            log_message(log_module, MSG_ERROR, "ip : You have to start a channel first (using new_channel)\n");
            return -1;
        }

        substring = strtok(NULL, delimiteurs);
        if (strlen(substring) > 19) {
            log_message(log_module, MSG_ERROR, "The Ip address %s is too long.\n", substring);
            return -1;
        }
        sscanf(substring, "%s\n", c_chan->ip4Out);
        MU_F(c_chan->ip4Out) = F_USER;
    } else if (!strcmp(substring, "ip6")) {
        if (c_chan == NULL) {
            log_message(log_module, MSG_ERROR, "ip6 : You have to start a channel first (using new_channel)\n");
            return -1;
        }

        substring = strtok(NULL, delimiteurs);
        if (strlen(substring) > (IPV6_CHAR_LEN - 1)) {
            log_message(log_module, MSG_ERROR, "The Ip v6 address %s is too long.\n", substring);
            return -1;
        }
        sscanf(substring, "%s\n", c_chan->ip6Out);
        MU_F(c_chan->ip6Out) = F_USER;
    } else if (!strcmp(substring, "port")) {
        if (c_chan == NULL) {
            log_message(log_module, MSG_ERROR, "port : You have to start a channel first (using new_channel)\n");
            return -1;
        }
        substring = strtok(NULL, delimiteurs);
        c_chan->portOut = atoi(substring);
        MU_F(c_chan->portOut) = F_USER;
    } else if (!strcmp(substring, "rtp_header")) {
        substring = strtok(NULL, delimiteurs);
        multi_p->rtp_header = atoi(substring);
        if (multi_p->rtp_header == 1)
            log_message(log_module, MSG_INFO, "You decided to send the RTP header (multicast only).\n");
    } else if (!strcmp(substring, "multicast_iface4")) {
        substring = strtok(NULL, delimiteurs);
        if (strlen(substring) > (IF_NAMESIZE)) {
            log_message(log_module, MSG_ERROR, "The interface name ipv4 address %s is too long.\n", substring);
            return -1;
        }
        sscanf(substring, "%s\n", multi_p->iface4);
    } else if (!strcmp(substring, "multicast_iface6")) {
        substring = strtok(NULL, delimiteurs);
        if (strlen(substring) > (IF_NAMESIZE)) {
            log_message(log_module, MSG_ERROR, "The interface name ipv6 address %s is too long.\n", substring);
            return -1;
        }
        sscanf(substring, "%s\n", multi_p->iface6);
    } else {
        return 0; //Nothing concerning multicast, we return 0 to explore the other possibilities
    }

    return 1; //We found something for multicast, we tell main to go for the next line
}
