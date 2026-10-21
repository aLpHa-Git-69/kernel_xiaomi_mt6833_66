// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/panic_notifier.h>
#include <linux/kdebug.h>

#define MTK_SIP_DVFSRC_START		0x01
#define VCOREFS_SMC_CMD_PAUSE_ENABLE	0x21

/*
 * camellia: older (Android S, 2022) MT6833 ATF has no MTK_SIP_VCOREFS_CONTROL
 * (0x506); its 4.14 kernel starts DVFSRC through MTK_SIP_KERNEL_SPM_VCOREFS_ARGS
 * (0x226): CMD_5 = SPM fw already running?, else CMD_0 (init) + CMD_1 (flags).
 * Used only when the 0x506 call is reported as unknown.
 */
#define MTK_SIP_LEGACY_SPM_VCOREFS_ARGS	MTK_SIP_SMC_CMD(0x226)
#define LEGACY_VCOREFS_SMC_CMD_INIT	0
#define LEGACY_VCOREFS_SMC_CMD_FLAGS	1
#define LEGACY_VCOREFS_SMC_CMD_FW_STATUS	5
#define LEGACY_SPM_FLAG_RUN_COMMON_SCENARIO	(1U << 10)

static void mtk_dvfsrc_legacy_start(struct device *dev)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_LEGACY_SPM_VCOREFS_ARGS, LEGACY_VCOREFS_SMC_CMD_FW_STATUS,
		0, 0, 0, 0, 0, 0, &ares);
	if (ares.a0 && ares.a0 != (unsigned long)SMCCC_RET_NOT_SUPPORTED) {
		dev_info(dev, "legacy ATF: SPM vcorefs firmware already running\n");
		return;
	}
	arm_smccc_smc(MTK_SIP_LEGACY_SPM_VCOREFS_ARGS, LEGACY_VCOREFS_SMC_CMD_INIT,
		0, 0, 0, 0, 0, 0, &ares);
	arm_smccc_smc(MTK_SIP_LEGACY_SPM_VCOREFS_ARGS, LEGACY_VCOREFS_SMC_CMD_FLAGS,
		LEGACY_SPM_FLAG_RUN_COMMON_SCENARIO, 0, 0, 0, 0, 0, &ares);
	dev_info(dev, "legacy ATF: started DVFSRC via SPM vcorefs (0x226), ret %ld\n",
		(long)ares.a0);
}

struct mtk_dvfsrc_start {
	struct device *dev;
};

static struct mtk_dvfsrc_start *dvfsrc_drv;

static int panic_pause_dvfsrc(struct notifier_block *this, unsigned long event, void *ptr)
{
	static atomic_t first_exception = ATOMIC_INIT(0);
	struct arm_smccc_res ares;

	if (dvfsrc_drv) {
		mtk_dvfsrc_send_request(dvfsrc_drv->dev->parent,
			MTK_DVFSRC_CMD_FORCEOPP_REQUEST, 0xDEAD);
	}

	if (atomic_cmpxchg(&first_exception, 0, 1) != 0)
		return NOTIFY_DONE;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, VCOREFS_SMC_CMD_PAUSE_ENABLE, 1, 0, 0,
		0, 0, 0, &ares);

	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call = panic_pause_dvfsrc,
	.priority = INT_MAX,
};

static struct notifier_block die_blk = {
	.notifier_call = panic_pause_dvfsrc,
	.priority = INT_MAX,
};

/* We let dvfsrc working on parent main driver and setup
 * framework (interconnect, regulator, ..) for user register
 * and request, so we will lock high opp in that stage.
 * We will release this lock in this driver and let probe on
 * late_initsync stage. It send smc cmd MTK_SIP_DVFSRC_RUN
 * to release lock let dvfsrc free run.
 */

static int mtk_dvfsrc_start_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;
	struct mtk_dvfsrc_start *dvfsrc;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dev = &pdev->dev;
	dvfsrc_drv = dvfsrc;
	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_DVFSRC_START, 0, 0, 0,
		0, 0, 0, &ares);
	if (ares.a0 == (unsigned long)SMCCC_RET_NOT_SUPPORTED)
		mtk_dvfsrc_legacy_start(&pdev->dev);
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	register_die_notifier(&die_blk);

	return 0;
}

static struct platform_driver mtk_dvfsrc_run_drv = {
	.probe	= mtk_dvfsrc_start_probe,
	.driver = {
		.name	= "mtk-dvfsrc-start",
	},
};

static int __init mtk_dvfsrc_run_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_run_drv);
}
late_initcall_sync(mtk_dvfsrc_run_init);

static void __exit mtk_dvfsrc_run_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_run_drv);
}
module_exit(mtk_dvfsrc_run_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC enable free run driver");
