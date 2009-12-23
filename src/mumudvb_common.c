/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2009 Brice DUBOST
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
 * @brief This file contains some common functions
 */


#include "mumudvb.h"
#include "log.h"
#include "errors.h"

#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>



/** @brief : poll the file descriptors fds with a limit in the number of errors
 *
 * @param fds : the file descriptors
 */
int mumudvb_poll(fds_t *fds)
{
  int poll_try;
  int poll_eintr=0;
  int last_poll_error;
  int Interrupted;

  poll_try=0;
  poll_eintr=0;
  last_poll_error=0;
  while((poll (fds->pfds, fds->pfdsnum, 500)<0)&&(poll_try<MAX_POLL_TRIES))
  {
    if(errno != EINTR) //EINTR means Interrupted System Call, it normally shouldn't matter so much so we don't count it for our Poll tries
    {
      poll_try++;
      last_poll_error=errno;
    }
    else
    {
      poll_eintr++;
      if(poll_eintr==10)
      {
        log_message( MSG_DEBUG, "Poll : 10 successive EINTR\n");
        poll_eintr=0;
      }
    }
    /**@todo : put a maximum number of interrupted system calls per unit time*/
  }

  if(poll_try==MAX_POLL_TRIES)
  {
    log_message( MSG_ERROR, "Poll : We reach the maximum number of polling tries\n\tLast error when polling: %s\n", strerror (errno));
    Interrupted=errno<<8; //the <<8 is to make difference beetween signals and errors;
    return Interrupted;
  }
  else if(poll_try)
  {
    log_message( MSG_WARN, "Poll : Warning : error when polling: %s\n", strerror (last_poll_error));
  }
  return 0;
}

/** @brief replace a tring by another
* @param source
* @param length the length of the source buffer
* @param can_realloc Is the source string allocated by a malloc or fixed. The realloc is done only when the dest is bigger
* @param toreplace the pattern to replace
* @param replacement the replacement string for the pattern
*/
char *mumu_string_replace(char *source, int *length, int can_realloc, char *toreplace, char *replacement)
{
  char *pospattern;
  char *reallocresult;
  char *tempstring=NULL;
  int lengthpattern;
  int lengthreplacment;
  int lengthtempstring;
  int lengthsource;

  pospattern=strstr(source,toreplace);
  if(pospattern==NULL)
    return source;
  lengthpattern=strlen(toreplace);
  lengthreplacment=strlen(replacement);
  lengthsource=strlen(source);
  lengthtempstring=lengthsource+1;
  tempstring=malloc(sizeof(char)*lengthtempstring);
  strcpy(tempstring,source);
  pospattern=strstr(tempstring,toreplace);
  while(pospattern!=NULL)
  {
    if(lengthreplacment>lengthpattern)
    {
      tempstring=realloc(tempstring,sizeof(char)*(lengthtempstring+lengthreplacment-lengthpattern+1));
      pospattern=strstr(tempstring,toreplace);
    }
    memmove(pospattern+lengthreplacment,pospattern+lengthpattern,lengthtempstring-((int)(pospattern-tempstring))-lengthpattern-1);
    memcpy(pospattern,replacement,lengthreplacment);
    lengthtempstring+=lengthreplacment-lengthpattern;
    pospattern=strstr(tempstring,toreplace);
  }
  tempstring[lengthtempstring-1]='\0';
  if(can_realloc)
  {
    if(lengthtempstring>*length)
    {
      reallocresult=realloc(source,sizeof(char)*(lengthtempstring));
      if(reallocresult==NULL)
      {
        log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
        return mumudvb_close(ERROR_MEMORY<<8);
      }
      source=reallocresult;
      *length=lengthtempstring;
    }
    strcpy(source,tempstring);
  }
  else if(lengthtempstring<=*length)
  {
    strcpy(source,tempstring);
  }
  else
  {
    strncpy(source,tempstring,*length-1);
    source[*length-1]='\0';
  }
  return source;
}
