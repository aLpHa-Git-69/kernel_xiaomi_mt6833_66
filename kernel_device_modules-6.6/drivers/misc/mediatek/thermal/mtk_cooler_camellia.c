// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: the two Xiaomi coolers stock 4.14 registers for mi_thermald
 * (/vendor/etc/thermald-devices.conf looks them up by name):
 * mtk-cl-backlight (mtk_cooler_backlight_cus.c) and mtk-cl-gpu
 * (mtk_cooler_gpu_cus.c). Without them mi_thermald cannot cap the
 * backlight or the GPU ("generic_devices_init: open failed").
 *
 * Copyright (C) 2017-2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/thermal.h>
#if IS_ENABLED(CONFIG_LEDS_MTK)
#include <linux/leds-mtk.h>
#endif

#include "mt-plat/mtk_thermal_monitor.h"
#include <tscpu_settings.h>
#include <ap_thermal_limit.h>

#define MAX_BACKLIGHT_BRIGHTNESS 100

#define mtk_cooler_camellia_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/camellia " fmt, ##args)

#if IS_ENABLED(CONFIG_LEDS_MTK)
static struct thermal_cooling_device *cl_backlight_dev;
static unsigned int g_backlight_id;
static unsigned int g_backlight_level = MAX_BACKLIGHT_BRIGHTNESS;

static int mtk_cl_backlight_get_max_state(struct thermal_cooling_device *cdev,
					  unsigned long *state)
{
	*state = MAX_BACKLIGHT_BRIGHTNESS;
	return 0;
}

static int mtk_cl_backlight_get_cur_state(struct thermal_cooling_device *cdev,
					  unsigned long *state)
{
	*state = g_backlight_level;
	return 0;
}

/* state is the brightness cap in percent; 100 or more lifts the cap */
static int mtk_cl_backlight_set_cur_state(struct thermal_cooling_device *cdev,
					  unsigned long state)
{
	bool enable = state < MAX_BACKLIGHT_BRIGHTNESS;

	state = min_t(unsigned long, state, MAX_BACKLIGHT_BRIGHTNESS);
	setMaxBrightness(-1, state, enable);
	g_backlight_level = state;
	mtk_cooler_camellia_dprintk("backlight %d\n", g_backlight_level);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_backlight_ops = {
	.get_max_state = mtk_cl_backlight_get_max_state,
	.get_cur_state = mtk_cl_backlight_get_cur_state,
	.set_cur_state = mtk_cl_backlight_set_cur_state,
};
#endif

static struct thermal_cooling_device *cl_gpu_dev;
static unsigned int g_gpu_id;
static unsigned int g_gpu_level;
/* own limit user, so ATM/DTM limits still apply (4.14 wrote gpufreq directly) */
static struct apthermolmt_user ap_cl_gpu;
static char *ap_cl_gpu_log = "ap_cl_gpu";

static int mtk_cl_gpu_get_max_state(struct thermal_cooling_device *cdev,
				    unsigned long *state)
{
	*state = Num_of_GPU_OPP - gpu_max_opp - 1;
	return 0;
}

static int mtk_cl_gpu_get_cur_state(struct thermal_cooling_device *cdev,
				    unsigned long *state)
{
	*state = g_gpu_level;
	return 0;
}

/* state is a GPU OPP index below the top one; 0 lifts the limit */
static int mtk_cl_gpu_set_cur_state(struct thermal_cooling_device *cdev,
				    unsigned long state)
{
	if (mtk_gpu_power == NULL || state >= Num_of_GPU_OPP - gpu_max_opp) {
		mtk_cooler_camellia_dprintk("gpu: table=%p state=%lu opp=%d\n",
			mtk_gpu_power, state, Num_of_GPU_OPP - gpu_max_opp);
		return 0;
	}

	g_gpu_level = state;
	apthermolmt_set_gpu_power_limit(&ap_cl_gpu, (g_gpu_level == 0) ?
		0x7FFFFFFF : mtk_gpu_power[g_gpu_level + gpu_max_opp].gpufreq_power);
	mtk_cooler_camellia_dprintk("gpu %d\n", g_gpu_level);

	return 0;
}

static struct thermal_cooling_device_ops mtk_cl_gpu_ops = {
	.get_max_state = mtk_cl_gpu_get_max_state,
	.get_cur_state = mtk_cl_gpu_get_cur_state,
	.set_cur_state = mtk_cl_gpu_set_cur_state,
};

int mtk_cooler_camellia_init(void)
{
#if IS_ENABLED(CONFIG_LEDS_MTK)
	cl_backlight_dev = mtk_thermal_cooling_device_register("mtk-cl-backlight",
		(void *)&g_backlight_id, &mtk_cl_backlight_ops);
	/* stock mi_thermald releases the cap with def_target 2047 (> max_state) */
	if (!IS_ERR_OR_NULL(cl_backlight_dev))
		cl_backlight_dev->clamp_cur_state = true;
#endif
	if (apthermolmt_register_user(&ap_cl_gpu, ap_cl_gpu_log) < 0)
		mtk_cooler_camellia_dprintk("no apthermolmt slot for gpu\n");
	else
		cl_gpu_dev = mtk_thermal_cooling_device_register("mtk-cl-gpu",
			(void *)&g_gpu_id, &mtk_cl_gpu_ops);

	return 0;
}

void mtk_cooler_camellia_exit(void)
{
#if IS_ENABLED(CONFIG_LEDS_MTK)
	if (cl_backlight_dev) {
		mtk_thermal_cooling_device_unregister(cl_backlight_dev);
		cl_backlight_dev = NULL;
	}
#endif
	if (cl_gpu_dev) {
		mtk_thermal_cooling_device_unregister(cl_gpu_dev);
		cl_gpu_dev = NULL;
		apthermolmt_unregister_user(&ap_cl_gpu);
	}
}
