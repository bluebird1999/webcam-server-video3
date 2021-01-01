/*
 * config_video.c
 *
 *  Created on: Sep 1, 2020
 *      Author: ning
 */


/*
 * header
 */
//system header
#include <pthread.h>
#include <stdio.h>
#include <rtsvideo.h>
#include <malloc.h>
#ifdef DMALLOC_ENABLE
#include <dmalloc.h>
#endif
//program header
#include "../../tools/tools_interface.h"
#include "../../manager/manager_interface.h"
//server header
#include "config.h"
/*
 * static
 */
//variable
static int							dirty;
static video3_config_t				video3_config;

static config_map_t video3_config_profile_map[] = {
	{"quality",				&(video3_config.profile.quality),						cfg_u32, 2,0,0,32,},
    {"low_format", 			&(video3_config.profile.profile.fmt),				cfg_u32, 3,0,0,32,},
    {"low_width",			&(video3_config.profile.profile.video.width),		cfg_u32, 320,0,0,4800,},
	{"low_height",			&(video3_config.profile.profile.video.height),		cfg_u32, 240,0,0,4800,},
	{"low_numerator",		&(video3_config.profile.profile.video.numerator),	cfg_u32, 1,0,0,100,},
	{"low_denominator",		&(video3_config.profile.profile.video.denominator),	cfg_u32, 15,0,0,100,},
    {NULL,},
};

static config_map_t video3_config_isp_map[] = {
	{"isp_buf_num",				&(video3_config.isp.isp_buf_num),	cfg_s32, 1,0,0,10,},
	{"isp_id", 					&(video3_config.isp.isp_id), 		cfg_s32, 0,0,0,2,},
    {NULL,},
};

static config_map_t video3_config_jpg_map[] = {
    {"enable", 		&(video3_config.jpg.enable), 				cfg_u32, 1,0,0,1,},
    {"rotation",	&(video3_config.jpg.jpg_ctrl.rotation),		cfg_u32, 0,0,0,4,},
	{"image_path",	&(video3_config.jpg.image_path), 			cfg_string, "0",0,0,32,},
    {NULL,},
};

static config_map_t video3_config_md_map[] = {
    {"enable", 			&(video3_config.md.enable), 				cfg_u32, 1,0,0,1,},
	{"polling",			&(video3_config.md.polling),				cfg_u32, 1,0,0,1,},
	{"trig",			&(video3_config.md.trig),					cfg_u32, 1,0,0,1,},
    {"cloud_report",	&(video3_config.md.cloud_report),			cfg_u32, 1,0,0,1,},
    {"alarm_interval", 	&(video3_config.md.alarm_interval), 		cfg_u32, 1,0,1,30,},
    {"sensitivity",		&(video3_config.md.sensitivity),			cfg_u32, 30,0,0,100,},
	{"recording_length",&(video3_config.md.recording_length),		cfg_u32, 6,0,0,30,},
    {"start", 			&(video3_config.md.start), 					cfg_string, '20:00-23:00',0, 0,32,},
    {"end",				&(video3_config.md.end),					cfg_string, '20:00-23:00',0, 0,32,},
    {NULL,},
};

static config_map_t video3_config_spd_map[] = {
    {"enable", 			&(video3_config.spd.enable), 				cfg_u32, 1,0,0,1,},
    {"cloud_report",	&(video3_config.spd.cloud_report),			cfg_u32, 1,0,0,1,},
    {"alarm_interval", 	&(video3_config.spd.alarm_interval), 		cfg_u32, 1,0,1,30,},
	{"recording_length",&(video3_config.spd.recording_length),		cfg_u32, 6,0,0,30,},
    {"file_path",		&(video3_config.spd.file_path), 			cfg_string, '/opt/qcy/data/',0, 0,64,},
    {NULL,},
};
//function
static int video3_config_save(void);


/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static int video3_config_save(void)
{
	int ret = 0;
	message_t msg;
	char fname[MAX_SYSTEM_STRING_SIZE*2];
	if( misc_get_bit(dirty, CONFIG_VIDEO3_PROFILE) ) {
		memset(fname,0,sizeof(fname));
		sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_PROFILE_PATH);
		ret = write_config_file(&video3_config_profile_map, fname);
		if(!ret)
			misc_set_bit(&dirty, CONFIG_VIDEO3_PROFILE, 0);
	}
	else if( misc_get_bit(dirty, CONFIG_VIDEO3_ISP) ) {
		memset(fname,0,sizeof(fname));
		sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_ISP_PATH);
		ret = write_config_file(&video3_config_isp_map, fname);
		if(!ret)
			misc_set_bit(&dirty, CONFIG_VIDEO3_ISP, 0);
	}
	else if( misc_get_bit(dirty, CONFIG_VIDEO3_JPG) ) {
		memset(fname,0,sizeof(fname));
		sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_JPG_PATH);
		ret = write_config_file(&video3_config_jpg_map, fname);
		if(!ret)
			misc_set_bit(&dirty, CONFIG_VIDEO3_JPG, 0);
	}
	else if( misc_get_bit(dirty, CONFIG_VIDEO3_MD) ) {
		memset(fname,0,sizeof(fname));
		sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_MD_PATH);
		ret = write_config_file(&video3_config_md_map, fname);
		if(!ret)
			misc_set_bit(&dirty, CONFIG_VIDEO3_MD, 0);
	}
	else if( misc_get_bit(dirty, CONFIG_VIDEO3_SPD) ) {
		memset(fname,0,sizeof(fname));
		sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_SPD_PATH);
		ret = write_config_file(&video3_config_spd_map, fname);
		if(!ret)
			misc_set_bit(&dirty, CONFIG_VIDEO3_SPD, 0);
	}
	if( !dirty ) {
		/********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_TIMER_REMOVE;
		msg.arg_in.handler = video3_config_save;
		/****************************/
		manager_common_send_message(SERVER_MANAGER, &msg);
	}
	return ret;
}

int video3_config_video_read(video3_config_t *vconf)
{
	int ret,ret1=0;
	char fname[MAX_SYSTEM_STRING_SIZE*2];
	memset(fname,0,sizeof(fname));
	sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_PROFILE_PATH);
	ret = read_config_file(&video3_config_profile_map, fname);
	if(!ret)
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_PROFILE,1);
	else
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_PROFILE,0);
	ret1 |= ret;
	memset(fname,0,sizeof(fname));
	sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_ISP_PATH);
	ret = read_config_file(&video3_config_isp_map,fname );
	if(!ret)
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_ISP,1);
	else
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_ISP,0);
	ret1 |= ret;
	memset(fname,0,sizeof(fname));
	sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_JPG_PATH);
	ret = read_config_file(&video3_config_jpg_map,fname );
	if(!ret)
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_JPG,1);
	else
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_JPG,0);
	ret1 |= ret;
	memset(fname,0,sizeof(fname));
	sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_MD_PATH);
	ret = read_config_file(&video3_config_md_map, fname);
	if(!ret)
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_MD,1);
	else
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_MD,0);
	ret1 |= ret;
	memset(fname,0,sizeof(fname));
	sprintf(fname,"%s%s",_config_.qcy_path, CONFIG_VIDEO3_SPD_PATH);
	ret = read_config_file(&video3_config_spd_map, fname);
	if(!ret)
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_SPD,1);
	else
		misc_set_bit(&video3_config.status, CONFIG_VIDEO3_SPD,0);
	ret1 |= ret;
	memcpy(vconf,&video3_config,sizeof(video3_config_t));
	return ret1;
}

int video3_config_video_set(int module, void* arg)
{
	int ret = 0;
	if(dirty==0) {
		message_t msg;
	    /********message body********/
		msg_init(&msg);
		msg.message = MSG_MANAGER_TIMER_ADD;
		msg.sender = SERVER_VIDEO3;
		msg.arg_in.cat = 30000;	//1min
		msg.arg_in.dog = 0;
		msg.arg_in.duck = 0;
		msg.arg_in.handler = &video3_config_save;
		/****************************/
		manager_common_send_message(SERVER_MANAGER, &msg);
	}
	misc_set_bit(&dirty, module, 1);
	if( module == CONFIG_VIDEO3_PROFILE) {
		memcpy( (video3_profile_config_t*)(&video3_config.profile), arg, sizeof(video3_profile_config_t));
	}
	else if ( module == CONFIG_VIDEO3_ISP ) {
		memcpy( (struct rts_isp_attr *)(&video3_config.isp), arg, sizeof(struct rts_isp_attr));
	}
	else if ( module == CONFIG_VIDEO3_JPG ) {
		memcpy( (video3_jpg_config_t*)(&video3_config.jpg), arg, sizeof(video3_jpg_config_t));
	}
	else if ( module == CONFIG_VIDEO3_MD ) {
		memcpy( (video3_md_config_t*)(&video3_config.md), arg, sizeof(video3_md_config_t));
	}
	else if ( module == CONFIG_VIDEO3_SPD ) {
		memcpy( (video3_spd_config_t*)(&video3_config.spd), arg, sizeof(video3_spd_config_t));
	}
	return ret;
}
