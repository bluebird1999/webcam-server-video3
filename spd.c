/*
 * spd.c
 *
 *  Created on: Oct 2, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <stdio.h>
#include <stdlib.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <signal.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtscolor.h>
#include <rtscamkit.h>
#include <getopt.h>
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../tools/tools_interface.h"
#include "../../server/miio/miio_interface.h"
#include "../../server/recorder/recorder_interface.h"
#include "../../server/realtek/realtek_interface.h"
#include "../../server/video/video_interface.h"
#include "../../server/device/device_interface.h"
//server header
#include "spd.h"
#include "video3.h"
#include "config.h"
#include "video3_interface.h"

/*
 * static
 */

//variable
static void *md_handle;
static void *pd_handle;
static void *mt_handle;
static rts_img *src_img;
static rts_md_res *md_res;
static rts_mt_res *mt_res;
static rts_md_cfg md_cfg;
static rts_pd_cfg pd_cfg;
static rts_mt_cfg mt_cfg;
static rts_pd_res pd_res = {0};
static unsigned int	buffer_frames;

//function

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static void spd_set_md_default_parameter_mt(rts_md_cfg *cfg, int width, int height)
{
	cfg->w = width;
	cfg->h = height;
	cfg->md_opt = MD_FRAME_DIFF;

	cfg->sensitivity = 4;
	cfg->scene_thd = 0.5 * width * height;

	//background modeling
	cfg->bgm_cfg.train_frames = 3;
	cfg->bgm_cfg.learn_rate = 248;
	cfg->bgm_cfg.fade_rate = 10;
	cfg->bgm_cfg.nr_bins = 16;
	cfg->bgm_cfg.bin_bits = 2;
	cfg->bgm_cfg.back_thd = 2;

	//frame difference
	cfg->fd_cfg.y_thd = 30;
	cfg->fd_cfg.uv_thd = 15;

	cfg->pp_en = 1;
	cfg->ioc_en = 0;
	cfg->nr_obj = 10;
}

static void spd_set_md_default_parameter_spd(rts_md_cfg *cfg, int width, int height)
{
	cfg->w = width;
	cfg->h = height;
	cfg->md_opt = MD_BACKGROUND_MODELING;
	cfg->dmem_opt = 0;

	cfg->sensitivity = 5;
	cfg->scene_thd = 0.8 * width * height;

	//background modeling
	cfg->bgm_cfg.train_frames = 3;
	cfg->bgm_cfg.learn_rate = 248;
	cfg->bgm_cfg.fade_rate = 240;
	cfg->bgm_cfg.nr_bins = 16;
	cfg->bgm_cfg.bin_bits = 2;
	cfg->bgm_cfg.back_thd = 2;

	//frame difference
	cfg->fd_cfg.y_thd = 30;
	cfg->fd_cfg.uv_thd = 15;

	cfg->pp_en = 1;
	cfg->ioc_en = 0;
	cfg->nr_obj = 5;
}

static void spd_set_mt_default_parameter(rts_mt_cfg *cfg, int width, int height)
{
	cfg->w = width;
	cfg->h = height;

	cfg->cc_cfg.nr_cc_thd = 20;
	cfg->cc_cfg.min_ar = 0.0001;
	cfg->cc_cfg.max_ar = 10.0;
	cfg->cc_cfg.cc_ratio = 0.2;

	cfg->cont_thd = 1;
	cfg->min_x_ratio = 0.005;
	cfg->min_y_ratio = 0.03;
}

static void spd_set_pd_default_parameter(rts_pd_cfg *cfg, int width, int height, char *fname)
{
	cfg->cc_cfg.nr_cc_thd = 0.002 * width * height;
	cfg->cc_cfg.min_ar = 0.05;
	cfg->cc_cfg.max_ar = 3;
	cfg->cc_cfg.cc_ratio = 0.5;

	cfg->net_cfg.version = 3.1;
	cfg->net_cfg.cfg_filename = NULL;
	cfg->net_cfg.weights_filename = fname;

	cfg->conf_thd = 0.5;
	cfg->pd_frames = 1;
	cfg->nr_pd = 5;

	cfg->w = width;
	cfg->h = height;
}

static void spd_motion_tracking(rts_point mov_dis, rts_md_src *src, int *ad_bf)
{
	message_t msg;
	float x_ratio = 45 / 128.0;
	float y_ratio = 1.0;
	int motor_x = -mov_dis.x * x_ratio;
	int motor_y = -mov_dis.y * y_ratio;
	if (motor_x != 0) {
		src->retrain_flag = 1;
		msg_init(&msg);
		msg.sender = msg.receiver = SERVER_VIDEO3;
		msg.message = MSG_DEVICE_CTRL_DIRECT;
		if(  motor_x > 0 )
			msg.arg_in.cat = DEVICE_CTRL_MOTOR_HOR_RIGHT;
		else
			msg.arg_in.cat = DEVICE_CTRL_MOTOR_HOR_LEFT;
		msg.arg_in.wolf = 0;
		msg.arg_in.handler = 0;
		msg.arg_pass.cat = 0;
		msg.arg_pass.wolf = 0;
		msg.arg_pass.handler = 0;
		manager_common_send_message(SERVER_DEVICE, &msg);
	}
	if (motor_y != 0) {
		src->retrain_flag = 1;
		msg_init(&msg);
		msg.sender = msg.receiver = SERVER_VIDEO3;
		msg.message = MSG_DEVICE_CTRL_DIRECT;
		if(  motor_y > 0 )
			msg.arg_in.cat = DEVICE_CTRL_MOTOR_VER_UP;
		else
			msg.arg_in.cat = DEVICE_CTRL_MOTOR_VER_DOWN;
		msg.arg_in.wolf = 0;
		msg.arg_in.handler = 0;
		msg.arg_pass.cat = 0;
		msg.arg_pass.wolf = 0;
		msg.arg_pass.handler = 0;
		manager_common_send_message(SERVER_DEVICE, &msg);
	}
	//drop frames for stable image
	//motr_steps * stpes_per_circle * secs_per_circle * fps
	*ad_bf = abs(motor_x) / 170.0 * 8 * 30;
}

int video3_spd_proc(video3_spd_config_t *ctrl, int channel, rts_md_src *md_src, rts_pd_src *pd_src)
{
	int ret = 0, ret_pd, ret_mt;
	static unsigned int count = 0;
	struct rts_av_buffer *b = NULL;
	static struct rts_av_buffer *last_b;
	//***
	RTS_SAFE_RELEASE(last_b, rts_av_put_buffer);
	ret = rts_av_poll(channel);
	if (ret < 0)
		return ret;
	ret = rts_av_recv(channel, &b);
	if (ret < 0)
		return ret;
	rts_set_rts_img_data(src_img, b->vm_addr, b->bytesused, NULL);
	last_b = b;
	//***
	ret = rts_run_md(md_handle, md_src, md_res);
	if (ret < 0) {
    	ret = -1;
    	return ret;
	}
	ret_pd = rts_run_pd(pd_handle, pd_src, &pd_res);
	if (ret_pd >= 0) {
		log_qcy(DEBUG_VERBOSE, "md_res fl<%d>\t nrobj<%d>", md_res->motion_flag, md_res->nr_obj);
		log_qcy(DEBUG_VERBOSE, "\t pd_fl <%d>\t nrpd<%d>\n", pd_res.pd_flag, pd_res.nr_pd);
		if (pd_res.pd_flag) {
			for (int i = 0; i < pd_res.nr_pd; i++) {
				log_qcy(DEBUG_VERBOSE, "conf[%d] %.2f %.2f\n",
					i,
					pd_res.pd_boxes->conf[0],
					pd_res.pd_boxes->conf[1]
					);
			}
			video3_spd_trigger_message();
		}
	}
	if( ctrl->mt_enable ) {
		if( !count )
			count = 2 + buffer_frames;
		count--;
		if( !count ) {
			ret_mt = rts_run_mt(mt_handle, md_res, mt_res);
			if (ret_mt >=0 ) {
				spd_motion_tracking(mt_res->mov_dis, &md_src, &buffer_frames);
				int motion_flag = md_res->motion_flag ||
						(mt_res->mov_dis.x != 0
						|| mt_res->mov_dis.y != 0);
				if (motion_flag == MD_STAT_MOTION) {
					log_qcy(DEBUG_VERBOSE, "motion_detected");
				}
				if( mt_res->mov_dis.x != 0 || mt_res->mov_dis.y != 0 ) {
					log_qcy(DEBUG_INFO, "motion tracking----x: %d, y: %d\n\n", mt_res->mov_dis.x, mt_res->mov_dis.y);
				}
			}
		}
	}
    return ret;
}

int video3_spd_init(video3_spd_config_t *ctrl, int channel, rts_md_src *md_src, rts_pd_src *pd_src)
{
	int ret = 0;
	char fname[MAX_SYSTEM_STRING_SIZE*2];
	//settings
	spd_set_md_default_parameter_spd(&md_cfg, ctrl->width, ctrl->height);
	memset(fname, 0, sizeof(fname));
	sprintf(fname, "%s%s", ctrl->file_path, SPD_WEIGHT_FILE_NAME);
	spd_set_pd_default_parameter(&pd_cfg, ctrl->width, ctrl->height, fname);
	spd_set_mt_default_parameter(&mt_cfg, ctrl->width, ctrl->height);
	//init
	md_handle = rts_init_md(&md_cfg);
	pd_handle = rts_init_pd(&pd_cfg);
	mt_handle = rts_init_mt(&mt_cfg);
	src_img = rts_create_rts_img(ctrl->width, ctrl->height, RTS_8U, 2,
				RTS_FORMAT_YUV420, NULL);
	if (!md_handle || !mt_handle || !pd_handle || !src_img) {
		ret = -1;
		video3_spd_release(-1);
		return ret;
	}
	md_res = rts_create_res(md_handle);
	mt_res = rts_create_res(mt_handle);
	if (!md_res || !mt_res ) {
		ret = -1;
		video3_spd_release(-1);
		return ret;
	}
	md_src->src_img = src_img;
	pd_src->src_img = src_img;
	pd_src->md_res = md_res;
	if( channel != -1) {
		ret = rts_av_start_recv(channel);
		if (ret) {
			log_qcy(DEBUG_SERIOUS, "start recv isp fail, ret = %d", ret);
			video3_spd_release(-1);
			return -1;
		}
	}
	return ret;
}

int video3_spd_release(int channel)
{
	if( channel != -1) {
		rts_av_stop_recv(channel);
	}
	rts_release_obj(&src_img);
	rts_release_handle(&md_handle);
	rts_release_handle(&pd_handle);
	rts_release_handle(&mt_handle);
	rts_release_obj(&md_res);
	rts_release_obj(&mt_res);
}
