/* A simple filter (stdin -> stdout) to extract multiple streams from a
   multiplexed TS.  Specify the PID on the command-line 

   Updated 29th January 2003 - Added some error checking and reporting.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  int pid,n;
  int filters[8192];
  unsigned int i=0;
  unsigned int j=0;
  unsigned char buf[188];
  unsigned char my_cc[8192];
  int errors=0;
  FILE   *rapport;

  for (i=0;i<8192;i++) { filters[i]=0; my_cc[i]=0xff;}

  if((rapport=fopen("./pids.txt", "a")) != NULL)
    {
      n=fread(buf,1,188,stdin);
      i=1;
      while (n==188&&i<200000) {
	if (buf[0]!=0x47) {
	  // TO DO: Re-sync.
	  fprintf(stderr,"FATAL ERROR IN STREAM AT PACKET %d\n",i);
	  //      exit;
	}
	pid=(((buf[1] & 0x1f) << 8) | buf[2]);
	if (my_cc[pid]==0xff) my_cc[pid]=buf[3]&0x0f;

	filters[pid]++;
    
	n=fread(buf,1,188,stdin);
	i++;
      }
      for(pid=0;pid<8192;pid++)
	{
	  if(filters[pid])
	    fprintf(rapport,"Pid trouve : %d vu %d fois soit %f %% du flux total\n",pid,filters[pid],100.*filters[pid]/i);
	}
      fclose(rapport);
      fprintf(stderr,"Read %d packets, wrote %d.\n",i,j);
      fprintf(stderr,"%d incontinuity errors.\n",errors);
    }


  return(0);
}
