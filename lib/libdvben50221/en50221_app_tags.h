/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#ifndef __EN50221_APP_TAGS_H__
#define __EN50221_APP_TAGS_H__

/*	Resource Manager		*/
#define TAG_PROFILE_ENQUIRY		0x9f8010
#define TAG_PROFILE			0x9f8011
#define TAG_PROFILE_CHANGE		0x9f8012

/*	Application Info		*/
#define TAG_APP_INFO_ENQUIRY		0x9f8020
#define TAG_APP_INFO			0x9f8021
#define TAG_ENTER_MENU			0x9f8022

/*	CA Support			*/
#define TAG_CA_INFO_ENQUIRY		0x9f8030
#define TAG_CA_INFO			0x9f8031
#define TAG_CA_PMT			0x9f8032
#define TAG_CA_PMT_REPLY		0x9f8033

/*	Host Control			*/
#define TAG_TUNE			0x9f8400
#define TAG_REPLACE			0x9f8401
#define TAG_CLEAR_REPLACE		0x9f8402
#define TAG_ASK_RELEASE			0x9f8403

/*	Date and Time			*/
#define TAG_DATE_TIME_ENQUIRY		0x9f8440
#define TAG_DATE_TIME			0x9f8441

/*	Man Machine Interface (MMI)	*/
#define TAG_CLOSE_MMI			0x9f8800
#define TAG_DISPLAY_CONTROL		0x9f8801
#define TAG_DISPLAY_REPLY		0x9f8802
#define TAG_TEXT_LAST			0x9f8803
#define TAG_TEXT_MORE			0x9f8804
#define TAG_KEYPAD_CONTROL		0x9f8805
#define TAG_KEYPRESS			0x9f8806
#define TAG_ENQUIRY			0x9f8807
#define TAG_ANSWER			0x9f8808
#define TAG_MENU_LAST			0x9f8809
#define TAG_MENU_MORE			0x9f880a
#define TAG_MENU_ANSWER			0x9f880b
#define TAG_LIST_LAST			0x9f880c
#define TAG_LIST_MORE			0x9f880d
#define TAG_SUBTITLE_SEGMENT_LAST	0x9f880e
#define TAG_SUBTITLE_SEGMENT_MORE	0x9f880f
#define TAG_DISPLAY_MESSAGE		0x9f8810
#define TAG_SCENE_END_MARK		0x9f8811
#define TAG_SCENE_DONE			0x9f8812
#define TAG_SCENE_CONTROL		0x9f8813
#define TAG_SUBTITLE_DOWNLOAD_LAST	0x9f8814
#define TAG_SUBTITLE_DOWNLOAD_MORE	0x9f8815
#define TAG_FLUSH_DOWNLOAD		0x9f8816
#define TAG_DOWNLOAD_REPLY		0x9f8817

/*	Low Speed Communications	*/
#define TAG_COMMS_COMMAND		0x9f8c00
#define TAG_CONNECTION_DESCRIPTOR	0x9f8c01
#define TAG_COMMS_REPLY			0x9f8c02
#define TAG_COMMS_SEND_LAST		0x9f8c03
#define TAG_COMMS_SEND_MORE		0x9f8c04
#define TAG_COMMS_RECV_LAST		0x9f8c05
#define TAG_COMMS_RECV_MORE		0x9f8c06

/* Authentication */
#define TAG_AUTH_REQ			0x9f8200
#define TAG_AUTH_RESP			0x9f8201

/* Teletext */
#define TAG_TELETEXT_EBU		0x9f9000

/* Smartcard */
#define TAG_SMARTCARD_COMMAND		0x9f8e00
#define TAG_SMARTCARD_REPLY		0x9f8e01
#define TAG_SMARTCARD_SEND		0x9f8e02
#define TAG_SMARTCARD_RCV		0x9f8e03

/* EPG */
#define TAG_EPG_ENQUIRY         	0x9f8f00
#define TAG_EPG_REPLY           	0x9f8f01

#endif
