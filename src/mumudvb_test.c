/*
 * MuMuDVB - Stream a DVB transport stream.
 * Testing suite
 *
 * (C) 2010 Brice DUBOST <mumudvb@braice.net>
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


/* The test files can be found here :
http://mumudvb.braice.net/mumudvb/test/
*/

// To compile this code, run "make check"

#define PRESS_ENTER 0

#define FILES_TEST_READ_SDT_TS "tests/BBC123_pids0_18.dump.ts", "tests/TestDump17.ts"//,"tests/test_autoconf_numericableparis_PAT_SDT.ts","tests/astra_TP_11856.00V_PAT_SDT.ts"
#define NUM_READ_SDT 50
#define NUM_FILES_TEST_READ_SDT 2
#define FILES_TEST_READ_RAND "tests/random_1.ts","tests/random_2.ts"
#define NUM_FILES_TEST_READ_RAND 0
#define TEST_STRING_COMPUT "2+2*3+100"
#define TEST_STRING_COMPUT_RES 108

#define FILES_TEST_AUTOCONF "tests/astra_TP_11856.00V_pids_0_18.ts","tests/test_autoconf_numericableparis.ts","tests/astra_TP_11856.00V_pids_0_18__2.ts","tests/BBC123_pids0_18.dump.ts","tests/BBC123.dump.ts"
#define NUM_FILES_TEST_AUTOCONF 0

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ts.h"

#include "mumudvb.h"
#include "log.h"
#include "autoconf.h"
#include "rewrite.h"

//Prototypes
void autoconf_free_services(mumudvb_service_t *services);
int autoconf_read_sdt(unsigned char *buf,int len, mumudvb_service_t *services);
void autoconf_sort_services(mumudvb_service_t *services);


//Functions implemented here
void autoconf_print_services(mumudvb_service_t *services);
int autoconf_count_services(mumudvb_service_t *services);




int Interrupted;
long real_start_time;
multicast_parameters_t multicast_vars;
extern log_params_t log_params;

static char *log_module="======TEST======: ";
void press_enter_func(int press_enter)
{
    if(press_enter)
    {
      log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
      getchar();
    }
    else
      log_message( log_module, MSG_INFO,"===================================================================\n");
}

int main(void)
{
  int press_enter = PRESS_ENTER;

  //We initalise the logging parameters
  log_params.verbosity = 999;
  log_params.log_type=LOGGING_CONSOLE;


  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Testing program for MuMuDVB\n");
  log_message( log_module, MSG_INFO,"===================================================================\n");

  /****************************  Very basic test ****************************************************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Display Ca system id 1\n" );
  log_message( log_module, MSG_INFO,"===================================================================\n");

  log_message( log_module, MSG_INFO,"%s\n\n" ,ca_sys_id_to_str(1));

  /****************************  Testing string compute *********************************************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Testing string compute %s\n",TEST_STRING_COMPUT );
  log_message( log_module, MSG_INFO,"===================================================================\n");

  int resultat;
  resultat=string_comput(TEST_STRING_COMPUT);
  if(resultat==TEST_STRING_COMPUT_RES)
    log_message( log_module, MSG_INFO,"%d  --  PASS\n\n" ,resultat);
  else
    log_message( log_module, MSG_INFO,"%d  --  FAIL\n\n" ,resultat);

  /************************************* Testing the SDT parser *************************************/
  char *files_sdt[NUM_FILES_TEST_READ_SDT]={FILES_TEST_READ_SDT_TS};
  for(int i_file=0;i_file<NUM_FILES_TEST_READ_SDT;i_file++)
  {
    log_message( log_module, MSG_INFO,"===================================================================\n");
    log_message( log_module, MSG_INFO,"Testing TS read for SDT - test %d on %d file %s\n", i_file+1, NUM_FILES_TEST_READ_SDT , files_sdt[i_file] );
    press_enter_func(press_enter);
    FILE *testfile;
    testfile=fopen (files_sdt[i_file], "r");
    if(testfile!=NULL)
      {
        //We read all the packets contained in the file
        unsigned char ts_packet_raw[TS_PACKET_SIZE];
        int num_sdt_read=0;
        mumudvb_ts_packet_t ts_packet_mumu;
        memset(&ts_packet_mumu, 0, sizeof(mumudvb_ts_packet_t));

        mumudvb_service_t services;
        memset(&services, 0, sizeof(mumudvb_service_t));
        //Just to make pthread happy
        pthread_mutex_init(&ts_packet_mumu.packetmutex,NULL);
        int iRet,pid;
        log_message( log_module, MSG_INFO,"File opened, reading packets\n" );
        while(fread(ts_packet_raw,TS_PACKET_SIZE,1, testfile) && num_sdt_read<NUM_READ_SDT)
          {
            /************ SYNC *************/
            if(ts_packet_raw[0] != 0x47)
              {
                log_message( log_module, MSG_INFO," !!!!!!!!!!!!! Sync error, we search for a new sync byte !!!!!!!!!!!!!!\n");
                unsigned char sync;
                //We search the next sync byte
                while(fread(&sync,1,1, testfile) && sync!=0x47);
                ts_packet_raw[0]=sync;
                //We found a "sync byte" we get the rest of the packet
                if(!fread(ts_packet_raw-1,188-1,1, testfile))
                  continue;
                else
                  log_message( log_module, MSG_INFO," sync byte found :) \n");
              }
            // Get the PID of the received TS packet
            pid = HILO(((ts_header_t *)ts_packet_raw)->pid);
            log_message( log_module, MSG_DEBUG,"New elementary (188bytes TS packet) pid %d continuity_counter %d",
                         pid,
                         ((ts_header_t *)ts_packet_raw)->continuity_counter );
            if(pid != 17) continue;
            iRet=get_ts_packet(ts_packet_raw, &ts_packet_mumu);
            //If it's the beginning of a new packet we display some information
            if(((ts_header_t *)ts_packet_raw)->payload_unit_start_indicator)
              log_message(log_module, MSG_FLOOD, "First two bytes of the packet 0x%02x %02x",
                          ts_packet_mumu.packet[0],
                          ts_packet_mumu.packet[1]);

            if(iRet==1)//packet is parsed
              {
                log_message( log_module, MSG_INFO,"New packet -- parsing\n" );
                num_sdt_read++;
                autoconf_read_sdt(ts_packet_mumu.packet,ts_packet_mumu.len,&services);
                ts_packet_mumu.empty=1;
              }
          }
        log_message( log_module, MSG_INFO,"Final services list .... \n");
        autoconf_print_services(&services);
        log_message( log_module, MSG_INFO,"===================================================================\n");
        log_message( log_module, MSG_INFO,"Testing service sorting on this list\n" );
        press_enter_func(press_enter);
        autoconf_sort_services(&services);
        autoconf_print_services(&services);
        //We free starting at the next since the first is not malloc'ed
        autoconf_free_services(services.next);
        fclose(testfile);

      }
    else
      log_message( log_module, MSG_INFO,"Test file %s cannot be open : %s\n", files_sdt[i_file],strerror(errno) );

  }


  /************************************* Testing the resistance to strange data *********************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  char *files_rand[NUM_FILES_TEST_READ_RAND]={FILES_TEST_READ_RAND};
  for(int i_file=0;i_file<NUM_FILES_TEST_READ_RAND;i_file++)
    {
      log_message( log_module, MSG_INFO,"Testing Resistance to bad packets file %d on %d : %s\n", i_file+1, NUM_FILES_TEST_READ_RAND, files_rand[i_file] );
      press_enter_func(press_enter);
      FILE *testfile;
      testfile=fopen (files_rand[i_file] , "r");
      if(testfile!=NULL)
	{
	  //We read all the packets contained in the file
	  unsigned char ts_packet_raw[TS_PACKET_SIZE];
	  int num_rand_read=0;
	  mumudvb_ts_packet_t ts_packet_mumu;
	  ts_packet_mumu.empty=1;
	  //Just to make pthread happy
	  pthread_mutex_init(&ts_packet_mumu.packetmutex,NULL);
	  int iRet;
	  log_message( log_module, MSG_INFO,"File opened, reading packets\n" );
	  while(fread(ts_packet_raw,TS_PACKET_SIZE,1, testfile))
	    {
	      num_rand_read++;
	      log_message( log_module, MSG_INFO,"Position %d packets\n", num_rand_read);
	      iRet=get_ts_packet(ts_packet_raw, &ts_packet_mumu);
	      if(iRet==1)//packet is parsed
		{
		  log_message( log_module, MSG_INFO,"New VALID packet\n" );
		  ts_packet_mumu.empty=1;
		}
	    }
	  fclose(testfile);
	}
      else
	log_message( log_module, MSG_INFO,"Test file %s cannot be open : %s\n", files_rand[i_file], strerror(errno) );
    }
  /************************ Testing autoconfiguration with a dump **********************************/
  char *files_autoconf[NUM_FILES_TEST_AUTOCONF]={FILES_TEST_AUTOCONF};
  for(int i_file=0;i_file<NUM_FILES_TEST_AUTOCONF;i_file++)
  {

    log_message( log_module, MSG_INFO,"===================================================================\n");
    log_message( log_module, MSG_INFO,"Testing autoconfiguration file %d on %d %s\n",i_file+1, NUM_FILES_TEST_AUTOCONF, files_autoconf[i_file]);
    press_enter_func(press_enter);

    FILE *testfile;
    testfile=fopen (files_autoconf[i_file] , "r");
    if(testfile!=NULL)
    {
      int iret;
      mumudvb_chan_and_pids_t chan_and_pids;
      memset(&chan_and_pids,0,sizeof(mumudvb_chan_and_pids_t));
      chan_and_pids.number_of_channels=0;

      //autoconfiguration
      autoconf_parameters_t autoconf_vars;
      memset(&autoconf_vars,0,sizeof(autoconf_parameters_t));
      autoconf_vars.autoconfiguration=AUTOCONF_MODE_FULL;
      autoconf_vars.autoconf_radios=1;
      autoconf_vars.autoconf_scrambled=1;
      strcpy (autoconf_vars.autoconf_ip4,"239.100.%card.%number");
      autoconf_vars.transport_stream_id=-1;


      unicast_parameters_t unicast_vars;
      memset(&unicast_vars,0,sizeof(unicast_parameters_t));
      unicast_vars.unicast=0;

      //multicast parameters
      multicast_parameters_t multicast_vars;
      memset(&multicast_vars,0,sizeof(multicast_parameters_t));
      multicast_vars.multicast=0;

      fds_t fds;
      memset(&fds,0,sizeof(fds_t));
      tuning_parameters_t tuneparams;
      memset(&tuneparams,0,sizeof(tuning_parameters_t));

      iret=autoconf_init(&autoconf_vars, chan_and_pids.channels,chan_and_pids.number_of_channels);
      if(iret)
      {
        log_message( log_module, MSG_INFO,"error with autoconfiguration init\n");
      }

      unsigned char actual_ts_packet[TS_PACKET_SIZE];
      int pid;
      while(fread(actual_ts_packet,TS_PACKET_SIZE,1, testfile))
      {
        // get the pid of the received ts packet
        pid = ((actual_ts_packet[1] & 0x1f) << 8) | (actual_ts_packet[2]);
        log_message( log_module, MSG_DEBUG,"New elementary (188bytes TS packet) pid %d continuity_counter %d",
                         pid,
                         ((ts_header_t *)actual_ts_packet)->continuity_counter );

        iret = autoconf_new_packet(pid, actual_ts_packet, &autoconf_vars,  &fds, &chan_and_pids, &tuneparams, &multicast_vars, &unicast_vars, 0);

      }
      fclose(testfile);

      log_message( log_module, MSG_INFO,"============ DONE we display the services ========\n");
      if(autoconf_vars.services)
      {
        autoconf_sort_services(autoconf_vars.services);
        autoconf_print_services(autoconf_vars.services);
      }
      else
        log_message( log_module, MSG_INFO,"No services or services freed\n");

      log_message( log_module, MSG_INFO,"============ DONE we display the channels ========\n");
      if(chan_and_pids.number_of_channels)
      {
        log_streamed_channels(log_module,chan_and_pids.number_of_channels, chan_and_pids.channels, 1, 0, 0, 0,"");
        //We can tests other things here like REWRITE etc ....
        log_message( log_module, MSG_INFO,"===================================================================\n");
        log_message( log_module, MSG_INFO,"Testing SDT rewrite on this file (%s)\n", files_autoconf[i_file]);
        press_enter_func(press_enter);
        FILE *testfile;
        testfile=fopen (files_autoconf[i_file] , "r");
        if(testfile!=NULL)
        {
          //Parameters for rewriting
          rewrite_parameters_t rewrite_vars={
            .rewrite_pat = OPTION_OFF,
            .pat_version=-1,
            .full_pat=NULL,
            .pat_needs_update=1,
            .full_pat_ok=0,
            .pat_continuity_counter=0,
            .rewrite_sdt = OPTION_ON,
            .sdt_version=-1,
            .full_sdt=NULL,
            .sdt_needs_update=1,
            .full_sdt_ok=0,
            .sdt_continuity_counter=0,
            .eit_sort=OPTION_OFF,
          };
          for (int curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
            chan_and_pids.channels[curr_channel].generated_sdt_version=-1;
          rewrite_vars.full_sdt=malloc(sizeof(mumudvb_ts_packet_t));
          if(rewrite_vars.full_sdt==NULL)
          {
            log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          }
          memset (rewrite_vars.full_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
          pthread_mutex_init(&rewrite_vars.full_sdt->packetmutex,NULL);

          while(fread(actual_ts_packet,TS_PACKET_SIZE,1, testfile))
          {
            // get the pid of the received ts packet
            pid = ((actual_ts_packet[1] & 0x1f) << 8) | (actual_ts_packet[2]);
            if( (pid == 17) ) //This is a SDT PID
              {
                //we check the new packet and if it's fully updated we set the skip to 0
                if(sdt_rewrite_new_global_packet(actual_ts_packet, &rewrite_vars)==1)
                {
                  log_message( log_module, MSG_DETAIL,"The SDT version changed, we force the update of all the channels.\n");
                  for (int curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
                    chan_and_pids.channels[curr_channel].sdt_rewrite_skip=0;
                }
                for (int curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
                {
                  if(!chan_and_pids.channels[curr_channel].sdt_rewrite_skip ) //AND the generation was successful
                    sdt_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_and_pids.channels[curr_channel], curr_channel);
                }
              }

          }
          fclose(testfile);
          log_message( log_module, MSG_INFO,"===================================================================\n");
          log_message( log_module, MSG_INFO,"End of testing SDT rewritef\n");
          log_message( log_module, MSG_INFO,"===================================================================\n");
        }

      }
      else
        log_message( log_module, MSG_INFO,"No channels generated by the autoconfiguration\n");
      autoconf_freeing(&autoconf_vars);
    }
    else
      log_message( log_module, MSG_INFO,"Test file %s cannot be open : %s\n", files_rand[i_file], strerror(errno) );

  }

  /**************************************************************************************************/

  /**************************************************************************************************/
  //log_message( log_module, MSG_INFO,"===================================================================\n");
  //press_enter_func(press_enter);
  /**************************************************************************************************/
  /**************************************************************************************************/

  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"=========================== Testing done ==========================\n");

}


void autoconf_print_services(mumudvb_service_t *services)
{

  mumudvb_service_t *act_service;
  if(services)
    act_service=services->next;
  else act_service=NULL;
  while(act_service!=NULL)
    {
      log_message( log_module, MSG_INFO,"Services listing\n");
      log_message( log_module, MSG_INFO,"Service : id %d running_status %d free_ca_mode %d\n",
		   act_service->id, act_service->running_status, act_service->free_ca_mode);
      log_message( log_module, MSG_INFO,"name %s\n",
		   act_service->name);
      log_message( log_module, MSG_INFO,"pmt_pid %d\n",
                   act_service->pmt_pid);
      display_service_type(act_service->type, MSG_DEBUG,log_module);
      act_service=act_service->next;
    }
}

