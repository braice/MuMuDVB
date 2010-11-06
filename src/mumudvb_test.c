
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ts.h"

#include "mumudvb.h"
#include "log.h"
#include "autoconf.h"

//Prototypes
void autoconf_free_services(mumudvb_service_t *services);
int autoconf_read_sdt(unsigned char *buf,int len, mumudvb_service_t *services);
//#include "ts.h"

#define FILE_TEST_READ_SDT_TS1 "tests/TestDump17.ts"

int Interrupted;
long real_start_time;
multicast_parameters_t multicast_vars;


extern log_params_t log_params;

static char *log_module="======TEST======: ";


int main(void)
{

  //We initalise the logging parameters
  log_params.verbosity = 999;
  log_params.log_type=LOGGING_CONSOLE;


  log_message( log_module, MSG_INFO,"Testing program for MuMuDVB\n===========================\n" );
  log_message( log_module, MSG_INFO,"Display Ca system id 1\n" );

  log_message( log_module, MSG_INFO,"%s\n\n" ,ca_sys_id_to_str(1));

  log_message( log_module, MSG_INFO,"Testing TS read for SDT - test1\n" );
  FILE *testfile;
  testfile=fopen (FILE_TEST_READ_SDT_TS1, "r");
  if(testfile!=NULL)
    {
      //We read all the packets contained in the file
      unsigned char ts_packet_raw[188];
      mumudvb_ts_packet_t ts_packet_mumu;
      mumudvb_service_t services;
      mumudvb_service_t *act_service;
      memset(&services, 0, sizeof(mumudvb_service_t));
      //Just to make pthread happy
      pthread_mutex_init(&ts_packet_mumu.packetmutex,NULL);
      int iRet;
      log_message( log_module, MSG_INFO,"File opened, reading packets\n" );
      while(fread(ts_packet_raw,188,1, testfile))
	{
	  iRet=get_ts_packet(ts_packet_raw, &ts_packet_mumu);
	  log_message( log_module, MSG_INFO,"New packet -- parsing\n" );
	  if(iRet==1)//packet is parsed
	    {
	      autoconf_read_sdt(ts_packet_mumu.packet,ts_packet_mumu.len,&services);
	      act_service=&services;
	      while(act_service!=NULL)
		{
		  log_message( log_module, MSG_INFO,"New service : id %d running_status %d free_ca_mode %d\n",
			 act_service->id, act_service->running_status, act_service->free_ca_mode);
		  log_message( log_module, MSG_INFO,"type %d running_status %s\n",
			 act_service->type, act_service->name);
		  //display_service_type(service->type, MSG_DEBUG,log_module);
		  
		  act_service=act_service->next;
		}
	      
	    }
	}
      //We free starting at the next since the first is not malloc'ed
      autoconf_free_services(services.next);
    }
  else
    log_message( log_module, MSG_INFO,"Test file %s cannot be open : %s\n", FILE_TEST_READ_SDT_TS1,strerror(errno) );



  log_message( log_module, MSG_INFO,"===========================\nTesting done\n" );

}
