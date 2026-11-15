#!/bin/bash
TC="${CLANG_BIN:?set CLANG_BIN to the bin/ of Android clang r510928 (CLANG_VERSION in build.config.constants)}"
export PATH="$TC:$PATH"
JOBS="${JOBS:-$(nproc)}"
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || { echo "JOBS must be a positive integer" >&2; exit 1; }
R="$(cd "$(dirname "$0")/.." && pwd)"
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
"$K/scripts/config" --file "$O/.config" -e SDCARD_FS -e PSTORE -e PSTORE_RAM -e PSTORE_CONSOLE -e PSTORE_PMSG -e PSTORE_FTRACE \
  -e SERIAL_8250 -e SERIAL_8250_CONSOLE -e SERIAL_8250_MT6577 -e SERIAL_EARLYCON \
  --set-str CMDLINE "stack_depot_disable=on cgroup_disable=pressure kasan.stacktrace=off bootconfig mem_sleep_default=s2idle"
"$K/scripts/config" --file "$O/.config" -e FB -e FB_DEVICE -e DRM_FBDEV_EMULATION -d FRAMEBUFFER_CONSOLE
"$K/scripts/config" --file "$O/.config" -d ZRAM_DEF_COMP_LZORLE -e ZRAM_DEF_COMP_LZ4 --set-str ZRAM_DEF_COMP lz4
if [ "${SUKISU:-0}" = 1 ]; then
  "$K/scripts/config" --file "$O/.config" -e KSU -d KPM
fi
if [ -n "${LOCALVERSION_SUFFIX:-}" ]; then
  "$K/scripts/config" --file "$O/.config" --set-str LOCALVERSION "$LOCALVERSION_SUFFIX"
fi
make "${A[@]}" olddefconfig >> "$O/config.log" 2>&1
fi
start=$(date +%s)
[ "$1" = mtk-only ] || make "${A[@]}" -k -j"$JOBS" Image modules > "$O/build-kernel.log" 2>&1; echo "kernel exit $?" >> "$O/build-kernel.log"
REL=kernel_device_modules-6.6
mkdir -p "$O/$REL" "$O/vendor"
for d in "$REL" vendor; do [ -L "$O/../$d" ] || ln -s "kernel/$d" "$O/../$d"; done
MTK=(DEVICE_MODULES_REL_DIR="$REL" DEVICE_MODULES_PATH="$DM" DEVCIE_MODULES_INCLUDE="-I$DM/include")
[ "$1" = mtk-only ] || true
make "${A[@]}" "${MTK[@]}" -k -j"$JOBS" M="$REL" modules > "$O/build-mtk.log" 2>&1; rc=$?; echo "mtk exit $rc" >> "$O/build-mtk.log"
make "${A[@]}" "${MTK[@]}" -k -j"$JOBS" M=vendor/mediatek/kernel_modules/gpu BUILD_RULE=OOT \
  CONFIG_MALI_MEMORY_GROUP_MANAGER=y CONFIG_MALI_PROTECTED_MEMORY_ALLOCATOR=y CONFIG_DMA_SHARED_BUFFER_TEST_EXPORTER=y \
  KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers" modules > "$O/build-gpu.log" 2>&1 || rc=$?
CN=vendor/mediatek/kernel_modules/connectivity; CO="$O/../vendor/mediatek/kernel_modules/connectivity"
conn() { local d=$1; shift; make "${A[@]}" "${MTK[@]}" TOP="$R" -j"$JOBS" M=$CN/$d "$@" modules > "$O/build-conn-${d//\//_}.log" 2>&1 || rc=$?; }
conn common KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers"
conn wlan/adaptor/build/connac1x KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
conn wlan/core/gen4m MODULE_NAME=wlan_drv_gen4m_6833 SEGMENT=SP \
  KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers $CO/wlan/adaptor/build/connac1x/Module.symvers"
conn bt/mt66xx/wmt BT_PLATFORM=connac1x LOG_TAG="[BT_Drv][wmt]" KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
conn gps/gps_stp MODULE_NAME=gps_drv KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
conn fmradio/Build/mt6631_6635 KCFLAGS=-DMT6631_FM KBUILD_EXTRA_SYMBOLS="$O/$REL/Module.symvers $CO/common/Module.symvers"
echo "total $(( $(date +%s) - start ))s" >> "$O/build-mtk.log"
if [ "$1" != mtk-only ] && ! grep -q "^kernel exit 0$" "$O/build-kernel.log"; then grep -m5 "error:" "$O/build-kernel.log" >&2; echo "kernel build failed" >&2; exit 1; fi
if [ "$rc" != 0 ]; then grep -m5 "error:" "$O/build-mtk.log" "$O/build-gpu.log" >&2; echo "mtk module build failed" >&2; exit 1; fi
