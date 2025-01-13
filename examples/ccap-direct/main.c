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
#include <linux/videodev2.h>

#define IOCTL_CCAP_FBD_SET	_IOW ('v', 61, struct ccap_fb_direct)
#define IOCTL_CCAP_FBD_GET	_IOW ('v', 62, struct ccap_fb_direct)

struct ccap_fb_direct {
	int enable;      /* 0: disable; 1: enable     */
	int is_overlay;  /* 0: main frame; 1: overlay */
	int fb_width;    /* frame buffer width        */
	int fb_height;   /* frame buffer height       */
	int img_x;       /* image render left x       */
	int img_y;       /* image render top y        */
	int img_width;   /* image width               */
	int img_height;  /* image height              */
};

void ccap_control()
{
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum frmsize;
	struct v4l2_frmivalenum frmival;
	struct ccap_fb_direct fb_direct;
	int fd, x, y;

	fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
	if (fd < 0) {
		printf("Failed to open /dev/video0\n");
		exit(EXIT_FAILURE);
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.index = 0;
	if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
    		printf("Pixel Format: %s\n", fmt.description);
	}

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
	fb_direct.is_overlay = 0;     /* fb0 */
	fb_direct.fb_width = 1024;    /* LCD WIDTH*/
	fb_direct.fb_height = 600;    /* LCD HEIGHT */
	fb_direct.img_x = 0;
	fb_direct.img_y = 0;
	fb_direct.img_width = 640;    /* capture image width */
	fb_direct.img_height = 480;   /* capture image height */
	ioctl(fd, IOCTL_CCAP_FBD_SET, &fb_direct);

	for (x = 0; ; x = (x + 30) % 300) {
		for (y = 0; y <= 120; y += 30) {
			sleep(1);
			fb_direct.img_x = x;
			fb_direct.img_y = y;
			ioctl(fd, IOCTL_CCAP_FBD_SET, &fb_direct);
		}
	}
}

void gstreamer_v4l2_capture()
{
	const char gst_command[] = "gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480,format=RGB16 ! fakesink";

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
