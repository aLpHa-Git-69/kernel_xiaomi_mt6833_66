#!/bin/bash
# Stage a complete kernel, merged device tree and matching first-stage modules.
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$R/out}"
O="$OUT/kernel"
TC="${CLANG_BIN:?set CLANG_BIN}"
TEMPLATE="${AK3_TEMPLATE:?set AK3_TEMPLATE to camellia r1.3 AnyKernel3.zip}"
VARIANT="${VARIANT:?set VARIANT to base or sukisu}"
export PATH="$TC:$PATH"
REL="$(<"$O/include/config/kernel.release")"
W="$OUT/ak3-stage"
P="$OUT/payload"
DTS="$R/kernel_device_modules-6.6/arch/arm64/boot/dts"
mkdir -p "$W" "$P/lib/modules" "$P/lib/firmware"

CPP=(cpp -nostdinc -undef -D__DTS__ -x assembler-with-cpp
  -I "$R/kernel_device_modules-6.6/include" -I "$R/include"
  -I "$R/scripts/dtc/include-prefixes" -I "$DTS" -I "$DTS/mediatek")
"${CPP[@]}" "$DTS/mediatek/mt6833.dts" -o "$W/mt6833.pre.dts"
"${CPP[@]}" "$DTS/mediatek/camellia.dts" -o "$W/camellia.pre.dts"
dtc -q -@ -I dts -O dtb -o "$W/mt6833.dtb" "$W/mt6833.pre.dts"
dtc -q -@ -I dts -O dtb -o "$W/camellia.dtbo" "$W/camellia.pre.dts"
fdtoverlay -i "$W/mt6833.dtb" -o "$P/dtb" "$W/camellia.dtbo"

python3 "$R/camellia/tools/select-ramdisk-modules.py" "$P/dtb" "$OUT" > "$W/modules-selected.txt"
M="$W/stage/lib/modules/$REL"
mkdir -p "$M"
while read -r ko; do
  src=$(find "$O" "$OUT/kernel_device_modules-6.6" "$OUT/vendor" -name "$ko" -print -quit)
  [ -n "$src" ] || { echo "missing module: $ko" >&2; exit 1; }
  "$TC/llvm-strip" --strip-debug -o "$M/$ko" "$src"
  case "$(modinfo -F vermagic "$M/$ko")" in
    "$REL "*) ;;
    *) echo "vermagic mismatch: $ko" >&2; exit 1;;
  esac
done < "$W/modules-selected.txt"
for ko in camellia_aw87559 camellia_fs1815 mt6833-mt6359p snd-soc-mt6359 snd-soc-mt6833-afe; do
  [ -s "$M/$ko.ko" ] || { echo "missing audio module: $ko" >&2; exit 1; }
done
depmod -b "$W/stage" "$REL"
depmod -e -F "$O/System.map" -b "$W/stage" "$REL" 2> "$W/depmod-check.log"
if grep -q 'unknown symbol' "$W/depmod-check.log"; then
  cat "$W/depmod-check.log" >&2
  exit 1
fi
for ko in "$M"/*.ko; do
  if "$TC/llvm-readelf" -r "$ko" | grep -E 'R_AARCH64_(ADR_GOT_PAGE|LD64_GOT)' > /dev/null; then
    echo "unsupported GOT relocation: $ko" >&2; exit 1
  fi
done

# Keep the early-provider and display ordering used by camellia/mkboot.sh.
python3 - "$M" <<'PY'
import pathlib, sys
d = pathlib.Path(sys.argv[1])
dep = {}
for line in (d / 'modules.dep').read_text().splitlines():
    module, dependencies = line.split(':', 1)
    dep[module] = dependencies.split()
norm = lambda name: name.replace('-', '_')
byname = {norm(pathlib.Path(m).stem): m for m in dep}
pre = {}
for line in (d / 'modules.softdep').read_text().splitlines():
    words = line.split()
    if len(words) > 3 and words[0] == 'softdep' and 'pre:' in words:
        start = words.index('pre:') + 1
        end = words.index('post:') if 'post:' in words else len(words)
        pre.setdefault(norm(words[1]), []).extend(map(norm, words[start:end]))
for name in byname:
    if name.startswith(('panel_', 'camellia_')):
        pre.setdefault('mediatek_drm', []).append(name)
pre.setdefault('timer_mediatek', []).append('mtk_lpm_plat_v1_legacy')
early = '''clk-common clk-mt6833 clk-fmeter-mt6833 clk-mt6833-bus clk-mt6833-i2c
clk-mt6833-mmsys clk-mt6833-img clk-mt6833-cam clk-mt6833-vcodec clk-mt6833-mfgcfg
clk-mt6833-audiosys pinctrl-mtk-v2 pinctrl-mt6833 reset-ti-syscon mtk-scpsys-mt6833
mtk_wdt mtk-pmic-wrap spmi-mtk-pmif mtk-spmi-pmic mt6397 mt6359p-regulator
mt6315-regulator i2c-mt65xx mt6360-core mt6360-regulator mt635x-auxadc
nvmem_mtk-devinfo mtk_tinysys_ipi mtk-mbox mcupm sspm mtk-dvfsrc mtk-dvfsrc-start
ufs-mediatek-mod mtk-mmc'''.split()
order, seen = [], set()
def visit(module):
    if module in seen:
        return
    seen.add(module)
    for name in pre.get(norm(pathlib.Path(module).stem), []):
        if name in byname:
            visit(byname[name])
    for dependency in dep[module]:
        visit(dependency)
    order.append(module)
for name in early:
    if norm(name) in byname:
        visit(byname[norm(name)])
for module in sorted(dep):
    visit(module)
for provider in ('camellia_aw87559.ko', 'camellia_fs1815.ko'):
    if order.index(provider) >= order.index('mt6833-mt6359p.ko'):
        sys.exit('speaker provider is ordered after sound card: ' + provider)
(d / 'modules.load').write_text('\n'.join(order) + '\n')
PY
cp "$M"/*.ko "$M"/modules.{dep,alias,softdep,load} "$P/lib/modules/"
: > "$P/lib/modules/modules.options"
unzip -p "$TEMPLATE" base/lib/firmware/focaltech_ts_fw_tianma.bin > "$P/lib/firmware/focaltech_ts_fw_tianma.bin"
[ -s "$P/lib/firmware/focaltech_ts_fw_tianma.bin" ]
gzip -9 -c "$O/arch/arm64/boot/Image" > "$P/Image.gz"
cp "$O/.config" "$W/kernel.config"
python3 - "$P" "$REL" "$VARIANT" "$R" <<'PY'
import json, pathlib, subprocess, sys
payload, release, variant, root = sys.argv[1:]
info = {
    'kernel_release': release,
    'variant': variant,
    'source_commit': subprocess.check_output(['git', '-C', root, 'rev-parse', 'HEAD'], text=True).strip(),
    'clang': 'r510928',
    'audio_fix': 'AW87559 alternative amplifier and stock vmax ABI',
}
if variant == 'sukisu':
    info['sukisu_commit'] = subprocess.check_output(['git', '-C', root + '/KernelSU', 'rev-parse', 'HEAD'], text=True).strip()
pathlib.Path(payload, 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
print(json.dumps(info, indent=2))
PY
