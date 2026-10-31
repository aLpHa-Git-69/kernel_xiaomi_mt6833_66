# SPDX-License-Identifier: GPL-2.0
# camellia Goodix fingerprint (SPI5, Microtrust TA), ported from Xiaomi 4.14 drivers/input/fingerprint/goodix
ccflags-y += -I$(DEVICE_MODULES_PATH)/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/teei/400/tz_driver/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/teei/400/common/include
obj-m += camellia_goodix_fp.o
camellia_goodix_fp-y := gf_spi_tee.o
