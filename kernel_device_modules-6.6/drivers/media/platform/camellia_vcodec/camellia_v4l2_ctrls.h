/* SPDX-License-Identifier: GPL-2.0 */
/*
 * camellia: 4.14 MTK patched v4l2_ctrl_get_name()/v4l2_ctrl_fill() in the
 * V4L2 core to know its private V4L2_CID_MPEG_MTK_* controls. 6.6 does not,
 * so v4l2_ctrl_new_std() on them has no name and fails with -ERANGE, which
 * fails every open of the decoder/encoder. Create them as custom controls
 * with the 4.14 name, type and flags instead, and log the id of any control
 * that puts the handler in error.
 */
#ifndef __CAMELLIA_V4L2_CTRLS_H__
#define __CAMELLIA_V4L2_CTRLS_H__

#include <media/v4l2-ctrls.h>
#include "camellia_mtk_v4l2.h"

static inline bool camellia_mtk_ctrl_fill(u32 id, const char **name,
					  enum v4l2_ctrl_type *type, u32 *flags)
{
	*type = V4L2_CTRL_TYPE_INTEGER;
	switch (id) {
	case V4L2_CID_MPEG_MTK_FRAME_INTERVAL:
		*name = "Video frame interval";
		*flags = V4L2_CTRL_FLAG_READ_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_ERRORMB_MAP:
		*name = "Video error map";
		*flags = V4L2_CTRL_FLAG_READ_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_ASPECT_RATIO:
		*name = "Video aspect ratio";
		*flags = V4L2_CTRL_FLAG_READ_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_FIX_BUFFERS:
		*name = "Video fix buffers";
		*flags = V4L2_CTRL_FLAG_READ_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_FIX_BUFFERS_SVP:
		*name = "Video fix buffers for svp";
		*flags = V4L2_CTRL_FLAG_READ_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_DECODE_MODE:
		*name = "Video decode mode";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_FRAME_SIZE:
		*name = "Video frame size";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER:
		*name = "Video fixed maximum frame size";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME:
		*name = "Wait key frame";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_OPERATING_RATE:
		*name = "Vdec Operating Rate";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_REAL_TIME_PRIORITY:
		*name = "Vdec Realtime Priority";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_SEC_DECODE:
		*name = "Video Sec Decode path";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_SEC_ENCODE:
		*name = "Video Sec Encode path";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT:
		*name = "Video queued frame buf count";
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP:
		*name = "I-Frame QP Value";
		*flags = 0;
		return true;
	case V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP:
		*name = "P-Frame QP Value";
		*flags = 0;
		return true;
	case V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP:
		*name = "B-Frame QP Value";
		*flags = 0;
		return true;
	case V4L2_CID_MPEG_MTK_CRC_PATH:
		*name = "Video crc path";
		*type = V4L2_CTRL_TYPE_STRING;
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_GOLDEN_PATH:
		*name = "Video golden path";
		*type = V4L2_CTRL_TYPE_STRING;
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	case V4L2_CID_MPEG_MTK_LOG:
		*name = "Video Log";
		*type = V4L2_CTRL_TYPE_STRING;
		*flags = V4L2_CTRL_FLAG_WRITE_ONLY;
		return true;
	}
	return false;
}

static inline void camellia_ctrl_check(struct v4l2_ctrl_handler *hdl,
				       int old_err, u32 id)
{
	if (hdl->error && !old_err)
		pr_err("camellia_vcodec: control 0x%x failed %d\n", id, hdl->error);
}

static inline struct v4l2_ctrl *
camellia_ctrl_new_std(struct v4l2_ctrl_handler *hdl, const struct v4l2_ctrl_ops *ops,
		      u32 id, s64 min, s64 max, u64 step, s64 def)
{
	struct v4l2_ctrl_config cfg = {
		.ops = ops, .id = id, .min = min, .max = max, .step = step, .def = def,
	};
	struct v4l2_ctrl *ctrl;
	int old_err = hdl->error;

	if (camellia_mtk_ctrl_fill(id, &cfg.name, &cfg.type, &cfg.flags))
		ctrl = v4l2_ctrl_new_custom(hdl, &cfg, NULL);
	else
		ctrl = v4l2_ctrl_new_std(hdl, ops, id, min, max, step, def);
	camellia_ctrl_check(hdl, old_err, id);
	return ctrl;
}

static inline struct v4l2_ctrl *
camellia_ctrl_new_custom(struct v4l2_ctrl_handler *hdl,
			 const struct v4l2_ctrl_config *cfg, void *priv)
{
	struct v4l2_ctrl *ctrl;
	int old_err = hdl->error;

	ctrl = v4l2_ctrl_new_custom(hdl, cfg, priv);
	camellia_ctrl_check(hdl, old_err, cfg->id);
	return ctrl;
}

static inline struct v4l2_ctrl *
camellia_ctrl_new_std_menu(struct v4l2_ctrl_handler *hdl, const struct v4l2_ctrl_ops *ops,
			   u32 id, u8 max, u64 mask, u8 def)
{
	struct v4l2_ctrl *ctrl;
	int old_err = hdl->error;

	ctrl = v4l2_ctrl_new_std_menu(hdl, ops, id, max, mask, def);
	camellia_ctrl_check(hdl, old_err, id);
	return ctrl;
}

#define v4l2_ctrl_new_std camellia_ctrl_new_std
#define v4l2_ctrl_new_custom camellia_ctrl_new_custom
#define v4l2_ctrl_new_std_menu camellia_ctrl_new_std_menu

#endif
