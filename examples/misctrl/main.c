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

#define SET_CPU_FREQ_500M	0x1005
#define SET_CPU_FREQ_600M	0x1006
#define SET_CPU_FREQ_650M	0x1007
#define SET_CPU_FREQ_800M	0x1008
#define GET_PMIC_VOLT		0x1101
#define SET_PMIC_VOLT		0x1102
#define SET_EPLL_DIV_BY_2	0x1202
#define SET_EPLL_DIV_BY_4	0x1204
#define SET_EPLL_DIV_BY_8	0x1208
#define SET_EPLL_RESTORE	0x120F
#define SET_SYS_SPD_LOW		0x1301
#define SET_SYS_SPD_RESTORE	0x1302
#define SDRAM_AUTOREF_ENABLE	0x1381
#define SDRAM_AUTOREF_DISABLE	0x1382
#define FORCE_CHIP_RESET	0x1401
#define GET_BL2_OFFSET		0x1501
#define GET_REGISTER		0x1511

int main(int argc, char **argv)
{
	unsigned int i, reg_addr, val;
	int fd_misctrl, fd_temp;
	char temp[16];
	int rev;
	int j_temp;

	printf("******************************************************\n");
	printf("*  Nuvoton MA35 Series SoC misctrl demo.             *\n");
	printf("*                                                    *\n");
	printf("*  ioctl commands not demostrated here can be        *\n");
	printf("*  found in another example, 'temp_ctrl'.            *\n");
	printf("******************************************************\n");
	printf("\n\n\n");

	fd_misctrl = open("/dev/ma35_misctrl", O_RDWR);
	if (fd_misctrl < 0) {
		printf("Failed to open /dev/ma35_misctrl ! errno = %d\n", fd_misctrl);
		return -ENODEV;
	}

	fd_temp = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY); // O_RDONLY // O_WRONLY
	if (fd_temp < 0) {
		printf("Failed to open /sys/class/thermal/thermal_zone0/temp ! errno = %d\n", fd_temp);
		return -ENODEV;
	}

	printf("+----------------------------------------+\n");
	printf("|  Get BL2 offset                        |\n");
	printf("+----------------------------------------+\n");
	val = ioctl(fd_misctrl, GET_BL2_OFFSET, 0);
	printf("The BL2 offset is 0x%x\n\n\n", val);

	printf("+----------------------------------------+\n");
	printf("|  Get Registers                         |\n");
	printf("+----------------------------------------+\n");
	for (reg_addr = 0x40410000; reg_addr <= 0x40410184; reg_addr += 4) {
		val = ioctl(fd_misctrl, GET_REGISTER, reg_addr);
		printf("Register 0x%08x = 0x%08x\n", reg_addr, val);
	}
	printf("\n\n");

	printf("+----------------------------------------+\n");
	printf("|  Disable SDRAM auto-refresh            |\n");
	printf("+----------------------------------------+\n");
	ioctl(fd_misctrl, SDRAM_AUTOREF_DISABLE, 0);
	for (i = 0; i < 30; i++) {
		sleep(10);

		memset(temp, 0, sizeof(temp));
		lseek(fd_temp, 0, 0);
		rev = read(fd_temp, temp, 6);
		j_temp = atoi(temp);
		printf("Junction temperature = %d\n", j_temp);
	}

	printf("+----------------------------------------+\n");
	printf("|  Enable SDRAM auto-refresh             |\n");
	printf("+----------------------------------------+\n");
	ioctl(fd_misctrl, SDRAM_AUTOREF_ENABLE, 0);
	for (i = 0; i < 30; i++) {
		sleep(10);

		memset(temp, 0, sizeof(temp));
		lseek(fd_temp, 0, 0);
		rev = read(fd_temp, temp, 6);
		j_temp = atoi(temp);
		printf("Junction temperature = %d\n", j_temp);
	}

	printf("+----------------------------------------+\n");
	printf("|  MA35 chip reset                       |\n");
	printf("+----------------------------------------+\n");
	printf("Press 'Enter' to start chip reset...\n");
	getchar();
	if (ioctl(fd_misctrl, FORCE_CHIP_RESET, 0))
		printf("Failed to issue reset command!\n");

	printf("MA35 SoC reset failed!!");
	return 0;
}
