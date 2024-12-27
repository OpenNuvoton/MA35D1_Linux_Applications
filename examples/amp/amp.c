/*
 * Copyright (c) 2024 Nuvoton technology corporation
 * All rights reserved.
 *
 * This project demonstrates AMP between dual cores of A35,
 * with Core0 running Linux and Core1/CM4 running FreeRTOS.
 * Fill in your endpoints in "eptinst[]" to complete the design.
 * Please ensure that RPMSG_CTRL_DEV_ID matches the device ID.
 *
 * Core0 (this core)     Core1 or CM4
 *   A (Tx & Rx)  <----->  B (High freq. short packet)
 *   C (Tx & Rx)  <----->  D (Low freq. long packet)
 *   E (Tx & Rx)  <----->  F (CRC test)
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
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/ioctl.h>
#include <termios.h>
#include <zlib.h>

#define RPMSG_CRTL_DEV_ID   0

#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)

#define EPT_TYPE_TX			0x01
#define EPT_TYPE_RX			0x10

#define THREAD_SLEEP_MS     1000

#define RPMSG_DEBUG_LEVEL   1
#if (RPMSG_DEBUG_LEVEL == 1)
	#define sysprintf printf
#else
	#define sysprintf
#endif

struct rpmsg_endpoint_info
{
	char name[32]; // Tx: name of local ept; Rx: name of remote ept to bind with
	__u32 type; // Tx or Rx ept
	__u32 size; // Tx: request data length in byte; Rx: reserved
};
typedef void *(*task_fn_t)(void *arg);

struct amp_endpoint
{
	struct rpmsg_endpoint_info eptinfo;
	task_fn_t task_fn; // User designed task
	int fd;
	pthread_t threadHandle;
	void *ept_priv;    // Reserved for passing cookies
};

#define TASKA_TX_SIZE	0x400
#define TASKA_RX_SIZE	0x400
#define TASKC_TX_SIZE	0x2800
#define TASKC_RX_SIZE	0x2800
#define TASKE_TX_SIZE	0x400

void *taskA_send(void *arg);
void *taskA_read(void *arg);
void *taskC_send(void *arg);
void *taskC_read(void *arg);
void *taskE_send(void *arg);
void *taskE_read(void *arg);

/* user defined endpoint */
struct amp_endpoint eptinst[] = {
	{ {"eptA->B", EPT_TYPE_TX, TASKA_TX_SIZE}, taskA_send }, /* Task A */
	{ {"eptB->A", EPT_TYPE_RX}, taskA_read },                /* Task A */
	{ {"eptC->D", EPT_TYPE_TX, TASKC_TX_SIZE}, taskC_send }, /* Task C */
	{ {"eptD->C", EPT_TYPE_RX}, taskC_read },                /* Task C */
	{ {"eptE->F", EPT_TYPE_TX, TASKE_TX_SIZE}, taskE_send }, /* Task E */
	{ {"eptF->E", EPT_TYPE_RX}, taskE_read },                /* Task E */
};

/**
 * @brief Create endpoint
 * 
 * @param fd file desc of AMP (rpmsg_ctrl)
 * @param amp_ept pointer to an inst of amp_endpoint
 * @return int 
 */
int amp_create_ept(int *fd, struct amp_endpoint *amp_ept)
{
	int id;
    char devname[32];

	id = ioctl(fd[0], RPMSG_CREATE_EPT_IOCTL, &amp_ept->eptinfo);
	if (id < 0) {
		printf("Failed to create endpoint \"%s\".\n", amp_ept->eptinfo.name);
	} else {
		printf("rpmsg%d: %s endpoint \"%s\" is created.\n", id,
				(amp_ept->eptinfo.type == EPT_TYPE_TX) ? "Tx" : "Rx",
				amp_ept->eptinfo.name);

		snprintf(devname, sizeof(devname), "/dev/rpmsg%d", id);
		amp_ept->fd = open(devname, O_RDWR | O_NONBLOCK);

		if (amp_ept->fd < 0) {
			printf("Failed to open device %s.\n", devname);
		} else {
			printf("%s: device is opened.\n", devname);
		}
	}

	return id;
}

/**
 * @brief Start AMP
 * 
 * @param fd file desc of AMP (rpmsg_ctrl)
 * @return int 
 */
int amp_open(int *fd)
{
	int i, ret, inst;
	char dev_path[32];

	snprintf(dev_path, sizeof(dev_path), "/dev/rpmsg_ctrl%d", RPMSG_CRTL_DEV_ID);
	fd[0] = open(dev_path, O_RDWR | O_NONBLOCK);
	if (fd[0] < 0) {
		printf("\nFailed to open AMP.\n");
		return -ENOENT;
	}

	inst = sizeof(eptinst) / sizeof(eptinst[0]);

	for(i = 0; i < inst; i++) {
		ret = amp_create_ept(fd, &eptinst[i]);
		if (ret < 0) {
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * @brief Recycle endpoint
 * Destroy and close endpoint, call this if you no longer need this endpoint.
 * 
 * @param amp_ept pointer to an inst of amp_endpoint
 * @return int 
 */
int amp_destroy_ept(struct amp_endpoint *amp_ept)
{
	if(ioctl(amp_ept->fd, RPMSG_DESTROY_EPT_IOCTL, NULL)) {
		printf("Failed to destroy endpoint \"%s\".\n", amp_ept->eptinfo.name);
		return -1;
	}
	else {
		close(amp_ept->fd);
		printf("Endpoint \"%s\" is destroyed.\n", amp_ept->eptinfo.name);
		return 0;
	}
}

/**
 * @brief End AMP
 * 
 * @param fd 
 * @return int 
 */
int amp_close(int *fd)
{
	int i, ret, inst;

	inst = sizeof(eptinst) / sizeof(eptinst[0]);

	for(i = 0; i < inst; i++) {
		ret = amp_destroy_ept(&eptinst[i]);
	}

	close(fd[0]);

	return 0;
} 

/**
 * @brief User Tx thread
 * 
 * @param arg pointer to amp_endpoint
 * @return void* 
 */
void *taskA_send(void *arg)
{
	struct amp_endpoint *ept = (struct amp_endpoint *)arg;
	char tx_buf[TASKA_TX_SIZE];
	int i, ret, size, j = 0;

	while(1)
	{
		/* Start of user write function */
		size = rand();
		size = size % TASKA_TX_SIZE;
		size = size ? size : TASKA_TX_SIZE;

		for(i = 0; i < size; i++) {
			tx_buf[i] = size + i;
		}
		/* End of user read function */

		ret = write(ept->fd, tx_buf, size);
		if(ret < 0) {
			if(errno == EPERM) {
				sysprintf("%s: Remote closed.\n", ept->eptinfo.name);
				/* 1. do nothing and try reconnecting 2. call amp_destroy_ept(ept) and exit */
			}
			else if(errno == EAGAIN)
				sysprintf("%s: Tx blocking.\n", ept->eptinfo.name);
			// else if(errno == EACCES)
			// 	sysprintf("%s: Remote ept not ready.\n", ept->eptinfo.name);
		}

		usleep(THREAD_SLEEP_MS*10);
	}

	pthread_exit(NULL);
}

/**
 * @brief User Rx thread
 * 
 * @param arg pointer to amp_endpoint
 * @return void* 
 */
void *taskA_read(void *arg)
{
	struct amp_endpoint *ept = (struct amp_endpoint *)arg;
	int ret, j = 0;
	ssize_t rxlen;
	char rx_buf[TASKA_RX_SIZE];
	struct pollfd fds;

    fds.fd = ept->fd;
	fds.events = POLLIN | POLLHUP;

	while(1)
	{
		ret = poll(&fds, 1, 0);

		if (ret > 0) {
			if (fds.revents & POLLIN) {
				/* Start of user read function */
				ret = rxlen = 0;
				while(1) {
					ret = read(ept->fd, rx_buf + rxlen, sizeof(rx_buf));
					if(ret > 0)
						rxlen += ret;
					else
						break;
				}
				/* End of user read function */
			}

			if (fds.revents & POLLHUP) {
				sysprintf("%s: Remote closed.\n", ept->eptinfo.name);
				/* 1. do nothing and try reconnecting 2. call amp_destroy_ept(ept) and exit */
			}
		} else if (ret < 0) {
			printf("End of %s thread\n", ept->eptinfo.name);
			perror("poll");
			return NULL;
		}
		// else
		// 	sysprintf("Timeout waiting for data\n");

		usleep(THREAD_SLEEP_MS*2);
	}

	pthread_exit(NULL);
}

/**
 * @brief User Tx thread
 * 
 * @param arg pointer to amp_endpoint
 * @return void* 
 */
void *taskC_send(void *arg)
{
	struct amp_endpoint *ept = (struct amp_endpoint *)arg;
	char tx_buf[TASKC_TX_SIZE];
	int i, ret, size, j = 0;

	while(1)
	{
		/* Start of user write function */
		size = rand();
		size = size % TASKC_TX_SIZE;
		size = size ? size : TASKC_TX_SIZE;

		for(i = 0; i < size; i++) {
			tx_buf[i] = size + i;
		}
		/* End of user write function */

		ret = write(ept->fd, tx_buf, size);
		if(ret < 0) {
			if(errno == EPERM) {
				sysprintf("%s: Remote closed.\n", ept->eptinfo.name);
				/* 1. do nothing and try reconnecting 2. call amp_destroy_ept(ept) and exit */
			}
			else if(errno == EAGAIN)
				sysprintf("%s: Tx blocking.\n", ept->eptinfo.name);
			// else if(errno == EACCES)
			// 	sysprintf("%s: Remote ept not ready.\n", ept->eptinfo.name);
		}

		usleep(THREAD_SLEEP_MS*100);
	}

	pthread_exit(NULL);
}

/**
 * @brief User Rx thread
 * 
 * @param arg pointer to amp_endpoint
 * @return void* 
 */
void *taskC_read(void *arg)
{
	struct amp_endpoint *ept = (struct amp_endpoint *)arg;
	int ret, j = 0;
	ssize_t rxlen;
	char rx_buf[TASKC_RX_SIZE];
	struct pollfd fds;

    fds.fd = ept->fd;
    fds.events = POLLIN | POLLHUP;

	while(1)
	{
		ret = poll(&fds, 1, 0);

		if (ret > 0) {
			if (fds.revents & POLLIN) {
				/* Start of user read function */
				ret = rxlen = 0;
				while(1) {
					ret = read(ept->fd, rx_buf + rxlen, sizeof(rx_buf));
					if(ret > 0)
						rxlen += ret;
					else
						break;
				}
				/* End of user read function */
			}

			if (fds.revents & POLLHUP) {
				sysprintf("%s: Remote closed.\n", ept->eptinfo.name);
				/* 1. do nothing and try reconnecting 2. call amp_destroy_ept(ept) and exit */
			}
		} else if (ret < 0) {
			printf("End of %s thread\n", ept->eptinfo.name);
			perror("poll");
			return NULL;
		}
		// else
		// 	sysprintf("Timeout waiting for data\n");

		usleep(THREAD_SLEEP_MS*10);
	}

	pthread_exit(NULL);
}

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int wait_crc = 0, mute = 0;
unsigned int crc_cmp, crc_cal, crc_err;
/**
 * @brief User Tx thread
 * 
 * @param arg pointer to amp_endpoint
 * @return void* 
 */
void *taskE_send(void *arg)
{
	struct amp_endpoint *ept = (struct amp_endpoint *)arg;
	char tx_buf[TASKE_TX_SIZE];
	int i, ret, size;

	while(1)
	{
		/* Start of user write function */
		pthread_mutex_lock(&lock);

		size = rand();
		size = size % TASKE_TX_SIZE;
		size = size ? size : TASKE_TX_SIZE;

		for(i = 0; i < size; i++) {
			tx_buf[i] = size + i;
		}
		/* End of user write function */

		ret = write(ept->fd, tx_buf, size);
		if(ret < 0) {
			if(errno == EPERM) {
				/* 1. do nothing and try reconnecting 2. call amp_destroy_ept(ept) and exit */
			}
			else if(errno == EAGAIN)
				sysprintf("%s: Tx blocking.\n", ept->eptinfo.name);
			// else if(errno == EACCES)
			// 	sysprintf("%s: Remote ept not ready.\n", ept->eptinfo.name);
		}

		crc_cal = crc32(0, tx_buf, size);
		wait_crc = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);

		usleep(THREAD_SLEEP_MS*1000);
	}

	pthread_exit(NULL);
}

/**
 * @brief User Rx thread
 * 
 * @param arg pointer to amp_endpoint
 * @return void* 
 */
void *taskE_read(void *arg)
{
	struct amp_endpoint *ept = (struct amp_endpoint *)arg;
	int ret;
	unsigned int rx_buf;
	struct pollfd fds;

    fds.fd = ept->fd;
    fds.events = POLLIN | POLLHUP;

	while(1)
	{
		pthread_mutex_lock(&lock);
        // Wait until data is sent
        while (!wait_crc) {
            pthread_cond_wait(&cond, &lock);
        }

		ret = poll(&fds, 1, 1000);

		if (ret > 0) {
			if (fds.revents & POLLIN) {
				/* Start of user read function */
				ret = read(ept->fd, &rx_buf, sizeof(rx_buf));

				crc_cmp++;
				if(rx_buf != crc_cal)
					crc_err++;
				
				if(!mute)
					printf("recv: 0x%08x, calc: 0x%08x\n", rx_buf, crc_cal);
				/* End of user read function */
			}

			if (fds.revents & POLLHUP) {
				/* 1. do nothing and try reconnecting 2. call amp_destroy_ept(ept) and exit */
			}
		} else if (ret < 0) {
			printf("End of %s thread\n", ept->eptinfo.name);
			perror("poll");
			return NULL;
		}
		else
			sysprintf("Timeout waiting for data.\n");

		wait_crc = 0;
		pthread_mutex_unlock(&lock);
	}

	pthread_exit(NULL);
}

void *task_cmd(void *arg)
{
	struct amp_endpoint *amp_ept = arg;
	char c;

	while(1)
	{
        c = getchar();
        if (c != EOF)
        {
            switch(c)
            {
                case '0':
					amp_destroy_ept(&eptinst[c-'0']);
                    break;
                case '1':
					amp_destroy_ept(&eptinst[c-'0']);
                    break;
                case '2':
					amp_destroy_ept(&eptinst[c-'0']);
                    break;
                case '3':
					amp_destroy_ept(&eptinst[c-'0']);
                    break;
				case 'M':
				case 'm':
					mute ^= 1;
					break;
				case 'Q':
                case 'q':
					for(int i = 0; i < sizeof(eptinst) / sizeof(eptinst[0]); i++) {
						pthread_cancel(eptinst[i].threadHandle);
						amp_destroy_ept(&eptinst[i]);
					}
                    printf("Quit.\n");
                    pthread_exit(NULL);
                    break;
				case 'S':
				case 's':
					printf("CRC error: %d/%d\n", crc_err, crc_cmp);
                    break;
                default:
                    break;
            }
        }
		usleep(THREAD_SLEEP_MS*1000);
	}

	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	const int tasknum = sizeof(eptinst) / sizeof(eptinst[0]);
	int i = 0;
	int fd;
	pthread_t cmdthread;

	printf("\nMA35D1 AMP core#0 demo\n");
	printf("\nPlease refer to source code for detailed command info.\n\n");
	fflush(stdout);

	if(amp_open(&fd))  {
		printf("Failed to init amp.\n");
		exit(1);
	}

	for(i = 0; i < tasknum; i++)
	{
		if(pthread_create(&eptinst[i].threadHandle, NULL, eptinst[i].task_fn, &eptinst[i]))
		{
			printf("Create %s error: %s \n", eptinst[i].eptinfo.name, strerror(errno));
			exit(1);
		}
	}
	pthread_create(&cmdthread, NULL, task_cmd, eptinst);

	for(i = 0; i < tasknum; i++)
	{
		pthread_join(eptinst[i].threadHandle, NULL);
	}
	pthread_join(cmdthread, NULL);

	return 0;
}
