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

/**
 * Default shared memory size is 0x80 bytes for SRAM.
 *
 * If and only if "rpmsg-ddr-buf" in device tree is defined,
 * shared memory size could be between 0x80 and 0x4000,
 * but it should not exceed the range of memory region
 * "rpmsg_buf" defined in the device tree.
 */
#define MAILBOX_LEN            0x80

/* Debug level 0~2 */
#define RPMSG_DEBUG_LEVEL      1

/**
 * @brief CMD frame
 * ---Appl Level---
 * SubCMD(4 bytes) + ServerSEQ(4 bytes) + ClientSEQ(4 bytes) + reserved(4 bytes) + payload(0-112 bytes)
 * As server :
 *     Tx : Send ServerSEQ, Rx : Check for ClientSEQ
 * As client :
 *     Rx : Receive ServerSEQ, Tx : Reply ClientSEQ
 */
#define SUBCMD_LEN     16
#define PAYLOAD_LEN    (MAILBOX_LEN - SUBCMD_LEN)
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

#if (MAILBOX_LEN > 0x4000)
	#error The maximum length of mailbox should not over 16KB
#endif

typedef int (*rpmsg_tx_cb_t)(unsigned char *, int *);
typedef void (*rpmsg_rx_cb_t)(unsigned char *, int);

struct rpmsg_endpoint_info
{
	char name[32];
	__u32 src;
	__u32 dst;
};

struct ma35_rpmsg {
	unsigned int ack_ready;
	// A35 as server
	unsigned int tx_server_seq;
	unsigned int rx_client_seq;
	// A35 as client
	unsigned int rx_server_seq;
	unsigned int tx_client_seq;
	unsigned int status_flag;
	unsigned int seq_start;

	unsigned int subCmd[4];
	int tx_process;
	int tx_en;
	int rx_en;
	rpmsg_tx_cb_t tx_cb;
	rpmsg_rx_cb_t rx_cb;
};

struct ma35_rpmsg rpmsg;
int fd[2];

/**
 * @brief Send data A35 -> RTP
 *
 * @param data
 * @param len payload only
 * @return int
 */
int rpmsg_tx_cb(unsigned char *data, int *len)
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

/**
 * @brief Receive data RTP -> A35
 *
 * @param data
 * @param len payload only
 */
void rpmsg_rx_cb(unsigned char *data, int len)
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
		rpmsg.status_flag |= SUBCMD_EXIT;
		Tx_Buffer[0] |= SUBCMD_EXIT;
		ret = write(fd[1], Tx_Buffer, SUBCMD_LEN);
		if (ret < 0) {
			printf("\n Failed to write#1 \n");
			while(1);
		}
		sysprintf("\nSIGINT signal catched!\n");
	}
}

int txdata_pack(unsigned char *txbuffer, int *len)
{
	unsigned int subCmd[4] = {0};

	if(rpmsg.tx_cb == NULL)
		return 0;

	if(rpmsg.tx_cb(txbuffer + SUBCMD_LEN, len)) {
		subCmd[0] |= SUBCMD_SEQ;
		subCmd[1] |= ++rpmsg.tx_server_seq & SEQ_DIGITS;
		memcpy(txbuffer, subCmd, SUBCMD_LEN);
		*len += SUBCMD_LEN;
		if (rpmsg.tx_server_seq%1000 == 0 && rpmsg.tx_server_seq)
			sysprintf("Send #%d\n", rpmsg.tx_server_seq);
		return 1;
	}

	if(*len == 0) {
		*len = SUBCMD_LEN;
		return 1;
	}

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

	rpmsg.seq_start = rpmsg.tx_client_seq;
	subCmd[0] = *(unsigned int *)Tx_Buffer | SUBCMD_SEQACK;
	subCmd[1] = *((unsigned int *)Tx_Buffer + 1);
	subCmd[2] = rpmsg.seq_start & SEQ_DIGITS;
	memcpy(Tx_Buffer, subCmd, SUBCMD_LEN);

	ret = write(fd[1], Tx_Buffer, len);
	if (ret < 0) {
		if(rpmsg.status_flag & SUBCMD_EXIT)
			return;
		printf("\n Failed to write#2 \n");
		while(1);
	}
	rpmsg.ack_ready = ACK_READY;

	if (rpmsg.tx_client_seq%1000 == 0 && rpmsg.tx_client_seq)
		sysprintf("ack #%d\n", rpmsg.tx_client_seq);

	while(1) {
		err = poll(fds, 1, 1000);

		if (err == -1) {
			sysprintf("\n w3=%d ", err);
		}
		else {
			break;
		}
		if (rpmsg.status_flag & SUBCMD_EXIT)
			return;
	}
}

int ma35_rpmsg_kick(struct ma35_rpmsg *rpmsg, unsigned char *Tx_Buffer)
{
	int ret, err;

	/* A start cmd to release RTP */
	while(1) {
		struct pollfd fds[] = {
		{
			.fd = fd[1],
			.events = POLLOUT },
		};

		memset(rpmsg->subCmd, 0, sizeof(rpmsg->subCmd));
		rpmsg->subCmd[0] |= SUBCMD_START;

		memcpy(Tx_Buffer, rpmsg->subCmd, SUBCMD_LEN);
		// tell M4 rpsmg window size
		*(unsigned int *)(Tx_Buffer + SUBCMD_LEN) = RPMSG_WND;

		ret = write(fd[1], Tx_Buffer, SUBCMD_LEN + 4);
		if (ret < 0) {
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
			if (rpmsg->status_flag & SUBCMD_EXIT)
				return 0;
		}
		printf("\n Write start CMD to RTP, demo start! \n");
		break;
	}

	return 0;
}

int ma35_rpmsg_open(struct ma35_rpmsg *rpmsg, int *fd, struct rpmsg_endpoint_info *info,
					rpmsg_rx_cb_t rxcb, rpmsg_tx_cb_t txcb)
{
	int ret;
	unsigned char Tx_Buffer[SUBCMD_LEN*2];

	if(sizeof(fd) < sizeof(int) * 2)
	{
		printf("file desc not enough.\n");
		return -EPERM;
	}
	memset(rpmsg, 0, sizeof(rpmsg));
	rpmsg->rx_cb = rxcb;
	rpmsg->tx_cb = txcb;

	fd[0] = open("/dev/rpmsg_ctrl0", O_RDWR | O_NONBLOCK);
	if (fd[0] < 0) {
		printf("\n Failed to open \n");
		return -ENOENT;
	}

	strcpy(info->name, "rpmsg-test");
	info->src = 0;
	info->dst = 0xFFFFFFFF;

	ret = rpmsg_create_ept(fd[0], info);
	if (ret) {
		printf("failed to create RPMsg endpoint.\n");
		return -EINVAL;
	}

	fd[1] = open("/dev/rpmsg0", O_RDWR | O_NONBLOCK);

	if (fd[1] < 0) {
		printf("\n Failed to open rpmsg0 \n");
		while(1);
	}

	ma35_rpmsg_kick(rpmsg, Tx_Buffer);

	return 0;
}

int rpmsg_ack_process(struct ma35_rpmsg *rpmsg, struct pollfd *fds, unsigned char *Rx_Buffer, int *len)
{
	int err, ret;

	while(1) {
		err = poll(fds, 1, 1000);

		if (err == 1) {
			do {
				ret = read(fd[1], Rx_Buffer, *len);
				if(rpmsg->status_flag & SUBCMD_EXIT)
					break;
			} while(ret < 0);
			*len = ret;
			break;
		}
		else if (err == 0) {
			if(rpmsg->status_flag & SUBCMD_EXIT)
				return -1;
		}
	}

	if (*(unsigned int *)Rx_Buffer & SUBCMD_SEQ) {
		// update seq from RTP
		rpmsg->rx_server_seq = *(unsigned int *)(Rx_Buffer + SERVER_SEQ_OFFSET) & SEQ_DIGITS;
		rpmsg->tx_client_seq = rpmsg->rx_server_seq;
	}

	return 0;
}

int ma35_rpmsg_read(struct ma35_rpmsg *rpmsg, struct pollfd *fds, unsigned char *Rx_Buffer, int *len)
{
	*len = MAILBOX_LEN;
	if(rpmsg_ack_process(rpmsg, fds, Rx_Buffer, len)) {
		printf("failed to read.\n");
		return -1;
	}

	rpmsg->rx_en = 1;

	if(rpmsg->rx_cb)
		rpmsg->rx_cb(Rx_Buffer + SUBCMD_LEN, *len - SUBCMD_LEN);

	return 0;
}

int ma35_rpmsg_send(struct ma35_rpmsg *rpmsg, unsigned char *Tx_Buffer, int *len)
{
	memset(Tx_Buffer, 0, sizeof(Tx_Buffer));
	rpmsg->tx_process = txdata_pack(Tx_Buffer, len);

	if (rpmsg->tx_process) {
		rpmsg->ack_ready = ACK_READY;
		seqack_append(Tx_Buffer, *len);
		rpmsg->tx_process = 0;
	}

	rpmsg->tx_en = 1;

	return 0;
}

int ma35_rpmsg_cmd_handler(struct ma35_rpmsg *rpmsg, struct pollfd *fds)
{
	int err;
	unsigned int subCmd[4] = {0};

	if(rpmsg->tx_en)
		rpmsg->tx_en = 0;
	else
		seqack_append(subCmd, SUBCMD_LEN);

	if(rpmsg->rx_en)
		rpmsg->rx_en = 0;
	else
	{
		int len = SUBCMD_LEN;
		unsigned char Rx_Buffer[SUBCMD_LEN];

		rpmsg_ack_process(rpmsg, fds, Rx_Buffer, &len);
	}

	if (rpmsg->status_flag & SUBCMD_EXIT) {
		/* check for the last RTP response before exit */
		err = poll(fds, 1, 1000);

		return -1;
	}

	return 0;
}

/**
 *@breif 	main()
 */
int main(int argc, char **argv)
{
	char *dev[10] = {"/dev/rpmsg_ctrl0", " "};
	unsigned int len;
	struct rpmsg_endpoint_info eptinfo;
	unsigned char Tx_Buffer[MAILBOX_LEN];
	unsigned char Rx_Buffer[MAILBOX_LEN];
	int ret;

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		printf("Failed to catach CTRL-C signal!\n");
		return -1;
	}

	printf("\n Demo rpmsg v2 \n");
	printf("\n Please ensure that the configurations in device tree are compatible with RTP.\n");

	ret = ma35_rpmsg_open(&rpmsg, fd, &eptinfo, rpmsg_rx_cb, rpmsg_tx_cb);
	if(ret) {
		printf("Fail to open rpmsg.\n");
		return 0;
	}

	/* Start demo */
	while(1) {
		struct pollfd fds[] = {
		{
			.fd = fd[1],
			.events = POLLIN, },
		};

		ma35_rpmsg_read(&rpmsg, fds, Rx_Buffer, &len);

		ma35_rpmsg_send(&rpmsg, Tx_Buffer, &len);

		if(ma35_rpmsg_cmd_handler(&rpmsg, fds))
			break;
	}

	return 0;
}
