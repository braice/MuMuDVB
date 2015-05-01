/*
 * unicast_EIT.c
 *
 *  Created on: 22 juin 2014
 *      Author: braice
 */


#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "dvb.h"
#include "tune.h"
#include "rewrite.h"
#include "autoconf.h"
#ifdef ENABLE_CAM_SUPPORT
#include "cam.h"
#endif
#ifdef ENABLE_SCAM_SUPPORT
#include "scam_capmt.h"
#include "scam_common.h"
#include "scam_getcw.h"
#include "scam_decsa.h"
#endif

static char *log_module="Unicast : ";
void eit_display_contents(mumudvb_ts_packet_t *full_eit, struct unicast_reply* reply);

void
unicast_send_EIT_section (mumudvb_ts_packet_t *eit_section, int num, struct unicast_reply* reply)
{

	unicast_reply_write(reply, "\n{\n");
	unicast_reply_write(reply, "\t\"number\" : %d,\n",num);
	eit_display_contents(eit_section, reply);
	unicast_reply_write(reply, "}");
}



char *running_status_descr[] ={
		"undefined",
		"not running",
		"starts in a few seconds",
		"pausing",
		"running",
		"service off-air",
		"reserved for future use",
		"reserved for future use",
};

void eit_display_descriptor(unsigned char *buf,int descriptors_loop_len, struct unicast_reply* reply);
void eit_show_short_evt_descr(unsigned char *buf, struct unicast_reply* reply);
void eit_show_ext_evt_descr(unsigned char *buf, struct unicast_reply* reply);
void eit_show_component_descr(unsigned char *buf, struct unicast_reply* reply);
void eit_show_parent_rating_descr(unsigned char *buf, struct unicast_reply* reply);
void eit_show_multiling_comp_descr(unsigned char *buf, struct unicast_reply* reply);
void eit_show_content_descriptor(unsigned char *buf, struct unicast_reply* reply);
void eit_show_CA_identifier_descriptor(unsigned char *buf, struct unicast_reply* reply);


#define MAX_DESCR_LEN 4096

// This function convert an UTF8 string into a JSON valid string, escaping the control characters.
// For the moment it works only with static memory allocation
//todo can realloc
void mumu_string_to_json(char *in, int lin, char *out, int lout)
{
	int offin, offout;
	char c;
	char hex[5];
	offin=0;
	offout=0;

	while((offin<lin) && (offout<lout) && (c=in[offin]))
	{
		if((c & 0x80) == 0) //ASCII bit
		{
			//Control character of " or / which are forbidden in JSON
			if(c<' ' || c=='"' || c == '/'|| c == '\\')
			{
		        switch(c)
		        {
		            case '\\': out[offout]='\\';offout++;out[offout]='\\';offout++; break;
		            case '\"': out[offout]='\\';offout++;out[offout]='\"';offout++; break;
		            case '\b': out[offout]='\\';offout++;out[offout]='\b';offout++; break;
		            case '\f': out[offout]='\\';offout++;out[offout]='\f';offout++; break;
		            case '\n': out[offout]='\\';offout++;out[offout]='\n';offout++; break;
		            case '\r': out[offout]='\\';offout++;out[offout]='\r';offout++; break;
		            case '\t': out[offout]='\\';offout++;out[offout]='\t';offout++; break;
		            case '/':  out[offout]='\\';offout++;out[offout]='/'; offout++; break;
		            default: //other control character
		            {
		            	out[offout]='\\';offout++;
		            	out[offout]='u';offout++;
		            	out[offout]='0';offout++;
		            	out[offout]='0';offout++;
		            	snprintf(hex,5,"%04x",c);
		            	out[offout]=hex[2];offout++;
		            	out[offout]=hex[3];offout++;
		            }
		        }
			}
			else
			{
				out[offout]=c;
				offout++;
			}

			offin++;
		}
		else if(((c & 0xE0) == 0xC0) &&
				((lout-offout)>3) &&
				((lin-offin)>3) &&
				((in[offin+1] & 0xC0) == 0x80) ) //Two byte (>3 for including the final '\0')
		{
			out[offout]=in[offin]; offout++; offin++;
			out[offout]=in[offin]; offout++; offin++;
		}
		else if(((c & 0xF0) == 0xE0) &&
				((lout-offout)>4) &&
				((lin-offin)>4) &&
				((in[offin+1] & 0xC0) == 0x80) &&
				((in[offin+2] & 0xC0) == 0x80) ) //Three byte
		{
			out[offout]=in[offin]; offout++; offin++;
			out[offout]=in[offin]; offout++; offin++;
			out[offout]=in[offin]; offout++; offin++;
		}
		else if(((c & 0xF8) == 0xF0) &&
				((lout-offout)>5) &&
				((lin-offin)>5) &&
				((in[offin+1] & 0xC0) == 0x80) &&
				((in[offin+2] & 0xC0) == 0x80) &&
				((in[offin+3] & 0xC0) == 0x80) ) //four byte
		{
			out[offout]=in[offin]; offout++; offin++;
			out[offout]=in[offin]; offout++; offin++;
			out[offout]=in[offin]; offout++; offin++;
			out[offout]=in[offin]; offout++; offin++;
		}
		else //Middle of byte sequence or invalid sequence we don't copy
			offin++;

	}
	if(offout<lout)
		out[offout]=0;
	else// Warn here
		out[lout]=0;

}


/** @brief Display the contents of the EIT table
 *
 */
void eit_display_contents(mumudvb_ts_packet_t *full_eit, struct unicast_reply* reply)
{
	eit_t       *eit=NULL;
	eit=(eit_t*)(full_eit->data_full);

	unicast_reply_write(reply, "\t\"length\" : %d,\n",full_eit->len_full);
	unicast_reply_write(reply, "\t\"table_id\" :%d,\n",eit->table_id);
	unicast_reply_write(reply, "\t\"section_length\" : %d,\n",HILO(eit->section_length));
	unicast_reply_write(reply, "\t\"section_syntax_indicator\" :%d,\n",eit->section_syntax_indicator);
	unicast_reply_write(reply, "\t\"service_id\" :%d,\n",HILO(eit->service_id));
	unicast_reply_write(reply, "\t\"current_next_indicator\" :%d,\n",eit->current_next_indicator);
	unicast_reply_write(reply, "\t\"version_number\" :%d,\n",eit->version_number);
	unicast_reply_write(reply, "\t\"section_number\" :%d,\n",eit->section_number);
	unicast_reply_write(reply, "\t\"last_section_number\" :%d,\n",eit->last_section_number);
	unicast_reply_write(reply, "\t\"transport_stream_id\" :%d,\n",HILO(eit->transport_stream_id));
	unicast_reply_write(reply, "\t\"original_network_id\" :%d,\n",HILO(eit->original_network_id));
	unicast_reply_write(reply, "\t\"segment_last_section_number\" :%d,\n",eit->segment_last_section_number);
	unicast_reply_write(reply, "\t\"segment_last_table_id\" :%d,\n",eit->segment_last_table_id);

	//Loop over different events in the EIT
	int len,delta,first;
	unsigned char *buf;
	eit_event_t *event_header;
	len=full_eit->len_full;
	buf=full_eit->data_full;
	delta=EIT_LEN;
	first=1;
	unicast_reply_write(reply, "\"EIT_events\":[\n");
	while((len-delta)>=(4+EIT_EVENT_LEN))
	{
		if(!first)
			unicast_reply_write(reply, ",");
		else
			first=0;
		unicast_reply_write(reply, "{\n");

		event_header=(eit_event_t *)(buf + delta );
		unicast_reply_write(reply, "\t\t\"event_id\" :%d,\n",HILO(event_header->event_id));
		//Compute and display start time
		//compute day
		int MJD;
		MJD=(event_header->start_time_0<<8)+(event_header->start_time_1);
		//MJD=0xC079; //1993-10-13
		//MJD=45218; //1993-10-13
		int YY,MM,D,Y,M,K; //see annex C ETSI EN 300 468
		YY=(int)((MJD-15078.2)/365.25);
		MM=(int)((MJD-14956.1-(int)(YY*365.25))/30.6001);
		D=MJD-14956-(int)(YY*365.25)-(int)(MM*30.6001);
		K=(MM==14 || MM==15)?1:0;
		Y=YY+K+1900;
		M=MM-1-K*12;

		unicast_reply_write(reply, "\t\t\"start_time day \" : \"%d-%02d-%02d (yy-mm-dd)\",\n",
						Y,M,D);
		//compute hours
		int hh,mm,ss;
		hh=(event_header->start_time_2>>4)*10+(event_header->start_time_2&0x0F);
		mm=(event_header->start_time_3>>4)*10+(event_header->start_time_3&0x0F);
		ss=(event_header->start_time_4>>4)*10+(event_header->start_time_4&0x0F);
		unicast_reply_write(reply, "\t\t\"start_time\" : \"%02d:%02d:%02d\",\n",hh,mm,ss);

		//Compute and display duration
		int dhh,dmm,dss;
		dhh=(event_header->duration_0>>4)*10+(event_header->duration_0&0x0F);
		dmm=(event_header->duration_1>>4)*10+(event_header->duration_1&0x0F);
		dss=(event_header->duration_2>>4)*10+(event_header->duration_2&0x0F);
		unicast_reply_write(reply, "\t\t\"duration\" : \"%02d:%02d:%02d\",\n",dhh,dmm,dss);



		unicast_reply_write(reply, "\t\t\"running_status\" :%d,\n",event_header->running_status);
		unicast_reply_write(reply, "\t\t\"running_status_descr\" : \"%s\",\n",running_status_descr[event_header->running_status]);
		unicast_reply_write(reply, "\t\t\"free_ca_mode\" :%d,\n",event_header->free_ca_mode);




		unicast_reply_write(reply, "\t\t\"descriptors_loop_length\" : %d,\n",HILO(event_header->descriptors_loop_length));


		eit_display_descriptor(buf+delta+EIT_EVENT_LEN,HILO(event_header->descriptors_loop_length),reply);
		delta+=HILO(event_header->descriptors_loop_length)+EIT_EVENT_LEN;
		unicast_reply_write(reply, "}");
	}
	unicast_reply_write(reply, "\n]\n");
}


/** @brief Display the contents of an EIT descriptor
 *
 */
void eit_display_descriptor(unsigned char *buf,int descriptors_loop_len, struct unicast_reply* reply)
{

	int first;
	first=1;
	unicast_reply_write(reply, "\"EIT_descriptors\":[\n");
	while (descriptors_loop_len > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;
		if(!first)
			unicast_reply_write(reply, ",");
		else
			first=0;
		unicast_reply_write(reply, "\n{\n");
		unicast_reply_write(reply, "\t\t\"tag\" :%d,\n",descriptor_tag);
		unicast_reply_write(reply, "\t\t\"len\" : %d",	descriptor_len);

		if (!descriptor_len)
		{
			unicast_reply_write(reply, "\n}\n");
			break;
		}

		//The service descriptor provides the names of the service provider and the service in text form together with the service_type.
		switch(descriptor_tag)
		{
		case 0x4d:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Short event descriptor\",\n");
			unicast_reply_write(reply, "\t\t\"short_evt\":{\n");
			eit_show_short_evt_descr(buf,reply);
			unicast_reply_write(reply, "}\n");
			break;
		case 0x4e:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Extended event descriptor\",\n");
			unicast_reply_write(reply, "\t\t\"ext_evt\":{\n");
			eit_show_ext_evt_descr(buf,reply);
			unicast_reply_write(reply, "}\n");
			break;
		case 0x4f:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Time shifted event descriptor\"\n");
			break;
		case 0x50:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Component descriptor\",\n");
			unicast_reply_write(reply, "\t\t\"component\":{\n");
			eit_show_component_descr(buf,reply);
			unicast_reply_write(reply, "}\n");
			break;
		case 0x53:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"CA identifier descriptor\",\n");
			unicast_reply_write(reply, "\t\t\"CA_identifier\":{\n");
			eit_show_CA_identifier_descriptor(buf,reply);
			unicast_reply_write(reply, "}\n");
			break;
		case 0x54:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Content descriptor (type of program cf tabl 28 EN 300 468 V1.13.1)\",\n");
			unicast_reply_write(reply, "\t\t\"content\":{\n");
			eit_show_content_descriptor(buf,reply);
			unicast_reply_write(reply, "}\n");
			break;
		case 0x55:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Parental rating descriptor\",\n");
			unicast_reply_write(reply, "\t\t\"parental_rating\":[\n");
			eit_show_parent_rating_descr(buf,reply);
			unicast_reply_write(reply, "]\n");
			break;
		case 0x57:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Telephone descriptor\"\n");
			break;
		case 0x5E:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Multilingual component descriptor\",\n");
			unicast_reply_write(reply, "\t\t\"multiling_component\":{\n");
			eit_show_multiling_comp_descr(buf,reply);
			unicast_reply_write(reply, "}\n");
			break;
		case 0x5F:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Private data specifier descriptor\"\n");
			break;
		case 0x61:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Short smoothing buffer descriptor\"\n");
			break;
		case 0x69:
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"PDC descriptor\"\n");
			break;
		default: //0X42 0X4A 0X64 0X75 0X76 7D 7E 7F
			unicast_reply_write(reply, ",\n\t\t\"descr\" : \"Unknown descriptor\"\n");
			break;
		}
		buf += descriptor_len;
		descriptors_loop_len -= descriptor_len;

		unicast_reply_write(reply, "}");
	}
	unicast_reply_write(reply, "]\n");

}

/** @brief : show the contents of the CA identifier descriptor
 *
 * @param buf : the buffer containing the descriptor
 */
void eit_show_CA_identifier_descriptor(unsigned char *buf, struct unicast_reply* reply)
{

	int length,i,ca_id;


	unicast_reply_write(reply, "\t\t\"CA_system_id\":[\n");

	length=buf[1];
	buf+=2;
	int first = 1;
	for(i=0;i<length;i+=2)
	{
		if(first)
		{
			unicast_reply_write(reply, "{\n");
			first = 0;
		}
		else
			unicast_reply_write(reply, ",\n{\n");
		ca_id=(buf[i]<<8)+buf[i+1];
		unicast_reply_write(reply, "\t\t\t\"id\" :%d,\n",ca_id);
		unicast_reply_write(reply, "\t\t\t\"descr\" : \"%s\"}\n", ca_sys_id_to_str(ca_id));
	}
	unicast_reply_write(reply, "]\n");
}


void eit_show_content_descriptor(unsigned char *buf, struct unicast_reply* reply)
{

	int delta=2;
	unicast_reply_write(reply, "\t\t\"content_nibble_lvl_1\" :%d,\n",buf[2]>>4);
	unicast_reply_write(reply, "\t\t\"content_nibble_lvl_2\" :%d,\n",buf[2]&0xF);
	delta++;
	unicast_reply_write(reply, "\t\t\"user byte\" :%d\n",buf[delta]);
}

void eit_show_parent_rating_descr(unsigned char *buf, struct unicast_reply* reply)
{



	int delta,length;
	delta=2;
	length=(buf[1]);
	while(length>0)
	{
		if (delta>2)
			unicast_reply_write(reply, ",\n");
		unicast_reply_write(reply, "\t\t{\n\t\t\t\"language\" : \"%c%c%c\",\n",buf[0+delta],buf[1+delta],buf[2+delta]);
		delta+=3;
		if(buf[delta]==0)
			unicast_reply_write(reply, "\t\t\"rating\" : \"undefined\"\n");
		else if(buf[delta]<=0X0F)
			unicast_reply_write(reply, "\t\t\"rating\" : \"minimum age %d\"\n",buf[delta]+3);
		else
			unicast_reply_write(reply, "\t\t\"rating\" : \"defined by broadcaster\"\n");
		unicast_reply_write(reply, "\t\t}");
		delta++;
		length-=4;
	}

}

void eit_show_multiling_comp_descr(unsigned char *buf, struct unicast_reply* reply)
{

	char text[MAX_DESCR_LEN];
	char text2[MAX_DESCR_LEN];

	int delta=2;
	int length;
	int text_length;
	length=(buf[1])-1;
	unicast_reply_write(reply, "\t\t\"component_tag\" :%d,\n",buf[delta]);
	delta++;
	unicast_reply_write(reply, "\t\t\"components\":[\n");
	int first = 1;
	while(length>0)
	{
		if(first)
		{
			unicast_reply_write(reply, "{\n");
			first = 0;
		}
		else
			unicast_reply_write(reply, ",\n{\n");
		unicast_reply_write(reply, "\t\t\"language\" : \"%c%c%c\",\n",buf[0+delta],buf[1+delta],buf[2+delta]);
		delta+=3;
		length-=4;
		text_length=buf[delta];
		delta++;
		memcpy(text,buf+delta,text_length*sizeof(char));
		text[text_length]='\0';
		convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
		unicast_reply_write(reply, "\t\t\"text\" : \"%s\"}", text);
		delta+=text_length;
		length-=text_length;
	}
	unicast_reply_write(reply, "]\n");
}

void eit_show_short_evt_descr(unsigned char *buf, struct unicast_reply* reply)
{

	int delta=2;
	unicast_reply_write(reply, "\t\t\"language\" : \"%c%c%c\",\n",buf[0+delta],buf[1+delta],buf[2+delta]);
	delta+=3;
	char text[MAX_DESCR_LEN];
	char text2[MAX_DESCR_LEN];
	int text_length;
	text_length=buf[delta]&0xff;
	delta++;
	memcpy(text,buf+delta,text_length*sizeof(char));
	text[text_length]='\0';
	convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
	unicast_reply_write(reply, "\t\t\"name\" : \"%s\",\n", text2);
	delta+=text_length;
	text_length=buf[delta]&0xff;
	delta++;
	memcpy(text,buf+delta,text_length*sizeof(char));
	text[text_length]='\0';
	convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
	unicast_reply_write(reply, "\t\t\"text\" : \"%s\"\n", text2);
}

void eit_show_ext_evt_descr(unsigned char *buf, struct unicast_reply* reply)
{
	int delta;
	delta=2;
	unicast_reply_write(reply, "\t\t\"descr_number\" : %d ,\n",(buf[delta]>>4)+1);
	unicast_reply_write(reply, "\t\t\"descr_total_number\" : %d,\n",(buf[delta]&0xF)+1);
	delta++;
	unicast_reply_write(reply, "\t\t\"language\" : \"%c%c%c\",\n",buf[0+delta],buf[1+delta],buf[2+delta]);
	delta+=3;
	int length_of_items,item_description_length,item_length,text_length;
	length_of_items=buf[delta]&0xff;
	delta++;
	char text[MAX_DESCR_LEN];
	char text2[MAX_DESCR_LEN];

	unicast_reply_write(reply, "\t\t\"items\":[");
	int first = 1;
	while(length_of_items>0)
	{
		if(first)
		{
			unicast_reply_write(reply, "{\n");
			first = 0;
		}
		else
			unicast_reply_write(reply, ",\n{\n");
		item_description_length=buf[delta]&0xff;
		delta++;
		memcpy(text,buf+delta,item_description_length*sizeof(char));
		text[item_description_length]='\0';
		convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
		unicast_reply_write(reply, "\t\t\"item_descr\" : \"%s\",\n", text2);
		delta+=item_description_length;
		item_length=buf[delta]&0xff;
		delta++;
		memcpy(text,buf+delta,item_length*sizeof(char));
		text[item_length]='\0';
		convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
		unicast_reply_write(reply, "\t\t\"item\" : \"%s\"}\n", text2);
		delta+=item_length;
		length_of_items-=(2+item_description_length+item_length);
	}
	unicast_reply_write(reply, "],\n");
	text_length=buf[delta]&0xff;
	delta++;
	memcpy(text,buf+delta,text_length*sizeof(char));
	text[text_length]='\0';
	convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
	unicast_reply_write(reply, "\t\t\"text\": \"%s\"\n", text2);
}

void eit_show_component_descr(unsigned char *buf, struct unicast_reply* reply)
{
	int delta;
	delta=2;

	unicast_reply_write(reply, "\t\t\"len\" : %d",buf[1]&0xff);
	if((buf[1]&0xff)>5)
	{
		unicast_reply_write(reply, ",\n");
		//stream_content
		// see EN 300 468 V1.13.1 table 26
		unicast_reply_write(reply, "\t\t\"stream_content\" :%d,\n", buf[delta]&0x0F);
		delta++;
		// see EN 300 468 V1.13.1 table 26
		unicast_reply_write(reply, "\t\t\"component_type\" :%d,\n", buf[delta]);
		delta++;
		unicast_reply_write(reply, "\t\t\"component_tag\" :%d,\n", buf[delta]);
		delta++;
		unicast_reply_write(reply, "\t\t\"language\" : \"%c%c%c\",\n",buf[0+delta],buf[1+delta],buf[2+delta]);
		delta+=3;
		int text_length;
		char text[MAX_DESCR_LEN];
		char text2[MAX_DESCR_LEN];
		text_length=(buf[1])-5;
		memcpy(text,buf+delta,text_length*sizeof(char));
		text[text_length]='\0';
		convert_en300468_string(text, MAX_DESCR_LEN,0);mumu_string_to_json(text,MAX_DESCR_LEN,text2,MAX_DESCR_LEN);
		unicast_reply_write(reply, "\t\t\"text\" : \"%s\"\n", text2);
	}
	else
		unicast_reply_write(reply, "\n");

}

