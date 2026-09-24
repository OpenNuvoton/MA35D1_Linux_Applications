// SPDX-License-Identifier: GPL-2.0
/*
 * MA35D1 BL2, FIP, and Linux OTP version-counter demo.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ma35d1_ks.h"

#define OTP_COUNTER_WORDS       11U
#define OTP_BITS_PER_WORD       32U
#define OTP_COUNTER_MAX         (OTP_COUNTER_WORDS * OTP_BITS_PER_WORD)

struct otp_counter_region {
	const char *name;
	uint32_t low;
	uint32_t high;
};

static const struct otp_counter_region bl2_counter = {
	.name = "BL2",
	.low = 0x14C,
	.high = 0x174,
};

static const struct otp_counter_region fip_counter = {
	.name = "FIP",
	.low = 0x120,
	.high = 0x148,
};

static const struct otp_counter_region linux_counter = {
	.name = "Linux",
	.low = 0x1A4,
	.high = 0x1CC,
};

static int read_counter(int fd, const struct otp_counter_region *region,
			unsigned int *version)
{
	struct ks_read_args args = { 0 };
	unsigned int count = 0;
	unsigned int n;
	int seen_zero = 0;

	args.key_idx = region->low;
	args.word_cnt = OTP_COUNTER_WORDS;
	if (ioctl(fd, KS_IOCTL_READ_OTP, &args) < 0) {
		fprintf(stderr, "%s counter read failed: %s\n",
			region->name, strerror(errno));
		return -1;
	}

	/* Programming order is high address to low, LSB to MSB. */
	for (n = 0; n < OTP_COUNTER_MAX; n++) {
		unsigned int word = OTP_COUNTER_WORDS - 1 -
				    n / OTP_BITS_PER_WORD;
		unsigned int bit = n % OTP_BITS_PER_WORD;
		int programmed = !!(args.key[word] & (1U << bit));

		if (!programmed) {
			seen_zero = 1;
		} else {
			if (seen_zero) {
				fprintf(stderr,
					"%s counter has a non-contiguous OTP pattern\n",
					region->name);
				return -1;
			}
			count++;
		}
	}

	*version = count;
	return 0;
}

static int show_counters(int fd)
{
	unsigned int bl2_version;
	unsigned int fip_version;
	unsigned int linux_version;

	if (read_counter(fd, &bl2_counter, &bl2_version) ||
	    read_counter(fd, &fip_counter, &fip_version) ||
	    read_counter(fd, &linux_counter, &linux_version))
		return -1;

	printf("BL2 OTP version counter:   %u\n", bl2_version);
	printf("FIP OTP version counter:   %u\n", fip_version);
	printf("Linux OTP version counter: %u\n", linux_version);
	return 0;
}

static int update_counter(int fd, const struct otp_counter_region *region,
			  unsigned int target)
{
	struct ks_read_args args = { 0 };
	unsigned int current;

	if (target > OTP_COUNTER_MAX) {
		fprintf(stderr, "Version must be between 0 and %u\n",
			OTP_COUNTER_MAX);
		return -1;
	}

	if (read_counter(fd, region, &current))
		return -1;
	if (target < current) {
		fprintf(stderr, "%s rollback rejected: current=%u requested=%u\n",
			region->name, current, target);
		return -1;
	}

	while (current < target) {
		unsigned int word = current / OTP_BITS_PER_WORD;
		unsigned int bit = current % OTP_BITS_PER_WORD;

		memset(&args, 0, sizeof(args));
		args.key_idx = region->high - word * sizeof(uint32_t);
		args.word_cnt = 1;
		args.key[0] = 1U << bit;
		if (ioctl(fd, KS_IOCTL_PROGRAM_OTP, &args) < 0) {
			fprintf(stderr,
				"%s counter program failed at 0x%x bit %u: %s\n",
				region->name, args.key_idx, bit, strerror(errno));
			return -1;
		}
		current++;
	}

	if (read_counter(fd, region, &current) || current != target) {
		fprintf(stderr, "%s counter verification failed\n", region->name);
		return -1;
	}

	printf("%s OTP version counter is now %u\n", region->name, current);
	return 0;
}

static const struct otp_counter_region *find_region(const char *name)
{
	if (!strcmp(name, "fip"))
		return &fip_counter;
	if (!strcmp(name, "linux"))
		return &linux_counter;
	return NULL;
}

static void usage(const char *program)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s read\n", program);
	fprintf(stderr, "  %s update <fip|linux> <version>\n", program);
}

int main(int argc, char **argv)
{
	const struct otp_counter_region *region;
	unsigned long target;
	char *end;
	int fd;
	int ret;

	fd = open("/dev/ksdev", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open /dev/ksdev: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "read"))) {
		ret = show_counters(fd);
	} else if (argc == 4 && !strcmp(argv[1], "update")) {
		region = find_region(argv[2]);
		errno = 0;
		target = strtoul(argv[3], &end, 0);
		if (!region || errno || *end || target > UINT32_MAX) {
			usage(argv[0]);
			ret = -1;
		} else {
			printf("WARNING: OTP programming is permanent.\n");
			ret = update_counter(fd, region, (unsigned int)target);
		}
	} else {
		usage(argv[0]);
		ret = -1;
	}

	close(fd);
	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
