/*
 * vedio3_interface.h
 *
 *  Created on: Aug 27, 2020
 *      Author: ning
 */
#ifndef SERVER_VIDEO3_INTERFACE_H_
#define SERVER_VIDEO3_INTERFACE_H_

/*
 * header
 */
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		SERVER_VIDEO3_VERSION_STRING		"alpha-8.0"

#define		MSG_VIDEO3_BASE						(SERVER_VIDEO3<<16)
#define		MSG_VIDEO3_SIGINT					(MSG_VIDEO3_BASE | 0x0000)
#define		MSG_VIDEO3_SIGINT_ACK				(MSG_VIDEO3_BASE | 0x1000)
//video control message
#define		MSG_VIDEO3_START					(MSG_VIDEO3_BASE | 0x0010)
#define		MSG_VIDEO3_START_ACK				(MSG_VIDEO3_BASE | 0x1010)
#define		MSG_VIDEO3_STOP						(MSG_VIDEO3_BASE | 0x0011)
#define		MSG_VIDEO3_STOP_ACK					(MSG_VIDEO3_BASE | 0x1011)
#define		MSG_VIDEO3_PROPERTY_SET_DIRECT		(MSG_VIDEO3_BASE | 0x0014)
#define		MSG_VIDEO3_PROPERTY_SET_DIRECT_ACK	(MSG_VIDEO3_BASE | 0x1014)
#define		MSG_VIDEO3_PROPERTY_GET				(MSG_VIDEO3_BASE | 0x0015)
#define		MSG_VIDEO3_PROPERTY_GET_ACK			(MSG_VIDEO3_BASE | 0x1015)
#define		MSG_VIDEO3_SNAPSHOT					(MSG_VIDEO3_BASE | 0x0016)
#define		MSG_VIDEO3_SNAPSHOT_ACK				(MSG_VIDEO3_BASE | 0x1016)

#define 	VIDEO3_RUN_MODE_MD_HARD		0
#define 	VIDEO3_RUN_MODE_SPD			1
#define 	VIDEO3_RUN_MODE_SNAP		2

//standard motion detection
#define 	VIDEO3_PROPERTY_MOTION_SWITCH          		(0x0010 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO3_PROPERTY_MOTION_ALARM_INTERVAL    	(0x0011 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO3_PROPERTY_MOTION_SENSITIVITY 			(0x0012 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO3_PROPERTY_MOTION_START		        (0x0013 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
#define 	VIDEO3_PROPERTY_MOTION_END		          	(0x0014 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)
//qcy custom
#define 	VIDEO3_PROPERTY_CUSTOM_WARNING_PUSH         (0x0100 | PROPERTY_TYPE_GET | PROPERTY_TYPE_SET)

/*
 * structure
 */
/*
 * function
 */
int server_video3_start(void);
int server_video3_message(message_t *msg);

#endif
