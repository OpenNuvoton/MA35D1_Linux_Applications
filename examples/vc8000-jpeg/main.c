/* V4L2 video picture grabber
   Copyright (C) 2009 Mauro Carvalho Chehab <mchehab@infradead.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <linux/fb.h>

#include "dcfb.h"

//#define DO_PERFORMANCE_TEST

#define JPEG_FILE_SIZE_MAX      0x780000

/* Number of output planes */
#define OUT_PLANES		1

/* Number of capture planes */
#define CAP_PLANES		1

struct v4l2_plane planes_cap[CAP_PLANES];
struct v4l2_plane planes_out[OUT_PLANES];

/*
 *  V4L2 device default ioctl for VC8K
 */
struct vc8k_pp_params {
	int   enable_pp;
	int   frame_buf_paddr;           /* physical address of frame buffer          */
	int   frame_buff_size;
	int   frame_buf_w;               /* width of frame buffer width               */
	int   frame_buf_h;               /* height of frame buffer                    */
	int   img_out_x;                 /* image original point(x,y) on frame buffer */
	int   img_out_y;                 /* image original point(x,y) on frame buffer */
	int   img_out_w;                 /* image output width on frame buffer        */
	int   img_out_h;                 /* image output height on frame buffer       */
	int   img_out_fmt;               /* image output format                       */
	int   rotation;
	int   pp_out_dst;                /* PP output destination.                    */
					 /* 0: fb0                                    */
					 /* 1: fb1                                    */
					 /* otherwise: frame_buf_paddr                */
	int   libjpeg_mode;              /* 0: v4l2-only; 1: libjpeg+v4l2             */
	int   resserved[8];
};

#define VC8KIOC_PP_SET_CONFIG	_IOW ('v', 91, struct vc8k_pp_params)
#define VC8KIOC_PP_GET_CONFIG	_IOW ('v', 92, struct vc8k_pp_params)

#define PP_ROTATION_NONE                                0U
#define PP_ROTATION_RIGHT_90                            1U
#define PP_ROTATION_LEFT_90                             2U
#define PP_ROTATION_HOR_FLIP                            3U
#define PP_ROTATION_VER_FLIP                            4U
#define PP_ROTATION_180                                 5U

#define PP_DST_FB0                                      0U
#define PP_DST_FB1                                      1U

struct buffer {
	void   *start;
	size_t length;
};
struct buffer   buffers_cap, buffers_out;

dc_frame_info UserFrameBufferSize = {
	.width = 1024,
	.height = 600,
	.stride = 4096,
};

static struct fb_var_screeninfo FBVar;

static int  decode_jpeg_file(int fd, int bytes_read);

static int read_jpeg_file(char *file_name, unsigned char *buff, unsigned int *bytes_read)
{
	struct stat in_stat;
	int   fd, size;
	char  *p;

	fd = open(file_name, O_RDONLY);
	if (!fd) {
		printf("\nFailed to open file: %s\n", file_name);
		return -1;
	}
	fstat(fd, &in_stat);
	size = in_stat.st_size;
	
	if (size > JPEG_FILE_SIZE_MAX) {
		printf("JPEG file size is large than %d. Skip this file.\n", JPEG_FILE_SIZE_MAX);
		return -1;
	}

	p = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		printf("Failed to map input file");
		return -1;
	}
	memcpy(buff, p, size);
	
	munmap(p, size);
	close(fd);
	
	return 0;
}

static void xioctl(int fh, char *req_name, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

	if (r == -1) {
		fprintf(stderr, "xioctl req %s error %d, %s\\n", req_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int fb_open(char *name)
{
	struct fb_fix_screeninfo FBFix;
	unsigned int ScreenSizeFb;
	unsigned int OneframeSizeFb;
	int  fd, ret;

	fd = open(name, O_RDWR, 0);
	if (fd < 0) {
		printf("Failed to open fb device: %s", name);
		return -1;
	}

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &FBVar);
	if (ret < 0) {
		printf("FBIOGET_VSCREENINFO failed!\n");
		close(fd);
		return -1;
	}

	ret = ioctl(fd, FBIOPUT_VSCREENINFO, &FBVar);
	if (ret != 0) {
		printf("FBIOPUT_VSCREENINFO failed!\n");
		close(fd);
		return -1;
	}
	return fd;
}	

int main(int argc, char **argv)
{
	struct v4l2_format          fmt;
	struct v4l2_requestbuffers  req;
	enum v4l2_buf_type          type;
	struct v4l2_buffer          buf;
	int                         fd = -1;
	int                         fb_fd;
	char                        *dev_name = "/dev/video0";
	char                        *fb_name = "/dev/fb0";
	unsigned int                bytes_read;
	char                        dir_name[256];
	char                        file_name[256];
	int                         i, ret;
	struct timeval              t0, t1;
	int c;
	struct vc8k_pp_params       pp;

	strcpy(file_name, argv[1]);
	fb_fd = fb_open(fb_name);
	if (fb_fd < 0) {
		printf("Cannot open %s\n", fb_name);
		exit(EXIT_FAILURE);
	}

	fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
	if (fd < 0) {
		printf("Cannot open %s\n", dev_name);
		exit(EXIT_FAILURE);
	}

	/*
	 *  Configure PP
	 */
	memset(&pp, 0, sizeof(pp));
	pp.pp_out_dst = 0;
	pp.enable_pp = 1;
	pp.frame_buf_w = FBVar.xres;
	pp.frame_buf_h = FBVar.yres;
	pp.img_out_x = 0;
	pp.img_out_y = 0;
	pp.img_out_w = pp.frame_buf_w;
	pp.img_out_h = pp.frame_buf_h;
	pp.rotation  = PP_ROTATION_NONE;
	pp.img_out_fmt = V4L2_PIX_FMT_ARGB32;   /* or V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_NV12 */

	if (argc > 2)
		pp.img_out_w = atoi(argv[2]);

	if (argc > 3)
		pp.img_out_h = atoi(argv[3]);

	printf("Set PP out frame buffer size: %d x %d\n", pp.frame_buf_w, pp.frame_buf_h);
	printf("Set PP out to (%d,%d) %d x %d\n", pp.img_out_x, pp.img_out_y, pp.img_out_w, pp.img_out_h);

	if (ioctl(fd, VC8KIOC_PP_SET_CONFIG, &pp) != 0) {
		printf("VC8KIOC_PP_SET_CONFIG failed (%s)", strerror(errno));
		return -1;
	}
	printf("PP setting done.\n");

	/*-----------------------------------------------------*/
	/*  Set output mplane format  (VIDIOC_S_FMT)           */
	/*-----------------------------------------------------*/
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix.width       = 1920;
	fmt.fmt.pix.height      = 1080;
	fmt.fmt.pix_mp.num_planes = 1;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = JPEG_FILE_SIZE_MAX;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
	xioctl(fd, "VIDIOC_S_FMT", VIDIOC_S_FMT, &fmt);
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_JPEG) {
		printf("VIDIOC_S_FMT failed to set  V4L2_PIX_FMT_JPEG!\n");
		exit(EXIT_FAILURE);
	}

	printf("OUTPUT: Set format %ux%u, sizeimage %u, bpl %u, pixelformat:%x\n",
		fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
		fmt.fmt.pix_mp.plane_fmt[0].sizeimage,
		fmt.fmt.pix_mp.plane_fmt[0].bytesperline,
		fmt.fmt.pix_mp.pixelformat);

	/*-----------------------------------------------------*/
	/*  Set capture mplane format (VIDIOC_S_FMT)           */
	/*-----------------------------------------------------*/
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix.width       = 1920;
	fmt.fmt.pix.height      = 1088;
	fmt.fmt.pix_mp.num_planes = 1;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 1920*1080*2;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	// fmt.fmt.pix.field    = V4L2_FIELD_INTERLACED;
	xioctl(fd, "VIDIOC_S_FMT", VIDIOC_S_FMT, &fmt);

	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUV420) {
		printf("VIDIOC_S_FMT failed to set  V4L2_PIX_FMT_YUV420!\n");
		exit(EXIT_FAILURE);
	}

	//printf("CAPTURE: Set format %ux%u, sizeimage %u, bpl %u, pixelformat:%c%c%c%c\n",
	//    fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, fmt.fmt.pix_mp.plane_fmt[0].sizeimage, fmt.fmt.pix_mp.plane_fmt[0].bytesperline,
	//    fmt.fmt.pix_mp.pixelformat & 0xff, (fmt.fmt.pix_mp.pixelformat>>8) & 0xff, (fmt.fmt.pix_mp.pixelformat>>16) & 0xff, (fmt.fmt.pix_mp.pixelformat>>24) & 0xff);

	/*-----------------------------------------------------*/
	/*  Request output mplane buffer (VIDIOC_REQBUFS)      */
	/*  Query output mplane buffer (VIDIOC_QUERYBUF)       */
	/*-----------------------------------------------------*/
	memset(&req, 0, sizeof(req));
	req.count = 1;
	req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	xioctl(fd, "VIDIOC_REQBUFS", VIDIOC_REQBUFS, &req);

	memset(&buf, 0, sizeof(buf));
	memset(planes_out, 0, sizeof(planes_out));
	buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = 0;
	buf.m.planes    = planes_out;
	buf.length      = OUT_PLANES;
	xioctl(fd, "VIDIOC_QUERYBUF", VIDIOC_QUERYBUF, &buf);

	buffers_out.length = buf.length;
	buffers_out.start = mmap(NULL, buf.m.planes[0].length,
				 PROT_READ | PROT_WRITE, MAP_SHARED,
				 fd, buf.m.planes[0].m.mem_offset);
	if (MAP_FAILED == buffers_out.start) {
	        printf("out mmap");
	        exit(EXIT_FAILURE);
	}

	/*-----------------------------------------------------*/
	/*  Request capture mplane buffer (VIDIOC_REQBUFS)     */
	/*  Query capture mplane buffer (VIDIOC_QUERYBUF)      */
	/*-----------------------------------------------------*/
	memset(&req, 0, sizeof(req));
	req.count = 1;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	xioctl(fd, "VIDIOC_REQBUFS", VIDIOC_REQBUFS, &req);

	memset(&buf, 0, sizeof(buf));
	memset(planes_cap, 0, sizeof(planes_cap));
	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = 0;
	buf.m.planes    = planes_cap;
	buf.length      = CAP_PLANES;
	xioctl(fd, "VIDIOC_QUERYBUF", VIDIOC_QUERYBUF, &buf);
	
	buffers_cap.length = buf.length;
	buffers_cap.start = mmap(NULL, buf.m.planes[0].length,
				 PROT_READ | PROT_WRITE, MAP_SHARED,
				 fd, buf.m.planes[0].m.mem_offset);
	if (MAP_FAILED == buffers_cap.start) {
	        printf("cap mmap");
	        exit(EXIT_FAILURE);
	}

	/*-----------------------------------------------------*/
	/*  Read JPEG files and decode                         */
	/*-----------------------------------------------------*/

	printf("\n\nDecode JPEG file: %s\n", file_name);
	if (read_jpeg_file(file_name, buffers_out.start , &bytes_read) == 0) {
#ifdef DO_PERFORMANCE_TEST			
		gettimeofday(&t0, NULL);
		for (i = 0; i < 100; i++)
			decode_jpeg_file(fd, bytes_read);
		gettimeofday(&t1, NULL);
		i = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
		printf("Decode 100 times spend %ds, %dus\n", i/1000000, i % 1000000);
		printf("JPEG decode time: %d us\n", i/100);
#else
		gettimeofday(&t0, NULL);
		decode_jpeg_file(fd, bytes_read);
		gettimeofday(&t1, NULL);
		i = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
		printf("Decode time spend %ds, %dus\n", i/1000000, i % 1000000);
#endif		
	}
	
	munmap(buffers_out.start, buffers_out.length);
	munmap(buffers_cap.start, buffers_cap.length);

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	xioctl(fd, "VIDIOC_STREAMOFF", VIDIOC_STREAMOFF, &type);

	        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	xioctl(fd, "VIDIOC_STREAMOFF", VIDIOC_STREAMOFF, &type);

	close(fd);
	close(fb_fd);
	return 0;
}

/*
 *  fd:  video device file descriptor id
 *  file_name:  JPEG file name
 */
static int  decode_jpeg_file(int fd, int bytes_read)
{
	struct v4l2_buffer cap_buf, out_buf;
	enum v4l2_buf_type type;
	struct timeval tv, t0, t1;
	fd_set fds;
	int r, ret; 

	/*-----------------------------------------------------*/
	/*  Queue capture plane buffers (VIDIOC_QBUF)          */
	/*-----------------------------------------------------*/
	memset(&cap_buf, 0, sizeof(cap_buf));
	cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap_buf.memory = V4L2_MEMORY_MMAP;
	cap_buf.index = 0;
	cap_buf.length = CAP_PLANES;
	cap_buf.m.planes = planes_cap;
	cap_buf.m.planes[0].bytesused = bytes_read;
	cap_buf.m.planes[0].data_offset = 0;
	xioctl(fd, "VIDIOC_QBUF", VIDIOC_QBUF, &cap_buf);

	/*-----------------------------------------------------*/
	/*  Queue output plane buffers (VIDIOC_QBUF)           */
	/*-----------------------------------------------------*/
	memset(&out_buf, 0, sizeof(out_buf));
	out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	out_buf.memory = V4L2_MEMORY_MMAP;
	out_buf.index = 0;
	out_buf.length = OUT_PLANES;
	out_buf.m.planes = planes_out;
	out_buf.m.planes[0].bytesused = 0;
	out_buf.m.planes[0].data_offset = 0;
	xioctl(fd, "VIDIOC_QBUF", VIDIOC_QBUF, &out_buf);

	/*-----------------------------------------------------*/
	/*  Start streaming                                    */
	/*-----------------------------------------------------*/
	// printf("Start streaming...\n");

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	xioctl(fd, "VIDIOC_STREAMON", VIDIOC_STREAMON, &type);

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	xioctl(fd, "VIDIOC_STREAMON", VIDIOC_STREAMON, &type);

	do {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
	
		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;
	
		r = select(fd + 1, &fds, NULL, NULL, &tv);
	} while ((r == -1 && (errno = EINTR)));
	if (r == -1) {
		printf("select");
		return errno;
	}

	/*
	 *  Decode done
	 */
	xioctl(fd, "VIDIOC_DQBUF", VIDIOC_DQBUF, &cap_buf);
	xioctl(fd, "VIDIOC_DQBUF", VIDIOC_DQBUF, &out_buf);
	return 0;
}
