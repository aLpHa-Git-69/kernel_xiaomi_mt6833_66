// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: /proc/cpumaxfreq, which Xiaomi's 4.14 mtk_cpufreq_interface.c
 * ("BSP.System - add cpumaxfreq node") provided. MIUI's "All specs" page
 * shows it as the CPU clock ("2.2" GHz on MT6833); without it MIUI falls
 * back to a wrong number.
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int cpumaxfreq_show(struct seq_file *m, void *v)
{
	struct cpufreq_policy *policy;
	unsigned long freq = 0;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		freq = max_t(unsigned long, freq, policy->cpuinfo.max_freq);
		cpufreq_cpu_put(policy);
	}
	/* kHz, printed as GHz with one decimal like 4.14 */
	freq /= 10000;
	seq_printf(m, "%lu.%01lu\n", freq / 100, (freq % 100) / 10);
	return 0;
}

static int __init camellia_cpumaxfreq_init(void)
{
	return proc_create_single("cpumaxfreq", 0444, NULL, cpumaxfreq_show) ? 0 : -ENOMEM;
}

static void __exit camellia_cpumaxfreq_exit(void)
{
	remove_proc_entry("cpumaxfreq", NULL);
}

module_init(camellia_cpumaxfreq_init);
module_exit(camellia_cpumaxfreq_exit);
MODULE_LICENSE("GPL v2");
