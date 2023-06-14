/*
 * This sample shows gamma operation in Video layer
 *
 * Copyright (c) 2023 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "math.h"
#include "util.h"
#include "ma35d1-dcultrafb.h"


int FramebufferFd;
int OverlayFd;

struct fb_var_screeninfo FBVar;
struct fb_fix_screeninfo FBFix;
struct fb_var_screeninfo OverlayVar;
struct fb_fix_screeninfo OverlayFix;

char *UserMemFb = NULL;
char *UserMemOverlay = NULL;
unsigned int ScreenSizeFb;
unsigned int OneframeSizeFb;
unsigned int ScreenSizeOverlay;
unsigned int OneframeSizeOverlay;

dc_frame_info UserFrameBufferSize = {
	.width = 1024,
	.height = 600,
	.stride = 4096,
};

dc_filter_tap UserFrameBufferScaleFilterTap = {
	.vertical_filter_tap = 3,
	.horizontal_filter_tap = 3,
};

int OpenDevice(void)
{
	int i= 0;
	int fd, LastFd;
	char FbDevName[10];

	sprintf(FbDevName, "/dev/fb%d", i);
	fd = open(FbDevName, O_RDWR);
	if (fd < 0)
	{
		return -1;
	}

	while (fd > 0)
	{
		LastFd = fd;
		i++;
		sprintf(FbDevName, "/dev/fb%d", i);
		fd = open(FbDevName, O_RDWR);
	}

	OverlayFd = LastFd;
	FramebufferFd = LastFd - 1;
	return 0;
}

int InitDevice(void)
{
	int ret;

	ret = OpenDevice();
	if (ret < 0)
	{
		printf("can't open device\n");
		return -1;
	}

	ret = GetVarScreenInfo(FramebufferFd, &FBVar);
	if (ret < 0)
	{
		return -1;
	}

	ret = GetVarScreenInfo(OverlayFd, &OverlayVar);
	if (ret < 0)
	{
		return -1;
	}

	ret = GetFixScreenInfo(FramebufferFd, &FBFix);
	if (ret < 0)
	{
		return -1;
	}

	ret = GetFixScreenInfo(OverlayFd, &OverlayFix);
	if (ret < 0)
	{
		return -1;
	}

	/* framebuffer info */
	OneframeSizeFb = FBVar.xres * FBVar.yres * FBVar.bits_per_pixel / 8;
	ScreenSizeFb = FBVar.xres * FBVar.yres * (FBVar.yres_virtual / FBVar.yres) * FBVar.bits_per_pixel / 8;
	UserMemFb = (char *)mmap(NULL, ScreenSizeFb, PROT_READ | PROT_WRITE, MAP_SHARED, FramebufferFd, 0);
	if (UserMemFb == NULL)
	{
		printf("can't mmap\n");
		return -1;
	}

	/* overlay info */
	OneframeSizeOverlay = OverlayVar.xres * OverlayVar.yres * OverlayVar.bits_per_pixel / 8;
	ScreenSizeOverlay = OverlayVar.xres * OverlayVar.yres * (OverlayVar.yres_virtual / OverlayVar.yres) * OverlayVar.bits_per_pixel / 8;
	UserMemOverlay = (char *)mmap(NULL, ScreenSizeFb, PROT_READ | PROT_WRITE, MAP_SHARED, OverlayFd, 0);
	if (UserMemOverlay == NULL)
	{
		printf("can't mmap\n");
		return -1;
	}
	return 0;
}

void FBClose()
{
	munmap(UserMemOverlay, ScreenSizeOverlay);
	munmap(UserMemFb, ScreenSizeFb);
	close(OverlayFd);
	close(FramebufferFd);
}

int main()
{
	int ret = 0, i;

	ret = InitDevice();
	if (ret < 0)
	{
		printf("can't init device\n");
		return -1;
	}

	ShowPage("dual_src_w1024_h600.bgra", &FBVar, UserMemFb, OneframeSizeFb);

	for (i = 0; i < 3; i++)
	{
		SetEnableGamma(FramebufferFd, 0);
		SetVarScreenInfo(FramebufferFd, &FBVar);
		sleep(3);
		SetBufferSize(FramebufferFd, UserFrameBufferSize);

		SetEnableGamma(FramebufferFd, 1);
		SetVarScreenInfo(FramebufferFd, &FBVar);
		sleep(3);
	}
	FBClose();

	printf("Gamma test done.\n");
	return 0;
}
