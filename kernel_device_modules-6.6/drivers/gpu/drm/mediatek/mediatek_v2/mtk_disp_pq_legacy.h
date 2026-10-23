/* SPDX-License-Identifier: GPL-2.0 */
/*
 * camellia: the 4.14 per-feature PQ ioctls, which the stock MIUI PQ/AAL
 * services (vendor.mediatek.hardware.pq@2.2-service) still use. 6.6 folded
 * them into DRM_MTK_PQ_PROXY_IOCTL and DRM_MTK_PQ_FRAME_CONFIG.
 */
#ifndef __MTK_DISP_PQ_LEGACY_H__
#define __MTK_DISP_PQ_LEGACY_H__

#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/mediatek_drm.h>

#define DRM_MTK_LEGACY_PQ_PERSIST_PROPERTY	0x1F
#define DRM_MTK_LEGACY_SET_CCORR		0x20
#define DRM_MTK_LEGACY_CCORR_EVENTCTL		0x21
#define DRM_MTK_LEGACY_CCORR_GET_IRQ		0x22
#define DRM_MTK_LEGACY_SET_GAMMALUT		0x23
#define DRM_MTK_LEGACY_SET_PQPARAM		0x24
#define DRM_MTK_LEGACY_SET_PQINDEX		0x25
#define DRM_MTK_LEGACY_SET_COLOR_REG		0x26
#define DRM_MTK_LEGACY_MUTEX_CONTROL		0x27
#define DRM_MTK_LEGACY_READ_REG			0x28
#define DRM_MTK_LEGACY_WRITE_REG		0x29
#define DRM_MTK_LEGACY_BYPASS_COLOR		0x2A
#define DRM_MTK_LEGACY_PQ_SET_WINDOW		0x2B
#define DRM_MTK_LEGACY_READ_SW_REG		0x2E
#define DRM_MTK_LEGACY_WRITE_SW_REG		0x2F
#define DRM_MTK_LEGACY_AAL_INIT_REG		0x30
#define DRM_MTK_LEGACY_AAL_GET_HIST		0x31
#define DRM_MTK_LEGACY_AAL_SET_PARAM		0x32
#define DRM_MTK_LEGACY_AAL_EVENTCTL		0x33
#define DRM_MTK_LEGACY_AAL_INIT_DRE30		0x34
#define DRM_MTK_LEGACY_AAL_GET_SIZE		0x35

/* 4.14 layouts of the structs 6.6 extended */
struct DISP_AAL_INITREG_414 {
	/* DRE */
	int dre_map_bypass;
	/* ESS */
	int cabc_gainlmt[33];
	/* DRE 3.0 Reg. */
	int dre_s_lower;
	int dre_s_upper;
	int dre_y_lower;
	int dre_y_upper;
	int dre_h_lower;
	int dre_h_upper;
	int dre_h_slope;
	int dre_s_slope;
	int dre_y_slope;
	int dre_x_alpha_base;
	int dre_x_alpha_shift_bit;
	int dre_y_alpha_base;
	int dre_y_alpha_shift_bit;
	int act_win_x_end;
	int dre_blk_x_num;
	int dre_blk_y_num;
	int dre_blk_height;
	int dre_blk_width;
	int dre_blk_area;
	int dre_blk_area_min;
	int hist_bin_type;
	int dre_flat_length_slope;
	int dre_flat_length_th;
	int act_win_y_start;
	int act_win_y_end;
	int blk_num_x_start;
	int blk_num_x_end;
	int dre0_blk_num_x_start;
	int dre0_blk_num_x_end;
	int dre1_blk_num_x_start;
	int dre1_blk_num_x_end;
	int blk_cnt_x_start;
	int blk_cnt_x_end;
	int blk_num_y_start;
	int blk_num_y_end;
	int blk_cnt_y_start;
	int blk_cnt_y_end;
	int last_tile_x_flag;
	int last_tile_y_flag;
};

struct DISP_AAL_HIST_414 {
	unsigned int serviceFlags;
	int backlight;
	int aal0_colorHist;
	int aal1_colorHist;
	unsigned int aal0_maxHist[AAL_HIST_BIN];
	unsigned int aal1_maxHist[AAL_HIST_BIN];
	int requestPartial;
	unsigned long long dre30_hist;
	unsigned int panel_type;
	int essStrengthIndex;
	int ess_enable;
	int dre_enable;
	unsigned int aal0_yHist[AAL_HIST_BIN];
	unsigned int aal1_yHist[AAL_HIST_BIN];
	unsigned int MaxHis_denominator_pipe0[AAL_DRE_BLK_NUM];
	unsigned int MaxHis_denominator_pipe1[AAL_DRE_BLK_NUM];
};

struct DISP_AAL_PARAM_414 {
	int DREGainFltStatus[AAL_DRE_POINT_NUM];
	int cabc_fltgain_force;	/* 10-bit ; [0,1023] */
	int cabc_gainlmt[33];
	int FinalBacklight;	/* 10-bit ; [0,1023] */
	int silky_bright_flag;
	int allowPartial;
	int refreshLatency;	/* DISP_AAL_REFRESH_LATENCY */
	unsigned int silky_bright_gain[3];	/* 13-bit ; [1,8192] */
	unsigned long long dre30_gain;
};

struct DISP_AAL_DISPLAY_SIZE_414 {
	int width;
	int height;
	bool isdualpipe;
};

#define DRM_IOCTL_MTK_LEGACY(nr, type) \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_MTK_LEGACY_##nr, type)
#define DRM_IOCTL_MTK_LEGACY_PQ_PERSIST_PROPERTY \
	DRM_IOCTL_MTK_LEGACY(PQ_PERSIST_PROPERTY, unsigned int [32])
#define DRM_IOCTL_MTK_LEGACY_SET_CCORR \
	DRM_IOCTL_MTK_LEGACY(SET_CCORR, struct DRM_DISP_CCORR_COEF_T)
#define DRM_IOCTL_MTK_LEGACY_CCORR_EVENTCTL \
	DRM_IOCTL_MTK_LEGACY(CCORR_EVENTCTL, unsigned int)
#define DRM_IOCTL_MTK_LEGACY_CCORR_GET_IRQ \
	DRM_IOCTL_MTK_LEGACY(CCORR_GET_IRQ, unsigned int)
#define DRM_IOCTL_MTK_LEGACY_SET_GAMMALUT \
	DRM_IOCTL_MTK_LEGACY(SET_GAMMALUT, struct DISP_GAMMA_LUT_T)
#define DRM_IOCTL_MTK_LEGACY_SET_PQPARAM \
	DRM_IOCTL_MTK_LEGACY(SET_PQPARAM, struct DISP_PQ_PARAM)
#define DRM_IOCTL_MTK_LEGACY_SET_PQINDEX \
	DRM_IOCTL_MTK_LEGACY(SET_PQINDEX, struct DISPLAY_PQ_T)
#define DRM_IOCTL_MTK_LEGACY_SET_COLOR_REG \
	DRM_IOCTL_MTK_LEGACY(SET_COLOR_REG, struct DISPLAY_COLOR_REG)
#define DRM_IOCTL_MTK_LEGACY_MUTEX_CONTROL \
	DRM_IOCTL_MTK_LEGACY(MUTEX_CONTROL, unsigned int)
#define DRM_IOCTL_MTK_LEGACY_READ_REG \
	DRM_IOCTL_MTK_LEGACY(READ_REG, struct DISP_READ_REG)
#define DRM_IOCTL_MTK_LEGACY_WRITE_REG \
	DRM_IOCTL_MTK_LEGACY(WRITE_REG, struct DISP_WRITE_REG)
#define DRM_IOCTL_MTK_LEGACY_BYPASS_COLOR \
	DRM_IOCTL_MTK_LEGACY(BYPASS_COLOR, unsigned int)
#define DRM_IOCTL_MTK_LEGACY_PQ_SET_WINDOW \
	DRM_IOCTL_MTK_LEGACY(PQ_SET_WINDOW, struct DISP_PQ_WIN_PARAM)
#define DRM_IOCTL_MTK_LEGACY_READ_SW_REG \
	DRM_IOCTL_MTK_LEGACY(READ_SW_REG, struct DISP_READ_REG)
#define DRM_IOCTL_MTK_LEGACY_WRITE_SW_REG \
	DRM_IOCTL_MTK_LEGACY(WRITE_SW_REG, struct DISP_WRITE_REG)
#define DRM_IOCTL_MTK_LEGACY_AAL_INIT_REG \
	DRM_IOCTL_MTK_LEGACY(AAL_INIT_REG, struct DISP_AAL_INITREG_414)
#define DRM_IOCTL_MTK_LEGACY_AAL_GET_HIST \
	DRM_IOCTL_MTK_LEGACY(AAL_GET_HIST, struct DISP_AAL_HIST_414)
#define DRM_IOCTL_MTK_LEGACY_AAL_SET_PARAM \
	DRM_IOCTL_MTK_LEGACY(AAL_SET_PARAM, struct DISP_AAL_PARAM_414)
#define DRM_IOCTL_MTK_LEGACY_AAL_EVENTCTL \
	DRM_IOCTL_MTK_LEGACY(AAL_EVENTCTL, unsigned int)
#define DRM_IOCTL_MTK_LEGACY_AAL_INIT_DRE30 \
	DRM_IOCTL_MTK_LEGACY(AAL_INIT_DRE30, struct DISP_DRE30_INIT)
#define DRM_IOCTL_MTK_LEGACY_AAL_GET_SIZE \
	DRM_IOCTL_MTK_LEGACY(AAL_GET_SIZE, struct DISP_AAL_DISPLAY_SIZE_414)

int mtk_drm_legacy_pq_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_set_ccorr(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_ccorr_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_ccorr_get_irq(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_set_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_set_pqparam(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_set_pqindex(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_set_color_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_mutex_control(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_read_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_write_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_bypass_color(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_pq_set_window(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_read_sw_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_write_sw_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_aal_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_legacy_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#define MTK_LEGACY_PQ_IOCTLS \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_PQ_PERSIST_PROPERTY, mtk_drm_legacy_pq_persist_property, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_SET_CCORR, mtk_drm_legacy_set_ccorr, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_CCORR_EVENTCTL, mtk_drm_legacy_ccorr_eventctl, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_CCORR_GET_IRQ, mtk_drm_legacy_ccorr_get_irq, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_SET_GAMMALUT, mtk_drm_legacy_set_gammalut, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_SET_PQPARAM, mtk_drm_legacy_set_pqparam, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_SET_PQINDEX, mtk_drm_legacy_set_pqindex, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_SET_COLOR_REG, mtk_drm_legacy_set_color_reg, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_MUTEX_CONTROL, mtk_drm_legacy_mutex_control, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_READ_REG, mtk_drm_legacy_read_reg, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_WRITE_REG, mtk_drm_legacy_write_reg, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_BYPASS_COLOR, mtk_drm_legacy_bypass_color, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_PQ_SET_WINDOW, mtk_drm_legacy_pq_set_window, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_READ_SW_REG, mtk_drm_legacy_read_sw_reg, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_WRITE_SW_REG, mtk_drm_legacy_write_sw_reg, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_AAL_INIT_REG, mtk_drm_legacy_aal_init_reg, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_AAL_GET_HIST, mtk_drm_legacy_aal_get_hist, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_AAL_SET_PARAM, mtk_drm_legacy_aal_set_param, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_AAL_EVENTCTL, mtk_drm_legacy_aal_eventctl, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_AAL_INIT_DRE30, mtk_drm_legacy_aal_init_dre30, DRM_UNLOCKED), \
	DRM_IOCTL_DEF_DRV(MTK_LEGACY_AAL_GET_SIZE, mtk_drm_legacy_aal_get_size, DRM_UNLOCKED)

#endif
