
#define FILE_TEST_READ_SDT_TS1 "tests/TestDump17.ts"
#define NUM_READ_SDT 200
#define FILES_TEST_READ_RAND "tests/random_1.ts","tests/random_2.ts"
#define NUM_FILES_TEST_READ_RAND 2
#define TEST_STRING_COMPUT "2+2*3+100"


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


//Functions implemented here
void autoconf_print_services(mumudvb_service_t *services);
int autoconf_count_services(mumudvb_service_t *services);




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


  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Testing program for MuMuDVB\n");
  log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
  getchar();

  /****************************  Very basic test ****************************************************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Display Ca system id 1\n" );
  log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
  getchar();
  log_message( log_module, MSG_INFO,"%s\n\n" ,ca_sys_id_to_str(1));

  /****************************  Testing string compute *********************************************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Testing string compute %s\n",TEST_STRING_COMPUT );
  log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
  getchar();
  log_message( log_module, MSG_INFO,"%d\n\n" ,string_comput(TEST_STRING_COMPUT));

  /************************************* Testing the SDT parser *************************************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"Testing TS read for SDT - test1\n" );
  log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
  getchar();
  FILE *testfile;
  testfile=fopen (FILE_TEST_READ_SDT_TS1, "r");
  if(testfile!=NULL)
    {
      //We read all the packets contained in the file
      unsigned char ts_packet_raw[188];
      int num_sdt_read=0;
      mumudvb_ts_packet_t ts_packet_mumu;
      ts_packet_mumu.empty=1;
      mumudvb_service_t services;
      memset(&services, 0, sizeof(mumudvb_service_t));
      //Just to make pthread happy
      pthread_mutex_init(&ts_packet_mumu.packetmutex,NULL);
      int iRet,pid;
      log_message( log_module, MSG_INFO,"File opened, reading packets\n" );
      while(fread(ts_packet_raw,188,1, testfile) && num_sdt_read<NUM_READ_SDT)
	{
	  log_message( log_module, MSG_DEBUG,"New elementary (188bytes TS packet)\n" );
	  // Get the PID of the received TS packet
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
	  pid = ((ts_packet_raw[1] & 0x1f) << 8) | (ts_packet_raw[2]);
	  if(pid != 17) continue;
	  iRet=get_ts_packet(ts_packet_raw, &ts_packet_mumu);
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
      //We free starting at the next since the first is not malloc'ed
      autoconf_free_services(services.next);
      fclose(testfile);

    }
  else
    log_message( log_module, MSG_INFO,"Test file %s cannot be open : %s\n", FILE_TEST_READ_SDT_TS1,strerror(errno) );




  /************************************* Testing the resistance to strange data *********************/
  log_message( log_module, MSG_INFO,"===================================================================\n");
  char *files[NUM_FILES_TEST_READ_RAND]={FILES_TEST_READ_RAND};
  for(int i_file=0;i_file<NUM_FILES_TEST_READ_RAND;i_file++)
    {
      log_message( log_module, MSG_INFO,"Testing Resistance to bad packets file %d on %d : %s\n", i_file+1, NUM_FILES_TEST_READ_RAND, files[i_file] );
      log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
      getchar();
  
      testfile=fopen (files[i_file] , "r");
      if(testfile!=NULL)
	{
	  //We read all the packets contained in the file
	  unsigned char ts_packet_raw[188];
	  int num_rand_read=0;
	  mumudvb_ts_packet_t ts_packet_mumu;
	  ts_packet_mumu.empty=1;
	  //Just to make pthread happy
	  pthread_mutex_init(&ts_packet_mumu.packetmutex,NULL);
	  int iRet;
	  fpos_t pos;
	  log_message( log_module, MSG_INFO,"File opened, reading packets\n" );
	  while(fread(ts_packet_raw,188,1, testfile))
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
	log_message( log_module, MSG_INFO,"Test file %s cannot be open : %s\n", files[i_file], strerror(errno) );
    }
  /**************************************************************************************************/
  //log_message( log_module, MSG_INFO,"===================================================================\n");
  //log_message( log_module, MSG_INFO,"================= Press enter to continue =========================\n");
  //getchar();
  /**************************************************************************************************/
  /**************************************************************************************************/
  /**************************************************************************************************/
  /**************************************************************************************************/

  log_message( log_module, MSG_INFO,"===================================================================\n");
  log_message( log_module, MSG_INFO,"=========================== Testing done ==========================\n");

}


void autoconf_print_services(mumudvb_service_t *services)
{

  mumudvb_service_t *act_service;
  act_service=services;
  while(act_service!=NULL)
    {
      log_message( log_module, MSG_INFO,"Services listing\n");
      log_message( log_module, MSG_INFO,"Service : id %d running_status %d free_ca_mode %d\n",
		   act_service->id, act_service->running_status, act_service->free_ca_mode);
      log_message( log_module, MSG_INFO,"running_status %s\n",
		   act_service->name);
      display_service_type(act_service->type, MSG_DEBUG,log_module);
      
      act_service=act_service->next;
    }
}

