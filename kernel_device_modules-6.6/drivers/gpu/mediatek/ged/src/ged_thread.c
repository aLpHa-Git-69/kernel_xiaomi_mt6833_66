// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/kthread.h>
#include "ged_base.h"
#include "ged_thread.h"

struct GED_THREAD_DATA {
	struct task_struct *psThread;
	void (*pFunc)(void *f);
	void *pvData;
};

GED_ERROR ged_thread_destroy(GED_THREAD_HANDLE hThread)
{
	struct GED_THREAD_DATA *psThreadData =
		(struct GED_THREAD_DATA *)hThread;

	if (psThreadData == NULL)
		return GED_ERROR_INVALID_PARAMS;

	kthread_stop(psThreadData->psThread);
	ged_free(psThreadData, sizeof(struct GED_THREAD_DATA));
	return GED_OK;
}
