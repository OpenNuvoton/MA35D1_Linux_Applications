// SPDX-License-Identifier: GPL-2.0
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/videodev2.h>

#define IMAGE_WIDTH		640
#define IMAGE_HEIGHT		480

#define DO_SNAPSHOT_DEMO	1

#define IOCTL_CCAP_FBD_SET		_IOW('v', 61, struct ccap_fb_direct)
#define IOCTL_CCAP_FBD_GET		_IOW('v', 62, struct ccap_fb_direct)
#define IOCTL_CCAP_SNAPSHOT_DO		_IOW('v', 65, int)
#define IOCTL_CCAP_SNAPSHOT_IDLE	_IOW('v', 66, int)

struct ccap_fb_direct {
	int enable;       /* 0: disable; 1: enable     */
	int wait_sync;    /* wait for LCD VSYNC        */
	int is_overlay;   /* 0: main frame; 1: overlay */
	int fb_width;     /* frame buffer width        */
	int fb_height;    /* frame buffer height       */
	int img_x;        /* image render left x       */
	int img_y;        /* image render top y        */
	int img_width;    /* image width               */
	int img_height;   /* image height              */
	int do_snapshot;  /* -1: idle; 0: doing; 1: to do */
};

struct overlay {
	int fd;
	__u8 *memp;
} _overlay;

int init_overlay(void)
{
	struct fb_fix_screeninfo finfo;
	__u8 *fb_mem, *image;
	int fd;

	fd = open("/dev/fb1", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Failed to open fb1\n");
		return -1;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Error reading fixed information");
		close(fd);
		exit(EXIT_FAILURE);
	}

	_overlay.memp = (__u8 *)mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (_overlay.memp == MAP_FAILED) {
		perror("fb1 mmap failed!!\n");
		close(fd);
		exit(EXIT_FAILURE);
	}

	memset(_overlay.memp, 0, finfo.smem_len);

	_overlay.fd = fd;
	return 0;
}

int do_sanp_shot(int fd)
{
	int x = 1, wait_time = 1000;

	ioctl(fd, IOCTL_CCAP_SNAPSHOT_DO, &x);
	do {
		usleep(1000);
		if (wait_time-- <= 0)
			return -1;
	} while (ioctl(fd, IOCTL_CCAP_SNAPSHOT_IDLE, &x) != 0);
	return 0;
}

void ccap_control(void)
{
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmivalenum frmival;
	struct ccap_fb_direct fb_direct;
	int fd, x, y;

	fd = open("/dev/video0", O_RDWR | O_NONBLOCK | O_SYNC, 0);
	if (fd < 0) {
		printf("Failed to open /dev/video0\n");
		exit(EXIT_FAILURE);
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.index = 0;
	if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0)
		printf("Pixel Format: %s\n", fmt.description);

	printf("Supported Pixel Formats:\n");
	for (fmt.index = 0, fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	     ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0; fmt.index++) {

		printf("  %d: %s (0x%08X)\n", fmt.index, fmt.description, fmt.pixelformat);

		/* enumerate reslution */
		frmsize.pixel_format = fmt.pixelformat;
		for (frmsize.index = 0;
		     ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0; frmsize.index++) {
			if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
				printf("    Resolution: %ux%u\n",
				frmsize.discrete.width, frmsize.discrete.height);

				// enumerate frame rate
				frmival.pixel_format = fmt.pixelformat;
				frmival.width = frmsize.discrete.width;
				frmival.height = frmsize.discrete.height;
				for (frmival.index = 0;
				     ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0;
				     frmival.index++) {
					if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
						printf("      Frame rate: %.2f fps\n",
							(float)frmival.discrete.denominator /
							frmival.discrete.numerator);
					}
				}
			} else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
				printf("    Resolution Range: %ux%u to %ux%u (step %ux%u)\n",
					frmsize.stepwise.min_width, frmsize.stepwise.min_height,
					frmsize.stepwise.max_width, frmsize.stepwise.max_height,
					frmsize.stepwise.step_width, frmsize.stepwise.step_height);
			}
		}
	}

	fb_direct.enable = 1;
	fb_direct.wait_sync = 1;
	fb_direct.is_overlay = 0;     /* fb0 */
	fb_direct.fb_width = 1024;    /* LCD WIDTH*/
	fb_direct.fb_height = 600;    /* LCD HEIGHT */
	fb_direct.img_x = 0;
	fb_direct.img_y = 0;
	fb_direct.img_width = IMAGE_WIDTH;
	fb_direct.img_height = IMAGE_HEIGHT;
	ioctl(fd, IOCTL_CCAP_FBD_SET, &fb_direct);

	if (DO_SNAPSHOT_DEMO) {
		__u8 *image;

		if (init_overlay() != 0)
			exit(EXIT_FAILURE);

		/* get ccap memory mapping */
		image = mmap(NULL, IMAGE_WIDTH * IMAGE_HEIGHT * 4, PROT_READ, MAP_SHARED, fd, 0);
		if (image == MAP_FAILED || !image) {
			printf("mmap failed!!\n");
			close(fd);
			exit(EXIT_FAILURE);
		}

		while (1) {
			sleep(1);
			if (do_sanp_shot(fd) == 0)
				memcpy(_overlay.memp, image, IMAGE_WIDTH * IMAGE_HEIGHT * 4);
			else
				printf("Failed to catpure images!\n");
		}
	} else {
		for (x = 0; ; x = (x + 30) % 300) {
			for (y = 0; y <= 120; y += 30) {
				sleep(1);
				fb_direct.img_x = x;
				fb_direct.img_y = y;
				ioctl(fd, IOCTL_CCAP_FBD_SET, &fb_direct);
			}
		}
	}
}

void gstreamer_v4l2_capture(void)
{
	char gst_command[512];

	snprintf(gst_command, sizeof(gst_command),
		"gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=%d,height=%d,format=RGB16 ! fakesink",
		IMAGE_WIDTH, IMAGE_HEIGHT);

	printf("Starting GStreamer command: %s\n", gst_command);

	// Execute GStreamer command
	execlp("sh", "sh", "-c", gst_command, (char *)NULL);

	// If execlp fails
	perror("Failed to execute GStreamer command");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("Fork failed");
		return EXIT_FAILURE;
	}

	if (pid == 0) {
		// Child process: Run GStreamer
		gstreamer_v4l2_capture();
	} else {
		// Parent process: Print "hello"
		ccap_control();

		// Wait for child process to finish (optional, if needed)
		wait(NULL);
	}
	return EXIT_SUCCESS;
}
