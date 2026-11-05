# SPDX-License-Identifier: GPL-2.0
# camellia: Novatek NT36672C/D (in-cell, SPI) touch from camellia's 4.14 kernel, one module
ccflags-y += -I$(srctree)/$(src) -I$(srctree)/$(src)/../camellia_ft8722
ccflags-y += -DCONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
obj-m += camellia-nt36672c-ts.o
camellia-nt36672c-ts-objs := nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
# nvt_selftest_open/nvt_tp_selftest_store keep a 4 KB criteria buffer on the stack (self-test via /proc only)
CFLAGS_nt36xxx_mp_ctrlram.o += -Wframe-larger-than=4608
