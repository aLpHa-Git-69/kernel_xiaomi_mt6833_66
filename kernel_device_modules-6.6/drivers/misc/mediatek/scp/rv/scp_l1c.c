// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/vmalloc.h>		 /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>	   /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"
#include "scp_l1c.h"
#include <mt-plat/aee.h>

