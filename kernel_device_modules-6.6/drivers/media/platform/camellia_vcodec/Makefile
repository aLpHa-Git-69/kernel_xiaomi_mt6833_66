# SPDX-License-Identifier: GPL-2.0
#
# camellia: the 4.14 MediaTek VCU-mode video codec stack (mtk-vcu + mtk-vcodec,
# mt6833), the kernel side of the stock vpud / codec2 userspace. The 6.6
# mtk-vcu/mtk-vcodec speak a newer vpud protocol (IPI ids, message layouts).

ccflags-y += -DCAMELLIA_VCODEC_VCU
ccflags-y += -I$(srctree)/$(src) -I$(srctree)/$(src)/vcu -I$(srctree)/$(src)/mt6833
ccflags-y += -I$(DEVICE_MODULES_PATH)/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox

obj-m += camellia-vcodec.o

camellia-vcodec-y := camellia_vcodec_main.o \
	vcu/mtk_vcu.o vcu/mtk_vcodec_mem.o \
	mt6833/mtk_vcodec_intr.o mtk_vcodec_util.o mt6833/vcodec_dvfs.o mt6833/vcodec_bw.o \
	mtk_vcodec_dec_drv.o vdec_drv_if.o mtk_vcodec_dec.o mt6833/mtk_vcodec_dec_pm.o \
	vdec/vdec_common_if.o vdec/vdec_log_if.o vdec_vcu_if.o \
	mtk_vcodec_enc.o mtk_vcodec_enc_drv.o mt6833/mtk_vcodec_enc_pm.o \
	venc_drv_if.o venc/venc_common_if.o venc/venc_log_if.o venc_vcu_if.o
