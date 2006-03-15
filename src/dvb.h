#ifndef _DVB_H
#define _DVB_H

#include <syslog.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <resolv.h>


// DVB includes:
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "mumudvb.h"

typedef struct {
  int fd_dvr;
  int fd_frontend;
  int fd[MAX_CHAINES][MAX_PIDS_PAR_CHAINE];
  int fd_mandatory[MAX_MANDATORY];
}fds_t;


int open_fe (int *fd_frontend, int card);
void set_ts_filt (int fd,uint16_t pid, dmx_pes_type_t pestype);
void affiche_puissance (fds_t fds, int no_daemon);
int create_card_fd(int card, int nb_flux, int *num_pids, int *mandatory_pid, fds_t *fds);
void close_card_fd(int card, int nb_flux, int *num_pids, int *mandatory_pid, fds_t fds);
#endif
