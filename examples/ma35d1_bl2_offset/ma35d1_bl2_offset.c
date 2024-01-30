/*
 * Copyright (c) 2024 Nuvoton technology corporation
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

#define GET_BL2_OFFSET		0x1501

int main(int argc, char **argv)
{
	int fd_misctrl;
	unsigned int offset;

	fd_misctrl = open("/dev/ma35_misctrl", O_RDWR);
	if (fd_misctrl < 0) {
		printf("Failed to open /dev/ma35_misctrl ! errno = %d\n", fd_misctrl);
		return -ENODEV;
	}

	offset = ioctl(fd_misctrl, GET_BL2_OFFSET, 0);
	printf("The BL2 offset is 0x%x\n", offset);

	return 0;
}
