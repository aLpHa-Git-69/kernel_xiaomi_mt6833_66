// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "mtk-mmdvfs-ftrace.h"

#define CREATE_TRACE_POINTS
#include "mmdvfs_events.h"

void ftrace_record_opp_v1(unsigned long rec, unsigned long opp)
{
	trace_mmdvfs__record_opp_v1(rec, opp);
}
EXPORT_SYMBOL_GPL(ftrace_record_opp_v1);

MODULE_DESCRIPTION("MMDVFS FTRACE");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
