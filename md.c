/*
 * md.c
 *
 *  Created on: Oct 2, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtsbmp.h>
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../tools/tools_interface.h"
#include "../../server/miio/miio_interface.h"
#include "../../server/recorder/recorder_interface.h"
#include "../../server/micloud/micloud_interface.h"
#include "../../server/video/video_interface.h"
//server header
#include "md.h"
#include "video3_interface.h"
#include "config.h"
#include "video3.h"

/*
 * static
 */

//variable
static video3_md_config_t	config;
static struct rts_video_md_attr *attr = NULL;
struct rts_video_md_result result;
static const int GRID_R = 72;
static const int GRID_C = 128;
static unsigned long long int last_report = 0;

//function
static int md_enable(int polling, int trig, unsigned int data_mode_mask, int width, int height, int sensitivity);
static int md_disable(void);

/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static int md_trigger_message(void)
{
	int ret = 0;
	recorder_init_t init;
	unsigned long long int now;
	if( config.cloud_report ) {
		now = time_get_now_stamp();
		if( config.alarm_interval < 1)
			config.alarm_interval = 1;
		if( ( now - last_report) >= config.alarm_interval * 60 ) {
			last_report = now;
			message_t msg;
			/********motion notification********/
			msg_init(&msg);
			msg.message = MICLOUD_EVENT_TYPE_OBJECTMOTION;
			msg.sender = msg.receiver = SERVER_VIDEO3;
			msg.extra = &now;
			msg.extra_size = sizeof(now);
			ret = manager_common_send_message(SERVER_MICLOUD, &msg);
			/********recorder********/
			msg_init(&msg);
			msg.message = MSG_RECORDER_ADD;
			msg.sender = msg.receiver = SERVER_VIDEO3;
			memset(&init, 0, sizeof(init));
			init.video_channel = 0;
			init.mode = RECORDER_MODE_BY_TIME;
			init.type = RECORDER_TYPE_MOTION_DETECTION;
			init.audio = 1;
			memset(init.start, 0, sizeof(init.start));
			time_get_now_str(init.start);
			now += config.recording_length;
			memset(init.stop, 0, sizeof(init.stop));
			time_stamp_to_date(now, init.stop);
			init.repeat = 0;
			init.repeat_interval = 0;
			init.quality = 0;
			msg.arg = &init;
			msg.arg_size = sizeof(recorder_init_t);
			msg.extra = &now;
			msg.extra_size = sizeof(now);
			ret = manager_common_send_message(SERVER_RECORDER,    &msg);
			/********snap shot********/
			msg_init(&msg);
			msg.sender = msg.receiver = SERVER_VIDEO;
			msg.arg_in.cat = 0;
			msg.arg_in.dog = 1;
			msg.arg_in.duck = 0;
			msg.arg_in.tiger = RTS_AV_CB_TYPE_ASYNC;
			msg.arg_in.chick = RECORDER_TYPE_MOTION_DETECTION;
			msg.message = MSG_VIDEO_SNAPSHOT;
			manager_common_send_message(SERVER_VIDEO, &msg);
			/**********************************************/
		}
	}
	return ret;
}

static int md_received(int idx, struct rts_video_md_result *result, void *priv)
{
	int ret = 0;
	if (!result)
		return -1;
	log_qcy(DEBUG_INFO, "motion data received\n");
	if(!result) {
		md_trigger_message();
	}
	return 0;
}

static int md_enable(int polling, int trig, unsigned int data_mode_mask, int width, int height, int sensitivity)
{
	int i;
	int enable = 0;
	int ret;
	for (i = 0; i < attr->number; i++) {
		struct rts_video_md_block *block = attr->blocks + i;
		unsigned int detect_mode;
		int len;
		if (trig)
			detect_mode = RTS_VIDEO_MD_DETECT_USER_TRIG;
		else
			detect_mode = RTS_VIDEO_MD_DETECT_HW;
		block->enable = 0;
		if (i > 0)
			continue;
		log_qcy(DEBUG_INFO, "%x %x %d\n",
				block->supported_data_mode,
				block->supported_detect_mode,
				block->supported_grid_num);
		data_mode_mask &= block->supported_data_mode;
		if (!RTS_CHECK_BIT(block->supported_detect_mode, detect_mode)) {
			log_qcy(DEBUG_SERIOUS, "detect mode %x is not support\n", detect_mode);
			continue;
		}
		if (GRID_R * GRID_C > block->supported_grid_num) {
			log_qcy(DEBUG_SERIOUS, "grid size (%d,%d) is out of range\n", GRID_R, GRID_C);
			continue;
		}
		len = RTS_DIV_ROUND_UP(GRID_R * GRID_C, 8);
		block->data_mode_mask = data_mode_mask;
		block->detect_mode = detect_mode;
		block->area.start.x = 0;
		block->area.start.y = 0;
		block->area.cell.width = width / GRID_C;
		block->area.cell.height = height / GRID_R;
		block->area.size.rows = GRID_R;
		block->area.size.columns = GRID_C;
		block->sensitivity = sensitivity;
		block->percentage = 30;
		block->frame_interval = 5;
		if (!polling) {
			block->ops.motion_received = md_received;
		}
		block->enable = 1;
		enable++;
	}
	ret = rts_av_set_isp_md(attr);
	if (ret)
		return ret;
	if (!enable)
		return -1;
	return 0;
}

static int md_disable(void)
{
	int i;
	for (i = 0; i < attr->number; i++) {
		struct rts_video_md_block *block = attr->blocks + i;
		block->enable = 0;
	}
	return rts_av_set_isp_md(attr);
}

/*
 * interface
 */

int video3_md_get_scheduler_time(char *input, scheduler_time_t *st, int *mode)
{
    char timestr[16];
    char start_hour_str[4]={0};
    char start_min_str[4]={0};
    char end_hour_str[4]={0};
    char end_min_str[4]={0};
    int start_hour = 0;
    int start_min = 0;
    int end_hour = 0;
    int end_min = 0;
    if(strlen(input) > 0) {
        memcpy(timestr,input,strlen(input));
		memcpy(start_hour_str,timestr,2);
        start_hour_str[2] = '\0';
		memcpy(start_min_str,timestr+3,2);
        start_min_str[2] = '\0';
        memcpy(end_hour_str,timestr+3+3,2);
        end_hour_str[2] = '\0';
		memcpy(end_min_str,timestr+3+3+3,2);
        end_min_str[2] = '\0';
        log_qcy(DEBUG_INFO, "time:%s:%s-%s:%s\n",start_hour_str,start_min_str,end_hour_str,end_min_str);
        start_hour =  atoi(start_hour_str);
        start_min =  atoi(start_min_str);
        end_hour =  atoi(end_hour_str);
        end_min =  atoi(end_min_str);

        if(!start_hour&&!start_min&&!end_hour&&!end_min){
            *mode = 0;
        }
        else {
        	*mode = 1;
			st->start_hour = start_hour;
			st->start_min = start_min;
			st->start_sec= 0;
            if(end_hour > start_hour) {
    			st->stop_hour = end_hour;
    			st->stop_min = end_min;
    			st->stop_sec= 0;
            }
            else if(end_hour == start_hour) {
                if(end_min > start_min) {
        			st->stop_hour = end_hour;
        			st->stop_min = end_min;
        			st->stop_sec= 0;
                }
                else if(end_min == start_min) {
                    *mode = 0;
        			st->stop_hour = end_hour;
        			st->stop_min = end_min;
        			st->stop_sec= 0;
                }
                else {
        			st->stop_hour = 23;
        			st->stop_min = 59;
        			st->stop_sec= 0;
                }
            }
            else {
    			st->stop_hour = 23;
    			st->stop_min = 59;
    			st->stop_sec= 0;
            }
        }
    }
    else
    	*mode = 0;
    return 0;
}

int video3_md_check_scheduler_time(scheduler_time_t *st, int *mode)
{
	int ret = 0;
    time_t timep;
    struct tm  tv={0};
    int	start, end, now;

	if( *mode==0 ) return 1;
    timep = time(NULL);
    localtime_r(&timep, &tv);
    start = st->start_hour * 3600 + st->start_min * 60 + st->start_sec;
    end = st->stop_hour * 3600 + st->stop_min * 60 + st->stop_sec;
    now = tv.tm_hour * 3600 + tv.tm_min * 60 + tv.tm_sec;
    if( now >= start && now <= end ) return 1;
    else if( now > end) return 2;
    return ret;
}

static int md_process_data(struct rts_video_md_result *result)
{
	unsigned int i;
	unsigned int bytesused = 0;
	if (!result)
		return -1;
	return 0;
}

int video3_md_proc(void)
{
	int ret;
	static int status = 0;
	if (!status) {
		if (config.trig) {
			ret = rts_av_trig_isp_md(attr, 0);
			if (ret)
				return 0;
			if (config.polling)
				status = 1;
		} else if (config.polling) {
			status = rts_av_check_isp_md_status(attr, 0);
		}
		return 0;
	}
	ret = rts_av_get_isp_md_result(attr, 0, &result);
	if (ret)
		return 0;
	log_qcy(DEBUG_INFO, "get data\n");
	ret = md_process_data(&result);
	if(!ret) {
		md_trigger_message();
	}
	status = 0;
	return 0;
}

int video3_md_init(video3_md_config_t *md_config, int width, int height)
{
	int ret = 0;
	int mask;
	memcpy(&config, md_config, sizeof(video3_md_config_t) );
	ret = rts_av_query_isp_md(&attr, width, height);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "query isp md attr fail, ret = %d\n", ret);
		video3_md_release();
	}
	mask = RTS_VIDEO_MD_DATA_TYPE_AVGY |
           RTS_VIDEO_MD_DATA_TYPE_RLTPRE |
           RTS_VIDEO_MD_DATA_TYPE_RLTCUR |
           RTS_VIDEO_MD_DATA_TYPE_BACKY |
           RTS_VIDEO_MD_DATA_TYPE_BACKF |
           RTS_VIDEO_MD_DATA_TYPE_BACKC;
	ret = md_enable(config.polling, config.trig, mask, width, height, config.sensitivity);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "enable md fail\n");
		video3_md_release();
	}
	if (config.polling) {
		unsigned int mask = attr->blocks->data_mode_mask;
		rts_av_init_md_result(&result, mask);
		log_qcy(DEBUG_INFO, "%d\n", result.count);
	}
	return ret;
}

int video3_md_release(void)
{
	if( config.polling)
		rts_av_uninit_md_result(&result);
	if( attr )
		md_disable();
	RTS_SAFE_RELEASE(attr, rts_av_release_isp_md);
}
