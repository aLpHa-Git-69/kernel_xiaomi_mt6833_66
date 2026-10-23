// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: 4.14 per-feature PQ ioctls on top of the 6.6 PQ proxy and frame
 * config paths. The stock PQ service still issues these. Without them the
 * colour engine, CCORR (night light, reading mode) and AAL (CABC/DRE) stay at
 * their boot defaults and libaal reports "Kernel does not support AAL".
 * The 4.14 ioctls carry no crtc id; they always acted on the primary display.
 */

#include <linux/slab.h>
#include <drm/drm_crtc.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_pq_legacy.h"
#include "mtk_log.h"

#define PQ_CMD(type, cmd)	(((type) << 16) | (cmd))

static_assert(sizeof(struct DISP_AAL_INITREG_414) == 0x124);
static_assert(sizeof(struct DISP_GAMMA_LUT_T) == 0x804);
static_assert(sizeof(struct DISPLAY_PQ_T) == 0x34e0);
static_assert(sizeof(struct DISPLAY_COLOR_REG) == 0x5e0);

static struct drm_crtc *legacy_crtc(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	return priv ? priv->crtc[0] : NULL;
}

static int legacy_proxy(struct drm_device *dev, struct drm_file *file_priv,
	unsigned int type, unsigned int cmd, void *kdata, unsigned int size)
{
	struct drm_crtc *crtc = legacy_crtc(dev);

	if (!crtc)
		return -ENODEV;
	return disp_pq_proxy_kdata(dev, file_priv, crtc, PQ_CMD(type, cmd),
				   kdata, size) < 0 ? -EINVAL : 0;
}

static int legacy_config(struct drm_device *dev, unsigned int type, unsigned int cmd,
	void *kdata, unsigned int size, unsigned int check_trigger)
{
	struct drm_crtc *crtc = legacy_crtc(dev);

	if (!crtc)
		return -ENODEV;
	return disp_pq_helper_frame_config_kdata(crtc, PQ_CMD(type, cmd), kdata,
						 size, check_trigger) < 0 ? -EINVAL : 0;
}

int mtk_drm_legacy_pq_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE,
			    PQ_VIRTUAL_SET_PROPERTY, data, sizeof(unsigned int [32]));
}

int mtk_drm_legacy_set_ccorr(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_config(dev, MTK_DISP_PQ_CCORR, PQ_CCORR_SET_CCORR, data,
			     sizeof(struct DRM_DISP_CCORR_COEF_T), 1);
}

int mtk_drm_legacy_ccorr_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_PQ_CCORR, PQ_CCORR_EVENTCTL,
			    data, sizeof(unsigned int));
}

/* waits for the CCORR irq and returns the current backlight, as on 4.14 */
int mtk_drm_legacy_ccorr_get_irq(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE, PQ_VIRTUAL_GET_IRQ,
			    data, sizeof(unsigned int));
}

int mtk_drm_legacy_set_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_config(dev, MTK_DISP_PQ_GAMMA, PQ_GAMMA_SET_GAMMALUT, data,
			     sizeof(struct DISP_GAMMA_LUT_T), 0);
}

/*
 * 4.14 derived the COLOR registers in the kernel from PQPARAM + PQINDEX. 6.6
 * only takes the registers computed by userspace (SET_COLOR_REG), which the
 * stock service sends as well, so there is nothing left to apply here.
 */
int mtk_drm_legacy_set_pqparam(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return 0;
}

int mtk_drm_legacy_set_pqindex(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_PQ_COLOR, PQ_COLOR_SET_PQINDEX,
			    data, sizeof(struct DISPLAY_PQ_T));
}

int mtk_drm_legacy_set_color_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_config(dev, MTK_DISP_PQ_COLOR, PQ_COLOR_SET_COLOR_REG, data,
			     sizeof(struct DISPLAY_COLOR_REG), 0);
}

int mtk_drm_legacy_mutex_control(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_PQ_COLOR, PQ_COLOR_MUTEX_CONTROL,
			    data, sizeof(unsigned int));
}

int mtk_drm_legacy_read_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE, PQ_COLOR_READ_REG,
			    data, sizeof(struct DISP_READ_REG));
}

int mtk_drm_legacy_write_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE, PQ_COLOR_WRITE_REG,
			    data, sizeof(struct DISP_WRITE_REG));
}

int mtk_drm_legacy_bypass_color(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_pq_relay_enable relay = {
		.enable = !!*(unsigned int *)data,
		.wait_hw_config_done = true,
		.relay_engines = MTK_DISP_PQ_COLOR_RELAY,
		.caller = PQ_FEATURE_DEFAULT,
	};

	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE,
			    PQ_VIRTUAL_RELAY_ENGINES, &relay, sizeof(relay));
}

int mtk_drm_legacy_pq_set_window(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_PQ_COLOR, PQ_COLOR_SET_WINDOW,
			    data, sizeof(struct DISP_PQ_WIN_PARAM));
}

int mtk_drm_legacy_read_sw_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE, PQ_COLOR_READ_SW_REG,
			    data, sizeof(struct DISP_READ_REG));
}

int mtk_drm_legacy_write_sw_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_VIRTUAL_TYPE, PQ_COLOR_WRITE_SW_REG,
			    data, sizeof(struct DISP_WRITE_REG));
}

int mtk_drm_legacy_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct DISP_AAL_DISPLAY_SIZE_414 *dst = data;
	struct DISP_AAL_DISPLAY_SIZE size = {};
	int ret;

	ret = legacy_proxy(dev, file_priv, MTK_DISP_PQ_AAL, PQ_AAL_GET_SIZE,
			   &size, sizeof(size));
	if (ret)
		return ret;
	dst->width = size.width;
	dst->height = size.height;
	dst->isdualpipe = size.isdualpipe;
	return 0;
}

/* 6.6 appended the dual pipe / active window fields; single pipe here */
int mtk_drm_legacy_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct DISP_AAL_INITREG *regs;
	struct DISP_AAL_DISPLAY_SIZE size = {};
	int ret;

	ret = legacy_proxy(dev, file_priv, MTK_DISP_PQ_AAL, PQ_AAL_GET_SIZE,
			   &size, sizeof(size));
	if (ret)
		return ret;

	regs = kzalloc(sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return -ENOMEM;
	memcpy(regs, data, sizeof(struct DISP_AAL_INITREG_414));
	regs->width = size.width;
	regs->height = size.height;
	regs->isdual = size.isdualpipe;
	ret = legacy_config(dev, MTK_DISP_PQ_AAL, PQ_AAL_INIT_REG, regs,
			    sizeof(*regs), 0);
	kfree(regs);
	return ret;
}

int mtk_drm_legacy_aal_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct DISP_AAL_HIST_414 *dst = data;
	struct DISP_AAL_HIST *hist;
	int ret;

	hist = kzalloc(sizeof(*hist), GFP_KERNEL);
	if (!hist)
		return -ENOMEM;
	/* serviceFlags and requestPartial are inputs */
	hist->serviceFlags = dst->serviceFlags;
	hist->requestPartial = dst->requestPartial;
	ret = legacy_proxy(dev, file_priv, MTK_DISP_PQ_AAL, PQ_AAL_GET_HIST,
			   hist, sizeof(*hist));
	if (!ret) {
		dst->serviceFlags = hist->serviceFlags;
		dst->backlight = hist->backlight;
		dst->aal0_colorHist = hist->aal0_colorHist;
		dst->aal1_colorHist = hist->aal1_colorHist;
		memcpy(dst->aal0_maxHist, hist->aal0_maxHist, sizeof(dst->aal0_maxHist));
		memcpy(dst->aal1_maxHist, hist->aal1_maxHist, sizeof(dst->aal1_maxHist));
		dst->requestPartial = hist->requestPartial;
		dst->dre30_hist = hist->dre30_hist;
		dst->panel_type = hist->panel_type;
		dst->essStrengthIndex = hist->essStrengthIndex;
		dst->ess_enable = hist->ess_enable;
		dst->dre_enable = hist->dre_enable;
		memcpy(dst->aal0_yHist, hist->aal0_yHist, sizeof(dst->aal0_yHist));
		memcpy(dst->aal1_yHist, hist->aal1_yHist, sizeof(dst->aal1_yHist));
		memcpy(dst->MaxHis_denominator_pipe0, hist->MaxHis_denominator_pipe0,
		       sizeof(dst->MaxHis_denominator_pipe0));
		memcpy(dst->MaxHis_denominator_pipe1, hist->MaxHis_denominator_pipe1,
		       sizeof(dst->MaxHis_denominator_pipe1));
	}
	kfree(hist);
	return ret;
}

/* 6.6 inserted silky_gain_range; the 4.14 gains are 13-bit */
int mtk_drm_legacy_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct DISP_AAL_PARAM_414 *src = data;
	struct DISP_AAL_PARAM *param;
	int ret;

	param = kzalloc(sizeof(*param), GFP_KERNEL);
	if (!param)
		return -ENOMEM;
	memcpy(param->DREGainFltStatus, src->DREGainFltStatus,
	       sizeof(param->DREGainFltStatus));
	param->cabc_fltgain_force = src->cabc_fltgain_force;
	memcpy(param->cabc_gainlmt, src->cabc_gainlmt, sizeof(param->cabc_gainlmt));
	param->FinalBacklight = src->FinalBacklight;
	param->silky_bright_flag = src->silky_bright_flag;
	param->allowPartial = src->allowPartial;
	param->refreshLatency = src->refreshLatency;
	param->silky_gain_range = 8192;
	memcpy(param->silky_bright_gain, src->silky_bright_gain,
	       sizeof(param->silky_bright_gain));
	param->dre30_gain = src->dre30_gain;
	ret = legacy_config(dev, MTK_DISP_PQ_AAL, PQ_AAL_SET_PARAM, param,
			    sizeof(*param), 0);
	kfree(param);
	return ret;
}

int mtk_drm_legacy_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_PQ_AAL, PQ_AAL_EVENTCTL,
			    data, sizeof(unsigned int));
}

int mtk_drm_legacy_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	return legacy_proxy(dev, file_priv, MTK_DISP_PQ_AAL, PQ_AAL_INIT_DRE30,
			    data, sizeof(struct DISP_DRE30_INIT));
}
