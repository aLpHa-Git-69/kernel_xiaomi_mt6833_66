// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/ktime.h>
#include <linux/mutex.h>
#include "ged_mali_event.h"

static DEFINE_MUTEX(ged_mali_event_callback_lock);

static fence_timeout_notify_callback fence_timeout_notify_callback_list[MAX_CALLBACK_NUM];
static gpu_reset_done_notify_callback gpu_reset_done_notify_callback_list[MAX_CALLBACK_NUM];
static struct ged_mali_event_info ged_mali_event_info;
static uint32_t cs_error_info_idx;
static uint32_t device_lost_info_idx;
static uint32_t gpu_reset_info_idx;

// only use the function under interrupt context with spinlock held
