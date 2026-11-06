/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Cheng-Jung Ho <cheng-jung.ho@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "vcodec_bw.h"

#if 0 /* camellia: unused: validate_bw */
bool validate_bw(struct vcodec_bw *bw, int type)
{
	if (bw == 0)
		return false;

	if (bw->smi_bw_mon[type] == 0xFFFFFFFF)
		return false;
	else
		return true;
}
#endif /* validate_bw */

#if 0 /* camellia: unused: find_bw_by_id */
struct vcodec_bw *find_bw_by_id(struct vcodec_bw *bw_list, int id)
{
	struct vcodec_bw *cur_bw = bw_list;

	if (cur_bw == 0)
		return 0;

	while (cur_bw->id != id) {
		cur_bw = cur_bw->next;
		if (cur_bw == 0)
			return 0;
	}

	return cur_bw;
}
#endif /* find_bw_by_id */

#if 0 /* camellia: unused: add_bw_by_id */
struct vcodec_bw *add_bw_by_id(struct vcodec_bw **bw_list, int id)
{
	struct vcodec_bw *bw = 0;

	bw = find_bw_by_id(*bw_list, id);

	if (bw == 0) {
		bw = kmalloc(sizeof(struct vcodec_bw), GFP_KERNEL);
		if (bw == 0)
			return 0;
		memset(bw, 0, sizeof(struct vcodec_bw));
		bw->id = id;
		bw->next = *bw_list;
		*bw_list = bw;
	}

	if (*bw_list == 0)
		*bw_list = bw;

	return bw;
}
#endif /* add_bw_by_id */

#if 0 /* camellia: unused: remove_bw_by_id */
struct vcodec_bw *remove_bw_by_id(struct vcodec_bw **bw_list, int id)
{
	struct vcodec_bw *cur_bw = *bw_list;
	struct vcodec_bw *next_bw = 0;

	if (cur_bw == 0)
		return 0;

	if (cur_bw->id == id) {
		*bw_list = cur_bw->next;
		return cur_bw;
	}

	while (cur_bw->next != 0 && cur_bw->next->id != id)
		cur_bw = cur_bw->next;

	if (cur_bw->next == 0)
		return 0;

	if (cur_bw->next->id == id) {
		next_bw = cur_bw->next;
		cur_bw->next = next_bw->next;
	}
	return next_bw;
}
#endif /* remove_bw_by_id */

#if 0 /* camellia: unused: free_all_bw */
void free_all_bw(struct vcodec_bw **bw_list)
{
	struct vcodec_bw *bw = 0;

	while (*bw_list != 0) {
		bw = *bw_list;
		*bw_list = bw->next;
		kfree(bw);
	}
}
#endif /* free_all_bw */
