#ifndef _CAM_H
#define _CAM_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>

#define CA_DEV "/dev/dvb/adapter%d/ca%d"

struct ca_info {
  int sys_num;
  uint16_t sys_id[256];
  char app_name[256];
};

#endif
