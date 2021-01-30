/*
 * video3.c
 *
 *  Created on: Aug 27, 2020
 *      Author: ning
 */

/*
 * header
 */
//system header
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <malloc.h>
#include <miss.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../manager/manager_interface.h"
#include "../../server/realtek/realtek_interface.h"
#include "../../tools/tools_interface.h"
#include "../../server/miss/miss_interface.h"
#include "../../server/miio/miio_interface.h"
#include "../../server/recorder/recorder_interface.h"
#include "../../server/device/device_interface.h"
#include "../../server/micloud/micloud_interface.h"
#include "../../server/video/video_interface.h"
//server header
#include "video3.h"
#include "config.h"
#include "md.h"
#include "spd.h"
#include "video3_interface.h"
/*
 * static
 */
//variable
static 	message_buffer_t	message;
static 	server_info_t 		info;
static	video3_motion_run_t	md_run;
static	video3_motion_run_t	spd_run;
static	video_stream_t		stream={-1,-1,-1,-1};
static	video3_config_t		config;
static	pthread_mutex_t		mutex = PTHREAD_MUTEX_INITIALIZER;
static	pthread_cond_t		cond = PTHREAD_COND_INITIALIZER;
static  pthread_rwlock_t	ilock = PTHREAD_RWLOCK_INITIALIZER;
static	long long int		last_report = 0;
static 	int					motor_ready = 0;

//function
//common
static void *server_func(void);
static int server_message_proc(void);
static void server_release_1(void);
static void server_release_2(void);
static void server_release_3(void);
static int server_restart(void);
static void task_default(void);
static void task_start(void);
static void task_stop(void);
static void task_exit(void);
static void server_thread_termination(int sign);
//specific
static int stream_init(void);
static int stream_destroy(void);
static int stream_start(void);
static int stream_stop(void);
static int video3_init(void);
static int md_init_scheduler(void);
static int spd_check_scheduler(void);
static int md_check_scheduler(void);
/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */

/*
 * helper
 */
static int video3_check_sd(void)
{
	if( access("/mnt/media/normal", F_OK) ) {
		log_qcy(DEBUG_INFO, "SD card access failed!, quit all snapshot!----");
		return -1;
	}
	else
		return 0;
}

static int video3_get_property(message_t *msg)
{
	int ret = 0, st;
	int temp;
	message_t send_msg;
    /********message body********/
	msg_init(&send_msg);
	memcpy(&(send_msg.arg_pass), &(msg->arg_pass),sizeof(message_arg_t));
	send_msg.message = msg->message | 0x1000;
	send_msg.sender = send_msg.receiver = SERVER_VIDEO3;
	send_msg.arg_in.cat = msg->arg_in.cat;
	send_msg.result = 0;
	/****************************/
	if( send_msg.arg_in.cat == VIDEO3_PROPERTY_MOTION_SWITCH) {
		send_msg.arg = (void*)(&config.md.enable);
		send_msg.arg_size = sizeof(config.md.enable);
	}
	else if( send_msg.arg_in.cat == VIDEO3_PROPERTY_MOTION_TRACKING_SWITCH) {
		send_msg.arg = (void*)(&config.spd.mt_enable);
		send_msg.arg_size = sizeof(config.spd.mt_enable);
	}
	else if( send_msg.arg_in.cat == VIDEO3_PROPERTY_MOTION_ALARM_INTERVAL) {
		send_msg.arg = (void*)(&config.md.alarm_interval);
		send_msg.arg_size = sizeof(config.md.alarm_interval);
	}
	else if( send_msg.arg_in.cat == VIDEO3_PROPERTY_MOTION_SENSITIVITY) {
		send_msg.arg = (void*)(&config.md.sensitivity);
		send_msg.arg_size = sizeof(config.md.sensitivity);
	}
	else if( send_msg.arg_in.cat == VIDEO3_PROPERTY_MOTION_START) {
		send_msg.arg = (void*)(config.md.start);
		send_msg.arg_size = strlen(config.md.start) + 1;
	}
	else if( send_msg.arg_in.cat == VIDEO3_PROPERTY_MOTION_END) {
		send_msg.arg = (void*)(config.md.end);
		send_msg.arg_size = strlen(config.md.end) + 1;
	}
	else if( send_msg.arg_in.cat == VIDEO3_PROPERTY_CUSTOM_WARNING_PUSH) {
		send_msg.arg = (void*)(&config.md.cloud_report);
		send_msg.arg_size = sizeof(config.md.cloud_report);
	}
	ret = manager_common_send_message( msg->receiver, &send_msg);
	return ret;
}

static int video3_set_property(message_t *msg)
{
	int ret= 0, mode = -1;
	message_t send_msg;
	int temp;
    /********message body********/
	msg_init(&send_msg);
	memcpy(&(send_msg.arg_pass), &(msg->arg_pass),sizeof(message_arg_t));
	send_msg.message = msg->message | 0x1000;
	send_msg.sender = send_msg.receiver = SERVER_VIDEO3;
	send_msg.arg_in.cat = msg->arg_in.cat;
	send_msg.arg_in.wolf = 0;
	/****************************/
	if( msg->arg_in.cat == VIDEO3_PROPERTY_MOTION_SWITCH ) {
		temp = *((int*)(msg->arg));
		if( temp != config.md.enable ) {
			config.md.enable = temp;
			config.spd.enable = temp;
//			config.spd.mt_enable = temp;
			log_qcy(DEBUG_INFO, "changed the motion switch = %d", config.md.enable);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
			video3_config_video_set(CONFIG_VIDEO3_SPD, &config.spd);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
		}
	}
	if( msg->arg_in.cat == VIDEO3_PROPERTY_MOTION_TRACKING_SWITCH ) {
		temp = *((int*)(msg->arg));
		if( temp != config.spd.mt_enable ) {
			config.spd.mt_enable = temp;
			log_qcy(DEBUG_INFO, "changed the motion tracking switch = %d", config.spd.mt_enable);
			video3_config_video_set(CONFIG_VIDEO3_SPD, &config.spd);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
		}
	}
	else if( msg->arg_in.cat == VIDEO3_PROPERTY_MOTION_ALARM_INTERVAL ) {
		temp = *((int*)(msg->arg));
		if( temp != config.md.alarm_interval ) {
			config.md.alarm_interval = temp;
			config.spd.alarm_interval = temp;
			log_qcy(DEBUG_INFO, "changed the motion detection alarm interval = %d", config.md.alarm_interval);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
		}
	}
	else if( msg->arg_in.cat == VIDEO3_PROPERTY_MOTION_SENSITIVITY ) {
		temp = *((int*)(msg->arg));
		if( temp != config.md.sensitivity ) {
			config.md.sensitivity = temp;
			log_qcy(DEBUG_INFO, "changed the motion detection sensitivity = %d", config.md.sensitivity);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
			md_run.changed = 1;
		}
	}
	else if( msg->arg_in.cat == VIDEO3_PROPERTY_MOTION_START ) {
		if( strcmp(config.md.start, (char*)(msg->arg)) ) {
			strcpy( config.md.start, (char*)(msg->arg) );
			log_qcy(DEBUG_INFO, "changed the motion detection start = %s", config.md.start);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
			md_init_scheduler();
			send_msg.arg_in.wolf = 1;
			send_msg.arg = config.md.start;
			send_msg.arg_size = sizeof(config.md.start);
		}
	}
	else if( msg->arg_in.cat == VIDEO3_PROPERTY_MOTION_END ) {
		if( strcmp(config.md.end, (char*)(msg->arg)) ) {
			strcpy( config.md.end, (char*)(msg->arg) );
			log_qcy(DEBUG_INFO, "changed the motion detection end = %s", config.md.end);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
			md_init_scheduler();
			send_msg.arg_in.wolf = 1;
			send_msg.arg = config.md.end;
			send_msg.arg_size = sizeof(config.md.end);
		}
	}
	else if( msg->arg_in.cat == VIDEO3_PROPERTY_CUSTOM_WARNING_PUSH ) {
		temp = *((int*)(msg->arg));
		if( temp != config.md.cloud_report ) {
			config.md.cloud_report = temp;
			config.spd.cloud_report = temp;
			log_qcy(DEBUG_INFO, "changed the motion detection cloud push = %d", config.md.cloud_report);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
			send_msg.arg_in.wolf = 1;
			send_msg.arg = &temp;
			send_msg.arg_size = sizeof(temp);
		}
	}
	/***************************/
	send_msg.result = ret;
	ret = manager_common_send_message(msg->receiver, &send_msg);
	/***************************/
	return ret;
}

static void video3_mjpeg_func(void *priv, struct rts_av_profile *profile, struct rts_av_buffer *buffer)
{
    FILE *pfile = NULL;
    if( buffer->vm_addr == NULL ) {
		log_qcy(DEBUG_WARNING, "video3 JPEG buffer NULL error!");
		return;
    }
//    if( buffer->bytesused > VIDEO3_MAX_JPEG_SIZE ) {
//		log_qcy(DEBUG_WARNING, "video3 JPEG size %d bigger than limit!", buffer->bytesused);
//		return;
//    }
    pfile = fopen((char*)priv, "wb");
    if (!pfile) {
		log_qcy(DEBUG_WARNING, "open video3 jpg snapshot file %s fail\n", (char*)priv);
		return;
    }
    log_qcy(DEBUG_INFO, "---+++VIDEO3 jpeg function, file pt= %p, path = %s, data-addr=%p, size=%d", pfile, (char*)priv,
    		buffer->vm_addr, buffer->bytesused);
//    if( !video3_check_sd() )
   	fwrite(buffer->vm_addr, 1, buffer->bytesused, pfile);
    log_qcy(DEBUG_INFO, "---+++VIDEO3 jpeg function, write success!");
    RTS_SAFE_RELEASE(pfile, fclose);
    log_qcy(DEBUG_INFO, "---+++VIDEO3 jpeg function, clean success!");
    return;
}

static int video3_snapshot(message_t *msg)
{
	struct rts_av_callback cb;
    static char filename[4*MAX_SYSTEM_STRING_SIZE];
	int ret = 0;
	cb.func = video3_mjpeg_func;
	cb.start = msg->arg_in.cat;
	cb.times = msg->arg_in.dog;
	cb.interval = msg->arg_in.duck;
	cb.type = msg->arg_in.tiger;
	if( msg->arg_in.chick == RECORDER_TYPE_MOTION_DETECTION ) {
		memset( filename, 0, sizeof(filename) );
		sprintf( filename, "%smotion.jpg", config.jpg.image_path );
	}
	else if( msg->arg_in.chick == RECORDER_TYPE_HUMAN_DETECTION ) {
		memset( filename, 0, sizeof(filename) );
		sprintf( filename, "%smotion.jpg", config.jpg.image_path );
	}
	else {
		memset(filename, 0, sizeof(filename));
		strcpy(filename, (char*)msg->arg);
	}
	cb.priv = (void*)filename;
	ret = rts_av_set_callback(stream.jpg, &cb, 0);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "set mjpeg callback fail, ret = %d\n", ret);
		return ret;
	}
	return ret;
}

static int *video3_md_func(void *arg)
{
	video3_md_config_t ctrl;
	int st;
	int init = 0;
	char fname[MAX_SYSTEM_STRING_SIZE];
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    sprintf(fname, "md-%d",time_get_now_stamp());
    misc_set_thread_name(fname);
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl, (video3_md_config_t*)arg, sizeof(video3_md_config_t) );
    misc_set_bit(&info.thread_start, THREAD_MD, 1 );
    manager_common_send_dummy(SERVER_VIDEO3);
    log_qcy(DEBUG_INFO, "video3 object detection thread init success!");
    while( 1 ) {
    	st = info.status;
    	if( info.exit )
    		break;
    	if( misc_get_bit(info.thread_exit, THREAD_MD) )
    		break;
    	if(  (st != STATUS_IDLE) && (st != STATUS_START) && (st != STATUS_RUN) )
    		break;
    	else if( (st == STATUS_START) || (st == STATUS_IDLE) )
    		continue;
    	if( !init ) {
    	    video3_md_init( &ctrl, 1920, 1080);
    	    init = 1;
    	}
 		video3_md_proc();
 		usleep(100*1000);
    }
    //release
    video3_md_release();
    misc_set_bit(&info.thread_start, THREAD_MD, 0 );
    misc_set_bit( &info.thread_exit, THREAD_MD, 0);
    manager_common_send_dummy(SERVER_VIDEO3);
    log_qcy(DEBUG_INFO, "-----------thread exit: %s-----------",fname);
    pthread_exit(0);
}

static void *video3_spd_func(void *arg)
{
	int st;
	int ret;
	int init = 0;
	video3_spd_config_t ctrl;
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
	char fname[MAX_SYSTEM_STRING_SIZE];
    sprintf(fname, "spd-%d",time_get_now_stamp());
    misc_set_thread_name(fname);
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl, (video3_spd_config_t*)arg, sizeof(video3_spd_config_t) );
    misc_set_bit(&info.thread_start, THREAD_SPD, 1 );
    manager_common_send_dummy(SERVER_VIDEO3);
    log_qcy(DEBUG_INFO, "video3 human detection thread init success!");
    while( 1 ) {
    	ctrl.mt_enable = config.spd.mt_enable;
    	st = info.status;
    	if( info.exit )
    		break;
    	if( misc_get_bit(info.thread_exit, THREAD_SPD) )
    		break;
    	if(  (st != STATUS_IDLE) && (st != STATUS_START) && (st != STATUS_RUN) )
    		break;
    	else if( (st == STATUS_START) || (st == STATUS_IDLE) )
    		continue;
    	if( !init ) {
    	    ret = video3_spd_init( &ctrl, stream.isp);
    	    if( ret ) {
    	    	log_qcy(DEBUG_INFO, "video3 human detection thread init failed!");
    	    	break;
    	    }
    	    init = 1;
    	}
    	video3_spd_proc( &ctrl, stream.isp);
    	usleep(1000*1000 / config.profile.profile.video.denominator);	//
    }
    //release
    video3_spd_release(stream.isp);
    misc_set_bit( &info.thread_start, THREAD_SPD, 0 );
    misc_set_bit( &info.thread_exit, THREAD_SPD, 0);
exit:
    manager_common_send_dummy(SERVER_VIDEO3);
    log_qcy(DEBUG_INFO, "-----------thread exit: %s-----------",fname);
    pthread_exit(0);
}

static int stream_init(void)
{
	stream.isp = -1;
	stream.jpg = -1;
}

static int stream_destroy(void)
{
	int ret = 0;
	if (stream.isp >= 0) {
		rts_av_destroy_chn(stream.isp);
		stream.isp = -1;
	}
	if (stream.jpg >= 0) {
		rts_av_destroy_chn(stream.jpg);
		stream.jpg = -1;
	}
	return ret;
}

static int stream_start(void)
{
	int ret=0;
	if( stream.isp != -1 ) {
		ret = rts_av_enable_chn(stream.isp);
		if (ret) {
			log_qcy(DEBUG_SERIOUS, "enable isp fail, ret = %d", ret);
			return -1;
		}
	}
	else {
		return -1;
	}
	if( stream.jpg != -1 ) {
		ret = rts_av_enable_chn(stream.jpg);
		if (ret) {
			log_qcy(DEBUG_SERIOUS, "enable jpg fail, ret = %d", ret);
			return -1;
		}
	}
	else {
		return -1;
	}
//    ret = rts_av_start_recv(stream.isp);
//    if (ret) {
//    	log_qcy(DEBUG_SERIOUS, "start recv isp fail, ret = %d", ret);
//    	return -1;
//    }
    return 0;
}

static int stream_stop(void)
{
	int ret=0;
//	if(stream.isp!=-1)
//		ret = rts_av_stop_recv(stream.isp);
//	if(stream.jpg!=-1)
//		ret = rts_av_disable_chn(stream.jpg);
//	if(stream.isp!=-1)
//		ret = rts_av_disable_chn(stream.isp);
	return ret;
}

static int md_init_scheduler(void)
{
	int ret = 0;
	char final[MAX_SYSTEM_STRING_SIZE];
	memset(final, 0, MAX_SYSTEM_STRING_SIZE);
	sprintf(final, "%s-%s", config.md.start, config.md.end);
	ret = video3_md_get_scheduler_time(final, &md_run.scheduler, &md_run.mode);
	return ret;
}

static void motion_check_scheduler(void)
{
	message_t msg;
	//get from device
	if(!motor_ready)
	{
		msg_init(&msg);
		msg.message = MSG_DEVICE_PROPERTY_GET;
		msg.sender = msg.receiver = SERVER_VIDEO3;
		msg.arg_pass.cat = DEVICE_ACTION_MOTO_STATUS;
		manager_common_send_message(SERVER_DEVICE, &msg);
		/****************************/
	}
	md_check_scheduler();
	spd_check_scheduler();
}

static int md_check_scheduler(void)
{
	int ret;
	message_t msg;
	pthread_t md_id, spd_id;
	if( config.md.enable && motor_ready) {
		ret = video3_md_check_scheduler_time(&md_run.scheduler, &md_run.mode);
		if( ret==1 ) {
			if( !md_run.started && !misc_get_bit(info.thread_start, THREAD_MD) ) {
				//start the md thread
				md_run.started = 1;
				ret = pthread_create(&md_id, NULL, video3_md_func, (void*)&config.md);
				if( !ret ) {
					log_qcy(DEBUG_INFO, "md thread create successful!");
					/********message body********/
					msg_init(&msg);
					msg.message = MSG_VIDEO3_START;
					msg.sender = msg.receiver = SERVER_VIDEO3;
					msg.arg_in.wolf = VIDEO3_RUN_MODE_MD_HARD;
					manager_common_send_message(SERVER_VIDEO3, &msg);
					/****************************/
				}
				else {
					log_qcy(DEBUG_SERIOUS, "md thread create error! ret = %d",ret);
					goto stop_md;
				}
			}
			if( md_run.changed ) {
				md_run.changed = 0;
				goto stop_md;
			}
		}
		else {
			if( misc_get_bit(info.thread_start, THREAD_MD) ) {
				goto stop_md;
			}
		}
	}
	else {
		if( misc_get_bit(info.thread_start, THREAD_MD)) {
			goto stop_md;
		}
	}
	return ret;
stop_md:
	md_run.started = 0;
	misc_set_bit( &info.thread_exit, THREAD_MD, 1);
	/********message body********/
	msg_init(&msg);
	msg.message = MSG_VIDEO3_STOP;
	msg.sender = msg.receiver = SERVER_VIDEO3;
	msg.arg_in.wolf = VIDEO3_RUN_MODE_MD_HARD;
	manager_common_send_message(SERVER_VIDEO3, &msg);
	/****************************/
	return ret;
}

static int spd_check_scheduler(void)
{
	int ret;
	message_t msg;
	pthread_t spd_id;
	if( config.spd.enable && motor_ready) {
		ret = video3_md_check_scheduler_time(&md_run.scheduler, &md_run.mode);
		if( ret==1 ) {
			if( !spd_run.started && !misc_get_bit(info.thread_start, THREAD_SPD) ) {
				spd_run.started = 1;
				ret = pthread_create(&spd_id, NULL, video3_spd_func, (void*)&config.spd);
				if( !ret ) {
					log_qcy(DEBUG_INFO, "spd thread create successful!");
					/********message body********/
					msg_init(&msg);
					msg.message = MSG_VIDEO3_START;
					msg.sender = msg.receiver = SERVER_VIDEO3;
					msg.arg_in.wolf = VIDEO3_RUN_MODE_SPD;
					manager_common_send_message(SERVER_VIDEO3, &msg);
					/****************************/
				}
				else {
					log_qcy(DEBUG_SERIOUS, "spd thread create error! ret = %d",ret);
					goto stop_spd;
				}
			}
			if( spd_run.changed ) {
				spd_run.changed = 0;
				goto stop_spd;
			}
		}
		else {
			if( misc_get_bit(info.thread_start, THREAD_SPD)) {
				goto stop_spd;
			}
		}
	}
	else {
		if( misc_get_bit(info.thread_start, THREAD_SPD) ) {
			goto stop_spd;
		}
	}
	return ret;
stop_spd:
	spd_run.started = 0;
	misc_set_bit( &info.thread_exit, THREAD_SPD, 1);
	/********message body********/
	msg_init(&msg);
	msg.message = MSG_VIDEO3_STOP;
	msg.sender = msg.receiver = SERVER_VIDEO3;
	msg.arg_in.wolf = VIDEO3_RUN_MODE_SPD;
	manager_common_send_message(SERVER_VIDEO3, &msg);
	/****************************/
	return ret;
}

static int video3_init(void)
{
	int ret;
	stream_init();
	stream.isp = rts_av_create_isp_chn(&config.isp);
	if (stream.isp < 0) {
		log_qcy(DEBUG_SERIOUS, "fail to create isp chn, ret = %d", stream.isp);
		return -1;
	}
	log_qcy(DEBUG_INFO, "isp chnno:%d", stream.isp);
	config.profile.profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	ret = rts_av_set_profile(stream.isp, &config.profile.profile);
	if (ret) {
		log_qcy(DEBUG_SERIOUS, "set isp profile fail, ret = %d", ret);
		return -1;
	}
	//jpg
   	stream.jpg = rts_av_create_mjpeg_chn(&config.jpg.jpg_ctrl);
    if (stream.jpg < 0) {
    	log_qcy(DEBUG_SERIOUS, "fail to create jpg chn, ret = %d\n", stream.jpg);
        return -1;
    }
    log_qcy(DEBUG_INFO, "jpg chnno:%d stream.isp:%d", stream.jpg, stream.isp);
    ret = rts_av_bind(stream.isp, stream.jpg);
   	if (ret) {
   		log_qcy(DEBUG_SERIOUS, "fail to bind isp and jpg, ret %d", ret);
   		return -1;
   	}
   	md_init_scheduler();
	return 0;
}


static void server_thread_termination(int sign)
{
    /********message body********/
	message_t msg;
	msg_init(&msg);
	msg.message = MSG_VIDEO3_SIGINT;
	msg.sender = msg.receiver = SERVER_VIDEO3;
	manager_common_send_message(SERVER_MANAGER, &msg);
	/****************************/
}

static void video3_broadcast_thread_exit(void)
{
}

static void server_release_1(void)
{
	message_t msg;
	stream_stop();
	stream_destroy();
	usleep(1000*10);
	/********message body********/
	msg_init(&msg);
	msg.message = MSG_MANAGER_TIMER_REMOVE;
	msg.sender = msg.receiver = SERVER_VIDEO3;
	msg.arg_in.handler = motion_check_scheduler;
	manager_common_send_message(SERVER_MANAGER, &msg);
	/****************************/
}

static void server_release_2(void)
{
	msg_buffer_release2(&message, &mutex);
	memset(&config,0,sizeof(video3_config_t));
	memset(&stream,0,sizeof(video_stream_t));
	memset(&md_run,0,sizeof(video3_motion_run_t));
	memset(&spd_run,0,sizeof(video3_motion_run_t));
}

static void server_release_3(void)
{
	msg_free(&info.task.msg);
	memset(&info, 0, sizeof(server_info_t));
}

/*
 *
 */
static int video3_message_block(void)
{
	int ret = 0;
	int id = -1, id1, index = 0;
	message_t msg;
	//search for unblocked message and swap if necessory
	if( !info.msg_lock ) {
		log_qcy(DEBUG_VERBOSE, "===video3 message block, return 0 when first message is msg_lock=0");
		return 0;
	}
	index = 0;
	msg_init(&msg);
	ret = msg_buffer_probe_item(&message, index, &msg);
	if( ret ) {
		log_qcy(DEBUG_VERBOSE, "===video3 message block, return 0 when first message is empty");
		return 0;
	}
	if( msg_is_system(msg.message) || msg_is_response(msg.message) ) {
		log_qcy(DEBUG_VERBOSE, "===video3 message block, return 0 when first message is system or response message %x", msg.message);
		return 0;
	}
	id = msg.message;
	do {
		index++;
		msg_init(&msg);
		ret = msg_buffer_probe_item(&message, index, &msg);
		if(ret) {
			log_qcy(DEBUG_VERBOSE, "===video3 message block, return 1 when message index = %d is not found!", index);
			return 1;
		}
		if( msg_is_system(msg.message) ||
				msg_is_response(msg.message) ) {	//find one behind system or response message
			msg_buffer_swap(&message, 0, index);
			id1 = msg.message;
			log_qcy(DEBUG_INFO, "video3: swapped message happend, message %x was swapped with message %x", id, id1);
			return 0;
		}
	}
	while(!ret);
	return ret;
}

static int video3_message_filter(message_t  *msg)
{
	int ret = 0;
	if( info.task.func == task_exit) { //only system message
		if( !msg_is_system(msg->message) && !msg_is_response(msg->message) )
			return 1;
	}
	return ret;
}

static int server_message_proc(void)
{
	int ret = 0, ret1 = 0;
	message_t msg;
//condition
	pthread_mutex_lock(&mutex);
	if( message.head == message.tail ) {
		if( info.status == info.old_status	)
			pthread_cond_wait(&cond,&mutex);
	}
	if( video3_message_block() ) {
		pthread_mutex_unlock(&mutex);
		return 0;
	}
	msg_init(&msg);
	ret = msg_buffer_pop(&message, &msg);
	pthread_mutex_unlock(&mutex);
	if( ret == 1)
		return 0;
	if( video3_message_filter(&msg) ) {
		msg_free(&msg);
		log_qcy(DEBUG_VERBOSE, "VIDEO3 message--- sender=%d, message=%x, ret=%d, head=%d, tail=%d was screened, the current task is %p", msg.sender, msg.message,
				ret, message.head, message.tail, info.task.func);
		return -1;
	}
	log_qcy(DEBUG_VERBOSE, "-----pop out from the VIDEO3 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg.sender, msg.message,
			ret, message.head, message.tail);
	switch(msg.message) {
		case MSG_VIDEO3_START:
			break;
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.func = task_start;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO3_STOP:
			break;
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.msg.arg_in.cat = info.status2;
			if( info.task.msg.sender == SERVER_RECORDER)
				misc_set_bit(&info.task.msg.arg_in.cat, VIDEO3_RUN_MODE_SNAP, 0);
			else
				misc_set_bit(&info.task.msg.arg_in.cat, msg.arg_in.wolf, 0);
			info.task.func = task_stop;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO3_PROPERTY_SET_DIRECT:
			video3_set_property(&msg);
			break;
		case MSG_VIDEO3_PROPERTY_GET:
			ret = video3_get_property(&msg);
			break;
		case MSG_MANAGER_EXIT:
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.func = task_exit;
			info.status = EXIT_INIT;
			info.msg_lock = 0;
			break;
		case MSG_MANAGER_TIMER_ACK:
			((HANDLER)msg.arg_in.handler)();
			break;
		case MSG_MIIO_PROPERTY_NOTIFY:
		case MSG_MIIO_PROPERTY_GET_ACK:
			if( msg.arg_in.cat == MIIO_PROPERTY_TIME_SYNC ) {
				if( msg.arg_in.dog == 1 )
					misc_set_bit( &info.init_status, VIDEO3_INIT_CONDITION_MIIO_TIME, 1);
			}
			break;
		case MSG_REALTEK_PROPERTY_NOTIFY:
		case MSG_REALTEK_PROPERTY_GET_ACK:
			if( msg.arg_in.cat == REALTEK_PROPERTY_AV_STATUS ) {
				if( msg.arg_in.dog == 1 )
					misc_set_bit(&info.init_status, VIDEO3_INIT_CONDITION_REALTEK, 1);
			}
			break;
		case MSG_MANAGER_EXIT_ACK:
			misc_set_bit(&info.error, msg.sender, 0);
			break;
		case MSG_MANAGER_DUMMY:
			break;
		case MSG_MISS_BUFFER_FULL:
			break;
		case MSG_VIDEO3_SNAPSHOT:
			video3_snapshot(&msg);
			break;
		case MSG_DEVICE_PROPERTY_GET_ACK:
			if(msg.arg_pass.cat == DEVICE_ACTION_MOTO_STATUS) {
				if(msg.arg_in.dog == 1)
					motor_ready = 1;
			}
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "not processed message = %x", msg.message);
			break;
	}
	msg_free(&msg);
	return ret;
}

/*
 *
 */
static int server_none(void)
{
	int ret = 0;
	message_t msg;
	if( !misc_get_bit( info.init_status, VIDEO3_INIT_CONDITION_CONFIG ) ) {
		ret = video3_config_video_read(&config);
		if( !ret && misc_full_bit( config.status, CONFIG_VIDEO3_MODULE_NUM) ) {
			misc_set_bit(&info.init_status, VIDEO3_INIT_CONDITION_CONFIG, 1);
			config.spd.alarm_interval = config.md.alarm_interval;
			config.spd.cloud_report = config.md.cloud_report;
			config.spd.recording_length = config.md.recording_length;
			config.spd.width = config.profile.profile.video.width;
			config.spd.height = config.profile.profile.video.height;
		}
		else {
			info.status = STATUS_ERROR;
			return -1;
		}
	}
	if( !misc_get_bit( info.init_status, VIDEO3_INIT_CONDITION_REALTEK ) ) {
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_REALTEK_PROPERTY_GET;
		msg.sender = msg.receiver = SERVER_VIDEO3;
		msg.arg_in.cat = REALTEK_PROPERTY_AV_STATUS;
		manager_common_send_message(SERVER_REALTEK,    &msg);
		/****************************/
		usleep(MESSAGE_RESENT_SLEEP);
	}
	if( !misc_get_bit( info.init_status, VIDEO3_INIT_CONDITION_MIIO_TIME)) {
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_MIIO_PROPERTY_GET;
		msg.sender = msg.receiver = SERVER_VIDEO3;
		msg.arg_in.cat = MIIO_PROPERTY_TIME_SYNC;
		ret = manager_common_send_message(SERVER_MIIO, &msg);
		/***************************/
		usleep(MESSAGE_RESENT_SLEEP);
	}
	if( misc_full_bit( info.init_status, VIDEO3_INIT_CONDITION_NUM ) ) {
		info.status = STATUS_WAIT;
	}
	return ret;
}

static int server_setup(void)
{
	int ret = 0;
	message_t msg;
	if( video3_init() == 0) {
		info.status = STATUS_IDLE;
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_TIMER_ADD;
		msg.sender = SERVER_VIDEO3;
		msg.arg_in.cat = 1000;
		msg.arg_in.dog = 0;
		msg.arg_in.duck = 0;
		msg.arg_in.handler = &motion_check_scheduler;
		manager_common_send_message(SERVER_MANAGER, &msg);
		/****************************/
	}
	else
		info.status = STATUS_ERROR;
	return ret;
}

static int server_start(void)
{
	int ret = 0;
	if( stream_start()==0 )
		info.status = STATUS_RUN;
	else
		info.status = STATUS_ERROR;
	return ret;
}

static int server_stop(void)
{
	int ret = 0;
	if( stream_stop()==0 )
		info.status = STATUS_IDLE;
	else
		info.status = STATUS_ERROR;
	return ret;
}

static int server_restart(void)
{
	int ret = 0;
	stream_stop();
	stream_destroy();
	return ret;
}
/*
 * task
 */
/*
 * task start: idle->start
 */
static void task_start(void)
{
	message_t msg;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO3;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			info.status = STATUS_START;
			break;
		case STATUS_START:
			server_start();
			break;
		case STATUS_RUN:
			goto exit;
			break;
		case STATUS_ERROR:
			msg.result = -1;
			goto exit;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_start = %d", info.status);
			break;
	}
	return;
exit:
	if( msg.result == 0 ) {
		if( info.task.msg.sender == SERVER_RECORDER)
			misc_set_bit(&info.status2, VIDEO3_RUN_MODE_SNAP, 1);
		else
			misc_set_bit(&info.status2, info.task.msg.arg_in.wolf, 1);
	}
	manager_common_send_message(info.task.msg.receiver, &msg);
	msg_free(&info.task.msg);
	info.task.func = &task_default;
	info.msg_lock = 0;
	return;
}
/*
 * task start: run->stop->idle
 */
static void task_stop(void)
{
	message_t msg;
	/**************************/
	msg_init(&msg);
	memcpy(&(msg.arg_pass), &(info.task.msg.arg_pass),sizeof(message_arg_t));
	msg.message = info.task.msg.message | 0x1000;
	msg.sender = msg.receiver = SERVER_VIDEO3;
	msg.result = 0;
	/***************************/
	switch(info.status){
		case STATUS_NONE:
		case STATUS_WAIT:
		case STATUS_SETUP:
			goto exit;
			break;
		case STATUS_IDLE:
			if( info.thread_start == 0 )
				goto exit;
			break;
		case STATUS_START:
		case STATUS_RUN:
			if( (info.task.msg.arg_in.cat > 0) ||
					(!info.task.msg.arg_in.duck && (info.task.msg.sender == SERVER_RECORDER) ) ) {	//real stop == 0
				goto exit;
				break;
			}
			else
				server_stop();
			break;
		case STATUS_ERROR:
			msg.result = -1;
			goto exit;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_stop = %d", info.status);
			break;
	}
	return;
exit:
	if( msg.result==0 ) {
		if( info.task.msg.sender == SERVER_RECORDER)
			misc_set_bit(&info.status2, VIDEO3_RUN_MODE_SNAP, 0);
		else
			misc_set_bit(&info.status2, info.task.msg.arg_in.wolf, 0);
	}
	manager_common_send_message(info.task.msg.receiver, &msg);
	msg_free(&info.task.msg);
	info.task.func = &task_default;
	info.msg_lock = 0;
	return;
}
/*
 * task
 */
/*
 * default exit: *->exit
 */
static void task_exit(void)
{
	switch( info.status ){
		case EXIT_INIT:
			log_qcy(DEBUG_INFO,"VIDEO3: switch to exit task!");
			if( info.task.msg.sender == SERVER_MANAGER) {
				info.error = VIDEO3_EXIT_CONDITION;
				info.error &= (info.task.msg.arg_in.cat);
			}
			else {
				info.error = 0;
			}
			info.status = EXIT_SERVER;
			break;
		case EXIT_SERVER:
			if( !info.error )
				info.status = EXIT_STAGE1;
			break;
		case EXIT_STAGE1:
			server_release_1();
			info.status = EXIT_THREAD;
			break;
		case EXIT_THREAD:
			info.thread_exit = info.thread_start;
			video3_broadcast_thread_exit();
			if( !info.thread_start )
				info.status = EXIT_STAGE2;
			break;
			break;
		case EXIT_STAGE2:
			server_release_2();
			info.status = EXIT_FINISH;
			break;
		case EXIT_FINISH:
			info.exit = 1;
		    /********message body********/
			message_t msg;
			msg_init(&msg);
			msg.message = MSG_MANAGER_EXIT_ACK;
			msg.sender = SERVER_VIDEO3;
			manager_common_send_message(SERVER_MANAGER, &msg);
			/***************************/
			info.status = STATUS_NONE;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_exit = %d", info.status);
			break;
		}
	return;
}

/*
 * default task: none->run
 */
static void task_default(void)
{
	switch( info.status ){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			info.status = STATUS_SETUP;
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			info.status = STATUS_START;
			break;
		case STATUS_START:
			server_start();
			break;
		case STATUS_RUN:
			break;
		case STATUS_ERROR:
			info.task.func = task_exit;
			info.status = EXIT_INIT;
			info.msg_lock = 0;
			break;
		default:
			log_qcy(DEBUG_SERIOUS, "!!!!!!!unprocessed server status in task_default = %d", info.status);
			break;
		}
	return;
}

/*
 * server entry point
 */
static void *server_func(void)
{
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
	misc_set_thread_name("server_video3");
	pthread_detach(pthread_self());
	msg_buffer_init2(&message, _config_.msg_overrun, &mutex);
	info.init = 1;
	//default task
	info.task.func = task_default;
	while( !info.exit ) {
		info.old_status = info.status;
		info.task.func();
		server_message_proc();
	}
	server_release_3();
	log_qcy(DEBUG_INFO, "-----------thread exit: server_video3-----------");
	pthread_exit(0);
}

/*
 * internal interface
 */
int video3_md_trigger_message(void)
{
	int ret = 0;
	recorder_init_t init;
	unsigned long long int now;
	if( config.md.enable ) {
		now = time_get_now_stamp();
		if( config.md.alarm_interval < 1)
			config.md.alarm_interval = 1;
		pthread_rwlock_rdlock(&ilock);
		if( ( now - last_report) >= config.md.alarm_interval * 60 ) {
			pthread_rwlock_unlock(&ilock);
			pthread_rwlock_wrlock(&ilock);
			last_report = now;
			pthread_rwlock_unlock(&ilock);
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
			now += config.md.recording_length;
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
		else {
			pthread_rwlock_unlock(&ilock);
		}
	}
	return ret;
}

int video3_spd_trigger_message(void)
{
	int ret = 0;
	unsigned long long int now;
	recorder_init_t init;
	if( config.spd.enable ) {
		now = time_get_now_stamp();
		if( config.spd.alarm_interval < 1)
			config.spd.alarm_interval = 1;
		pthread_rwlock_rdlock(&ilock);
		if( ( now - last_report) >= config.spd.alarm_interval * 60 ) {
			pthread_rwlock_unlock(&ilock);
			pthread_rwlock_wrlock(&ilock);
			last_report = now;
			pthread_rwlock_unlock(&ilock);
			message_t msg;
			/********motion notification********/
			msg_init(&msg);
			msg.message = MICLOUD_EVENT_TYPE_PEOPLEMOTION;
			msg.sender = msg.receiver = SERVER_VIDEO3;
			msg.extra = &now;
			msg.extra_size = sizeof(now);
			ret = manager_common_send_message(SERVER_MICLOUD, &msg);
			/********recorder********/
			msg_init(&msg);
			memset(&init, 0, sizeof(init));
			msg.message = MSG_RECORDER_ADD;
			msg.sender = msg.receiver = SERVER_VIDEO3;
			init.video_channel = 0;
			init.mode = RECORDER_MODE_BY_TIME;
			init.type = RECORDER_TYPE_MOTION_DETECTION;
			init.audio = 1;
			memset(init.start, 0, sizeof(init.start));
			time_get_now_str(init.start);
			now += config.spd.recording_length;
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
			msg.arg_in.chick = RECORDER_TYPE_HUMAN_DETECTION;
			msg.message = MSG_VIDEO_SNAPSHOT;
			manager_common_send_message(SERVER_VIDEO, &msg);
			/**********************************************/
		}
		else {
			pthread_rwlock_unlock(&ilock);
		}
	}
	return ret;
}
/*
 * external interface
 */
int server_video3_start(void)
{
	int ret=-1;
	ret = pthread_create(&info.id, NULL, server_func, NULL);
	if(ret != 0) {
		log_qcy(DEBUG_SERIOUS, "video3 server create error! ret = %d",ret);
		 return ret;
	 }
	else {
		log_qcy(DEBUG_INFO, "video3 server create successful!");
		return 0;
	}
}

int server_video3_message(message_t *msg)
{
	int ret=0;
	pthread_mutex_lock(&mutex);
	if( !message.init ) {
		log_qcy(DEBUG_SERIOUS, "video3 server is not ready for message processing!");
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	ret = msg_buffer_push(&message, msg);
	log_qcy(DEBUG_SERIOUS, "push into the video3 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg->sender, msg->message, ret,
			message.head, message.tail);
	if( ret!=0 )
		log_qcy(DEBUG_WARNING, "message push in video3 error =%d", ret);
	else {
		pthread_cond_signal(&cond);
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}
