// SPDX-License-Identifier: GPL-2.0
/*
 * MA35D1 TRNG Driver Test/Example Program
 *
 * The sample program for MA35D1 TRNG
 *
 * Copyright (c) 2022 Nuvoton technology corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;version 2 of the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "../crypto/if_alg.h"

#if __has_include(<linux/ma35d1-trng.h>)
#include <linux/ma35d1-trng.h>
#else
#include <linux/ioctl.h>

#define MA35D1_TRNG_IOC_MAGIC		0xb8
#define MA35D1_TRNG_IOC_WRITE_KS	_IOWR(MA35D1_TRNG_IOC_MAGIC, 0x00, int)
#endif

#define MA35D1_KS_MAGIC			'K'
#define KS_IOCTL_READ			_IOWR(MA35D1_KS_MAGIC, 1, struct ks_read_args *)
#define KS_IOCTL_ERASE_ALL		_IOWR(MA35D1_KS_MAGIC, 5, unsigned long)
#define KS_SRAM				0x0

#ifndef MA35D1_TRNG_KS_OWNER_AES
#define MA35D1_TRNG_KS_OWNER_AES	0
#endif

#define TRNG_DEV	"/dev/hwrng"
#define TRNG_BYTES	64
#define TRNG_LOOPS	3
#define AES_BUF_SIZE	64
#define AES_KS_LOOPS	5

#ifndef AF_ALG
#define AF_ALG		38
#endif

#ifndef SOL_ALG
#define SOL_ALG		279
#endif

/* See examples/crypto/crypto.c: AES key from Key Store. */
#define AES_KS_KEYLEN	17
#define AES_KS_SZ_256	2
#define AES_KS_SRAM	0

struct ks_read_args {
	unsigned int  type;
	int           key_idx;
	int           word_cnt;        /* word count of the key */
	unsigned int  key[128];
};

static int read_full(int fd, void *buf, size_t count)
{
	unsigned char *pos = buf;
	size_t done = 0;

	while (done < count) {
		ssize_t ret = read(fd, pos + done, count - done);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("read");
			return -1;
		}

		if (ret == 0) {
			fprintf(stderr, "read: unexpected end of file\n");
			return -1;
		}

		done += ret;
	}

	return 0;
}

static void dump_words(const uint32_t *buf, size_t words)
{
	size_t i;

	for (i = 0; i < words; i++)
		printf("%08x%c", buf[i], (i + 1 == words) ? '\n' : ' ');
}

static void print_data(const char *str, const unsigned char *buf, size_t len)
{
	size_t i;

	printf("%s: ", str);
	for (i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
}

static int aes_crypt(int encrypt, const unsigned char *in, size_t inlen,
		     unsigned char *out, int key_num)
{
	static const unsigned char iv[16] = {
		0x3d, 0xaf, 0xba, 0x42, 0x9d, 0x9e, 0xb4, 0x30,
		0xb4, 0x22, 0xda, 0x80, 0x2c, 0x9f, 0xac, 0x41
	};
	unsigned char key_ks[AES_KS_KEYLEN] = {
		AES_KS_SZ_256, AES_KS_SRAM, key_num
	};
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "skcipher",
		.salg_name = "cbc(aes)"
	};
	char cbuf[CMSG_SPACE(sizeof(uint32_t)) +
		  CMSG_SPACE(sizeof(struct af_alg_iv) + sizeof(iv))] = {};
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	struct af_alg_iv *alg_iv;
	struct iovec iov;
	ssize_t ret;
	int opfd = -1;
	int tfmfd;
	int err = -1;

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0) {
		perror("socket(AF_ALG)");
		return -1;
	}

	if (bind(tfmfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind cbc(aes)");
		goto out_close_tfm;
	}

	if (setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY, key_ks,
		       AES_KS_KEYLEN) < 0) {
		fprintf(stderr, "ALG_SET_KEY key-store key %d failed: %s\n",
			key_num, strerror(errno));
		goto out_close_tfm;
	}

	opfd = accept(tfmfd, NULL, 0);
	if (opfd < 0) {
		perror("accept(AF_ALG)");
		goto out_close_tfm;
	}

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(sizeof(uint32_t));
	*(uint32_t *)CMSG_DATA(cmsg) = encrypt ? ALG_OP_ENCRYPT :
						ALG_OP_DECRYPT;

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_IV;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct af_alg_iv) + sizeof(iv));
	alg_iv = (void *)CMSG_DATA(cmsg);
	alg_iv->ivlen = sizeof(iv);
	memcpy(alg_iv->iv, iv, sizeof(iv));

	iov.iov_base = (void *)in;
	iov.iov_len = inlen;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	ret = sendmsg(opfd, &msg, 0);
	if (ret < 0) {
		perror(encrypt ? "AES encrypt sendmsg" : "AES decrypt sendmsg");
		goto out_close_op;
	}
	if ((size_t)ret != inlen) {
		fprintf(stderr, "%s accepted only %zd of %zu bytes\n",
			encrypt ? "AES encrypt" : "AES decrypt", ret, inlen);
		goto out_close_op;
	}

	ret = read(opfd, out, inlen);
	if (ret < 0) {
		perror(encrypt ? "AES encrypt read" : "AES decrypt read");
		goto out_close_op;
	}
	if ((size_t)ret != inlen) {
		fprintf(stderr, "%s returned %zd of %zu bytes\n",
			encrypt ? "AES encrypt" : "AES decrypt", ret, inlen);
		goto out_close_op;
	}

	err = 0;

out_close_op:
	close(opfd);
out_close_tfm:
	close(tfmfd);
	return err;
}

static int write_trng_aes_key_to_ks(int fd, int *key_num)
{
	int arg = MA35D1_TRNG_KS_OWNER_AES;

	if (ioctl(fd, MA35D1_TRNG_IOC_WRITE_KS, &arg) < 0) {
		fprintf(stderr,
			"MA35D1_TRNG_IOC_WRITE_KS(owner=AES) failed: %s\n",
			strerror(errno));
		return -1;
	}

	*key_num = arg;
	printf("MA35D1_TRNG_IOC_WRITE_KS(owner=AES) key number: %d\n",
	       *key_num);

	return 0;
}

static int aes_ks_key_test(int key_num)
{
	unsigned char plain[AES_BUF_SIZE];
	unsigned char cipher[AES_BUF_SIZE];
	unsigned char decoded[AES_BUF_SIZE];
	int i;

	for (i = 0; i < AES_BUF_SIZE; i++)
		plain[i] = i & 0xff;

	memset(cipher, 0, sizeof(cipher));
	memset(decoded, 0, sizeof(decoded));

	if (aes_crypt(1, plain, sizeof(plain), cipher, key_num) < 0) {
		fprintf(stderr,
			"AES key-store key %d encrypt test failed\n", key_num);
		return -1;
	}

	printf("AES key-store key %d encrypt output:\n", key_num);
	print_data("OUT", cipher, sizeof(cipher));

	if (aes_crypt(0, cipher, sizeof(cipher), decoded, key_num) < 0) {
		fprintf(stderr,
			"AES key-store key %d decrypt test failed\n", key_num);
		return -1;
	}

	if (memcmp(decoded, plain, sizeof(plain))) {
		fprintf(stderr,
			"AES key-store key %d decode compare failed\n", key_num);
		print_data("ORIGINAL", plain, sizeof(plain));
		print_data("DECODED ", decoded, sizeof(decoded));
		return -1;
	}

	printf("AES key-store key %d decode compare OK\n", key_num);

	return 0;
}

static int ks_sram_read_test(const int *key_nums, size_t key_count)
{
	struct ks_read_args r_args;
	size_t i;
	int fd;
	int ret;

	fd = open("/dev/ksdev", O_RDWR);
	if (fd < 0) {
		perror("open /dev/ksdev");
		return -1;
	}

	printf("\nVerify TRNG-generated KS SRAM keys are not readable\n");

	for (i = 0; i < key_count; i++) {
		memset(&r_args, 0, sizeof(r_args));
		r_args.type = KS_SRAM;
		r_args.key_idx = key_nums[i];
		r_args.word_cnt = 8; /* AES-256 */

		printf("[Read test %zu] SRAM key %d: ", i + 1, key_nums[i]);
		ret = ioctl(fd, KS_IOCTL_READ, &r_args);
		if (ret < 0) {
			printf("PASS, read blocked (%s)\n", strerror(errno));
			continue;
		}

		printf("FAIL, key data was readable\n");
		print_data("KEY", (unsigned char *)r_args.key,
			   r_args.word_cnt * sizeof(r_args.key[0]));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int trng_aes_ks_demo(int trng_fd)
{
	int key_nums[AES_KS_LOOPS];
	int i;

	printf("\nTRNG write AES key-store key and AES encode/decode demo\n");

	for (i = 0; i < AES_KS_LOOPS; i++) {
		printf("\n[Round %d] Generate AES key-store key from TRNG\n", i + 1);

		if (write_trng_aes_key_to_ks(trng_fd, &key_nums[i]) < 0) {
			fprintf(stderr,
				"Round %d failed: could not write TRNG AES key to key store\n",
				i + 1);
			return -1;
		}

		if (aes_ks_key_test(key_nums[i]) < 0) {
			fprintf(stderr,
				"Round %d failed: AES encode/decode test failed for key %d\n",
				i + 1, key_nums[i]);
			return -1;
		}

		printf("Round %d OK: key %d passed AES encode/decode\n",
		       i + 1, key_nums[i]);
	}

	printf("\nStored AES key numbers:");
	for (i = 0; i < AES_KS_LOOPS; i++)
		printf(" %d", key_nums[i]);
	printf("\n");

	if (ks_sram_read_test(key_nums, AES_KS_LOOPS) < 0)
		return -1;

	printf("\nRetest stored AES key-store keys without TRNG write\n");

	for (i = 0; i < AES_KS_LOOPS; i++) {
		printf("\n[Retest %d] AES key-store key number: %d\n",
		       i + 1, key_nums[i]);

		if (aes_ks_key_test(key_nums[i]) < 0) {
			fprintf(stderr,
				"Retest %d failed: AES encode/decode test failed for key %d\n",
				i + 1, key_nums[i]);
			return -1;
		}

		printf("Retest %d OK: key %d passed AES encode/decode\n",
		       i + 1, key_nums[i]);
	}

	printf("\nTRNG AES key-store demo passed\n");

	return 0;
}

int main(void)
{
	uint32_t trng_buff[TRNG_BYTES / sizeof(uint32_t)];
	int fd, ret;
	int loop;

	fd = open("/dev/ksdev", O_RDWR);
	if (fd < 0) {
		printf("open ks_dev faild!!!\n");
		return -1;
	}

	ret = ioctl(fd, KS_IOCTL_ERASE_ALL, 0);
	if (ret != 0) {
		printf("KS_IOCTL_ERASE_ALL failed: %d!\n", ret);
		return -1;
	}

	close(fd);

	fd = open(TRNG_DEV, O_RDONLY);
	if (fd < 0) {
		perror("open " TRNG_DEV);
		return 1;
	}

	if (trng_aes_ks_demo(fd) < 0) {
		fprintf(stderr, "TRNG AES key-store demo failed\n");
		close(fd);
		return 1;
	}

	for (loop = 0; loop < TRNG_LOOPS; loop++) {
		if (read_full(fd, trng_buff, sizeof(trng_buff)) < 0) {
			fprintf(stderr, "TRNG read loop %d failed\n", loop + 1);
			close(fd);
			return 1;
		}

		printf("TRNG DATA ==>\n");
		dump_words(trng_buff, sizeof(trng_buff) / sizeof(trng_buff[0]));
	}

	close(fd);
	return 0;
}
