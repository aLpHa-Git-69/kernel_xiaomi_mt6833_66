# Camellia 6.6 internal-speaker test

The failing logs show the `Ext_Speaker_Amp` DAPM event reaching
`fsm_speaker_onn()`, followed by `fsm_try_init: no found device`.
On the target phone, retrying the FourSemi probe at I2C6 address 0x34 returns
`-ENXIO`. Xiaomi's 4.14 source supports both FourSemi FS1815 and Awinic AW87559;
the 6.6 port previously included only the FourSemi path.

This test adds an AW87559 driver for the existing `awinic,aw87559_pa` node at
0x58. The sound card selects it only after a successful chip-ID probe, and
otherwise uses the FourSemi path. Both amplifier modules are loaded before
the sound card. A failed FourSemi GPIO request now stops its probe, so it
cannot toggle the reset GPIO belonging to the alternative amplifier.

The driver loads the ROM's own `aw87559_pid_5a_*_0.bin` register presets and
checks their lengths, register counts, format and checksums. It also restores
`/sys/bus/i2c/devices/6-0058/vmax`, which the target's
`/vendor/lib/hw/audio.primary.mt6833.so` reads for software volume limiting.
The ROM's `aw87xxx_vmax_0.bin` supplies the original battery-dependent table.

The shipped music preset has a stale checksum. The same file is present in
the vendor source repository (SHA-256
`ed73efc70412df520e3cdcf74c44353e29dc8aca28ddc9037b029c055e01f7f6`).
Only this exact known music file is accepted through a CRC32-verified quirk;
other checksum failures are rejected. Header bytes are never sent as register
settings.

## Build

`.github/workflows/camellia-speaker-test.yml` builds base and SukiSU-Ultra
v4.3.0-r2 variants with Android clang r510928. It runs on pushes to
`speaker-fix-test`. The final artifact is `camellia-speaker-test-AnyKernel3`.
The verified r1.3 release supplies the AnyKernel3 installer and touch firmware;
both kernels, merged DTBs and all kernel modules are newly built.

Module staging checks matching vermagic, unresolved symbols, unsupported GOT
relocations and speaker-provider load order. Packaging validates the arm64
Image, DTB, module list and ZIP integrity and writes a SHA-256 checksum.

## Device test

1. Flash the test AnyKernel3 ZIP. Volume Up selects SukiSU-Ultra; Volume Down
   selects base. Start speaker playback at a low volume after boot.
2. Test music/video, ringtone, call speakerphone, normal call audio, wired
   earphones and Bluetooth. Also stop and restart playback and test after
   disconnecting earphones.
3. While media is playing, capture:

   ```sh
   uname -r
   su -c dmesg > /storage/emulated/0/Download/dmesg-speaker-test.log
   su -c 'logcat -b all -d' > /storage/emulated/0/Download/logcat-speaker-test.log
   su -c 'cat /sys/bus/i2c/devices/6-0058/chip_id /sys/bus/i2c/devices/6-0058/mode /sys/bus/i2c/devices/6-0058/last_error /sys/bus/i2c/devices/6-0058/vmax'
   ```

On an AW87559 phone, expect `chip_id` to be `0x5a`, the current mode to be 1
while playing and `last_error` to be 0. Dmesg should contain `AW87559 detected`
and `scene music: 0`. Capture the logs promptly after reboot so probe messages
remain available; the test installer increases the kernel log buffer to 16 MiB.
