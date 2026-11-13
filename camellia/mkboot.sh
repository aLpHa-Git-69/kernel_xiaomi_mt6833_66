#!/bin/bash
# Package a camellia test boot.img (header v2, same layout as stock) from the camellia/build.sh output:
#   kernel  = out/kernel/arch/arm64/boot/Image (gzip)
#   dtb     = 6.6 mt6833.dts (+ stock-dtbo compat labels) with the camellia overlay applied
#   ramdisk = stock camellia ramdisk, unchanged, + appended cpio with lib/modules
#             (modules in camellia/ramdisk-modules.txt, stripped, depmod'd, modules.load in dep order)
#             + lib/firmware blobs that first-stage drivers request before /vendor is mounted,
#             copied from the stock vendor image (STOCK_VENDOR=, ext4 raw), never stored in the repo
# Needs: dtc, fdtoverlay, cpp, depmod, gzip, cpio, mkbootimg (MKBOOTIMG=), stock boot.img (STOCK_BOOT=).
# Writes out/bootimg/boot-6.6-test-A.img (kernel @0x40000000, 2MB aligned: 6.6 has text_offset 0)
#    and out/bootimg/boot-6.6-test-B.img (kernel @0x40080000, stock address) as a fallback.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/out"; K="$ROOT"; DM="$ROOT/kernel_device_modules-6.6"
TC="${CLANG_BIN:?set CLANG_BIN (see build.sh)}"
MKBOOTIMG="${MKBOOTIMG:-$(command -v mkbootimg)}"
UNPACK="${UNPACK_BOOTIMG:-$(dirname "$MKBOOTIMG")/unpack_bootimg}"
STOCK_VENDOR="${STOCK_VENDOR:?set STOCK_VENDOR to the stock vendor image (raw ext4, e.g. lpunpack of super.img)}"
STOCK_BOOT="${STOCK_BOOT:?set STOCK_BOOT to the stock boot.img (its ramdisk is reused unchanged)}"
[ -x "$MKBOOTIMG" ] || { echo "mkbootimg not found: set MKBOOTIMG" >&2; exit 1; }
W="$OUT/bootimg"; REL="$(cat "$OUT/kernel/include/config/kernel.release")"
rm -rf "$W"; mkdir -p "$W"

# stock pieces (ramdisk reused unchanged)
"$UNPACK" --boot_img "$STOCK_BOOT" --out "$W/stock" > /dev/null

# device tree
DTS="$DM/arch/arm64/boot/dts"; CPP=(cpp -nostdinc -undef -D__DTS__ -x assembler-with-cpp -I "$DM/include" -I "$K/include" -I "$K/scripts/dtc/include-prefixes" -I "$DTS" -I "$DTS/mediatek")
"${CPP[@]}" "$DTS/mediatek/mt6833.dts" -o "$W/mt6833.pre.dts"; dtc -q -@ -I dts -O dtb -o "$W/mt6833.dtb" "$W/mt6833.pre.dts"
"${CPP[@]}" "$DTS/mediatek/camellia.dts" -o "$W/camellia.pre.dts"; dtc -q -@ -I dts -O dtb -o "$W/camellia.dtbo" "$W/camellia.pre.dts"
fdtoverlay -i "$W/mt6833.dtb" -o "$W/camellia66.dtb" "$W/camellia.dtbo"

# modules: stripped, flat in lib/modules, depmod, modules.load in dependency order
M="$W/stage/lib/modules/$REL"; mkdir -p "$M"
while read -r ko; do
	src=$(find "$OUT/kernel" "$OUT/kernel_device_modules-6.6" "$OUT/vendor" -name "$ko" -print -quit)
	[ -n "$src" ] || { echo "missing module: $ko" >&2; exit 1; }
	"$TC/llvm-strip" --strip-debug -o "$M/$ko" "$src"
done < "$ROOT/camellia/ramdisk-modules.txt"
depmod -b "$W/stage" "$REL" 2>/dev/null
if depmod -e -F "$OUT/kernel/System.map" -b "$W/stage" "$REL" 2>&1 | grep 'unknown symbol'; then echo "unresolved symbols" >&2; exit 1; fi
# arm64 module loader rejects GOT relocations (e.g. from __weak externs): "unsupported RELA relocation: 311"
for ko in "$M"/*.ko; do
	if "$TC/llvm-readelf" -r "$ko" | grep -q -E 'R_AARCH64_(ADR_GOT_PAGE|LD64_GOT)'; then echo "GOT relocation in $(basename "$ko")" >&2; exit 1; fi
done
python3 - "$M" <<'PY'
import sys
d=sys.argv[1]; dep={}
for l in open(d+'/modules.dep'):
    k,v=l.split(':',1); dep[k.strip()]=v.split()
# "softdep X pre: Y" -> load Y before X (first-stage init loads modules.load in order)
pre={}
for l in open(d+'/modules.softdep'):
    w=l.split()
    if len(w)>3 and w[0]=='softdep' and 'pre:' in w:
        i=w.index('pre:'); j=w.index('post:') if 'post:' in w else len(w)
        pre.setdefault(w[1].replace('-','_'),[]).extend(x.replace('-','_') for x in w[i+1:j])
byname={m.rsplit('/',1)[-1][:-3].replace('-','_'):m for m in dep}
# mtk-dsi probes once, and if of_drm_find_panel() fails it unregisters the DSI
# host (deleting the panel's mipi-dsi device) and defers; it was never retried
# (run 17). Load the panels (and backlight ICs) before mediatek-drm so the panel
# binds while mipi_dsi_host_register() creates its device, as with 4.14's built-ins.
for m in dep:
    n=m.rsplit('/',1)[-1][:-3]
    if n.startswith('panel-') or n.startswith('camellia-'):
        pre.setdefault('mediatek_drm',[]).append(n.replace('-','_'))
# The SYST timer (timer-mediatek) becomes tick broadcast device; a powered-off core
# only wakes on it once MCUPM runs, so load it after the lpm platform module, which
# blocks deep idle until the MCUPM handshake (4.14: same block, SYST built in). With
# only the hrtimer broadcast one core never sleeps and MCUSYS never powers off.
pre.setdefault('timer_mediatek',[]).append('mtk_lpm_plat_v1_legacy')
order=[]; seen=set()
def visit(m):
    if m in seen: return
    seen.add(m)
    for x in pre.get(m.rsplit('/',1)[-1][:-3].replace('-','_'),[]):
        if x in byname: visit(byname[x])
    for x in dep.get(m,[]): visit(x)
    order.append(m)
# SoC core first (MediaTek's own builds load these early): with plain name order
# pinctrl-mt6833 came ~45 s in, so the PMIC, its regulators and UFS all waited and
# UFS finally probed without supplies after the deferred-probe timeout and hung.
# clk-mt6833-mmsys has no device table (no alias), scpsys waits on its syscon;
# ufshci needs the ti,syscon-reset controller.
EARLY='''clk-common clk-mt6833 clk-fmeter-mt6833 clk-mt6833-bus clk-mt6833-i2c clk-mt6833-mmsys clk-mt6833-img
clk-mt6833-cam clk-mt6833-vcodec clk-mt6833-mfgcfg clk-mt6833-audiosys pinctrl-mtk-v2 pinctrl-mt6833 reset-ti-syscon
mtk-scpsys-mt6833 mtk_wdt mtk-pmic-wrap spmi-mtk-pmif mtk-spmi-pmic mt6397
mt6359p-regulator mt6315-regulator i2c-mt65xx mt6360-core mt6360-regulator mt635x-auxadc
nvmem_mtk-devinfo mtk_tinysys_ipi mtk-mbox mcupm sspm mtk-dvfsrc mtk-dvfsrc-start
ufs-mediatek-mod mtk-mmc'''.split()
for e in EARLY:
    if e.replace('-','_') in byname: visit(byname[e.replace('-','_')])
for m in sorted(dep): visit(m)
open(d+'/modules.load','w').write('\n'.join(order)+'\n')
PY
mkdir -p "$W/modrd/lib/modules"; cp "$M"/*.ko "$M"/modules.{dep,alias,softdep,load} "$W/modrd/lib/modules/"
[ -f "$M/modules.options" ] && cp "$M/modules.options" "$W/modrd/lib/modules/" || : > "$W/modrd/lib/modules/modules.options"
# FT8722 (flashless TDDI touch): the driver downloads this from a work item at probe; if that runs
# before switch_root it finds it here, after it via firmware_class.path=/vendor/firmware (cmdline;
# 4.14 searched /system/vendor/firmware built in, so stock's cmdline does not carry it)
mkdir -p "$W/modrd/lib/firmware"
for f in focaltech_ts_fw_tianma.bin; do
	debugfs -R "dump /firmware/$f $W/modrd/lib/firmware/$f" "$STOCK_VENDOR" > /dev/null 2>&1
	[ -s "$W/modrd/lib/firmware/$f" ] || { echo "missing /vendor/firmware/$f in $STOCK_VENDOR" >&2; exit 1; }
done
(cd "$W/modrd" && find . | sort | cpio -o -H newc -R 0:0 2>/dev/null | gzip -9) > "$W/modules-ramdisk.cpio.gz"
cat "$W/stock/ramdisk" "$W/modules-ramdisk.cpio.gz" > "$W/ramdisk.gz"
gzip -9 -c "$OUT/kernel/arch/arm64/boot/Image" > "$W/Image.gz"

for v in "A 0x40000000" "B 0x40080000"; do
	set -- $v
	"$MKBOOTIMG" --header_version 2 --os_version 12.0.0 --os_patch_level 2023-09 \
		--kernel "$W/Image.gz" --ramdisk "$W/ramdisk.gz" --dtb "$W/camellia66.dtb" \
		--pagesize 0x800 --base 0 --kernel_offset "$2" --ramdisk_offset 0x51100000 \
		--second_offset 0 --tags_offset 0x47c80000 --dtb_offset 0x47c80000 --board "" \
		--cmdline "bootopt=64S3,32N2,64N2 buildvariant=user log_buf_len=2M console=ttyS0,921600n1 firmware_class.path=/vendor/firmware" \
		-o "$W/boot-6.6-test-$1.img"
done
size=$(stat -c %s "$W/boot-6.6-test-A.img"); [ "$size" -le 67108864 ] || { echo "boot.img too big: $size" >&2; exit 1; }
ls -l "$W"/boot-6.6-test-*.img; sha256sum "$W"/boot-6.6-test-*.img
