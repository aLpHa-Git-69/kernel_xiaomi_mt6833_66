// SPDX-License-Identifier: GPL-2.0
/*
 * musb_trace.c - MUSB Controller Trace Support
 *
 * Copyright (C) 2022 MediaTek Inc.
 *
 */

#define CREATE_TRACE_POINTS
#include "musb_trace.h"

#if 0 /* camellia: unused: musb_dbg */
void musb_dbg(struct musb *musb, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	trace_musb_log(musb, &vaf);

	va_end(args);
}
#endif /* musb_dbg */

EXPORT_TRACEPOINT_SYMBOL_GPL(musb_gadget_enable);
EXPORT_TRACEPOINT_SYMBOL_GPL(musb_gadget_disable);
EXPORT_TRACEPOINT_SYMBOL_GPL(musb_g_giveback);
EXPORT_TRACEPOINT_SYMBOL_GPL(musb_gadget_queue);
EXPORT_TRACEPOINT_SYMBOL_GPL(musb_host_urb_giveback);

