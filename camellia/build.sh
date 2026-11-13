#!/bin/bash
# Build the MT6833 6.6 kernel + MediaTek device modules for camellia, MediaTek's way without Bazel:
#   .config = gki_defconfig + mgk_64_k66_defconfig + mt6833_overlay + sign + userdebug
#   then GKI kernel Image, then MediaTek kernel_device_modules as external modules (-k: collect all errors).
# Android clang r510928 (clang 18), the tree's CLANG_VERSION: supports kCFI, so gki_defconfig's
# CFI_CLANG=y holds (stock 4.14 had CFI). clang 14 (r450784d) silently dropped it; system clang 21
# miscompiled something in the display path (camera open -> striped display).
TC="${CLANG_BIN:?set CLANG_BIN to the bin/ of Android clang r510928 (CLANG_VERSION in build.config.constants)}"
export PATH="$TC:$PATH"
JOBS="${JOBS:-$(nproc)}"   # set JOBS=2 to keep memory low
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || { echo "JOBS must be a positive integer" >&2; exit 1; }
R="$(cd "$(dirname "$0")/.." && pwd)"   # repo root = kernel source
K=$R; DM=$R/kernel_device_modules-6.6
O="${OUT:-$R/out}/kernel"
A=(-C "$K" O="$O" ARCH=arm64 LLVM=1 LLVM_IAS=1 KCONFIG_EXT_PREFIX="kernel_device_modules-6.6/")
if [ "$1" != mtk-only ]; then
mkdir -p "$O"; : > "$O/.config"
( cd "$O" && make "${A[@]}" gki_defconfig > config.log 2>&1 && \
  bash "$K/scripts/kconfig/merge_config.sh" -m .config "$DM/arch/arm64/configs/mgk_64_k66_defconfig" \
     "$DM/kernel/configs/mt6833_overlay.config" "$DM/kernel/configs/sign.config" "$DM/kernel/configs/userdebug.config" >> config.log 2>&1 )
"$K/scripts/config" --file "$O/.config" -d WERROR -d DEBUG_INFO_BTF -d MMC_DW -d MODULE_SIG_FORCE -d TRIM_UNUSED_KSYMS -d DEBUG_DRIVER --set-str MICROTRUST_TEE_VERSION 400 --set-str MODULE_SIG_KEY certs/signing_key.pem -d MTK_CROSS_SCHEDULER -d CSA37F71_SENSOR_NDT -d CHARGER_MT6379 -m MTK_IMGSENSOR_ISP6S_MON -e MACH_CAMERA_MT6833 --set-str CUSTOM_KERNEL_IMGSENSOR "ov48b_qtech_main_mipi_raw ov48b_aac_main_mipi_raw ov8856_ofilm_front_mipi_raw ov8856_aac_front_mipi_raw gc02m1b_qtech_depth_mipi_mono gc02m1b_aac_depth_mipi_mono hi259h_qtech_macro_mipi_raw hi259h_aac_macro_mipi_raw ov02b10_aac_macro_mipi_raw" -d DEVICE_MODULES_CHARGER_MT6370 -m CUSTOM_KERNEL_SENSORHUB -d CAMERA_PMIC_ENABLE -d MTK_LENS -d SND_SOC_SIA91XX_V3_1_0 -d DEVICE_MODULES_SND_SOC_MT6991 -m DRM_PANEL_CAMELLIA -m NFC_ST21NFC
make "${A[@]}" olddefconfig >> "$O/config.log" 2>&1
# first-boot debugging: pstore/ramoops (LK passes ramoops.mem_address/size on the cmdline),
# MediaTek UART console (LK sets console=ttyS0,921600), no protected KVM (camellia's 2022 ATF).
# mem_sleep_default=s2idle: stock suspends via s2idle (DT suspend-method), camellia's SPM
# firmware resets the SoC on the deep/PSCI SYSTEM_SUSPEND path
"$K/scripts/config" --file "$O/.config" -e SDCARD_FS -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG -e PSTORE_FTRACE \
  -e SERIAL_8250 -e SERIAL_8250_CONSOLE -e SERIAL_8250_MT6577 -e SERIAL_EARLYCON \
  --set-str CMDLINE "stack_depot_disable=on cgroup_disable=pressure kasan.stacktrace=off bootconfig mem_sleep_default=s2idle"
# fbdev emulation as on stock: the vendor HWC only powers the display off (CRTC ACTIVE=0)
# when /dev/graphics/fb0 opens; no fbcon
"$K/scripts/config" --file "$O/.config" -e FB -e FB_DEVICE -e DRM_FBDEV_EMULATION -d FRAMEBUFFER_CONSOLE
# zram lz4 as on stock: MIUI's system_perf_init resets zram after the vendor rc wrote lz4,
# and a 6.6 reset (unlike 4.14) goes back to the default compressor
"$K/scripts/config" --file "$O/.config" -d ZRAM_DEF_COMP_LZORLE -e ZRAM_DEF_COMP_LZ4 --set-str ZRAM_DEF_COMP lz4
make "${A[@]}" olddefconfig >> "$O/config.log" 2>&1
fi
start=$(date +%s)
[ "$1" = mtk-only ] || make "${A[@]}" -k -j"$JOBS" Image modules > "$O/build-kernel.log" 2>&1; echo "kernel exit $?" >> "$O/build-kernel.log"
REL=kernel_device_modules-6.6
# MediaTek's modules land in $O/<M>; their Makefiles (and mkboot) expect them next to $O, as in
# MediaTek's kernel-6.6/kernel_device_modules-6.6/vendor sibling layout
mkdir -p "$O/$REL" "$O/vendor"
for d in "$REL" vendor; do [ -L "$O/../$d" ] || ln -s "kernel/$d" "$O/../$d"; done
MTK=(DEVICE_MODULES_REL_DIR="$REL" DEVICE_MODULES_PATH="$DM" DEVCIE_MODULES_INCLUDE="-I$DM/include")
[ "$1" = mtk-only ] || true
make "${A[@]}" "${MTK[@]}" -k -j"$JOBS" M="$REL" modules > "$O/build-mtk.log" 2>&1; rc=$?; echo "mtk exit $rc" >> "$O/build-mtk.log"
# Mali kbase (r49p1, MediaTek's mt6833 DDK for 6.6; negotiates down to the stock r32p1 userspace's UK 11.x)
# from vendor/mediatek/kernel_modules/gpu, objects in out/vendor/mediatek/kernel_modules/gpu
make "${A[@]}" "${MTK[@]}" -k -j"$JOBS" M=vendor/mediatek/kernel_modules/gpu BUILD_RULE=OOT \
  CONFIG_MALI_MEMORY_GROUP_MANAGER=y CONFIG_MALI_PROTECTED_MEMORY_ALLOCATOR=y CONFIG_DMA_SHARED_BUFFER_TEST_EXPORTER=y \
  KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers" modules > "$O/build-gpu.log" 2>&1 || rc=$?
# connectivity (connac1x WMT: wmt_drv -> wmt_chrdev_wifi + wlan_drv_gen4m_6833, bt_drv_connac1x), objects in
# out/vendor/mediatek/kernel_modules/connectivity; stock vendor insmods its 4.14 copies, which then just fail
CN=vendor/mediatek/kernel_modules/connectivity; CO="$O/../vendor/mediatek/kernel_modules/connectivity"
# TOP: MediaTek's connectivity Kbuilds default it to $(srctree)/.. (the Android tree root); here it is the repo root
conn() { local d=$1; shift; make "${A[@]}" "${MTK[@]}" TOP="$R" -j"$JOBS" M=$CN/$d "$@" modules > "$O/build-conn-${d//\//_}.log" 2>&1 || rc=$?; }
conn common KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers"
conn wlan/adaptor/build/connac1x KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
conn wlan/core/gen4m MODULE_NAME=wlan_drv_gen4m_6833 SEGMENT=SP \
  KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers $CO/wlan/adaptor/build/connac1x/Module.symvers"
conn bt/mt66xx/wmt BT_PLATFORM=connac1x LOG_TAG="[BT_Drv][wmt]" KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
conn gps/gps_stp MODULE_NAME=gps_drv KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
# camellia's A-die is MT6631 (BT fw "..._MT6631_ASIC_..."): fix it at build time, since the
# combined build asks WMT, which only knows it after connsys power-on, so first-stage init failed
conn fmradio/Build/mt6631_6635 KCFLAGS=-DMT6631_FM KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
echo "total $(( $(date +%s) - start ))s" >> "$O/build-mtk.log"
# fail loudly: a module that does not build otherwise only shows up as "missing module" in mkboot
if [ "$1" != mtk-only ] && ! grep -q "^kernel exit 0$" "$O/build-kernel.log"; then grep -m5 "error:" "$O/build-kernel.log" >&2; echo "kernel build failed" >&2; exit 1; fi
if [ "$rc" != 0 ]; then grep -m5 "error:" "$O/build-mtk.log" "$O/build-gpu.log" >&2; echo "mtk module build failed" >&2; exit 1; fi
