#!/usr/bin/env python3
import glob, os, re, subprocess, sys


def run(args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit('select-ramdisk-modules: %s failed (%d): %s' % (' '.join(args), r.returncode, r.stderr.strip()))
    return r.stdout

dtb, out = sys.argv[1], sys.argv[2]
ESSENTIAL = """ufs_mediatek_mod pinctrl_mt6833 mtk_scpsys_mt6833 mt6359p_regulator mtk_wdt
clk_mt6833 nvmem_mt635x_efuse mt6833_mt6359p camellia_fs1815 camellia_aw87559 mtk_pmic_wrap spmi_mtk_pmif mt6397 mtk_spmi_pmic camellia_stubs mtk_boot_common camellia_ion camellia_cpumaxfreq
btif_drv wmt_drv wmt_chrdev_wifi wlan_drv_gen4m_6833 bt_drv_connac1x regulator_vibrator scp hf_manager nanohub gps_drv ccci_ccif ccci_cldma ccci_dpmaif camellia_goodix_fp st21nfc
mtk_tinysys_ipi sspm mcupm mtk_mbox_mailbox mtk_dvfsrc_devfreq rpmb rpmb_mtk mtk_sec_heap
system_heap mtk_iommu mtk_smi mtk_mmc mtk_pmic_keys mtk_kpd
mtk_charger_framework mt6360_pmu_chg mtk_pd_charging tcpc_mt6360 tcpc_class mtk_battery_manager
mtk_hvbpc mt6359p_battery mtk_chg_type_det
mtk_dvfsrc mtk_dvfsrc_start
camellia_lcm_bias panel_ft8722_tianma_fhdp_dsi_vdo_90hz panel_nt36672c_tianma_fhdp_dsi_vdo_90hz
panel_nt36672c_truly_fhdp_dsi_vdo_90hz panel_nt36672c_truly2_fhdp_dsi_vdo_90hz panel_nt36672d_tianma_fhdp_dsi_vdo_90hz
camellia_ti_lmu camellia_ktd3136 camellia_lm3697_bl leds_mtk_disp leds_mtk
camellia_xiaomi_touch camellia_ft8722_ts camellia_nt36672c_ts
musb_main tcpci_late_sync
monitor_hang mtk_printk_ctrl
fhctl zsmalloc zram
archcounter_timesync cam_qos camera_isp camera_dip_isp6s camera_mfb_isp6s camera_rsc_isp6s camera_wpe_isp6s
camera_fdvt_isp51 camera_dpe_isp60 ccu_isp6s camellia_main_lens fmradio_drv_mt6631_6635
isee clk_fmeter_mt6833 timer_mediatek""".split()

BZL = os.path.join(os.path.dirname(__file__), '../../kernel_device_modules-6.6/kernel/kleaf/mgk_64.bzl')
MT6833_SKIP = re.compile(r'test|fake|camera_|mcDrvModule|t-base|cmdq-bdg|^clk-(chk|dbg|fmeter)|^pd-chk|device-apc')
def mt6833_block():
    lines = open(BZL).read().split('\n')
    i = next(n for n, l in enumerate(lines) if '"mt6833_overlay.config" in DEFCONFIG_OVERLAYS' in l)
    j = next(n for n in range(i + 1, len(lines)) if re.match(r'    (if|elif) |def |\S', lines[n]))
    blk = '\n'.join(lines[i:j])
    base = lambda m: os.path.basename(m)[:-3]
    add = set(re.findall(r'append\("(drivers/[^"]+\.ko)"', blk))
    rm = set(re.findall(r'(?:remove|pop)\("(drivers/[^"]+\.ko)"', blk))
    add |= set(re.findall(r'"(drivers/[^"]+\.ko)"\s*:\s*"mt6833"', open(BZL).read()))
    return {base(m) for m in add - rm}, rm
MT6833_ADD, MT6833_RM = mt6833_block()
PREFER = ['ufs-mediatek-mod']
MT6833_RM |= {'drivers/misc/mediatek/lpm/modules/debug/v1/mtk-lpm-dbg-common-v1.ko',
              'drivers/misc/mediatek/lpm/modules/debug/v2/mtk-lpm-dbg-common-v2.ko'}

dts = run(['dtc', '-I', 'dtb', '-O', 'dts', dtb])
comp = set(re.findall(r'"([^"]+)"', ' '.join(re.findall(r'compatible = ([^;]+);', dts))))
mods = {}
for p in [q for q in glob.glob(f'{out}/kernel/**/*.ko', recursive=True)
          if not q.startswith((f'{out}/kernel/kernel_device_modules-6.6/', f'{out}/kernel/vendor/'))] + glob.glob(f'{out}/kernel_device_modules-6.6/**/*.ko', recursive=True) \
        + glob.glob(f'{out}/vendor/**/*.ko', recursive=True):
    mods.setdefault(os.path.basename(p)[:-3], p)
norm = lambda s: s.replace('-', '_')
builtin = {norm(os.path.basename(l.strip())[:-3]) for l in open(f'{out}/kernel/modules.builtin')}
mods = {k: v for k, v in mods.items() if norm(k) not in builtin}
byn = {norm(k): k for k in mods}
info = {}
for m, p in mods.items():
    o = run(['modinfo', '-F', 'alias', p])
    d = run(['modinfo', '-F', 'depends', p]).strip()
    info[m] = (set(re.findall(r'of:N\*T\*C([^\s*]+)', o)), [x for x in d.split(',') if x])
SUBST = {'scpsys-dummy': 'mtk-scpsys'}
info = {m: (a, [SUBST.get(x, x) for x in d]) for m, (a, d) in info.items()}
seed = {m for m, (a, _) in info.items() if a & comp} | {byn[norm(e)] for e in ESSENTIAL if norm(e) in byn}
seed |= {byn[norm(m)] for m in MT6833_ADD if norm(m) in byn and not MT6833_SKIP.search(m)}
NEVER = {'clk-disable-unused', 'mtk-pm-domain-disable-unused', 'cmdq-test'}
NEVER |= set(SUBST)
NEVER |= {'mtk-vcu', 'mtk-vcodec-common', 'mtk-vcodec-dec-v1', 'mtk-vcodec-enc-v1'}
relpath = lambda m: re.sub(r'^.*?/(kernel|kernel_device_modules-6\.6|vendor)/', '', mods[m])
removed = lambda m: relpath(m) in MT6833_RM
seed = {m for m in seed if not removed(m) and m not in NEVER}
missing = [e for e in ESSENTIAL if norm(e) not in byn]
if missing:
    print('not built (skipped):', ' '.join(missing), file=sys.stderr)
palias = {}
for m, p in mods.items():
    if removed(m):
        continue
    for a in run(['modinfo', '-F', 'alias', p]).split():
        if a.startswith('platform:'):
            palias.setdefault(a[len('platform:'):], set()).add(m)
def mfd_children(m):
    nm = run(['llvm-nm', '-u', mods[m]])
    if 'mfd_add_devices' not in nm:
        return []
    strs = set(re.findall(rb'[\x20-\x7e]{3,}', open(mods[m], 'rb').read()))
    return [c for name, cs in palias.items() if name.encode() in strs for c in cs
            if not re.match(r'mt63(6[1-9]|7\d)', c)]
sel, st = set(), list(seed)
while st:
    m = st.pop()
    if m in sel:
        continue
    sel.add(m)
    st += [byn[norm(d)] for d in info[m][1] if norm(d) in byn]
    st += mfd_children(m)
if any(removed(m) for m in sel):
    sys.exit('selected modules mt6833 removes: %s' % ' '.join(sorted(m for m in sel if removed(m))))
exp = {}
for m in sel:
    nm = run(['llvm-nm', mods[m]])
    for sym in re.findall(r' __ksymtab_(\S+)', nm):
        exp.setdefault(sym, set()).add(m)
dups = {frozenset(v) for v in exp.values() if len(v) > 1}
bad = []
pick = lambda d: [m for m in d if norm(m) in {norm(x) for x in MT6833_ADD} | {norm(x) for x in PREFER}]
for d in sorted(dups, key=lambda d: len(pick(d)) != 1):
    d = d & sel
    if len(d) < 2:
        continue
    keep = pick(d)
    if len(keep) != 1:
        bad.append(' '.join(sorted(d)))
        continue
    sel -= d - set(keep)
    print('duplicate exports: kept %s, dropped %s' % (keep[0], ' '.join(sorted(d - set(keep)))), file=sys.stderr)
if bad:
    sys.exit('modules export the same symbols, cannot pick one:\n  ' + '\n  '.join(sorted(bad)))
print('\n'.join(sorted(f'{m}.ko' for m in sel)))
print(f'{len(sel)} modules', file=sys.stderr)
