
#include <stdio.h>

#include "ts.h"

#include "mumudvb.h"
#include "log.h"
//#include "ts.h"

int Interrupted;



int main(void)
{

  printf("Testing program for MuMuDVB\n===========================\n" );
  printf("Display Ca system id 1\n" );

  printf("%s\n\n" ,ca_sys_id_to_str(1));


  printf("===========================\nTesting done\n" );

}
