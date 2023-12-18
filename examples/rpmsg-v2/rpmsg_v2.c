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
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/ioctl.h>

/* Debug level 0~2 */
#define RPMSG_DEBUG_LEVEL  1
#define ADD_TX_FLOW        0

/* Tx/Rx in free run mode, timer may not effect if enabled */
#if (ADD_TX_FLOW > 0)
    #define TXRX_FREE_RUN  0
#endif

/* The period of timer for handshake flow control */
#define ACK_TIMER_SEC      0
#define ACK_TIMER_USEC     100 /* 50 < ACK_TIMER_USEC < 10^6 */


/**
 * @brief CMD frame
 * ---Appl Level---
 * SubCMD(4 bytes) + ServerSEQ(4 bytes) + ClientSEQ(4 bytes) + reserved(4 bytes) + payload(0-112 bytes)
 * As server :
 *     Tx : Send ServerSEQ, Rx : Check for ClientSEQ
 * As client :
 *     Rx : Receive ServerSEQ, Tx : Reply ClientSEQ
 */
#define PAYLOAD_LEN    112
#define SUBCMD_LEN     16
#define SUBCMD_DIGITS  0xFFFF
#define SUBCMD_START   0x01
#define SUBCMD_SUSPEND 0x02
#define SUBCMD_RESUME  0x01
#define SUBCMD_EXIT    0x08
#define SUBCMD_SEQ     0x10
#define SUBCMD_SEQACK  0x20
#define SUBCMD_ERROR   0x8000

// seqack state machine
#define ACK_READY      0
#define ACK_BUSY       1

#define RPMSG_WND         1UL
#define SEQ_DIGITS        0xFFFFFFFF
#define SERVER_SEQ_OFFSET 4
#define CLIENT_SEQ_OFFSET 8

#define FALSE 0
#define TRUE 1

#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)

#if (RPMSG_DEBUG_LEVEL > 0)
	#define sysprintf printf
#else
	#define sysprintf
#endif

#if (ACK_TIMER_USEC < 50)
	#error The period of ACK_TIMER_USEC should not less than 50us
#endif

struct rpmsg_endpoint_info
{
	char name[32];
	__u32 src;
	__u32 dst;
};

timer_t timerid;
unsigned int ack_ready = ACK_READY;
static unsigned int tx_server_seq = 0, rx_client_seq = 0; // A35 as server
static unsigned int rx_server_seq = 0, tx_client_seq = 0; // A35 as client
volatile unsigned int status_flag = 0;
volatile unsigned int seq_start = 0, throughput = 0;

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
	unsigned char Tx_Buffer[SUBCMD_LEN] = {0};
	
	if (signo == SIGINT) {
		status_flag |= SUBCMD_EXIT;
		Tx_Buffer[0] |= SUBCMD_EXIT;
		ret = write(fd[1], Tx_Buffer, SUBCMD_LEN);
		if (ret < 0) {
			printf("\n Failed to write#1 \n");
			while(1);
		}
		sysprintf("\nSIGINT signal catched!\n");
	}
}

/**
 * @brief Receive data RTP -> A35 
 * 
 * @param data 
 * @param len payload only
 */
void receive_rxdata(unsigned char *data, int len)
{
#if (RPMSG_DEBUG_LEVEL > 1)
	int i;
    sysprintf("\n Receive %d bytes data from RTP: \n", len);
    for(i = 0; i < len; i++) {
        sysprintf(" 0x%x ", data[i]);
    }
	sysprintf("\n");
#endif
}

/**
 * @brief Send data A35 -> RTP
 * 
 * @param data 
 * @param len payload only
 * @return int 
 */
int prepare_txdata(unsigned char *data, int *len)
{
    int i;

    *len = PAYLOAD_LEN;
    for(i = 0; i < *len; i++) {
        data[i] = i;
    }

	if(*len > 0 && *len <= PAYLOAD_LEN)
		return 1;
	else
		return 0;
}

int txdata_pack(unsigned char *txbuffer, int *len)
{
#if (ADD_TX_FLOW > 0)
	unsigned int subCmd[4] = {0};

	if(prepare_txdata(txbuffer + SUBCMD_LEN, len)) {
		subCmd[0] |= SUBCMD_SEQ;
		subCmd[1] |= ++tx_server_seq & SEQ_DIGITS;
		memcpy(txbuffer, subCmd, SUBCMD_LEN);
		*len += SUBCMD_LEN;
		return 1;
	}
#endif
	return 0;
}

void seqack_append(void *Tx_Buffer, int len)
{
	unsigned int subCmd[4];
	int ret;
	int err;

	struct pollfd fds[] = {
		{
			.fd = fd[1],
			.events = POLLOUT,
		},
	};

	seq_start = tx_client_seq;
	subCmd[0] = *(unsigned int *)Tx_Buffer | SUBCMD_SEQACK;
	subCmd[1] = *((unsigned int *)Tx_Buffer + 1);
	subCmd[2] = seq_start & SEQ_DIGITS;
	memcpy(Tx_Buffer, subCmd, SUBCMD_LEN);

	ret = write(fd[1], Tx_Buffer, len);
	if (ret < 0) {
		if(status_flag & SUBCMD_EXIT)
			return;
		printf("\n Failed to write#2 \n");
		while(1);
	}
	ack_ready = ACK_READY;

	if (tx_client_seq%1000 == 0)
		sysprintf("ack #%d\n", tx_client_seq);

	while(1) {
		err = poll(fds, 1, 1000);

		if (err == -1) {
			sysprintf("\n w3=%d ", err);
		}
		else {
			break;
		}
	}
}

void alarm_handler(int signo)
{
	unsigned int subCmd[4] = {0};
	unsigned int len;
	unsigned char Tx_Buffer[130];

	if (ack_ready == ACK_BUSY) {
		if(txdata_pack(Tx_Buffer, &len)) {
			seqack_append(Tx_Buffer, len);
			if (tx_server_seq%1000 == 0)
				sysprintf("Send #%d\n", tx_server_seq);
		}
		else // single ack, no data
			seqack_append(subCmd, SUBCMD_LEN);
	}
}

void set_timer(struct itimerval *ts)
{
	ts->it_interval.tv_sec = 0;
	ts->it_interval.tv_usec = 0;
	ts->it_value.tv_sec = ACK_TIMER_SEC;
	ts->it_value.tv_usec = ACK_TIMER_USEC;
	setitimer(ITIMER_REAL, ts, NULL);
}

void del_timer(struct itimerval *ts)
{
	ts->it_interval.tv_sec = 0;
	ts->it_interval.tv_usec = 0;
	ts->it_value.tv_sec = 0;
	ts->it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, ts, NULL);
}

/**
 *@breif 	main()
 */
int main(int argc, char **argv)
{
	char *dev[10] = {"/dev/rpmsg_ctrl0", " "};
	unsigned int i, len = 128;
	int rev1, rev2;
	struct rpmsg_endpoint_info eptinfo;
	int ret;
	int err;
	unsigned int subCmd[4];
	unsigned char Tx_Buffer[130];
	unsigned char Rx_Buffer[130];
	struct itimerval ts;
	int tx_process = 0;

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		printf("Failed to catach CTRL-C signal!\n");
		return -1;
	}

	if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
		printf("Fail ot set alarm signal");
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

	ret = system("/sbin/mdev -s");
	if (ret < 0) {
		printf("\nFailed to load rpmsg_char driver.\n");
		while(1);
	}

	fd[1] = open("/dev/rpmsg0", O_RDWR | O_NONBLOCK);

	if (fd[1] < 0) {
		printf("\n Failed to open rpmsg0 \n");
		while(1);
	}

	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_usec = 0;
	ts.it_value.tv_sec = 0;

	/* A start cmd to release RTP */
	while(1) {
		struct pollfd fds[] = {
		{
			.fd = fd[1],
			.events = POLLOUT },
		};

		memset(subCmd, 0, sizeof(subCmd));
		subCmd[0] |= SUBCMD_START;

		memcpy(Tx_Buffer, subCmd, SUBCMD_LEN);
		// tell M4 rpsmg window size
		*(unsigned int *)(Tx_Buffer + SUBCMD_LEN) = RPMSG_WND;

		rev1 = write(fd[1], Tx_Buffer, SUBCMD_LEN + 4);
		if (rev1 < 0) {
			printf("\n Failed to write#3 \n");
			while(1);
		}

		while(1) {
			err = poll(fds, 1, 10000);

			if ((err == -1) || (err == 0)) {
				sysprintf("\n w1=%d ", err);
			}
			else {
				break;
			}
			if (status_flag & SUBCMD_EXIT)
				return 0;
		}
		printf("\n Write start CMD to RTP, demo start! \n");
		break;
	}

	/* Start demo */
	while(1) {
		struct pollfd fds[] = {
		{
			.fd = fd[1],
			.events = POLLIN, },
		};

		while(1) {
			err = poll(fds, 1, 1000);

			if (err == 1) {
				do {
					rev1 = read(fd[1], Rx_Buffer, 128);
					if(status_flag & SUBCMD_EXIT)
						break;
				} while(rev1 < 0);
				len = rev1;
				break;
			}
			else if (err == 0) {
				if(status_flag & SUBCMD_EXIT)
					return 0;
			}
		}

		if (*(unsigned int *)Rx_Buffer & SUBCMD_SEQ) {
			// update seq from RTP
			rx_server_seq = *(unsigned int *)(Rx_Buffer + SERVER_SEQ_OFFSET) & SEQ_DIGITS;
			tx_client_seq = rx_server_seq;
		}

		receive_rxdata(Rx_Buffer + SUBCMD_LEN, len - SUBCMD_LEN);

		// do write
		memset(Tx_Buffer, 0, sizeof(Tx_Buffer));
#if (TXRX_FREE_RUN > 0)
		tx_process = txdata_pack(Tx_Buffer, &len);
#endif

		// seqack controlled by timer or diff of seq.
		if ((ack_ready == ACK_READY) && (seq_start != tx_client_seq)) {
			set_timer(&ts);
			ack_ready = ACK_BUSY;
		}
		else if (abs((int)(tx_client_seq - seq_start)) >= RPMSG_WND/2) {
			// sender is faster than timer, reset timer and seqack now
			ack_ready = ACK_READY;
			del_timer(&ts);
			// seqack
			seqack_append(Tx_Buffer, SUBCMD_LEN);
			tx_process = 0;
		}

		if (tx_process) {
			ack_ready = ACK_READY;
			del_timer(&ts);
			// seqack
			seqack_append(Tx_Buffer, len);
			tx_process = 0;
			if (tx_server_seq%1000 == 0)
				sysprintf("Send #%d\n", tx_server_seq);
		}

		if (status_flag & SUBCMD_EXIT)
			return 0;
	}

	return 0;
}
