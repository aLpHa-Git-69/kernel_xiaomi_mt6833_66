# SPDX-License-Identifier: GPL-2.0

# camellia: the 6.6 vcodec/vcu are replaced by camellia_vcodec (same exported symbols)
# obj-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_VCODEC)	+= mtk-vcodec/

# obj-$(CONFIG_VIDEO_MEDIATEK_VCU)	+= mtk-vcu/
# camellia: 4.14 VCU-mode codec stack for the stock vpud/codec2 (see camellia_vcodec/Makefile)
obj-$(CONFIG_VIDEO_MEDIATEK_VCU)	+= camellia_vcodec/

obj-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_JPEG)	+= mtk-jpeg/

obj-y					+= mtk-isp/

obj-$(CONFIG_MTK_CAMERA_FD_ISP7S_ISP7SP)	+= mtk-aie/
obj-$(CONFIG_MTK_CAMERA_DPE_ISP7SP)	 += mtk-dpe/
