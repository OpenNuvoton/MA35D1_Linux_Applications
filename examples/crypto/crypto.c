/*
 * Copyright (c) 2022 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include "if_alg.h"

#ifndef AF_ALG
#define AF_ALG			38
#endif

#ifndef SOL_ALG
#define SOL_ALG			279
#endif

#define PTEST_TOTAL		0x40000UL
#define PTEST_BLK_MAX_SIZE	0x10000
#define SHA_TEST_TOTAL		0x10000UL

#define AES_BUF_SIZE		64

/* tricky for using AES key from Key Store */
#define AES_KS_KEYLEN		17
#define AES_KS_SZ_128		0
#define AES_KS_SZ_192		1
#define AES_KS_SZ_256		2
#define AES_KS_SRAM		0
#define AES_KS_OTP		1

static int use_std_alg;
static void hex_to_bin(const char *h, unsigned char *b);

char out[PTEST_BLK_MAX_SIZE];
char in[PTEST_BLK_MAX_SIZE];

/*
 *  ecb(aes) has ivsize 0, so no ALG_SET_IV is sent. AF_ALG then hands
 *  ZERO_SIZE_PTR as req->iv to ma35_aes_dma_start(), which dereferences it.
 *  Vector: FIPS-197 Appendix C.1 (AES-128).
 */
static void alg_set_name(struct sockaddr_alg *sa, const char *name)
{
	memset(sa->salg_name, 0, sizeof(sa->salg_name));
	strncpy((char *)sa->salg_name, name, sizeof(sa->salg_name) - 1);
}

/*
 * "ma35-optee-cbc-aes" or "ma35-cbc-aes" -> "cbc(aes)", "ma35-optee-hmac-sha256" ->
 * "hmac(sha256)", "ma35-optee-aes-gcm" -> "gcm(aes)", "ma35-optee-sha256" -> "sha256".
 * Returns 0 if the name is not a "ma35-" driver name.
 */
static int ma35_to_std(const char *drv, char *std, size_t size)
{
	static const char * const mode[] = { "cbc", "ecb", "cfb", "ofb", "ctr", "hmac" };
	const char *p, *dash;
	size_t i, len;

	if (strncmp(drv, "ma35-", 5))
		return 0;
	p = drv + 5;
	if (!strncmp(p, "optee-", 6))
		p += 6;

	if (!strcmp(p, "aes-gcm")) {
		snprintf(std, size, "gcm(aes)");
		return 1;
	}
	if (!strcmp(p, "aes-ccm")) {
		snprintf(std, size, "ccm(aes)");
		return 1;
	}
	dash = strchr(p, '-');
	if (dash) {
		len = dash - p;
		for (i = 0; i < sizeof(mode) / sizeof(mode[0]); i++) {
			if (strlen(mode[i]) == len && !strncmp(p, mode[i], len)) {
				snprintf(std, size, "%s(%s)", mode[i], dash + 1);
				return 1;
			}
		}
	}
	snprintf(std, size, "%s", p);
	return 1;
}

/*
 * Bind a "ma35-*" driver name (TSI on linux-6.18, 6.12 and 6.6). linux-5.10 names its drivers
 * "nuvoton-*" and keeps the TSI priority above the CPU, so if the name does not exist, fall
 * back to the generic algorithm name, which is then served by TSI. On linux-6.18 that fallback is
 * the ARMv8 CE implementation, not TSI.
 */
static int alg_bind(int fd, struct sockaddr_alg *sa)
{
	char std[sizeof(sa->salg_name)], drv[sizeof(sa->salg_name)];
	int ret = bind(fd, (struct sockaddr *)sa, sizeof(*sa));

	if (ret == 0 || errno != ENOENT)
		return ret;
	snprintf(drv, sizeof(drv), "%s", (char *)sa->salg_name);
	if (!ma35_to_std(drv, std, sizeof(std)))
		return ret;
	alg_set_name(sa, std);
	ret = bind(fd, (struct sockaddr *)sa, sizeof(*sa));
	if (ret == 0)
		printf("  [bind] %s not found, using generic name %s\n", drv, std);
	return ret;
}

/*------------------------------------------------------------------------*/
/*  Regular demos: AES, SHA and performance                               */
/*------------------------------------------------------------------------*/

/*
 * On linux-6.18 the TSI algorithms have a lower priority than the ARMv8 CE ones,
 * so a generic name such as "cbc(aes)" selects the CPU. hw_bind() tries the TSI
 * driver name first ("cbc(aes)" -> "ma35-optee-cbc-aes", or "ma35-cbc-aes" for a
 * kernel without OP-TEE), then falls back to the generic name. linux-5.10 keeps
 * the TSI priority above CE, so the generic name reaches TSI there.
 * "std" on the command line skips the TSI names, to compare with the CPU.
 */
static int hw_bind(int fd, struct sockaddr_alg *sa, const char *stdname)
{
	static char shown[sizeof(sa->salg_name)];
	char mapped[sizeof(sa->salg_name)], name[sizeof(sa->salg_name) + 16];
	const char *p = strchr(stdname, '(');
	int ret = -1;

	if (p)
		snprintf(mapped, sizeof(mapped), "%.*s-%.*s", (int)(p - stdname), stdname,
			 (int)strcspn(p + 1, ")"), p + 1);
	else
		snprintf(mapped, sizeof(mapped), "%s", stdname);

	if (!use_std_alg) {
		snprintf(name, sizeof(name), "ma35-optee-%s", mapped);
		alg_set_name(sa, name);
		ret = bind(fd, (struct sockaddr *)sa, sizeof(*sa));
		if (ret < 0) {
			snprintf(name, sizeof(name), "ma35-%s", mapped);
			alg_set_name(sa, name);
			ret = bind(fd, (struct sockaddr *)sa, sizeof(*sa));
		}
	}
	if (ret < 0) {
		alg_set_name(sa, stdname);
		ret = bind(fd, (struct sockaddr *)sa, sizeof(*sa));
	}
	if (ret == 0 && strcmp(shown, (char *)sa->salg_name)) {
		snprintf(shown, sizeof(shown), "%s", (char *)sa->salg_name);
		printf("  [bind] %s -> %s%s\n", stdname, shown,
		       strcmp(shown, stdname) ? "" : " (generic name)");
	}
	return ret;
}

void print_data(char *str, char *buff, int len)
{
	int i;

	printf("%s: ", str);
	for (i = 0; i < len; i++)
		printf("%02x", (unsigned char)buff[i]);
	printf("\n");
}

/* returns 0, or -errno */
static int aes_crypt(const char *protocol, int encrypt, char *in, int inlen, char *out,
		     const char *key, int keylen, char *oiv)
{
	int opfd;
	int tfmfd;
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "skcipher",
	};
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	char cbuf[CMSG_SPACE(4) + CMSG_SPACE(20)] = {};
	struct af_alg_iv *iv;
	struct iovec iov;
	int ret = 0, n;

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0)
		return -errno;

	if (hw_bind(tfmfd, &sa, protocol) < 0 ||
	    setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY, key, keylen) < 0) {
		ret = -errno;
		close(tfmfd);
		return ret;
	}

	opfd = accept(tfmfd, NULL, 0);
	if (opfd < 0) {
		ret = -errno;
		close(tfmfd);
		return ret;
	}
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(__u32 *)CMSG_DATA(cmsg) = encrypt ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT;

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_IV;
	cmsg->cmsg_len = CMSG_LEN(20);
	iv = (void *)CMSG_DATA(cmsg);
	memcpy(iv->iv, oiv, 16);
	iv->ivlen = 16;
	iov.iov_base = in;
	iov.iov_len = inlen;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (sendmsg(opfd, &msg, 0) < 0) {
		ret = -errno;
	} else {
		n = read(opfd, out, inlen);
		if (n != inlen)
			ret = n < 0 ? -errno : -EIO;
	}
	close(opfd);
	close(tfmfd);
	return ret;
}

static const char aes_demo_key[32] = "\x06\xa9\x21\x40\x36\xb8\xa1\x5b"
				     "\x51\x2e\x03\xd5\x34\x12\x00\x06"
				     "\x06\xa9\x21\x40\x36\xb8\xa1\x5b"
				     "\x51\x2e\x03\xd5\x34\x12\x00\x06";
static const char aes_demo_iv[16] = "\x3d\xaf\xba\x42\x9d\x9e\xb4\x30\xb4\x22\xda\x80\x2c\x9f\xac\x41";

/* AES-256 CBC of 00 01 02 .. 3f, see the test pattern at the end of this file */
#define AES_DEMO_OUT \
	"ef8b1fccd72d0a1fd6ac191a7ae62292fd3a996a45761d2f79dd8ff2c7e5101375eadb453ee09c16f791b6a18a6090b829f569c6973b16cc9f84c9fb86bf398f"

/* encrypt in[] and decrypt it back; returns the number of failures */
static int aes_roundtrip(const char *key, int keylen, const unsigned char *expect)
{
	char iv[16], enc[AES_BUF_SIZE], dec[AES_BUF_SIZE];
	int i, r, fail = 0;

	for (i = 0; i < AES_BUF_SIZE; i++)
		in[i] = i & 0xff;

	memcpy(iv, aes_demo_iv, 16);
	memset(enc, 0, sizeof(enc));
	r = aes_crypt("cbc(aes)", 1, in, AES_BUF_SIZE, enc, key, keylen, iv);
	printf("\nAES encrypt result =>\n");
	print_data("IN", in, AES_BUF_SIZE);
	if (r < 0) {
		printf("  encrypt error %d (%s)\n", r, strerror(-r));
		return 1;
	}
	print_data("OUT", enc, AES_BUF_SIZE);
	if (expect && memcmp(enc, expect, AES_BUF_SIZE)) {
		printf("  ==> ciphertext MISMATCH\n");
		fail++;
	}

	memcpy(iv, aes_demo_iv, 16);
	memset(dec, 0, sizeof(dec));
	r = aes_crypt("cbc(aes)", 0, enc, AES_BUF_SIZE, dec, key, keylen, iv);
	printf("\nAES decrypt result =>\n");
	print_data("IN", enc, AES_BUF_SIZE);
	if (r < 0) {
		printf("  decrypt error %d (%s)\n", r, strerror(-r));
		return fail + 1;
	}
	print_data("OUT", dec, AES_BUF_SIZE);
	if (memcmp(dec, in, AES_BUF_SIZE)) {
		printf("  ==> plaintext MISMATCH\n");
		fail++;
	}
	return fail;
}

static int AES_demo(void)
{
	unsigned char expect[AES_BUF_SIZE];
	int fail;

	printf("\n+---------------------------------------------------------------+\n");
	printf("|  [AF_ALG] AES-256 CBC mode encrypt/descrpt demo               |\n");
	printf("+---------------------------------------------------------------+\n");

	hex_to_bin(AES_DEMO_OUT, expect);
	fail = aes_roundtrip(aes_demo_key, 32, expect);
	printf("  RESULT: %s\n", fail ? "FAIL" : "PASS");
	return fail ? -1 : 0;
}

/*======================================================================================*/
/*  Test vectors for AES key from Key Store                                             */
/*======================================================================================*/
/*
 *  This test vector was generated by using jason file:
 *  {
 *      "key3": {
 *          "key": "d37d7ab68bb84d7e89111b615b32dfa0e4299ef5190ea05b6d692d32df060068",
 *          "meta": "aes256-cpu-readable"
 *      },
 *  }
 *  The Key Store key is selected by a 17-byte "key" that only the TSI driver understands,
 *  so this demo does not work with the generic (CPU) algorithm.
 */
static int AES_KS_demo(void)
{
	const char key[32] = "\xd3\x7d\x7a\xb6\x8b\xb8\x4d\x7e"
			     "\x89\x11\x1b\x61\x5b\x32\xdf\xa0"
			     "\xe4\x29\x9e\xf5\x19\x0e\xa0\x5b"
			     "\x6d\x69\x2d\x32\xdf\x06\x00\x68";
	char key_ks[32] = { AES_KS_SZ_256, AES_KS_OTP, 3 };  /* use OTP key 3 of which key length 256 bits */
	char iv[16], enc[AES_BUF_SIZE], enc_ks[AES_BUF_SIZE];
	int i, r, fail = 0;

	printf("\n+---------------------------------------------------------------+\n");
	printf("|  [AF_ALG] AES-256 CBC mode encrypt/descrpt demo               |\n");
	printf("+---------------------------------------------------------------+\n");
	fail += aes_roundtrip(key, 32, NULL);

	printf("\n+-----------------------------------------------------------------+\n");
	printf("|  [AF_ALG] AES-256 CBC mode encrypt/descrpt with Key Store key   |\n");
	printf("+-----------------------------------------------------------------+\n");

	for (i = 0; i < AES_BUF_SIZE; i++)
		in[i] = i & 0xff;
	memcpy(iv, aes_demo_iv, 16);
	memset(enc, 0, sizeof(enc));
	r = aes_crypt("cbc(aes)", 1, in, AES_BUF_SIZE, enc, key, 32, iv);
	if (r < 0) {
		printf("  reference encrypt error %d (%s)\n", r, strerror(-r));
		fail++;
	}
	/* The Key Store key is the same as the key above, so the result must be identical. */
	memcpy(iv, aes_demo_iv, 16);
	memset(enc_ks, 0, sizeof(enc_ks));
	r = aes_crypt("cbc(aes)", 1, in, AES_BUF_SIZE, enc_ks, key_ks, AES_KS_KEYLEN, iv);
	if (r < 0) {
		printf("  Key Store encrypt error %d (%s). The TSI driver and OTP key 3 are required.\n",
		       r, strerror(-r));
		fail++;
	} else {
		print_data("OUT(KS)", enc_ks, AES_BUF_SIZE);
		if (memcmp(enc, enc_ks, AES_BUF_SIZE)) {
			printf("  ==> differs from the result of the same key in software\n");
			fail++;
		}
	}
	printf("  RESULT: %s\n", fail ? "FAIL" : "PASS");
	return fail ? -1 : 0;
}

static void AES_performance(void)
{
	static const struct {
		const char *alg;
		const char *label;
		int keylen;
	} mode[] = {
		{ "ctr(aes)", "AES-128-CTR", 16 }, { "ctr(aes)", "AES-192-CTR", 24 },
		{ "ctr(aes)", "AES-256-CTR", 32 }, { "cbc(aes)", "AES-128-CBC", 16 },
		{ "cbc(aes)", "AES-192-CBC", 24 }, { "cbc(aes)", "AES-256-CBC", 32 },
	};
	char iv[16];
	int blk_size[] = { 16, 64, 256, 1024, 4096, 16384, 0x10000 };
	struct timeval t0, t1;
	unsigned long dcnt, tt;
	int i, m, r;

	printf("\n\nAES performance test ==>\n");

	for (i = 0; i < sizeof(in); i++)
		in[i] = i & 0xff;

	for (i = 0; i < sizeof(blk_size) / sizeof(blk_size[0]); i++) {
		printf("\n\nTest with block size: %d bytes\n", blk_size[i]);

		for (m = 0; m < sizeof(mode) / sizeof(mode[0]); m++) {
			gettimeofday(&t0, NULL);
			for (dcnt = 0; dcnt < PTEST_TOTAL; dcnt += blk_size[i]) {
				memcpy(iv, aes_demo_iv, 16);
				r = aes_crypt(mode[m].alg, 1, in, blk_size[i], out,
					      aes_demo_key, mode[m].keylen, iv);
				if (r < 0) {
					printf("%s error %d (%s)\n", mode[m].label, r, strerror(-r));
					return;
				}
			}
			gettimeofday(&t1, NULL);
			tt = (t1.tv_sec - t0.tv_sec) * 1000000UL + t1.tv_usec - t0.tv_usec;
			tt = tt / 1000 ? tt / 1000 : 1;
			printf("%s %ld KB/s\n", mode[m].label, ((PTEST_TOTAL * 1000UL) / tt) / 1024);
		}
	}
	printf("AES performance test Done.\n");
}

/* test pattern: "abc", HMAC key 891132ef108a2290, see the end of this file */
static const struct {
	const char *alg;
	int bits;
	int hmac;
	const char *digest;
} sha_demo_vec[] = {
	{ "sha1", 160, 0, "a9993e364706816aba3e25717850c26c9cd0d89d" },
	{ "hmac(sha1)", 160, 1, "cfbdfa7b01f0f26996cd2f6bf77f092117fafe1b" },
	{ "sha224", 224, 0, "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7" },
	{ "hmac(sha224)", 224, 1, "1b0e67c793799a9e34c6b2face31ddc781939db23f2bd537ca041cb8" },
	{ "sha256", 256, 0, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
	{ "hmac(sha256)", 256, 1, "a887fe6178cd89f66d38788aa711db4aabebf6036d38c304323802421c927952" },
	{ "sha384", 384, 0,
	  "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7" },
	{ "hmac(sha384)", 384, 1,
	  "05b2b02ded41d928fae54e3549984b36ab13271f7485d1ead36351de0fd244c42d14dcd5018e5a890c93e70bc8aa1661" },
	{ "sha512", 512, 0,
	  "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" },
	{ "hmac(sha512)", 512, 1,
	  "534cd738b46b422d6cc6efc2766fb6f65374cd7858d9fb29f96fb6273717bc3dd5a496ff2ef30e23c28c2835cff737297b7032e4900b8f6ac60f51b8b3a1eb4a" },
	{ "sha3-224", 224, 0, "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf" },
	{ "sha3-256", 256, 0, "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532" },
	{ "sha3-384", 384, 0,
	  "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25" },
	{ "sha3-512", 512, 0,
	  "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0" },
};

static int SHA_demo(const char *alg_name, int digest_size, int is_hmac, const char *digest)
{
	int opfd;
	int tfmfd;
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "hash",
	};
	char msg1[] = { 0x61, 0x62, 0x63 };   /* abc */
	char hmac_key[8] = { 0x89, 0x11, 0x32, 0xef, 0x10, 0x8a, 0x22, 0x90 };
	unsigned char exp[64];
	char buf[64];
	int bytes = digest_size / 8, n, err = 0;

	printf("\n+---------------------------------------------------------------+\n");
	printf("|  [AF_ALG] %-14s demo                                |\n", alg_name);
	printf("+---------------------------------------------------------------+\n");

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0)
		return -errno;

	if (hw_bind(tfmfd, &sa, alg_name) < 0 ||
	    (is_hmac && setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY, hmac_key, 8) < 0)) {
		err = -errno;
		goto out_tfm;
	}

	opfd = accept(tfmfd, NULL, 0);
	if (opfd < 0) {
		err = -errno;
		goto out_tfm;
	}

	if (write(opfd, msg1, sizeof(msg1)) < 0) {
		err = -errno;
		goto out;
	}
	n = read(opfd, buf, bytes);
	if (n != bytes) {
		err = n < 0 ? -errno : -EIO;
		goto out;
	}
	print_data("  Output digest", buf, bytes);
	hex_to_bin(digest, exp);
	if (memcmp(buf, exp, bytes)) {
		printf("  ==> digest MISMATCH\n");
		err = -EBADMSG;
	}
out:
	close(opfd);
out_tfm:
	close(tfmfd);
	if (err && err != -EBADMSG)
		printf("  error %d (%s)\n", err, strerror(-err));
	printf("  RESULT: %s\n", err ? "FAIL" : "PASS");
	return err ? -1 : 0;
}

static int SHA_demo_all(void)
{
	int i, fail = 0;

	for (i = 0; i < sizeof(sha_demo_vec) / sizeof(sha_demo_vec[0]); i++)
		if (SHA_demo(sha_demo_vec[i].alg, sha_demo_vec[i].bits, sha_demo_vec[i].hmac,
			     sha_demo_vec[i].digest))
			fail++;
	return fail ? -1 : 0;
}

static void SHA_performance(const char *alg_name, int digest_size, int message_len)
{
	int opfd;
	int tfmfd;
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "hash",
	};
	struct timeval t0, t1;
	unsigned long tt, tm, dcnt;
	char buf[64];
	int bytes = digest_size / 8;

	printf("\n\n+---------------------------------------------------------------+\n");
	printf("|  %s performance test                                     |\n", alg_name);
	printf("+---------------------------------------------------------------+\n");

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0 || hw_bind(tfmfd, &sa, alg_name) < 0) {
		printf("  error: %s\n", strerror(errno));
		if (tfmfd >= 0)
			close(tfmfd);
		return;
	}

	opfd = accept(tfmfd, NULL, 0);
	if (opfd < 0) {
		printf("  error: %s\n", strerror(errno));
		close(tfmfd);
		return;
	}

	gettimeofday(&t0, NULL);
	for (dcnt = 0; dcnt < SHA_TEST_TOTAL; dcnt += message_len) {
		if (write(opfd, in, message_len) < 0) {
			printf("  error: %s\n", strerror(errno));
			goto out;
		}
	}
	if (read(opfd, buf, bytes) < 0) {
		printf("  error: %s\n", strerror(errno));
		goto out;
	}
	gettimeofday(&t1, NULL);

	tt = (t1.tv_sec - t0.tv_sec) * 1000000UL + t1.tv_usec - t0.tv_usec;
	tm = tt / 1000 ? tt / 1000 : 1;
	printf("PERFORMANCE: %s : %d : %lu us : %lu KB/s\n", alg_name, message_len, tt,
	       ((SHA_TEST_TOTAL * 1000UL) / tm) / 1024);
out:
	close(opfd);
	close(tfmfd);
}

static void SHA_performance_all(void)
{
	static const struct { const char *alg; int bits; } alg[] = {
		{ "sha1", 160 }, { "sha224", 224 }, { "sha256", 256 }, { "sha384", 384 },
		{ "sha512", 512 }, { "sha3-224", 224 }, { "sha3-256", 256 },
		{ "sha3-384", 384 }, { "sha3-512", 512 },
	};
	int message_len[] = { 16, 64, 256, 1024, 4096, 16384, PTEST_BLK_MAX_SIZE };
	int i, j;

	for (i = 0; i < sizeof(in); i++)
		in[i] = i & 0xff;
	for (i = 0; i < sizeof(message_len) / sizeof(message_len[0]); i++)
		for (j = 0; j < sizeof(alg) / sizeof(alg[0]); j++)
			SHA_performance(alg[j].alg, alg[j].bits, message_len[i]);
}

static int aes_ecb_noiv_test(const char *alg_name)
{
	const char key[16] = "\x00\x01\x02\x03\x04\x05\x06\x07"
			     "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f";
	const char pt[16] = "\x00\x11\x22\x33\x44\x55\x66\x77"
			    "\x88\x99\xaa\xbb\xcc\xdd\xee\xff";
	const char expect[16] = "\x69\xc4\xe0\xd8\x6a\x7b\x04\x30"
				"\xd8\xcd\xb7\x80\x70\xb4\xc5\x5a";
	char ct[16] = {0};
	struct sockaddr_alg sa = { .salg_family = AF_ALG, .salg_type = "skcipher" };
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	char cbuf[CMSG_SPACE(4)] = {};
	struct iovec iov;
	int tfmfd, opfd, ret;

	printf("\n+---------------------------------------------------------------+\n");
	printf("|  [AES ECB] AES-128 ECB via AF_ALG: %-26s    |\n", alg_name);
	printf("+---------------------------------------------------------------+\n");

	strncpy((char *)sa.salg_name, alg_name, sizeof(sa.salg_name) - 1);

	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0) {
		perror("socket(AF_ALG)");
		return -1;
	}
	if (alg_bind(tfmfd, &sa) < 0) {
		perror("bind");
		close(tfmfd);
		return -1;
	}
	if (setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY, key, sizeof(key)) < 0) {
		perror("setsockopt(ALG_SET_KEY)");
		close(tfmfd);
		return -1;
	}
	opfd = accept(tfmfd, NULL, 0);
	if (opfd < 0) {
		perror("accept");
		close(tfmfd);
		return -1;
	}

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(__u32 *)CMSG_DATA(cmsg) = ALG_OP_ENCRYPT;

	iov.iov_base = (void *)pt;
	iov.iov_len = sizeof(pt);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	printf("sending 16-byte ECB encrypt request without IV...\n");
	fflush(stdout);

	if (sendmsg(opfd, &msg, 0) < 0) {
		perror("sendmsg");
		ret = -1;
		goto out;
	}
	if (read(opfd, ct, sizeof(ct)) != sizeof(ct)) {
		perror("read");
		ret = -1;
		goto out;
	}

	print_data("IN    ", (char *)pt, sizeof(pt));
	print_data("OUT   ", ct, sizeof(ct));
	print_data("EXPECT", (char *)expect, sizeof(expect));
	ret = memcmp(ct, expect, sizeof(ct)) ? -1 : 0;
	printf("RESULT: %s\n", ret ? "FAIL (mismatch)" : "PASS");
out:
	close(opfd);
	close(tfmfd);
	return ret;
}

/*
 *  ma35-optee-aes-gcm / ma35-optee-aes-ccm.
 *  Every case is compared against the kernel software gcm(aes)/ccm(aes).
 *  The driver may panic, so run ONE case per invocation:
 *      ./crypto-demo aead <case>
 */
#define AEAD_TAGLEN	16
#define AEAD_MAXBUF	256

/* deliberately not byte-order symmetric, so key endianness bugs are not hidden */
static const unsigned char g_key[16] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

/* returns number of output bytes (without AAD), or -errno */
static int aead_run(const char *alg, const unsigned char *key, int keylen,
		    const unsigned char *iv, int ivlen,
		    const unsigned char *aad, int aadlen,
		    const unsigned char *data, int datalen,
		    int encrypt, unsigned char *out)
{
	struct sockaddr_alg sa = { .salg_family = AF_ALG, .salg_type = "aead" };
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	char cbuf[CMSG_SPACE(4) + CMSG_SPACE(4 + 16) + CMSG_SPACE(4)] = {};
	unsigned char inbuf[AEAD_MAXBUF], outbuf[AEAD_MAXBUF];
	struct iovec iov;
	struct af_alg_iv *aiv;
	int tfmfd, opfd, inlen, outlen, n, got = 0, ret;

	strncpy((char *)sa.salg_name, alg, sizeof(sa.salg_name) - 1);
	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0)
		return -errno;
	if (alg_bind(tfmfd, &sa) < 0) {
		ret = -errno;
		goto err_tfm;
	}
	if (setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY, key, keylen) < 0 ||
	    setsockopt(tfmfd, SOL_ALG, ALG_SET_AEAD_AUTHSIZE, NULL, AEAD_TAGLEN) < 0) {
		ret = -errno;
		goto err_tfm;
	}
	opfd = accept(tfmfd, NULL, 0);
	if (opfd < 0) {
		ret = -errno;
		goto err_tfm;
	}

	memcpy(inbuf, aad, aadlen);
	memcpy(inbuf + aadlen, data, datalen);
	inlen = aadlen + datalen;
	outlen = aadlen + datalen + (encrypt ? AEAD_TAGLEN : -AEAD_TAGLEN);

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(__u32 *)CMSG_DATA(cmsg) = encrypt ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT;

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_IV;
	cmsg->cmsg_len = CMSG_LEN(4 + ivlen);
	aiv = (void *)CMSG_DATA(cmsg);
	aiv->ivlen = ivlen;
	memcpy(aiv->iv, iv, ivlen);

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_AEAD_ASSOCLEN;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(__u32 *)CMSG_DATA(cmsg) = aadlen;

	iov.iov_base = inbuf;
	iov.iov_len = inlen;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (sendmsg(opfd, &msg, 0) < 0) {
		ret = -errno;
		goto err_op;
	}
	while (got < outlen) {
		n = read(opfd, outbuf + got, outlen - got);
		if (n < 0) {
			ret = -errno;
			goto err_op;
		}
		if (n == 0)
			break;
		got += n;
	}
	if (got < aadlen) {
		ret = -EIO;
		goto err_op;
	}
	memcpy(out, outbuf + aadlen, got - aadlen);
	ret = got - aadlen;
err_op:
	close(opfd);
err_tfm:
	close(tfmfd);
	return ret;
}

static void aead_dump(const char *tag, const unsigned char *b, int len)
{
	int i;

	printf("  %-8s: ", tag);
	for (i = 0; i < len; i++)
		printf("%02x", b[i]);
	printf("\n");
}

/* encrypt with the driver and with the software alg, compare */
static int aead_enc_cmp(const char *hw, const char *sw, const unsigned char *iv, int ivlen,
			  const unsigned char *aad, int aadlen, const unsigned char *pt, int ptlen)
{
	unsigned char key[16], o_hw[AEAD_MAXBUF], o_sw[AEAD_MAXBUF];
	int n_sw, n_hw;

	memcpy(key, g_key, sizeof(key));
	n_sw = aead_run(sw, key, 16, iv, ivlen, aad, aadlen, pt, ptlen, 1, o_sw);
	printf("  software %s -> %d\n", sw, n_sw);
	fflush(stdout);
	n_hw = aead_run(hw, key, 16, iv, ivlen, aad, aadlen, pt, ptlen, 1, o_hw);
	printf("  driver   %s -> %d\n", hw, n_hw);
	if (n_sw > 0)
		aead_dump("SW", o_sw, n_sw);
	if (n_hw > 0)
		aead_dump("HW", o_hw, n_hw);
	if (n_sw != n_hw || n_sw <= 0 || memcmp(o_sw, o_hw, n_sw)) {
		printf("  RESULT: FAIL (driver output differs from software)\n");
		return -1;
	}
	printf("  RESULT: PASS\n");
	return 0;
}

/* take the software ciphertext+tag, flip last bit of the tag, driver must reject (EBADMSG) */
static int aead_tamper(const char *hw, const char *sw, const unsigned char *iv, int ivlen,
			 const unsigned char *aad, int aadlen, const unsigned char *pt, int ptlen)
{
	unsigned char key[16], ct[AEAD_MAXBUF], out[AEAD_MAXBUF];
	int n, r;

	memcpy(key, g_key, sizeof(key));
	n = aead_run(sw, key, 16, iv, ivlen, aad, aadlen, pt, ptlen, 1, ct);
	if (n <= 0) {
		printf("  software encrypt failed: %d\n", n);
		return -1;
	}
	ct[n - 1] ^= 1;
	fflush(stdout);
	r = aead_run(hw, key, 16, iv, ivlen, aad, aadlen, ct, n, 0, out);
	if (r >= 0) {
		printf("  driver decrypt returned %d: TAMPERED TAG ACCEPTED\n  RESULT: FAIL (expected EBADMSG)\n", r);
		return -1;
	}
	printf("  driver decrypt returned %d (%s)\n  RESULT: %s\n", r, strerror(-r),
	       r == -EBADMSG ? "PASS" : "FAIL (expected EBADMSG)");
	return r == -EBADMSG ? 0 : -1;
}

/* decrypt software ciphertext+tag with the driver, must give back plaintext */
static int aead_dec_ok(const char *hw, const char *sw, const unsigned char *iv, int ivlen,
			 const unsigned char *aad, int aadlen, const unsigned char *pt, int ptlen)
{
	unsigned char key[16], ct[AEAD_MAXBUF], out[AEAD_MAXBUF];
	int n, r;

	memcpy(key, g_key, sizeof(key));
	n = aead_run(sw, key, 16, iv, ivlen, aad, aadlen, pt, ptlen, 1, ct);
	if (n <= 0)
		return -1;
	fflush(stdout);
	r = aead_run(hw, key, 16, iv, ivlen, aad, aadlen, ct, n, 0, out);
	printf("  driver decrypt returned %d\n", r);
	if (r == ptlen && !memcmp(out, pt, ptlen)) {
		printf("  RESULT: PASS\n");
		return 0;
	}
	if (r > 0)
		aead_dump("HW PT", out, r);
	aead_dump("EXPECT", pt, ptlen);
	printf("  RESULT: FAIL (plaintext wrong or error)\n");
	return -1;
}

static int aead_iters = 20;

/*
 * Randomised stress, like the customer's python script: random key/iv/aad/plaintext,
 * for each round: encrypt compare, decrypt of the correct ct+tag, decrypt with a flipped tag.
 * Prints the round number before each request so a panic can be located.
 */
static int aead_rand(int is_gcm, int iters)
{
	static const int ptl[] = { 0, 1, 15, 16, 17, 31, 32, 64 };
	static const int adl[] = { 0, 1, 16, 20 };
	const char *hw = is_gcm ? "ma35-optee-aes-gcm" : "ma35-optee-aes-ccm";
	const char *sw = is_gcm ? "gcm(aes)" : "ccm(aes)";
	int ivlen = is_gcm ? 12 : 16;
	int it, i, bad_enc = 0, bad_dec = 0, bad_tamper = 0, err = 0;
	unsigned seed = (unsigned)time(NULL);

	printf("  seed=%u iters=%d\n", seed, iters);
	srand(seed);

	for (it = 0; it < iters; it++) {
		unsigned char key[16], iv[16] = {0}, aad[32], pt[64];
		unsigned char c_sw[AEAD_MAXBUF], c_hw[AEAD_MAXBUF], p_hw[AEAD_MAXBUF];
		int pl = ptl[rand() % 8], al = adl[rand() % 4];
		int n_sw, n_hw, r;

		for (i = 0; i < 16; i++)
			key[i] = rand();
		for (i = 0; i < 16; i++)
			iv[i] = rand();
		if (!is_gcm) {
			iv[0] = 3;		/* 12-byte nonce */
			memset(iv + 13, 0, 3);
		}
		for (i = 0; i < 32; i++)
			aad[i] = rand();
		for (i = 0; i < 64; i++)
			pt[i] = rand();

		printf("  [%02d] ptlen=%d aadlen=%d ... ", it, pl, al);
		fflush(stdout);

		n_sw = aead_run(sw, key, 16, iv, ivlen, aad, al, pt, pl, 1, c_sw);
		if (n_sw != pl + AEAD_TAGLEN) {
			printf("software enc error %d\n", n_sw);
			err++;
			continue;
		}
		n_hw = aead_run(hw, key, 16, iv, ivlen, aad, al, pt, pl, 1, c_hw);
		if (n_hw != n_sw || memcmp(c_sw, c_hw, n_sw)) {
			bad_enc++;
			printf("ENC MISMATCH(%d) ", n_hw);
		} else {
			printf("enc ok ");
		}
		fflush(stdout);

		r = aead_run(hw, key, 16, iv, ivlen, aad, al, c_sw, n_sw, 0, p_hw);
		if (r != pl || memcmp(p_hw, pt, pl)) {
			bad_dec++;
			printf("DEC BAD(%d) ", r);
		} else {
			printf("dec ok ");
		}
		fflush(stdout);

		if (!pl && !al) {
			/* known TSI limitation: no data and no AAD, tag is not verified */
			printf("tamper check skipped (empty pt and aad)\n");
			continue;
		}
		c_sw[n_sw - 1] ^= 1;
		r = aead_run(hw, key, 16, iv, ivlen, aad, al, c_sw, n_sw, 0, p_hw);
		if (r != -EBADMSG) {
			bad_tamper++;
			printf("TAMPER ACCEPTED(%d)\n", r);
		} else {
			printf("tamper rejected\n");
		}
	}
	printf("  SUMMARY %s: enc_mismatch=%d dec_bad=%d tamper_accepted=%d other_err=%d / %d rounds\n",
	       hw, bad_enc, bad_dec, bad_tamper, err, iters);
	printf("  RESULT: %s\n", (bad_enc || bad_dec || bad_tamper || err) ? "FAIL" : "PASS");
	return (bad_enc || bad_dec || bad_tamper || err) ? -1 : 0;
}

static const char *aead_cases[] = {
	"gcm_empty_aad16", "gcm_tamper_empty_aad16", "gcm_pt16", "gcm_dec16",
	"gcm_tamper16", "ccm_pt16", "ccm_tamper16", "ccm_pt64_aad16",
	"gcm_rand", "ccm_rand", NULL
};

static int aead_test(const char *c)
{
	const char *gcm_hw = "ma35-optee-aes-gcm", *gcm_sw = "gcm(aes)";
	const char *ccm_hw = "ma35-optee-aes-ccm", *ccm_sw = "ccm(aes)";
	unsigned char giv[12], civ[16], aad[16], pt[64];
	int i;

	for (i = 0; i < 12; i++)
		giv[i] = 0xa0 + i;
	memset(civ, 0, sizeof(civ));
	civ[0] = 3;			/* L - 1 = 3 -> 12-byte nonce */
	for (i = 1; i < 12; i++)
		civ[i] = 0xb0 + i;
	for (i = 0; i < 16; i++)
		aad[i] = 0xc0 + i;
	for (i = 0; i < 64; i++)
		pt[i] = i;

	printf("\n[AEAD] case: %s\n", c);
	fflush(stdout);

	if (!strcmp(c, "gcm_empty_aad16"))
		return aead_enc_cmp(gcm_hw, gcm_sw, giv, 12, aad, 16, pt, 0);
	if (!strcmp(c, "gcm_tamper_empty_aad16"))
		return aead_tamper(gcm_hw, gcm_sw, giv, 12, aad, 16, pt, 0);
	if (!strcmp(c, "gcm_pt16"))
		return aead_enc_cmp(gcm_hw, gcm_sw, giv, 12, aad, 0, pt, 16);
	if (!strcmp(c, "gcm_dec16"))
		return aead_dec_ok(gcm_hw, gcm_sw, giv, 12, aad, 0, pt, 16);
	if (!strcmp(c, "gcm_tamper16"))
		return aead_tamper(gcm_hw, gcm_sw, giv, 12, aad, 0, pt, 16);
	if (!strcmp(c, "ccm_pt16"))
		return aead_enc_cmp(ccm_hw, ccm_sw, civ, 16, aad, 0, pt, 16);
	if (!strcmp(c, "ccm_tamper16"))
		return aead_tamper(ccm_hw, ccm_sw, civ, 16, aad, 0, pt, 16);
	if (!strcmp(c, "ccm_pt64_aad16"))
		return aead_enc_cmp(ccm_hw, ccm_sw, civ, 16, aad, 16, pt, 64);
	if (!strcmp(c, "gcm_rand"))
		return aead_rand(1, aead_iters);
	if (!strcmp(c, "ccm_rand"))
		return aead_rand(0, aead_iters);

	printf("unknown case. available:\n");
	for (i = 0; aead_cases[i]; i++)
		printf("  %s\n", aead_cases[i]);
	return -1;
}

/*------------------------------------------------------------------------*/
/*  SHA3 with 0-byte input, and TSI state afterwards             */
/*------------------------------------------------------------------------*/

/* returns digest length, or -errno */
static int hash_run(const char *drv, const void *data, int len, unsigned char *out, int outmax)
{
	struct sockaddr_alg sa = { .salg_family = AF_ALG, .salg_type = "hash" };
	int tfmfd, opfd, n;

	strncpy((char *)sa.salg_name, drv, sizeof(sa.salg_name) - 1);
	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0)
		return -errno;
	if (alg_bind(tfmfd, &sa) < 0) {
		n = -errno;
		close(tfmfd);
		return n;
	}
	opfd = accept(tfmfd, NULL, NULL);
	if (opfd < 0) {
		n = -errno;
		close(tfmfd);
		return n;
	}
	if (send(opfd, data, len, 0) < 0) {
		n = -errno;
		goto out;
	}
	n = read(opfd, out, outmax);
	if (n < 0)
		n = -errno;
out:
	close(opfd);
	close(tfmfd);
	return n;
}

static void hex_to_bin(const char *h, unsigned char *b)
{
	int i;

	for (i = 0; h[2 * i]; i++) {
		unsigned v;
		sscanf(h + 2 * i, "%2x", &v);
		b[i] = v;
	}
}

static const struct { const char *drv; const char *empty; } sha3_empty_sha3[] = {
	{ "ma35-optee-sha3-224", "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7" },
	{ "ma35-sha3-256", "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a" },
	{ "ma35-optee-sha3-384", "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004" },
	{ "ma35-sha3-512", "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26" },
};

/* "sha3": 0-byte SHA3 for all four sizes; "after": probe whether TSI still works */
static int sha3_empty_test(const char *c)
{
	unsigned char out[64], exp[64];
	int i, n, fail = 0;

	printf("\n[SHA3 empty] %s\n", c);

	if (!strcmp(c, "sha3")) {
		for (i = 0; i < 4; i++) {
			printf("  %s, 0-byte input ... ", sha3_empty_sha3[i].drv);
			fflush(stdout);
			n = hash_run(sha3_empty_sha3[i].drv, "", 0, out, sizeof(out));
			if (n < 0) {
				printf("error %d (%s)\n", n, strerror(-n));
				fail++;
				continue;
			}
			hex_to_bin(sha3_empty_sha3[i].empty, exp);
			if (n == (int)(strlen(sha3_empty_sha3[i].empty) / 2) && !memcmp(out, exp, n)) {
				printf("ok\n");
			} else {
				printf("WRONG digest\n");
				fail++;
			}
		}
	} else if (!strcmp(c, "after")) {
		n = hash_run("ma35-optee-sha256", "abc", 3, out, sizeof(out));
		hex_to_bin("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", exp);
		printf("  sha256(\"abc\") : %s\n", n == 32 && !memcmp(out, exp, 32) ? "ok" :
		       n < 0 ? strerror(-n) : "WRONG");
		if (n != 32 || memcmp(out, exp, 32))
			fail++;
		n = hash_run("ma35-sha3-256", "abc", 3, out, sizeof(out));
		hex_to_bin("3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532", exp);
		printf("  sha3-256(\"abc\"): %s\n", n == 32 && !memcmp(out, exp, 32) ? "ok" :
		       n < 0 ? strerror(-n) : "WRONG");
		if (n != 32 || memcmp(out, exp, 32))
			fail++;
		aes_ecb_noiv_test("ma35-optee-ecb-aes");
	} else {
		printf("  unknown case, use: sha3 | after\n");
		return -1;
	}
	printf("  RESULT: %s\n", fail ? "FAIL" : "PASS");
	return fail ? -1 : 0;
}

/*------------------------------------------------------------------------*/
/*  HMAC with a 0-byte message                                   */
/*------------------------------------------------------------------------*/

static int hmac_run(const char *drv, const void *key, int klen, const void *data, int len,
		    unsigned char *out, int outmax)
{
	struct sockaddr_alg sa = { .salg_family = AF_ALG, .salg_type = "hash" };
	int tfmfd, opfd, n;

	strncpy((char *)sa.salg_name, drv, sizeof(sa.salg_name) - 1);
	tfmfd = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfmfd < 0)
		return -errno;
	if (alg_bind(tfmfd, &sa) < 0 ||
	    setsockopt(tfmfd, SOL_ALG, ALG_SET_KEY, key, klen) < 0) {
		n = -errno;
		close(tfmfd);
		return n;
	}
	opfd = accept(tfmfd, NULL, NULL);
	if (opfd < 0) {
		n = -errno;
		close(tfmfd);
		return n;
	}
	if (send(opfd, data, len, 0) < 0) {
		n = -errno;
		goto out;
	}
	n = read(opfd, out, outmax);
	if (n < 0)
		n = -errno;
out:
	close(opfd);
	close(tfmfd);
	return n;
}

static int hmac_empty_test(void)
{
	static const struct { const char *name, *empty, *abc; } t[] = {
		{ "ma35-optee-hmac-sha224",
		  "661e3ad0761c2f252facc2476dc60749e741b2b8e84caa9c9bb6ba62",
		  "afb9c20837b38c13fca5a3b1263b42e898236a8d57f28d30d8fec457" },
		{ "ma35-optee-hmac-sha256",
		  "496dc93fa2d26eae500ec0bc37a122706b88f8963cebf0899d0245fae313e241",
		  "bcaa92843247bc258080d2824bddeba4afb1bd61b4688ca65f60a214e1d9759e" },
		{ "ma35-optee-hmac-sha384",
		  "2a0950b9b5734f94719e27528349c9763e70e28f99602cf4dd50b37c459ca3937ebca9dcb926a59dca9c30434698822d",
		  "aed945b97ceab9c4e18f00b2cca507996271c8c9291bab0e648b194a342b40278dac97abc19ffdc274c5bc9f6d9ad2fe" },
		{ "ma35-optee-hmac-sha512",
		  "adb0be411f72fc80db15ed651e8c2dc802992472d5ed7d0fea238e41af20234fcb839fa863d4f1806f5c6bad36866bbc9593c2019ebb668c5563f56467a16a69",
		  "b4eb029ed0c8f99d60dc5ff3ed6fc1a1c1a8a82eec3dd6363959c56b51e52d732748eb814e49be16b49fe7b5cd84af78e7a5ded326cbc5cf0db18ced31f67d87" },
	};
	const char *key = "0123456789abcdef";
	unsigned char out[64], exp[64];
	int i, j, n, fail = 0;

	printf("\n[HMAC empty] HMAC, key=\"%s\"\n", key);
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 2; j++) {
			const char *msg = j ? "abc" : "";
			const char *dg = j ? t[i].abc : t[i].empty;

			printf("  %-24s %-9s ... ", t[i].name, j ? "\"abc\"" : "0-byte");
			fflush(stdout);
			n = hmac_run(t[i].name, key, 16, msg, j ? 3 : 0, out, sizeof(out));
			if (n < 0) {
				printf("error %d (%s)\n", n, strerror(-n));
				fail++;
				continue;
			}
			hex_to_bin(dg, exp);
			if (n == (int)(strlen(dg) / 2) && !memcmp(out, exp, n)) {
				printf("ok\n");
			} else {
				printf("WRONG digest\n");
				fail++;
			}
		}
	}
	printf("  checking TSI afterwards:\n");
	aes_ecb_noiv_test("ma35-optee-ecb-aes");
	printf("  RESULT: %s\n", fail ? "FAIL" : "PASS");
	return fail ? -1 : 0;
}

/*------------------------------------------------------------------------*/
/*  sha1 / md5 / sm3 AF_ALG bind (ahash fallback)                */
/*------------------------------------------------------------------------*/

static int hash_legacy_test(void)
{
	static const struct { const char *name, *digest; } t[] = {
		{ "ma35-optee-sha1", "a9993e364706816aba3e25717850c26c9cd0d89d" },
		{ "sha1", "a9993e364706816aba3e25717850c26c9cd0d89d" },
		{ "ma35-optee-md5", "900150983cd24fb0d6963f7d28e17f72" },
		{ "md5", "900150983cd24fb0d6963f7d28e17f72" },
		{ "ma35-optee-sm3", "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0" },
		{ "sm3", "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0" },
	};
	unsigned char out[64], exp[64];
	int i, n, fail = 0;

	printf("\n[Legacy hash] hash(\"abc\") via AF_ALG\n");
	for (i = 0; i < 6; i++) {
		printf("  %-18s ... ", t[i].name);
		n = hash_run(t[i].name, "abc", 3, out, sizeof(out));
		if (n < 0) {
			printf("error %d (%s)\n", n, strerror(-n));
			fail++;
			continue;
		}
		hex_to_bin(t[i].digest, exp);
		if (n == (int)(strlen(t[i].digest) / 2) && !memcmp(out, exp, n)) {
			printf("ok\n");
		} else {
			printf("WRONG digest\n");
			fail++;
		}
	}
	printf("  RESULT: %s\n", fail ? "FAIL" : "PASS");
	return fail ? -1 : 0;
}

/*------------------------------------------------------------------------*/
/*  concurrent ECC_IOC_SIG_VERIFY from two processes             */
/*------------------------------------------------------------------------*/
#include <sys/ioctl.h>
#include <sys/wait.h>

#define ECC_KMAXL	160
#define ECC_IOC_SIG_VERIFY	_IOW('C', 58, unsigned char *)

struct ecc_args {
	int curve, keylen, knum_d, knum_x, knum_y;
	unsigned char d[ECC_KMAXL], Qx[ECC_KMAXL], Qy[ECC_KMAXL], sha_dgst[ECC_KMAXL],
		      k[ECC_KMAXL], R[ECC_KMAXL], S[ECC_KMAXL], out_x[ECC_KMAXL],
		      out_y[ECC_KMAXL];
};

/* P-256 (curve id 3), ECDSA over SHA-256("nuvoton ma35d0 ecc verify test") */
#define ECC_QX "8da66d001ff14d84500d3c5b70d239aabe5eabaa6c78f168dfc405df1e7ddf20"
#define ECC_QY "b6d89e6c94d73b16715e163ba7ea3bed2a87cbfdf2f0af458c184aee165426ed"
#define ECC_H  "fa9608cdabff14bf7d299fee88efe7e6bff9e8ea9363f8995da158e7e9c7f80f"
#define ECC_R  "c0f07c1254ae780ddfe58ea8e877d93a4ad25fbe31dee2a50bd12e34eddda8e0"
#define ECC_S  "4a1f2adb5bf5a095bd774091b5d122ca7aab945a0c4ec1029e6bf0606070e0d2"

/* returns 0 if the signature verified, otherwise -1 */
static int ecc_verify(int fd, int tamper)
{
	struct ecc_args a;

	memset(&a, 0, sizeof(a));
	a.curve = 3;
	a.keylen = 32;
	a.knum_d = a.knum_x = a.knum_y = -1;
	strcpy((char *)a.Qx, ECC_QX);
	strcpy((char *)a.Qy, ECC_QY);
	strcpy((char *)a.sha_dgst, ECC_H);
	strcpy((char *)a.R, ECC_R);
	strcpy((char *)a.S, ECC_S);
	if (tamper)
		a.Qx[0] = a.Qx[0] == '8' ? '9' : '8';
	return ioctl(fd, ECC_IOC_SIG_VERIFY, &a) == 0 ? 0 : -1;
}

static int ecc_verify_check_single(int fd)
{
	int good = ecc_verify(fd, 0), bad = ecc_verify(fd, 1), fail = 0;

	printf("  single process: valid signature %s, tampered public key %s\n",
	       good == 0 ? "ACCEPTED (ok)" : "REJECTED (WRONG)",
	       bad == 0 ? "ACCEPTED (WRONG)" : "rejected (ok)");
	if (good != 0 || bad == 0)
		fail = 1;
	return fail;
}

static int ecc_verify_test(const char *c, int iters)
{
	int fd, fail = 0, i, st, errs[2] = {0, 0}, pfd[2][2];
	pid_t pid[2];
	struct timeval t0, t1;

	fd = open("/dev/nuvoton-ecc", O_RDWR);
	if (fd < 0) {
		perror("open /dev/nuvoton-ecc");
		return -1;
	}
	printf("\n[ECC verify] %s\n", c);
	fail += ecc_verify_check_single(fd);

	if (!strcmp(c, "conc")) {
		printf("  two processes x %d verify of a valid signature ...\n", iters);
		gettimeofday(&t0, NULL);
		for (i = 0; i < 2; i++) {
			if (pipe(pfd[i]))
				return -1;
			pid[i] = fork();
			if (pid[i] == 0) {
				int j, e = 0, cfd = open("/dev/nuvoton-ecc", O_RDWR);

				for (j = 0; j < iters; j++)
					if (cfd < 0 || ecc_verify(cfd, 0))
						e++;
				write(pfd[i][1], &e, sizeof(e));
				_exit(0);
			}
		}
		for (i = 0; i < 2; i++) {
			waitpid(pid[i], &st, 0);
			read(pfd[i][0], &errs[i], sizeof(int));
		}
		gettimeofday(&t1, NULL);
		printf("  failures: proc0=%d proc1=%d, %.1f verify/s\n", errs[0], errs[1],
		       2.0 * iters / ((t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6));
		fail += errs[0] + errs[1];
		fail += ecc_verify_check_single(fd);
		printf("  checking AES/ECB after the concurrent test:\n");
		fail += aes_ecb_noiv_test("ma35-optee-ecb-aes") ? 1 : 0;
	} else if (strcmp(c, "single")) {
		printf("  unknown case, use: single | conc [iters]\n");
		return -1;
	}
	close(fd);
	printf("  RESULT: %s\n", fail ? "FAIL" : "PASS");
	return fail ? -1 : 0;
}

#define CRYPTO_DEMO_VERSION "2.0-regular"

static void usage(const char *prog)
{
	printf("usage: %s <test> [args] [std]\n"
	       "  demo               AES-256 CBC and SHA/HMAC/SHA3 known-answer demos\n"
	       "  aes-ks             AES with an OTP Key Store key (TSI driver only)\n"
	       "  perf               AES and SHA throughput\n"
	       "  ecb-noiv [alg]     AES ECB request without IV (default ma35-optee-ecb-aes)\n"
	       "  aead <case>        AES GCM/CCM; run without <case> to list them\n"
	       "  sha3-empty sha3|after  empty SHA3 input, then check that TSI still works\n"
	       "  ecc-verify [single|conc [N]]  ECC verify, single process or two processes\n"
	       "  hmac-empty         HMAC with an empty message\n"
	       "  hash-legacy        sha1, md5 and sm3\n"
	       "  all                every test above (aead: every case, ecc-verify: single)\n"
	       "\n"
	       "Algorithms are opened by the TSI driver name first (ma35-optee-*), then by the\n"
	       "generic name such as cbc(aes). Add \"std\" as the last argument to use only the\n"
	       "generic name, which is the ARMv8 CE implementation on linux-6.18.\n", prog);
}

int main(int argc, char **argv)
{
	const char *cmd;
	int i, fail = 0;

	printf("crypto-demo v%s (built %s %s)\n", CRYPTO_DEMO_VERSION, __DATE__, __TIME__);

	if (argc > 1 && !strcmp(argv[argc - 1], "std")) {
		use_std_alg = 1;
		argc--;
	}
	cmd = argc > 1 ? argv[1] : "";

	if (!strcmp(cmd, "demo"))
		return (AES_demo() | SHA_demo_all()) ? 1 : 0;
	if (!strcmp(cmd, "aes-ks"))
		return AES_KS_demo();
	if (!strcmp(cmd, "perf")) {
		AES_performance();
		SHA_performance_all();
		return 0;
	}
	if (!strcmp(cmd, "ecb-noiv"))
		return aes_ecb_noiv_test(argc > 2 ? argv[2] : "ma35-optee-ecb-aes") ? 1 : 0;
	if (!strcmp(cmd, "aead")) {
		if (argc > 3)
			aead_iters = atoi(argv[3]);
		return aead_test(argc > 2 ? argv[2] : "");
	}
	if (!strcmp(cmd, "sha3-empty"))
		return sha3_empty_test(argc > 2 ? argv[2] : "sha3");
	if (!strcmp(cmd, "ecc-verify"))
		return ecc_verify_test(argc > 2 ? argv[2] : "single", argc > 3 ? atoi(argv[3]) : 200);
	if (!strcmp(cmd, "hmac-empty"))
		return hmac_empty_test();
	if (!strcmp(cmd, "hash-legacy"))
		return hash_legacy_test();

	if (!strcmp(cmd, "all")) {
		/* a kernel panic in one case stops the run, so the order matters */
		fail += (AES_demo() | SHA_demo_all()) ? 1 : 0;
		fail += aes_ecb_noiv_test("ma35-optee-ecb-aes") ? 1 : 0;
		for (i = 0; aead_cases[i]; i++)
			fail += aead_test(aead_cases[i]) ? 1 : 0;
		fail += sha3_empty_test("sha3") ? 1 : 0;
		fail += sha3_empty_test("after") ? 1 : 0;
		fail += ecc_verify_test("single", 0) ? 1 : 0;
		fail += hmac_empty_test() ? 1 : 0;
		fail += hash_legacy_test() ? 1 : 0;
		printf("\nall: %s (%d test item(s) failed)\n", fail ? "FAIL" : "PASS", fail);
		return fail ? 1 : 0;
	}

	usage(argv[0]);
	return cmd[0] ? 1 : 0;
}

/*
 *  AES test pattern of this example.
 *  [AES-256 CBC Mode]
 *      KEY: 06a9214036b8a15b512e03d53412000606a9214036b8a15b512e03d534120006
 *      IV:  3dafba429d9eb430b422da802c9fac41
 *      IN:  000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f
 *      OUT: ef8b1fccd72d0a1fd6ac191a7ae62292fd3a996a45761d2f79dd8ff2c7e5101375eadb453ee09c16f791b6a18a6090b829f569c6973b16cc9f84c9fb86bf398f
 *
 *  SHA test pattern: "abc"
 *  [SHA-1]
 *      a9993e364706816aba3e25717850c26c9cd0d89d
 *  [SHA-224]
 *      23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7
 *  [SHA-256]
 *      ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 *  [SHA-384]
 *      cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7
 *  [SHA-512]
 *      ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f
 *  [SHA3-224]
 *      e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf
 *  [SHA3-256]
 *      3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
 *  [SHA3-384]
 *      ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25
 *  [SHA3-512]
 *      b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0
 *  
 *  HMAC-SHA test pattern: "abc"
 *      HMAC KEY: 891132ef108a2290
 *  [HMAC-SHA-1]
 *      cfbdfa7b01f0f26996cd2f6bf77f092117fafe1b
 *  [HMAC-SHA-224]
 *      1b0e67c793799a9e34c6b2face31ddc781939db23f2bd537ca041cb8
 *  [HMAC-SHA-256]
 *      a887fe6178cd89f66d38788aa711db4aabebf6036d38c304323802421c927952
 *  [HMAC-SHA-384]
 *      05b2b02ded41d928fae54e3549984b36ab13271f7485d1ead36351de0fd244c42d14dcd5018e5a890c93e70bc8aa1661
 *  [HMAC-SHA-512]
 *      534cd738b46b422d6cc6efc2766fb6f65374cd7858d9fb29f96fb6273717bc3dd5a496ff2ef30e23c28c2835cff737297b7032e4900b8f6ac60f51b8b3a1eb4a
 */
