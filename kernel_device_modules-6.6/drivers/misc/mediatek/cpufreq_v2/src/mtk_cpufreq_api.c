// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_platform.h"
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/slab.h>
#include <trace/events/power.h>
#include <trace/events/sched.h>


#if IS_ENABLED(CONFIG_MTK_CM_MGR_LEGACY)
cpuFreqsampler_func g_pCpuFreqSampler_func_cpi;
/* cpu governor freq sampler */
void mt_cpufreq_set_governor_freq_registerCB(cpuFreqsampler_func pCB)
{
	g_pCpuFreqSampler_func_cpi = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_set_governor_freq_registerCB);
#endif /* CONFIG_MTK_CM_MGR */

#ifndef VBOOT_VOLT
#define VBOOT_VOLT 80000
#endif
unsigned int mt_cpufreq_find_Vboot_idx(unsigned int cluster_id)
{
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int idx = 0;

	if (p == NULL)
		return 0;

	idx = _search_available_freq_idx_under_v(p, VBOOT_VOLT);

	if (idx > p->nr_opp_tbl)
		idx = p->nr_opp_tbl;

	return idx;
}
EXPORT_SYMBOL(mt_cpufreq_find_Vboot_idx);

cpuVoltsampler_func g_pCpuVoltSampler_met;
cpuVoltsampler_func g_pCpuVoltSampler_ocp;

/* cpu voltage sampler */

/* ocp cpu voltage sampler */

/* for PTP-OD */


unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
#ifdef HYBRID_CPU_DVFS
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return cpuhvfs_get_cur_volt(id);
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return vproc_p->cur_volt;
#endif
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_volt);

/* typedef int (*cpufreq_get_cur_volt_ptr)(enum mt_cpu_dvfs_id id);
cpufreq_get_cur_volt_ptr cpufreq_get_cur_volt_cb(void)
{
	return mt_cpufreq_get_cur_volt;
}
EXPORT_SYMBOL(cpufreq_get_cur_volt_cb); */


unsigned int mt_cpufreq_get_cur_freq(unsigned int id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
#ifdef HYBRID_CPU_DVFS
	int freq_idx;
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif

	freq_idx = cpuhvfs_get_cur_dvfs_freq_idx((enum mt_cpu_dvfs_id)id);

	if (freq_idx < 0)
		freq_idx = 0;

	if (freq_idx > 15)
		freq_idx = 15;

	return mt_cpufreq_get_freq_by_idx((enum mt_cpu_dvfs_id)id, freq_idx);
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs((enum mt_cpu_dvfs_id)id);

	if (p == NULL)
		return 0;
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return cpu_dvfs_get_cur_freq(p);
#endif
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_freq);

#ifndef ONE_CLUSTER

#endif

unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	FUNC_ENTER(FUNC_LV_API);

	if (p == NULL)
		return 0;

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_freq_by_idx(p, idx);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_freq_by_idx);



unsigned int mt_cpufreq_get_volt_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	FUNC_ENTER(FUNC_LV_API);

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_volt_by_idx(p, idx);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_volt_by_idx);

unsigned int mt_cpufreq_get_cur_phy_freq_no_lock(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int freq = 0;

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	FUNC_ENTER(FUNC_LV_LOCAL);

	freq = cpu_dvfs_get_cur_freq(p);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return freq;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_phy_freq_no_lock);

#ifdef DFD_WORKAROUND
void dfd_workaround(void)
{
	_dfd_workaround();
}
EXPORT_SYMBOL(dfd_workaround);
#endif

#ifdef READ_SRAM_VOLT
int mt_cpufreq_update_legacy_volt(enum mt_cpu_dvfs_id id,
		unsigned int *volt_tbl, int nr_volt_tbl)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	_mt_cpufreq_dvfs_request_wrapper(p, p->idx_opp_tbl,
		MT_CPU_DVFS_EEM_UPDATE,	(void *)&volt_tbl);

	FUNC_EXIT(FUNC_LV_API);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_update_legacy_volt);
#endif
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
void *get_mcupm_ipidev(void)
{
	return NULL;
}
EXPORT_SYMBOL(get_mcupm_ipidev);
#endif
