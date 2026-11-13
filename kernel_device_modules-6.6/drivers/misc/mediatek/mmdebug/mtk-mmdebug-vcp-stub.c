// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: mtk21306 <cindy-hy.chen@mediatek.com>
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>

#include "mtk-mmdebug-vcp.h"

int mtk_mmdebug_status_dump_register_notifier(struct notifier_block *nb)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdebug_status_dump_register_notifier);

MODULE_DESCRIPTION("MMDEBUG vcp Driver");
MODULE_AUTHOR("mtk21306 <cindy-hy.chen@mediatek.com>");
MODULE_LICENSE("GPL");

