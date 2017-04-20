/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2013 Brice DUBOST
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
 *
 */


/** @file
 * @brief T2-MI stream support
 */

#include <string.h>

#include "mumudvb.h"
#include "log.h"
#include <stdint.h>

static char *log_module = "T2MI: ";

bool t2mi_active=false;
bool t2mi_first=true;
int t2_partial_size=0;

int t2packetpos=0;
char t2packet[65536 + 10]; //Maximal T2 payload + header

/* rewritten by [anp/hsw], original code taken from https://github.com/newspaperman/t2-mi */
int processt2(unsigned char* input_buf, int input_buf_offset, unsigned char* output_buf, int output_buf_offset, uint8_t plpId) {

	unsigned int payload_start_offset=0;
	output_buf+=output_buf_offset;

	/* lookup for adaptation field control bits in TS input stream */
        switch(((input_buf[input_buf_offset + 3])&0x30)>>4) {
    	    case 0x03:	/* 11b = adaptation field followed by payload */
		/* number of bytes in AF, following this byte */
                payload_start_offset=(uint8_t)(input_buf[input_buf_offset + 4]) + 1;
                if(payload_start_offset > 183) {
            		log_message(log_module, MSG_DEBUG, "wrong AF len in input stream: %d\n", payload_start_offset);
                        return 0;
                }
            break;

            case 0x02:	/* 10b = adaptation field only, no payload */
                return 0;
            break;

            case 0x00:	/* 00b = reserved! */
            	log_message(log_module, MSG_DEBUG, "wrong AF (00) in input stream, accepting as ordinary packet\n");
            break;
            
            default: /* -Wswitch-default */
            break;
	}

	/* source buffer pointer to beginning of payload in packet */
        unsigned char* buf = input_buf + input_buf_offset + 4 + payload_start_offset;
        unsigned int len = TS_PACKET_SIZE - 4 - payload_start_offset;

        int output_bytes=0;

	/* check for payload unit start indicator */
        if((((input_buf[input_buf_offset + 1])&0x40)>>4)==0x04) {
                unsigned int offset=1;
                offset+=(uint8_t)(buf[0]);
                if(t2mi_active) {
                        if( 1 < offset && offset < 184) {
                                memcpy(&t2packet[t2packetpos],&buf[1],offset-1);
                        } else if (offset >= 184) {
            			log_message(log_module, MSG_DEBUG, "invalid payload offset: %u\n", offset);
            			return 0;
                        }
                        
			/* select source PLP */
                        if(t2packet[7]==plpId) {
                    		/* extract TS packet from T2-MI payload */
				/* Sync distance (bits) in the BB header then points to the first CRC-8 present in the data field */
                                unsigned int syncd = ((uint8_t)(t2packet[16]) << 8) + (uint8_t)(t2packet[17]);
                                syncd >>= 3;

				/* user packet len (bits) = sync byte + payload, CRC-8 of payload replaces next sync byte */
                                unsigned int upl = ((uint8_t)(t2packet[13]) << 8) + (uint8_t)(t2packet[14]);
                                upl >>= 3;
                                upl+=19;

                                int dnp=0;

                                if(t2packet[9]&0x4) {
                        	    dnp=1; // Deleted Null Packet
                        	}
                                if(syncd==0x1FFF ) { /* maximal sync value (in bytes) */
            				log_message(log_module, MSG_DEBUG, "sync value 0x1FFF!\n");
                                        if(upl >19) {
                                                memcpy(output_buf + output_bytes, &t2packet[19], upl-19);
                                                output_bytes+=(upl-19);
                                        }

                                } else {
                                        if(!t2mi_first && syncd > 0) {
                                            if (syncd-dnp > (sizeof(t2packet)-19)) {
	                                	    log_message(log_module, MSG_DEBUG, "position (syncd) out of buffer bounds: %d\n", syncd-dnp);
	                                	    goto t2_copy_end;
                                            }
                                            memcpy(output_buf + output_bytes, &t2packet[19], syncd-dnp);
                                            output_bytes+=(syncd-dnp);
                                        }

					/* detect unaligned packet in buffer */
                                        unsigned int output_part = (output_buf_offset + output_bytes) % TS_PACKET_SIZE;
                                        
                                        if (output_part > 0) {
                                    	    log_message(log_module, MSG_DETAIL, "unaligned packet in buffer pos %d/%d\n", output_buf_offset, output_bytes);
                                    	    output_bytes -= output_part; /* drop packet; TODO: check if we can add padding instead of dropping */
                                        }

                                        t2mi_first=false;
                                        unsigned int t2_copy_pos=19+syncd;

                                        /* copy T2-MI packet payload to output, add sync bytes */
                                        for(; t2_copy_pos < upl - 187; t2_copy_pos+=(187+dnp)) {
                                    		/* fullsize TS frame */
                                                if (t2_copy_pos > ((sizeof(t2packet) - 187))) {
                                        	    log_message(log_module, MSG_DEBUG, "position (full TS) out of buffer bounds: %d\n", t2_copy_pos);
                                        	    goto t2_copy_end;
                                                }
                                                output_buf[output_bytes] = TS_SYNC_BYTE;
                                                output_bytes++;
                                                memcpy(output_buf + output_bytes, &t2packet[t2_copy_pos], 187);
                                                output_bytes+=187;
                                        }
                                        if(t2_copy_pos < upl )  {
                                    		/* partial TS frame, we will fill rest of frame at next call */
                                                if (t2_copy_pos > (sizeof(t2packet)-((upl-t2_copy_pos)+1))) {
                                        	    log_message(log_module, MSG_DEBUG, "position (part TS) out of buffer bounds: %d\n", t2_copy_pos);
                                        	    goto t2_copy_end;
                                                }
                                                output_buf[output_bytes] = TS_SYNC_BYTE;
                                                output_bytes++;
                                                memcpy(output_buf + output_bytes, &t2packet[t2_copy_pos], upl-t2_copy_pos);
                                                output_bytes+=(upl-t2_copy_pos);
                                        }
                                        t2_copy_end: ;
                                }
                        }
                        t2mi_active=false;
                        memset(&t2packet, 0, sizeof(t2packet)); // end of processing t2-mi packet, clear it
                }

                if((buf[offset])==0x0) { //Baseband Frame
			/*	TODO: padding
				pad (pad_len bits) shall be filled with between 0 and 7 bits of padding such that the T2-MI packet is always an integer
				number of bytes in length, i.e. payload_len+pad_len shall be a multiple of 8. Each padding bit shall have the value 0. 
			*/
                        if(len > offset) {
                                memcpy(t2packet,&buf[offset],len-offset);
                                t2packetpos=len-offset;
                                t2mi_active=true;
                        }
                }
        } else if(t2mi_active) {
                memcpy(t2packet+t2packetpos,buf,len);
                t2packetpos+=len;
        }
        return output_bytes;
}
