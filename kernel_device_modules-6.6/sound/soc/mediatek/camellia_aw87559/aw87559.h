/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CAMELLIA_AW87559_H__
#define __CAMELLIA_AW87559_H__

#include <linux/types.h>

enum camellia_aw87559_mode {
	CAMELLIA_AW87559_OFF,
	CAMELLIA_AW87559_MUSIC,
	CAMELLIA_AW87559_VOICE,
	CAMELLIA_AW87559_FM,
	CAMELLIA_AW87559_RCV,
	CAMELLIA_AW87559_MODE_COUNT,
};

bool camellia_aw87559_is_present(void);
int camellia_aw87559_set_mode(unsigned int mode);

#endif
