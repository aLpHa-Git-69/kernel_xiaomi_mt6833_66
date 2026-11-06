// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "sspm_define.h"
#include "sspm_ipi_id.h"

/* debug API */
#if 0 /* camellia: unused: dump_emi_outstanding */
__weak void dump_emi_outstanding(void) {}
#endif /* dump_emi_outstanding */
#if 0 /* camellia: unused: mtk_spm_dump_debug_info */
__weak void mtk_spm_dump_debug_info(void) {}
#endif /* mtk_spm_dump_debug_info */
#if 0 /* camellia: unused: usb_dump_debug_register */
__weak void usb_dump_debug_register(void) {}
#endif /* usb_dump_debug_register */
#if 0 /* camellia: unused: dpmaif_dump_reg */
__weak void dpmaif_dump_reg(void) {}
#endif /* dpmaif_dump_reg */
#if 0 /* camellia: unused: ccci_md_debug_dump */
__weak void ccci_md_debug_dump(char *user_info) {}
#endif /* ccci_md_debug_dump */

static char *pin_name[SSPM_IPI_COUNT] = {
	"PPM",
	"QOS",
	"PMIC",
	"MET",
	"THERMAL",
	"GPU_DVFS",
	"GPU_PM",
	"PLATFORM",
	"SMI",
	"CM",
	"SLBC",
	"SPM_SUSPEND",
	"QOS",
	"MET",
	"GPU_DVFS",
	"PLATFORM",
	"SLBC",
	"FHCTL",
};

/* platform callback when ipi timeout */
void sspm_ipi_timeout_cb(int ipi_id)
{
	pr_info("Error: possible error IPI %d pin=%s\n",
		ipi_id, pin_name[ipi_id]);

	ipi_monitor_dump(&sspm_ipidev);

	BUG_ON(1);
}

