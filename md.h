/*
 * md.h
 *
 *  Created on: Oct 2, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO3_MD_H_
#define SERVER_VIDEO3_MD_H_

/*
 * header
 */
#include "config.h"

/*
 * define
 */


/*
 * structure
 */
typedef struct md_bmp_data_t {
        unsigned char *vm_addr;
        unsigned int length;
        unsigned int width;
        unsigned int height;
} md_bmp_data_t;

/*
 * function
 */
int video3_md_release(void);
int video3_md_init(video3_md_config_t *config, int width, int height);
int video3_md_proc(void);
int video3_md_get_scheduler_time(char *input, scheduler_time_t *st, int *mode);
int video3_md_check_scheduler_time(scheduler_time_t *st, int *mode);

#endif /* SERVER_VIDEO3_MD_H_ */
