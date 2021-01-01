/*
 * config_video.h
 *
 *  Created on: Sep 1, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO3_CONFIG_H_
#define SERVER_VIDEO3_CONFIG_H_

/*
 * header
 */
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		CONFIG_VIDEO3_MODULE_NUM		5

#define		CONFIG_VIDEO3_PROFILE			0
#define		CONFIG_VIDEO3_ISP				1
#define		CONFIG_VIDEO3_JPG				2
#define		CONFIG_VIDEO3_MD				3
#define		CONFIG_VIDEO3_SPD				4

#define 	CONFIG_VIDEO3_PROFILE_PATH		"config/video3_profile.config"
#define 	CONFIG_VIDEO3_ISP_PATH			"config/video3_isp.config"
#define 	CONFIG_VIDEO3_JPG_PATH			"config/video3_jpg.config"
#define 	CONFIG_VIDEO3_MD_PATH			"config/video3_md.config"
#define 	CONFIG_VIDEO3_SPD_PATH			"config/video3_spd.config"

/*
 * structure
 */

/*
 *
 */
typedef struct video3_profile_config_t {
	int						quality;
	struct rts_av_profile	profile;
} video3_profile_config_t;

typedef struct video3_jpg_config_t {
	int		enable;
	char	image_path[MAX_SYSTEM_STRING_SIZE];
	struct rts_jpgenc_attr		jpg_ctrl;
} video3_jpg_config_t;

typedef struct video3_md_config_t {
	int 	enable;
	int		polling;
	int		trig;
	int		cloud_report;
	int		alarm_interval;
	int		sensitivity;
	int		recording_length;
	char	start[MAX_SYSTEM_STRING_SIZE];
	char	end[MAX_SYSTEM_STRING_SIZE];
} video3_md_config_t;

typedef struct video3_spd_config_t {
	int 	enable;
	int		cloud_report;
	int		alarm_interval;
	int		recording_length;
	int		width;
	int		height;
	char	file_path[MAX_SYSTEM_STRING_SIZE*2];
} video3_spd_config_t;

typedef struct video3_config_t {
	int							status;
	video3_profile_config_t		profile;
	struct rts_isp_attr			isp;
	video3_jpg_config_t			jpg;
	video3_md_config_t			md;
	video3_spd_config_t			spd;
} video3_config_t;

/*
 * function
 */
int video3_config_video_read(video3_config_t *vconf);
int video3_config_video_set(int module, void *t);

#endif /* SERVER_VIDEO3_CONFIG_H_ */
