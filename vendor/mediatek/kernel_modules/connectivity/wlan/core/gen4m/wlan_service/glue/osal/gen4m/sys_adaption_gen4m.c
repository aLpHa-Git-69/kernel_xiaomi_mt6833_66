// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "sys_adaption.h"
#include "gl_os.h"
#include "gl_kal.h"
#include "gl_wext.h"
#include "precomp.h"
#include "debug.h"

#define UNUSED(x) ((void)(x))
/*****************************************************************************
 *	Packet free related functions
 *****************************************************************************/
void sys_ad_free_pkt(void *packet)
{

}

/*****************************************************************************
 *	OS memory allocate/manage/free related functions
 *****************************************************************************/
s_int32 sys_ad_alloc_mem(u_char **mem, u_long size)
{
	*mem = (u_char *)kalMemAlloc(size, PHY_MEM_TYPE);

	if (*mem)
		return SERV_STATUS_SUCCESS;
	else
		return SERV_STATUS_OSAL_SYS_FAIL;
}

void sys_ad_free_mem(void *mem)
{
	ASSERT(mem);
	kfree(mem);
}

void sys_ad_zero_mem(void *ptr, u_long length)
{
	kalMemZero(ptr, length);
}

void sys_ad_set_mem(void *ptr, u_long length, u_char value)
{
	kalMemSet(ptr, value, length);
}


void sys_ad_move_mem(void *dest, void *src, u_long length)
{
	kalMemMove(dest, src, length);
}

s_int32 sys_ad_cmp_mem(void *dest, void *src, u_long length)
{
	return kalMemCmp(dest, src, length);
}

/*****************************************************************************
 *	OS task create/manage/kill related functions
 *****************************************************************************/

/*****************************************************************************
 *	OS debug functions
 *****************************************************************************/
void sys_ad_mem_dump32(void *ptr, u_long length)
{
	dumpMemory32((s_int32 *)ptr, length);
}
