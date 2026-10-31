# SPDX-License-Identifier: GPL-2.0
# camellia: Xiaomi 4.14 legacy main-camera VCM driver (/dev/MAINAF) the vendor camera HAL drives
ccflags-y += -I$(srctree)/$(src)/inc -I$(srctree)/$(src)/common/lc898122af -I$(srctree)/$(src)/common/dw9800waf -I$(srctree)/$(src)/common/lc898212xdaf/inc -I$(srctree)/$(src)/common/bu63169af/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
# lc898212xdaf picks per-project tables by name
ccflags-y += -DCONFIG_ARCH_MTK_PROJECT=\"camellian\"
obj-m += camellia_main_lens.o
camellia_main_lens-y := main_lens.o \
	common/ak7371af/AK7371AF.o \
	common/bu63169af/BU63169AF.o \
	common/bu63169af/OIS_func.o \
	common/bu63169af/OIS_main.o \
	common/bu63169af/OIS_user.o \
	common/bu6424af/BU6424AF.o \
	common/bu64253gwzaf/BU64253GWZAF.o \
	common/bu6429af/BU6429AF.o \
	common/bu64748af/bu64748af.o \
	common/bu64748af/bu64748_function.o \
	common/dw9714af/DW9714AF.o \
	common/dw9718af/DW9718AF.o \
	common/dw9718saf/DW9718SAF.o \
	common/dw9718taf/DW9718TAF.o \
	common/dw9719taf/DW9719TAF.o \
	common/dw9763af/DW9763AF.o \
	common/dw9800waf/DW9800WAF.o \
	common/dw9814af/DW9814AF.o \
	common/dw9839af/DW9839AF.o \
	common/fp5510e2af/FP5510E2AF.o \
	common/gt9764af/GT9764AF.o \
	common/gt9768af/GT9768AF.o \
	common/gt9772af/GT9772AF.o \
	common/lc898122af/LC898122AF.o \
	common/lc898122af/OisCmd.o \
	common/lc898122af/OisIni.o \
	common/lc898212af/LC898212AF.o \
	common/lc898212xdaf/AfInit.o \
	common/lc898212xdaf/AfInter.o \
	common/lc898212xdaf/AfSTMV.o \
	common/lc898212xdaf/LC898212XDAF.o \
	common/lc898214af/LC898214AF.o \
	common/lc898217afa/LC898217AFA.o \
	common/lc898217afb/LC898217AFB.o \
	common/lc898217afc/LC898217AFC.o \
	common/lc898217af/LC898217AF.o \
	common/lc898229af/LC898229AF.o \
	common/ov5645af/OV5645AF.o \
	common/wv511aaf/WV511AAF.o
