/*
 * Copyright (c) 2022 Nuvoton technology corporation
 * All rights reserved.
 *
 * rpmsg can interact with RTP M4 OpenAMP sample code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/ioctl.h>
#include <fcntl.h> 
#include <termios.h>  
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/ioctl.h>

#define FALSE 0
#define TRUE  1

struct rpmsg_endpoint_info {
	char name[32];
	__u32 src;
	__u32 dst;
};

#define RPMSG_CREATE_EPT_IOCTL	_IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL	_IO(0xb5, 0x2)
#define STOPCODE "TERMINATE";

int fd[2];

static int rpmsg_create_ept(int rpfd, struct rpmsg_endpoint_info *eptinfo)
{
	int ret;

	ret = ioctl(rpfd, RPMSG_CREATE_EPT_IOCTL, eptinfo);
	if (ret)
		perror("Failed to create endpoint.\n");
	return ret;
}

void sig_handler(int signo)
{
	int ret;
	unsigned char Tx_Buffer[10] = STOPCODE;
	if (signo == SIGINT) {
		ret = write(fd[1], Tx_Buffer, 10);
		if (ret < 0) {
			printf("\n Failed to write \n");
			while(1);
		}
		printf("\nSIGINT signal catched!\n");
	}
}

/**
*@breif 	main()
*/
int main(int argc, char **argv)
{
	char *dev[10]={"/dev/rpmsg_ctrl0", " "};
	unsigned int i;
	int rev1, rev2;
	struct rpmsg_endpoint_info eptinfo;
	int ret;
	int err;
	unsigned char Tx_Buffer[130];
	unsigned char Rx_Buffer[130];

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		printf("Failed to catach CTRL-C signal!\n");
		return -1;
	}

	printf("\n demo rpmsg \n");

	fd[0] = open("/dev/rpmsg_ctrl0", O_RDWR | O_NONBLOCK);
	if (fd[0] < 0) {
		printf("\n Failed to open \n");
		return 0;
	}

	strcpy(eptinfo.name, "rpmsg-test");
	eptinfo.src = 0;
	eptinfo.dst = 0xFFFFFFFF;

	ret = rpmsg_create_ept(fd[0], &eptinfo);
	if (ret) {
		printf("failed to create RPMsg endpoint.\n");
		return -EINVAL;
	}

	//printf("\n demo 33 %d \n",ret);

	ret = system("/sbin/mdev -s");
	if (ret < 0) {
		printf("\nFailed to load rpmsg_char driver.\n");
		while(1);
		//return -EINVAL;
	}

	fd[1] = open("/dev/rpmsg0", O_RDWR | O_NONBLOCK);

	if (fd[1] < 0) {
		printf("\n Failed to open rpmsg0 \n");
		while(1);
	}

	while(1)
	{
		struct pollfd fds[] = {
		{
			.fd = fd[1],
			.events	= POLLOUT },
		};

		for (i = 0; i < 128; i++) {
			Tx_Buffer[i] = (255-i);
		}

		rev1 = write(fd[1], Tx_Buffer, 10);
		if (rev1 < 0) {
			printf("\n Failed to write \n");
			while(1);
	    	}

		while(1) {
			err = poll(fds, 1, 10000);

			if ((err == -1) || (err == 0)) {
				printf("\n w=%d ", err);
			} else {
				break;
			}
		}
		printf("\n Write 10 bytes data to RTP finish! \n");
		break;
	}

	while(1) {
		struct pollfd fds[] = {
		{
			.fd	= fd[1],
			.events	= POLLIN, },
		};

		while(1) {
			err = poll(fds, 1, 1000);
			if (err == -1) {
				printf("\n r=%d ", err);
			} else if (err == 0) {
				return 0;
			} else {
				break;
			}
		}

		rev1 = read(fd[1], Rx_Buffer, 128);
		if (rev1 < 0) {
			printf("\n Failed to read \n");
		}

		printf("\n Receive %d bytes data from RTP: \n", rev1);

		for(i = 0; i < rev1; i++) {
			printf(" 0x%x, \n", Rx_Buffer[i]);
		}
	}
	while(1);
	return 0;
}
