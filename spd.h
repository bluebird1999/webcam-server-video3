/*
 * spd.h
 *
 *  Created on: Oct 2, 2020
 *      Author: ning
 */

#ifndef SERVER_VIDEO3_SPD_H_
#define SERVER_VIDEO3_SPD_H_

/*
 * header
 */
#include <rts_file_io.h>
#include <rts_common_function.h>
#include <rts_md.h>
#include <rts_pd.h>
#include <rts_mt.h>
#include <rts_network.h>

#include "config.h"

/*
 * define
 */
#define 	SPD_CONFIG_FILE_NAME			"spd.config"
#define 	SPD_WEIGHT_FILE_NAME			"spd_weight.data"

/*
 * structure
 */

/*
 * function
 */
int video3_spd_proc(video3_spd_config_t *ctrl, int channel, rts_md_src *md_src, rts_pd_src *pd_src);
int video3_spd_init(video3_spd_config_t *ctrl, int channel, rts_md_src *md_src, rts_pd_src *pd_src);
int video3_spd_release(int channel);

#endif /* SERVER_VIDEO3_SPD_H_ */
