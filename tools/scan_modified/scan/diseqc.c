#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <time.h>

#include "scan.h"
#include "diseqc.h"


struct diseqc_cmd switch_cmds[] = {
	{ { { 0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf2, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf1, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf3, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf4, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf6, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf5, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf7, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf8, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfa, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf9, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfb, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfc, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfe, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfd, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xff, 0x00, 0x00 }, 4 }, 0 }
};


/*--------------------------------------------------------------------------*/

static inline
void msleep(uint32_t msec)
{
	struct timespec req = { msec / 1000, 1000000 * (msec % 1000) };

	while (nanosleep(&req, &req))
		;
}

#define printf(x...)


int diseqc_send_msg (int fd, fe_sec_voltage_t v, struct diseqc_cmd **cmd,
		     fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
	int err;

	if ((err = ioctl(fd, FE_SET_TONE, SEC_TONE_OFF)))
		return err;

	if ((err = ioctl(fd, FE_SET_VOLTAGE, v)))
		return err;

	msleep(15);
	while (*cmd) {
		debug("DiSEqC: %02x %02x %02x %02x %02x %02x\n",
		    (*cmd)->cmd.msg[0], (*cmd)->cmd.msg[1],
		    (*cmd)->cmd.msg[2], (*cmd)->cmd.msg[3],
		    (*cmd)->cmd.msg[4], (*cmd)->cmd.msg[5]);

		if ((err = ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd)))
			return err;

		msleep((*cmd)->wait);
		cmd++;
	}

	//debug(" %s ", v == SEC_VOLTAGE_13 ? "SEC_VOLTAGE_13" :
	//    v == SEC_VOLTAGE_18 ? "SEC_VOLTAGE_18" : "???");

	//debug(" %s ", b == SEC_MINI_A ? "SEC_MINI_A" :
	//    b == SEC_MINI_B ? "SEC_MINI_B" : "???");

	//debug(" %s\n", t == SEC_TONE_ON ? "SEC_TONE_ON" :
	//    t == SEC_TONE_OFF ? "SEC_TONE_OFF" : "???");

	msleep(15);

	if ((err = ioctl(fd, FE_DISEQC_SEND_BURST, b)))
		return err;

	msleep(15);

	return ioctl(fd, FE_SET_TONE, t);
}


int setup_switch (int frontend_fd, int switch_pos, int voltage_18, int hiband)
{
	struct diseqc_cmd *cmd[2] = { NULL, NULL };
	int i = 4 * switch_pos + 2 * hiband + (voltage_18 ? 1 : 0);

	verbose("DiSEqC: switch pos %i, %sV, %sband (index %d)\n",
	    switch_pos, voltage_18 ? "18" : "13", hiband ? "hi" : "lo", i);

	if (i < 0 || i >= (int) (sizeof(switch_cmds)/sizeof(struct diseqc_cmd)))
		return -EINVAL;

	cmd[0] = &switch_cmds[i];

	return diseqc_send_msg (frontend_fd,
				i % 2 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
				cmd,
				(i/2) % 2 ? SEC_TONE_ON : SEC_TONE_OFF,
				(i/4) % 2 ? SEC_MINI_B : SEC_MINI_A);
}


