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
static	video3_md_run_t		md_run;
static	video_stream_t		stream={-1,-1,-1,-1};
static	video3_config_t		config;
static	pthread_mutex_t		mutex = PTHREAD_MUTEX_INITIALIZER;
static	pthread_cond_t		cond = PTHREAD_COND_INITIALIZER;

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
			log_qcy(DEBUG_INFO, "changed the motion switch = %d", config.md.enable);
			video3_config_video_set(CONFIG_VIDEO3_MD, &config.md);
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
			md_run.changed = 1;
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
			md_run.changed = 1;
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
    pfile = fopen((char*)priv, "wb");
    if (!pfile) {
		log_qcy(DEBUG_WARNING, "open video3 jpg snapshot file %s fail\n", (char*)priv);
		return;
    }
    if( !video3_check_sd() )
    	fwrite(buffer->vm_addr, 1, buffer->bytesused, pfile);
    RTS_SAFE_RELEASE(pfile, fclose);
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
	char fname[MAX_SYSTEM_STRING_SIZE];
    sprintf(fname, "md-%d",time_get_now_stamp());
    misc_set_thread_name(fname);
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl, (video3_md_config_t*)arg, sizeof(video3_md_config_t) );
    video3_md_init( &ctrl, 1920, 1080);
    misc_set_bit(&info.thread_start, THREAD_MD, 1 );
    manager_common_send_dummy(SERVER_VIDEO3);
    while( 1 ) {
    	st = info.status;
    	if( info.exit )
    		break;
    	if( !md_run.started )
    		break;
    	if( (st != STATUS_START) && (st != STATUS_RUN) )
    		break;
    	else if( st == STATUS_START )
    		continue;
    	if( misc_get_bit(info.thread_exit, THREAD_MD) ) break;
 		video3_md_proc();
 		usleep(100*1000);
    }
    //release
    video3_md_release();
    misc_set_bit(&info.thread_start, THREAD_MD, 0 );
    manager_common_send_dummy(SERVER_VIDEO3);
    log_qcy(DEBUG_INFO, "-----------thread exit: %s-----------",fname);
    pthread_exit(0);
}

static void *video3_spd_func(void *arg)
{
	int st;
	int ret;
	video3_spd_config_t ctrl;
	rts_md_src md_src;
	rts_pd_src pd_src;
	char fname[MAX_SYSTEM_STRING_SIZE];
    sprintf(fname, "spd-%d",time_get_now_stamp());
    misc_set_thread_name(fname);
    pthread_detach(pthread_self());
    //init
    memcpy( &ctrl, (video3_spd_config_t*)arg, sizeof(video3_spd_config_t) );
    ret = video3_spd_init( &ctrl, stream.isp, &md_src, &pd_src);
    if( ret ) {
    	log_qcy(DEBUG_INFO, "video3 human detection thread init failed!");
    	goto exit;
    }
    misc_set_bit(&info.thread_start, THREAD_SPD, 1 );
    manager_common_send_dummy(SERVER_VIDEO3);
    while( 1 ) {
    	st = info.status;
    	if( info.exit )
    		break;
    	if( !md_run.started )
    		break;
    	if( (st != STATUS_START) && (st != STATUS_RUN) )
    		break;
    	else if( st == STATUS_START )
    		continue;
    	if( misc_get_bit(info.thread_exit, THREAD_SPD) )
    		break;
    	video3_spd_proc( &ctrl, stream.isp, &md_src, &pd_src);
    	usleep(1000*1000 / config.profile.profile.video.denominator);	//
    }
    //release
    video3_spd_release(stream.isp);
    misc_set_bit(&info.thread_start, THREAD_SPD, 0 );
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
/*    ret = rts_av_start_recv(stream.isp);
    if (ret) {
    	log_qcy(DEBUG_SERIOUS, "start recv isp fail, ret = %d", ret);
    	return -1;
    }
*/
    return 0;
}

static int stream_stop(void)
{
	int ret=0;
//	if(stream.isp!=-1)
//		ret = rts_av_stop_recv(stream.isp);
	if(stream.jpg!=-1)
		ret = rts_av_disable_chn(stream.jpg);
	if(stream.isp!=-1)
		ret = rts_av_disable_chn(stream.isp);
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

static int md_check_scheduler(void)
{
	int ret;
	message_t msg;
	pthread_t md_id, spd_id;
	if( config.md.enable ) {
		ret = video3_md_check_scheduler_time(&md_run.scheduler, &md_run.mode);
		if( ret==1 ) {
			if( !md_run.started ) {
				//start the md thread
				if( !misc_get_bit(info.thread_start, THREAD_MD) ) {
//					ret = pthread_create(&md_id, NULL, video3_md_func, (void*)&config.md);
				}
				ret = 0;
				if( config.spd.enable ) {
					config.spd.alarm_interval = config.md.alarm_interval;
					config.spd.cloud_report = config.md.cloud_report;
					config.spd.enable = config.md.enable;
					config.spd.recording_length = config.md.recording_length;
					config.spd.width = config.profile.profile.video.width;
					config.spd.height = config.profile.profile.video.height;
					if( !misc_get_bit(info.thread_start, THREAD_SPD) ) {
						ret |= pthread_create(&spd_id, NULL, video3_spd_func, (void*)&config.spd);
					}
				}
				if(ret != 0) {
					misc_set_bit( &info.thread_exit, THREAD_MD, 1);
					if( config.spd.enable ) {
						misc_set_bit( &info.thread_exit, THREAD_SPD, 1);
					}
					log_qcy(DEBUG_SERIOUS, "spd thread create error! ret = %d",ret);
					return -1;
				}
				else {
					log_qcy(DEBUG_INFO, "spd thread create successful!");
					md_run.started = 1;
				    /********message body********/
					msg_init(&msg);
					msg.message = MSG_VIDEO3_START;
					msg.sender = msg.receiver = SERVER_VIDEO3;
					msg.arg_in.wolf = VIDEO3_RUN_MODE_MD_HARD;
				    manager_common_send_message(SERVER_VIDEO3, &msg);
				    if( config.spd.enable ) {
				    	log_qcy(DEBUG_INFO, "spd thread create successful!");
						msg.arg_in.wolf = VIDEO3_RUN_MODE_SPD;
						manager_common_send_message(SERVER_VIDEO3, &msg);
				    }
					/****************************/
				}
			}
			else if( md_run.changed ) {
				md_run.changed = 0;
				goto stop_md;
			}
		}
		else {
			if( md_run.started ) {
				goto stop_md;
			}
		}
	}
	else {
		if( md_run.started ) {
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
	if( config.spd.enable ) {
		misc_set_bit( &info.thread_exit, THREAD_SPD, 1);
		msg.arg_in.wolf = VIDEO3_RUN_MODE_SPD;
		manager_common_send_message(SERVER_VIDEO3, &msg);
	}
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
    log_qcy(DEBUG_INFO, "jpg chnno:%d", stream.jpg);
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
	msg.arg_in.handler = md_check_scheduler;
	manager_common_send_message(SERVER_MANAGER, &msg);
	/****************************/
}

static void server_release_2(void)
{
	msg_buffer_release2(&message, &mutex);
	memset(&config,0,sizeof(video3_config_t));
	memset(&stream,0,sizeof(video_stream_t));
}

static void server_release_3(void)
{
	msg_free(&info.task.msg);
	memset(&info, 0, sizeof(server_info_t));
}

/*
 *
 */
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
	if( info.msg_lock ) {
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
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.func = task_start;
			info.task.start = info.status;
			info.msg_lock = 1;
			break;
		case MSG_VIDEO3_STOP:
			msg_init(&info.task.msg);
			msg_copy(&info.task.msg, &msg);
			info.task.msg.arg_in.cat = info.status2;
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
		msg.arg_in.handler = &md_check_scheduler;
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
			if( info.task.msg.arg_in.cat > 0 ) {
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
			break;
		case STATUS_START:
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
	misc_set_thread_name("server_video3");
	pthread_detach(pthread_self());
	msg_buffer_init2(&message, MSG_BUFFER_OVERFLOW_NO, &mutex);
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
	log_qcy(DEBUG_VERBOSE, "push into the video3 message queue: sender=%d, message=%x, ret=%d, head=%d, tail=%d", msg->sender, msg->message, ret,
			message.head, message.tail);
	if( ret!=0 )
		log_qcy(DEBUG_WARNING, "message push in video3 error =%d", ret);
	else {
		pthread_cond_signal(&cond);
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}
