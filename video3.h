/*
 * video3.h
 *
 *  Created on: Aug 28, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO3_VIDEO3_H_
#define SERVER_VIDEO3_VIDEO3_H_

/*
 * header
 */
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include "config.h"

/*
 * define
 */
#define		THREAD_MD			0
#define		THREAD_SPD			1

#define		VIDEO3_INIT_CONDITION_NUM			3
#define		VIDEO3_INIT_CONDITION_CONFIG		0
#define		VIDEO3_INIT_CONDITION_MIIO_TIME		1
#define		VIDEO3_INIT_CONDITION_REALTEK		2

#define		VIDEO3_EXIT_CONDITION			( (1 << SERVER_RECORDER) )

#define		VIDEO3_MAX_JPEG_SIZE			16*1024
/*
 * structure
 */
typedef struct video3_motion_run_t {
	int 				started;
	scheduler_time_t  	scheduler;
	int					mode;
	char				changed;
} video3_motion_run_t;
/*
 * function
 */

#endif /* SERVER_VIDEO3_VIDEO3_H_ */
