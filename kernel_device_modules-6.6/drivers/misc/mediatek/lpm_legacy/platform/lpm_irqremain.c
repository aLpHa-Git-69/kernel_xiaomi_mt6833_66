// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/list.h>
#include <linux/slab.h>

#if IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
#include <linux/irqchip/mtk-gic-extend.h>
#endif

#include <lpm_plat_common.h>
#include <lpm_module.h>
#include <lpm_internal.h>

#include "lpm_irqremain.h"

#define LPM_IRQREMAIN_NODE		"irq-remain"
#define LPM_IRQREMAIN_LISTNODE		"irq-remain-list"

#define LPM_IRQREMAIN_LIST_METHOD	"method"
#define LPM_IRQREMAIN_TARGET		"target"
#define LPM_IRQREMAIN_PROP		"value"

#define FOR_EACH_IRQ_REMAIN(edges)\
		list_for_each_entry(edges,\
				&lpm_irqremain, list)


enum LPM_IRQREMAIN_TYPE {
	LPM_IRQREMAIN_LIST,
	LPM_IRQREMAIN_SMC,
};

struct lpm_irqremain_node {
	unsigned int irq;
	unsigned int wakeup_src_cat;
	unsigned int wakeup_src;
	struct list_head list;
};

static LIST_HEAD(lpm_irqremain);

#define PLAT_COVERT_IRQ_NUM(irq)	virq_to_hwirq(irq)

#if !IS_ENABLED(CONFIG_MTK_GIC_V3_EXT)
static inline unsigned int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq;

	desc = irq_to_desc(virq);
	WARN_ON(!desc);
	hwirq = desc ? desc->irq_data.hwirq : 0;
	return hwirq;
}
#endif

int __init lpm_irqremain_parsing(struct device_node *parent)
{
	int ret = 0, idx = 0, irqnum;
	u32 irqidx = 0, irqtype, irq_do = 0;
	u32 wakeup_src = 0, wakeup_src_cat = 0;
	int remain_count = 0;
	struct lpm_irqremain_node *irqnode;
	const __be32 *next = NULL;
	struct property *prop;
	struct device_node *np = NULL;
	struct device_node *tar_np = NULL;

	while ((np = of_parse_phandle(parent, LPM_IRQREMAIN_NODE, idx))) {
		idx++;
		next = NULL;

		tar_np = of_parse_phandle(np, LPM_IRQREMAIN_TARGET, 0);
		prop = of_find_property(np, LPM_IRQREMAIN_PROP, NULL);

		do {

			if (!prop || !tar_np)
				break;

			next = of_prop_next_u32(prop, next, &irq_do);

			if (!next)
				break;

			next = of_prop_next_u32(prop, next, &irqidx);

			if (!next)
				break;

			next = of_prop_next_u32(prop, next, &wakeup_src_cat);

			if (!next)
				break;

			next = of_prop_next_u32(prop, next, &wakeup_src);

			if (!next)
				break;

			irqnum = of_irq_get(tar_np, irqidx);
			irqtype = irq_get_trigger_type(irqnum);

			if (!(irqtype & IRQ_TYPE_EDGE_BOTH))
				pr_info("[name:mtk_lpm][P] - %pOF irq(%d) type = 0x%x ?\n",
						tar_np, irqidx, irqtype);
			if (irq_do) {
				irq_set_irq_type(irqnum, irqtype);
				pr_info("[name:mtk_lpm][P] - %pOF set irq(%d) as type = 0x%x\n",
						tar_np, irqidx, irqtype);
			}

			if (irqnum >= 0) {
				irqnode = kcalloc(1, sizeof(*irqnode),
						  GFP_KERNEL);
				if (!irqnode)
					return -ENOMEM;

				irqnode->irq = irqnum;
				irqnode->wakeup_src_cat =
						wakeup_src_cat;
				irqnode->wakeup_src =
						wakeup_src;
				list_add(&irqnode->list,
					 &lpm_irqremain);

				pr_info("[name:mtk_lpm][P] - irq_%u, wakeup-src=0x%x (%pOF) (%s:%d)\n",
					irqnode->irq,
					irqnode->wakeup_src,
					tar_np,
					__func__, __LINE__);

				remain_count++;

			} else
				pr_info("[name:mtk_lpm][P] - invalid irq, erro=%d (%s:%d)\n",
						irqnum, __func__, __LINE__);

		} while (1);

		if (tar_np)
			of_node_put(tar_np);
		of_node_put(np);
	}

	lpm_smc_cpu_pm(IRQ_REMAIN_LIST_ALLOC, remain_count, 0, 0);
	lpm_smc_cpu_pm_lp(IRQS_REMAIN_ALLOC, MT_LPM_SMC_ACT_SET,
			      remain_count, 0);
	remain_count = 0;
	FOR_EACH_IRQ_REMAIN(irqnode) {
		lpm_smc_cpu_pm(IRQ_REMAIN_IRQ_ADD,
				   PLAT_COVERT_IRQ_NUM(irqnode->irq),
				   irqnode->wakeup_src_cat,
				   irqnode->wakeup_src);

		lpm_smc_cpu_pm_lp(IRQS_REMAIN_IRQ,
				   MT_LPM_SMC_ACT_SET,
				   PLAT_COVERT_IRQ_NUM(irqnode->irq), 0);
		lpm_smc_cpu_pm_lp(IRQS_REMAIN_WAKEUP_CAT,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->wakeup_src_cat, 0);
		lpm_smc_cpu_pm_lp(IRQS_REMAIN_WAKEUP_SRC,
				   MT_LPM_SMC_ACT_SET,
				   irqnode->wakeup_src, 0);
		lpm_smc_cpu_pm_lp(IRQS_REMAIN_CTRL,
				   MT_LPM_SMC_ACT_PUSH, 0, 0);
		remain_count++;
	}
	lpm_smc_cpu_pm(IRQ_REMAIN_IRQ_SUBMIT, 0, 0, 0);
	lpm_smc_cpu_pm_lp(IRQS_REMAIN_CTRL,
			      MT_LPM_SMC_ACT_SUBMIT, 0, 0);
	return ret;
}

