# SPDX-License-Identifier: GPL-2.0
# camellia (Redmi Note 10 5G) project: sensor drivers and power table from Xiaomi's 4.14 camera_project/camellian.
# Sensors are pulled in one by one by mt6853/Makefile (FILTER_DRV); this adds the project camera_hw.
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/mt6853/camera_project/camellian/camera_hw
imgsensor_isp6s_mon-objs += ../common/v1_1/camera_hw/regulator/regulator.o
imgsensor_isp6s_mon-objs += ../common/v1_1/camera_hw/gpio/gpio.o
imgsensor_isp6s_mon-objs += ../common/v1_1/camera_hw/mclk/mclk.o
imgsensor_isp6s_mon-objs += $(subdrv-rpath)/camera_hw/imgsensor_cfg_table.o
imgsensor_isp6s_mon-objs += $(subdrv-rpath)/camera_hw/smartldo/smartldo.o
