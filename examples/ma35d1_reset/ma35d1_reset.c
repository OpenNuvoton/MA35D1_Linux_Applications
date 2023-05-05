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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define FORCE_CHIP_RESET	0x1401

int main(int argc, char **argv)
{
	int fd_misctrl;
	char temp[16];
	int rev;
	unsigned int i = 0;

	fd_misctrl = open("/dev/ma35_misctrl", O_RDWR);
	if (fd_misctrl < 0) {
		printf("Failed to open /dev/ma35_misctrl ! errno = %d\n", fd_misctrl);
		return -ENODEV;
	}

	if (ioctl(fd_misctrl, FORCE_CHIP_RESET, 0))
		printf("Failed to issue reset command!\n");

	printf("MA35D1 reset failed!!");
	return 0;
}
