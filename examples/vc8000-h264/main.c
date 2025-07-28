// SPDX-License-Identifier: GPL-2.0
/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
 * Main file of the application
 *
 * Copyright 2012 Samsung Electronics Co., Ltd.
 * Copyright (c) 2015 Linaro Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <string.h>
#include <linux/videodev2.h>
#include "msm-v4l2-controls.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <linux/fb.h>

#include "args.h"
#include "common.h"
#include "fileops.h"
#include "video.h"
#include "parser.h"
#include "dcfb.h"

/*
 * This is the size of the buffer for the compressed stream.
 * It limits the maximum compressed frame size.
 */
#define STREAM_BUUFER_SIZE	(1024 * 1024)

/* The number of compress4ed stream buffers */
#define STREAM_BUFFER_CNT	2

/*
 * The number of extra buffers for the decoded output.
 * This is the number of buffers that the application can keep
 * used and still enable video device to decode with the hardware.
 */
#define RESULT_EXTRA_BUFFER_CNT 2

struct instance inst;

#define LCD_WIDTH	1024
#define LCD_HEIGHT	600

dc_frame_info UserFrameBufferSize = {
	.width = LCD_WIDTH,
	.height = LCD_HEIGHT,
	.stride = 4096,
};

/*
 *  V4L2 device default ioctl for VC8K
 */
struct vc8k_pp_params {
	int enable_pp;
	__u32 frame_buf_paddr;	/* physical address of frame buffer          */
	int frame_buff_size;
	int frame_buf_w;	/* width of frame buffer width               */
	int frame_buf_h;	/* height of frame buffer                    */
	int img_out_x;		/* image original point(x,y) on frame buffer */
	int img_out_y;		/* image original point(x,y) on frame buffer */
	int img_out_w;		/* image output width on frame buffer        */
	int img_out_h;		/* image output height on frame buffer       */
	int img_out_fmt;	/* image output format                       */
	int rotation;
	int pp_out_dst;		/* PP output destination.                    */
				/* 0: fb0                                    */
				/* 1: fb1                                    */
				/* otherwise: frame_buf_paddr                */
	int libjpeg_mode;	/* 0: v4l2-only; 1: libjpeg+v4l2             */
	int resserved[8];
};

#define VC8KIOC_PP_SET_CONFIG	_IOW('v', 91, struct vc8k_pp_params)
#define VC8KIOC_PP_GET_CONFIG	_IOW('v', 92, struct vc8k_pp_params)

struct vc8k_pp_params  pp;

#define PP_ROTATION_NONE	0U
#define PP_ROTATION_RIGHT_90	1U
#define PP_ROTATION_LEFT_90	2U
#define PP_ROTATION_HOR_FLIP	3U
#define PP_ROTATION_VER_FLIP	4U
#define PP_ROTATION_180		5U

static int handle_v4l_events(struct video *vid)
{
	struct v4l2_event event;
	int ret;

	memset(&event, 0, sizeof(event));
	ret = ioctl(vid->fd, VIDIOC_DQEVENT, &event);
	if (ret < 0) {
		err("vidioc_dqevent failed (%s) %d", strerror(errno), -errno);
		return -errno;
	}

	switch (event.type) {
	case V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT:
		dbg("Port Reconfig recieved insufficient\n");
		break;
	case V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT:
		dbg("Setting changed sufficient\n");
		break;
	case V4L2_EVENT_MSM_VIDC_FLUSH_DONE:
		dbg("Flush Done Recieved\n");
		break;
	case V4L2_EVENT_MSM_VIDC_CLOSE_DONE:
		dbg("Close Done Recieved\n");
		break;
	case V4L2_EVENT_MSM_VIDC_SYS_ERROR:
		dbg("SYS Error Recieved\n");
		break;
	default:
		dbg("unknown event type occurred %x\n", event.type);
		break;
	}

	return 0;
}

void cleanup(struct instance *i)
{
	printf("%s called.\n", __func__);
	if (i->video.fd)
		video_close(i);
	if (i->in.fd)
		input_close(i);

	if (i->fb.fd)
		close(i->fb.fd);
}

void sig_handler(int signo)
{
	if (signo == SIGINT) {
		printf("SIGINT signal catched!\n");
		cleanup(&inst);
	}
}


int extract_and_process_header(struct instance *i)
{
	int used = 0, fs;
	int ret;

	ret = i->parser.func(&i->parser.ctx,
			     i->in.p + i->in.offs,
			     i->in.size - i->in.offs,
			     i->video.out_buf_addr[0],
			     i->video.out_buf_size,
			     &used, &fs, 1);
	if (ret != 0) {
		err("Failed to extract header from stream");
		return -1;
	}

	i->in.offs += used;

	ret = video_queue_buf_out(i, 0, fs);
	if (ret)
		return -1;

	dbg("queued output buffer %d", 0);

	i->video.out_buf_flag[0] = 1;

	ret = video_stream(i, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			   VIDIOC_STREAMON);
	if (ret)
		return -1;

	return 0;
}

/*
 * This threads is responsible for parsing the stream and
 * feeding video decoder with consecutive frames to decode
 */
void *parser_thread_func(void *args)
{
	struct instance *i = (struct instance *)args;
	struct video *vid = &i->video;
	int used, fs, n;
	int ret;

	dbg("Parser thread started");

	while (!i->error && !i->finish && !i->parser.finished) {
		n = 0;
		pthread_mutex_lock(&i->lock);
		while (n < vid->out_buf_cnt && vid->out_buf_flag[n])
			n++;
		pthread_mutex_unlock(&i->lock);

		if (n < vid->out_buf_cnt && !i->parser.finished) {

			//printf("%s %d, n = %d, %d, %d, offset=0x%x, left=%d\n",
			//	__func__, __LINE__, n, vid->out_buf_cnt, i->parser.finished,
			//	i->in.offs, i->in.size - i->in.offs);

			ret = i->parser.func(&i->parser.ctx,
					     i->in.p + i->in.offs,
					     i->in.size - i->in.offs,
					     vid->out_buf_addr[n],
					     vid->out_buf_size,
					     &used, &fs, 0);

			if ((ret != 0) || (i->in.offs + 4 >= i->in.size) || (fs == 0)) {
				if (ret != 0)
					dbg("Parser has error, abort!\n");
				else
					dbg("Parser has extracted all frames");
				i->parser.finished = 1;
				fs = 0;
				break;
			}

			// dbg("Extracted frame of size %d", fs);

			pthread_mutex_lock(&i->lock);
			vid->out_buf_flag[n] = 1;
			pthread_mutex_unlock(&i->lock);

			// dbg("queued output buffer %d", n);

			ret = video_queue_buf_out(i, n, fs);
			i->in.offs += used;
		}
	}

	dbg("Parser thread finished");

	return NULL;
}

void *main_thread_func(void *args)
{
	struct instance *i = (struct instance *)args;
	struct video *vid = &i->video;
	struct pollfd pfd;
	short revents;
	int ret, n, finished;

	dbg("main thread started");

	pfd.fd = vid->fd;
	pfd.events = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM |
		     POLLRDBAND | POLLPRI;

	while (1) {
		ret = poll(&pfd, 1, 2000);
		if (!ret) {
			err("poll timeout");
			break;
		} else if (ret < 0) {
			err("poll error");
			break;
		}

		revents = pfd.revents;

		if (revents & (POLLIN | POLLRDNORM)) {
			unsigned int bytesused;

			/* capture buffer is ready */

			ret = video_dequeue_capture(i, &n, &finished,
						    &bytesused);
			if (ret < 0)
				goto next_event;

			vid->cap_buf_flag[n] = 0;

			// info("decoded frame %ld", vid->total_captured);

			if (finished)
				break;

			vid->total_captured++;

			ret = video_queue_buf_cap(i, n);
			if (!ret)
				vid->cap_buf_flag[n] = 1;
		}

next_event:
		if (revents & (POLLOUT | POLLWRNORM)) {

			ret = video_dequeue_output(i, &n);
			if (ret < 0) {
				err("dequeue output buffer fail");
			} else {
				pthread_mutex_lock(&i->lock);
				vid->out_buf_flag[n] = 0;
				pthread_mutex_unlock(&i->lock);
			}

			// dbg("dequeued output buffer %d", n);
		}

		if (revents & POLLPRI) {
			dbg("v4l2 event");
			handle_v4l_events(vid);
		}
	}

	dbg("main thread finished");

	return NULL;
}

int fb_open(struct instance *i, char *name)
{
	struct fb_var_screeninfo FBVar;
	int ret;

	i->fb.fd = open(name, O_RDWR, 0);
	if (i->fb.fd < 0) {
		err("Failed to open fb device: %s", name);
		return -1;
	}

	ret = ioctl(i->fb.fd, FBIOGET_VSCREENINFO, &FBVar);
	if (ret < 0) {
		printf("FBIOGET_VSCREENINFO failed!\n");
		return -1;
	}

	ret = ioctl(i->fb.fd, FBIOPUT_VSCREENINFO, &FBVar);
	if (ret != 0) {
		printf("FBIOPUT_VSCREENINFO failed!\n");
		return -1;
	}

	pp.frame_buff_size = LCD_WIDTH * LCD_HEIGHT * 2;

	return 0;
}

int main(int argc, char **argv)
{
	struct video *vid = &inst.video;
	pthread_t parser_thread;
	pthread_t main_thread;
	int ret, n;

	memset(&inst, 0, sizeof(inst));

	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		printf("Failed to catach CTRL-C signal!\n");
		return -1;
	}

	ret = parse_args(&inst, argc, argv);
	if (ret) {
		print_usage(argv[0]);
		return 1;
	}

	pthread_mutex_init(&inst.lock, 0);

	vid->total_captured = 0;

	ret = video_open(&inst, inst.video.name);
	if (ret)
		goto err;

	if (inst.pp_enable)
		pp.enable_pp = true;
	else
		pp.enable_pp = false;

	pp.frame_buf_w = LCD_WIDTH;
	pp.frame_buf_h = LCD_HEIGHT;
	pp.img_out_x = inst.x_pos;
	pp.img_out_y = inst.y_pos;
	pp.img_out_w = inst.width;
	pp.img_out_h = inst.height;
	pp.rotation = PP_ROTATION_NONE;

	if (strcmp(inst.out_format, "argb888") == 0)
		pp.img_out_fmt = V4L2_PIX_FMT_ARGB32;
	else if (strcmp(inst.out_format, "rgb565") == 0)
		pp.img_out_fmt = V4L2_PIX_FMT_RGB565;
	else
		pp.img_out_fmt = V4L2_PIX_FMT_NV12;

	ret = ioctl(vid->fd, VC8KIOC_PP_SET_CONFIG, pp);
	if (ret < 0) {
		err("VC8KIOC_PP_SET_CONFIG failed (%s)", strerror(errno));
		err("VC8KIOC_PP_SET_CONFIG failed (%s)", strerror(errno));
		goto err;
	}

	if (pp.enable_pp == false) {
		printf("VC8000 PP is disabled.\n");
		video_close(&inst);
		return 0;
	}

	video_stop(&inst);

	if (inst.config_only)
		printf("Only used to config VC8000 PP and open fbdev. Expected to run in background.\n");

	ret = fb_open(&inst, inst.fb.name);
	if (ret)
		goto err;

	ret = video_setup_output(&inst, inst.parser.codec,
				 STREAM_BUUFER_SIZE, 2);
	if (ret)
		goto err;

	ret = input_open(&inst, inst.in.name);
	if (ret)
		goto err;

	ret = parse_stream_init(&inst.parser.ctx);
	if (ret)
		goto err;

	ret = video_setup_capture(&inst, 0, inst.width, inst.height);
	if (ret)
		goto err;

	ret = extract_and_process_header(&inst);
	if (ret)
		goto err;

	/* queue all capture buffers */
	for (n = 0; n < vid->cap_buf_cnt; n++) {
		ret = video_queue_buf_cap(&inst, n);
		if (ret)
			goto err;
		vid->cap_buf_flag[n] = 1;
	}

	ret = video_stream(&inst, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			   VIDIOC_STREAMON);
	if (ret)
		goto err;

	dbg("Launching threads");

	if (pthread_create(&parser_thread, NULL, parser_thread_func, &inst))
		goto err;

	if (pthread_create(&main_thread, NULL, main_thread_func, &inst))
		goto err;

	pthread_join(parser_thread, 0);
	pthread_join(main_thread, 0);

	dbg("Threads have finished");

	video_stop(&inst);

	cleanup(&inst);

	pthread_mutex_destroy(&inst.lock);

	info("Total frames captured %ld", vid->total_captured);

	return 0;
err:
	printf("ERR OUT!\n");
	cleanup(&inst);
	return 1;
}

