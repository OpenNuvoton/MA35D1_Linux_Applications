/*
 * Real Time Clock Driver Test/Example Program
 *
 * Compile with:
 *  gcc -s -Wall -Wstrict-prototypes rtctest.c -o rtctest
 *
 * Copyright (C) 1996, Paul Gortmaker.
 *
 * Released under the GNU General Public License, version 2,
 * included herein by reference.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <signal.h>

#define RTC_TICK_ON	_IO('p', 0x03)  /* Update int. enable on        */
#define RTC_TICK_OFF	_IO('p', 0x04)  /* ... off                      */
#define RTC_TICK_SET	_IO('p', 0x05)  /* Periodic int. enable on      */
#define RTC_TICK_READ	_IO('p', 0x06)  /* ... off                      */

#define HR24		1
int fd_all = 0;

void alarm_func()
{
	printf("\n\n*Alarm interrupt come on,the func running!*\n");
}

void ctrl_c_signal (int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		printf("\n\n **You have input 'ctrl+c',so must be input 'X' exit DEMO!\n\n");
		/* Disable alarm interrupts */
		ioctl(fd_all, RTC_AIE_OFF, 0);
		ioctl(fd_all, RTC_TICK_OFF, 0);
		close(fd_all);
		return;

	default:
		printf ("Warning - unhandled signal %d\n", sig);
		close(fd_all);
	}
}

/*
 * setup_time():set up the sec, func
 * in rtc_tm, valid values are:
 *     year 1900-
 *     month 0-11
 *     day 1-[28, 29, 30, 31]
 *     hr 0-23
 *     min, sec 0 -59
 */

void setup_time(int fd)
{
	int i, retval;
	unsigned long tmp, data;
	struct rtc_time rtc_tm;
	int scale_type = HR24;
	struct rtc_wkalrm rtc_alarmtm;
	static char year[4], month[4],day[4],hour[4],min[4],sec[4];
	int ret;

	printf("Input the adjust time:\n");
	printf("You should input 6 words,and must be 'enter' per word!\n");
	printf("->year->month->day->hour->min->sec\n");

	scanf("%s", year);
	rtc_tm.tm_year = atoi(year) - 1900;
	scanf("%s", month);
	scanf("%s", day);
	scanf("%s", hour);
	scanf("%s", min);
	scanf("%s", sec);

	rtc_tm.tm_mon = atoi(month) - 1;
	rtc_tm.tm_mday = atoi(day);
	rtc_tm.tm_hour = atoi(hour);
	rtc_tm.tm_min = atoi(min);
	rtc_tm.tm_sec = atoi(sec);

	retval = ioctl(fd, RTC_SET_TIME, &rtc_tm);
	if (retval <0) {
		printf("ioctl RTC_SET_TIME  faild!!!\n");
		return ;
	}

	/*print current time*/
	printf("Adjust current RTC time as: %04d-%02d-%02d %02d:%02d:%02d\n\n",
		rtc_tm.tm_year + 1900,
		rtc_tm.tm_mon + 1,
		rtc_tm.tm_mday,
		rtc_tm.tm_hour,
		rtc_tm.tm_min,
		rtc_tm.tm_sec);
	printf("Adjust current RTC time test OK!\n");
}

void read_time(int fd)
{
	int i, retval;
	unsigned long tmp, data;
	struct rtc_time rtc_tm;
	int scale_type = HR24;
	struct rtc_wkalrm rtc_alarmtm;

	/*read current time*/

	retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
	if (retval < 0) {
		printf("ioctl RTC_RD_TIME  faild!!!\n");
		return;
	}

	/*print current time*/
	printf("Read current RTC time is: %04d-%02d-%02d %02d:%02d:%02d\n\n",
		rtc_tm.tm_year + 1900,
		rtc_tm.tm_mon + 1,
		rtc_tm.tm_mday,
		rtc_tm.tm_hour,
		rtc_tm.tm_min,
		rtc_tm.tm_sec);
	printf("Read current RTC time test OK!\n");
}

/*
 * setup_alarm():set up the sec,func
 */
void setup_alarm(int fd, int sec, void(*func)())
{
	int i, retval;
	unsigned long tmp, data;
	struct rtc_time rtc_tm;
	int scale_type = HR24;
	/*read current time*/

	retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
	if (retval < 0) {
		printf("ioctl RTC_RD_TIME  faild!!!");
		return;
	}

	printf("read current time is: %04d-%02d-%02d %02d:%02d:%02d\n\n",
		rtc_tm.tm_year + 1900,
		rtc_tm.tm_mon + 1,
		rtc_tm.tm_mday,
		rtc_tm.tm_hour,
		rtc_tm.tm_min,
		rtc_tm.tm_sec);

	rtc_tm.tm_sec = rtc_tm.tm_sec + sec; /* alarm to 'sec' seconds in the future */
	if (rtc_tm.tm_sec > 60) {
		rtc_tm.tm_sec = rtc_tm.tm_sec - 60;
		rtc_tm.tm_min = rtc_tm.tm_min + 1;
	}

	// rtc_alarmtm.enabled=0;
	// rtc_alarmtm.time=rtc_tm;

	/* Set the alarm to 5 sec in the future */

	retval = ioctl(fd, RTC_ALM_SET, &rtc_tm);
	if (retval < 0) {
		printf("ioctl RTC_ALM_SET faild!!!\n");
		return;
	}

	/* Read the current alarm settings */

	retval = ioctl(fd, RTC_ALM_READ, &rtc_tm);
	if (retval < 0) {
		printf("ioctl  RTC_ALM_READ faild!!!\n");
		return;
	}

	printf("read current alarm time is: %04d-%02d-%02d %02d:%02d:%02d\n\n",
		rtc_tm.tm_year + 1900,
		rtc_tm.tm_mon + 1,
		rtc_tm.tm_mday,
		rtc_tm.tm_hour,
		rtc_tm.tm_min,
		rtc_tm.tm_sec);

	/* Enable alarm interrupts */

	retval = ioctl(fd, RTC_AIE_ON, 0);
	if (retval < 0) {
		printf("ioctl  RTC_AIE_ON faild!!!\n");
		return;
	}

	fprintf(stderr, "Waiting %d seconds for alarm...\n\n",sec);
	fflush(stderr);

	/* This blocks until the alarm ring causes an interrupt */
	retval = read(fd, &data, sizeof(unsigned long));
	if (retval > 0) {
		func();
	} else {
		printf("!!!alarm faild!!!\n");
		return;
	}

	/* Disable alarm interrupts */
	retval = ioctl(fd, RTC_AIE_OFF, 0);
	if (retval == -1) {
		printf("ioctl RTC_AIE_OFF faild!!!\n");
		return;
	}
	printf("Test second alarm(hour,min,sec) test OK!\n");
}

/*
 * setup_alarm_powerdown(): set RTC alarm, enter Linux suspend-to-RAM,
 * and let RTC alarm wake up the system.
 */
void setup_alarm_powerdown(int fd, int sec, void(*func)())
{
	int retval;
	unsigned long data;
	struct rtc_time rtc_tm;
	FILE *fp;

	retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
	if (retval < 0) {
		printf("ioctl RTC_RD_TIME faild!!!\n");
		return;
	}

	printf("read current time is: %04d-%02d-%02d %02d:%02d:%02d\n\n",
		rtc_tm.tm_year + 1900,
		rtc_tm.tm_mon + 1,
		rtc_tm.tm_mday,
		rtc_tm.tm_hour,
		rtc_tm.tm_min,
		rtc_tm.tm_sec);

	rtc_tm.tm_sec = rtc_tm.tm_sec + sec; /* alarm to 'sec' seconds in the future */
	if (rtc_tm.tm_sec > 60) {
		rtc_tm.tm_sec = rtc_tm.tm_sec - 60;
		rtc_tm.tm_min = rtc_tm.tm_min + 1;
	}

	retval = ioctl(fd, RTC_ALM_SET, &rtc_tm);
	if (retval < 0) {
		printf("ioctl RTC_ALM_SET faild!!!\n");
		return;
	}

	retval = ioctl(fd, RTC_ALM_READ, &rtc_tm);
	if (retval < 0) {
		printf("ioctl RTC_ALM_READ faild!!!\n");
		return;
	}

	printf("read current alarm time is: %04d-%02d-%02d %02d:%02d:%02d\n\n",
		rtc_tm.tm_year + 1900,
		rtc_tm.tm_mon + 1,
		rtc_tm.tm_mday,
		rtc_tm.tm_hour,
		rtc_tm.tm_min,
		rtc_tm.tm_sec);

	retval = ioctl(fd, RTC_AIE_ON, 0);
	if (retval < 0) {
		printf("ioctl RTC_AIE_ON faild!!!\n");
		return;
	}

	/*
	 * Some platforms require RTC wakeup to be enabled explicitly.
	 * Ignore failure here because not every kernel exposes this node.
	 */
	fp = fopen("/sys/class/rtc/rtc0/device/power/wakeup", "w");
	if (fp) {
		fprintf(fp, "enabled\n");
		fclose(fp);
	}

	printf("Waiting %d seconds for RTC alarm wakeup...\n", sec);
	printf("Enter Linux power down: echo mem > /sys/power/state\n\n");
	fflush(stdout);

	retval = system("echo mem > /sys/power/state");
	if (retval != 0) {
		printf("echo mem > /sys/power/state faild!!!\n");
		ioctl(fd, RTC_AIE_OFF, 0);
		return;
	}

	/*
	 * After resume, the RTC alarm should already have fired.
	 * Read once to consume the pending alarm interrupt if the driver provides it.
	 */
	retval = read(fd, &data, sizeof(unsigned long));
	if (retval > 0) {
		func();
	} else {
		printf("!!!alarm wakeup faild or no pending RTC event!!!\n");
	}

	retval = ioctl(fd, RTC_AIE_OFF, 0);
	if (retval == -1) {
		printf("ioctl RTC_AIE_OFF faild!!!\n");
		return;
	}

	printf("Test RTC alarm wakeup from Linux power down OK!\n");
}

int main(void)
{
	int i, fd, retval, irqcount = 0;
	unsigned long tmp, data;
	struct rtc_time rtc_tm;
	int scale_type = HR24;
	struct rtc_wkalrm rtc_alarmtm;
	unsigned int select;

	fd = open ("/dev/rtc0", O_RDONLY);
	fd_all = fd;
	if (fd < 0) {
		printf("open dev/rtc0 faild!!!\n");
		return -1;
	}
	signal (SIGTERM, ctrl_c_signal);  /* for the TERM signal     */
	signal (SIGINT, ctrl_c_signal);   /* for the CTRL+C signal   */

	while (1) {
		printf("\n\n******** RTC Test Demo ***********\n");
		printf("1. Read current RTC time ..\n");
		printf("2. Adjust current RTC time ..\n");
		printf("3. Test 5 seconds alarm(hour,min,sec)..\n");
		printf("4. Test 5 seconds alarm wakeup from Linux power down..\n");
		printf("X. Exit(don't use 'ctrl+c',must be 'X')..\n");
		printf("**************************************\n");

		printf("Select : \n");
		select = getchar();

		switch (select) {
		case '1':
			read_time(fd);
			break;

		case '2':
			setup_time(fd);
			break;

		case '3':
			setup_alarm(fd, 5, alarm_func);
			break;

		case '4':
			setup_alarm_powerdown(fd, 5, alarm_func);
			break;

		case 'X':
		case 'x':
			goto end;

		default :
			break;
		}
	}
end:
	close(fd);
	return 0;
}

