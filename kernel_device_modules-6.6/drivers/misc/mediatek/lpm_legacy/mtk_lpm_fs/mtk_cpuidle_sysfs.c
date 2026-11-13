// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_cpuidle_sysfs.h"

#define MTK_CPUIDLE_SYS_FS_NAME	"cpuidle"
#define MTK_CPUIDLE_SYS_FS_MODE	0644

static struct mtk_lp_sysfs_handle mtk_cpuidle_fs_root;

int mtk_cpuidle_sysfs_entry_node_add(const char *name,
		int mode, const struct mtk_lp_sysfs_op *op,
		struct mtk_lp_sysfs_handle *handle)
{
	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_cpuidle_fs_root))
		mtk_cpuidle_sysfs_root_entry_create();

	return mtk_lp_sysfs_entry_func_node_add(name
			, mode, op, &mtk_cpuidle_fs_root, handle);
}
EXPORT_SYMBOL(mtk_cpuidle_sysfs_entry_node_add);

int mtk_cpuidle_sysfs_root_entry_create(void)
{
	int bRet = 0;

	if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_cpuidle_fs_root)) {
		bRet = mtk_lp_sysfs_entry_func_create(
			MTK_CPUIDLE_SYS_FS_NAME, MTK_CPUIDLE_SYS_FS_MODE
			, NULL, &mtk_cpuidle_fs_root);
	}
	return bRet;
}
EXPORT_SYMBOL(mtk_cpuidle_sysfs_root_entry_create);

int mtk_cpuidle_sysfs_sub_entry_add(const char *name, int mode,
					struct mtk_lp_sysfs_handle *parent,
					struct mtk_lp_sysfs_handle *handle)
{
	int bRet = 0;
	struct mtk_lp_sysfs_handle *p;

	p = parent;

	if (!p) {
		if (!IS_MTK_LP_SYS_HANDLE_VALID(&mtk_cpuidle_fs_root)) {
			bRet = mtk_cpuidle_sysfs_root_entry_create();
			if (bRet)
				return bRet;
		}
		p = &mtk_cpuidle_fs_root;
	}

	bRet = mtk_lp_sysfs_entry_func_create(name, mode, p, handle);
	return bRet;
}
EXPORT_SYMBOL(mtk_cpuidle_sysfs_sub_entry_add);

int mtk_cpuidle_sysfs_sub_entry_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle)
{
	return mtk_lp_sysfs_entry_func_node_add(name
			, mode, op, parent, handle);
}
EXPORT_SYMBOL(mtk_cpuidle_sysfs_sub_entry_node_add);
