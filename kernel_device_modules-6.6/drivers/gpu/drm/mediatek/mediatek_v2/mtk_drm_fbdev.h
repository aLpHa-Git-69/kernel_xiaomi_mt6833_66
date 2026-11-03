/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_FBDEV_H
#define MTK_DRM_FBDEV_H


#ifdef CONFIG_DRM_FBDEV_EMULATION
int mtk_fbdev_init(struct drm_device *dev);
void mtk_fbdev_fini(struct drm_device *dev);
#else
static inline int mtk_fbdev_init(struct drm_device *dev)
{
	return 0;
}

static inline void mtk_fbdev_fini(struct drm_device *dev)
{
}
#endif /* CONFIG_DRM_FBDEV_EMULATION */

int free_fb_buf(void);
#define MTKFB_FACTORY_AUTO_TEST _IOR('O', 25, unsigned long)
int pan_display_test(int frame_num, int bpp);

#endif /* MTK_DRM_FBDEV_H */
