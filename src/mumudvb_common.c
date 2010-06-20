/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2010 Brice DUBOST
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
#include <stdarg.h>



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
        log_message( NULL, MSG_DEBUG, "Poll : 10 successive EINTR\n");
        poll_eintr=0;
      }
    }
    /**@todo : put a maximum number of interrupted system calls per unit time*/
  }

  if(poll_try==MAX_POLL_TRIES)
  {
    log_message( NULL, MSG_ERROR, "Poll : We reach the maximum number of polling tries\n\tLast error when polling: %s\n", strerror (errno));
    Interrupted=errno<<8; //the <<8 is to make difference beetween signals and errors;
    return Interrupted;
  }
  else if(poll_try)
  {
    log_message( NULL, MSG_WARN, "Poll : Warning : error when polling: %s\n", strerror (last_poll_error));
  }
  return 0;
}

/** @brief replace a tring by another
* @param source
* @param length the length of the source buffer (including '\0')
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
        log_message(NULL, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
        return NULL;
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

int string_mult(char *string);
/** @brief Evaluate a string containing sum and mult keeping the priority of the mult over the +
 * Ex : string_sum("2+2*3") returns 8
 * @param string the string to evaluate
*/
int string_comput(char *string)
{
  int number1,len;
  char *pluspos=NULL;
  char *tempchar;
  pluspos=strchr(string,'+');
  if(pluspos==NULL)
  {
    len=strlen(string);
  }
  else
  {
    len=pluspos-string;
  }
  tempchar=malloc(sizeof(char)*(len+1));
  strncpy(tempchar,string,len);
  tempchar[len]='\0';
  number1=string_mult(tempchar);
  free(tempchar);
  if(pluspos==NULL)
    return number1;
  if(strchr(pluspos+1,'+')!=NULL)
    return number1+string_comput(pluspos+1);
  return number1+string_mult(pluspos+1);
}

/** @brief Evaluate a string containing a multiplication. Doesn't work if there is a sum inside
 * Ex : string_sum("2*6") returns 6
 * @param string the string to evaluate
*/
int string_mult(char *string)
{
  int number1,len;
  char *multpos=NULL;
  char *tempchar;
  multpos=strchr(string,'*');
  if(multpos==NULL)
    return atoi(string);
  len=multpos-string;
  tempchar=malloc(sizeof(char)*(len+1));
  strncpy(tempchar,string,len);
  tempchar[len]='\0';
  number1=atoi(tempchar);
  free(tempchar);
  if(strchr(multpos+1,'*')!=NULL)
    return number1*string_mult(multpos+1);
  return number1*atoi(multpos+1);
}

/** @brief Special sprintf wich append the text to an existing string and allocate the memory for it
*/
int mumu_string_append(mumu_string_t *string, const char *psz_format, ...)
{
  int size;
  va_list args;

  va_start( args, psz_format );

  size=vsnprintf(NULL, 0, psz_format, args);
  string->string=realloc(string->string,(string->length+size+1)*sizeof(char));
  if(string->string==NULL)
  {
    log_message(NULL,MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return ERROR_MEMORY<<8;
  }
  vsnprintf(string->string+string->length, size+1, psz_format, args);
  string->length=string->length+size;
  va_end( args );
  return 0;
}

/** @brief Free a MuMuDVB string
*/
void mumu_free_string(mumu_string_t *string)
{
  if(string->string)
  {
    free(string->string);
    string->string=NULL;
    string->length=0;
  }
}