# Complete speaker-test boot image

## Confirmed device test

The full SukiSU boot image built from `907092a3926ada9fe54b90e66e82632519251c84`
successfully booted on the target Camellia phone. The user confirmed that the
internal speaker now works. Runtime checks reported:

- Kernel: `6.6.30-4k-g907092a3926a-dirty`
- AW87559 chip ID: `0x5a`
- Amplifier `last_error`: `0`
- GitHub build run `43736910427`: successful

## Image construction and flashing

The confirmed working image is `boot-camellia-6.6-speaker-fix-sukisu.img`.
The earlier AK3 package omitted nine modules from the working
r1.3 inventory, including the Mali GPU driver. Module staging now uses the
checked-in working inventory, with the AW87559 driver added, and requires every
module to exist and have matching version metadata.

The full image reuses the exact stock MIUI ramdisk gzip member from the verified
r1.3 boot image. This member is byte-identical to the current working slot-B
ramdisk. A new gzip member contains all matching modules and touch firmware.
The original boot-v2 addresses, page size, OS fields and DTB layout are retained;
component sizes and the boot-header checksum are regenerated and verified.

`.github/workflows/camellia-boot-test.yml` builds the SukiSU variant using the
existing compiler cache and publishes the image, checksum and build manifest.

Flash the image to **Boot**, on the intended active slot. The inspected Android
slot was B. Verify the slot in recovery or with `fastboot getvar current-slot`;
for slot B, the fastboot command is:

```sh
fastboot flash boot_b boot-camellia-6.6-speaker-fix-sukisu.img
fastboot reboot
```

The stock ramdisk includes MIUI recovery. After a successful boot, check
`uname -r`, test speaker playback, and collect fresh dmesg/logcat logs. The old
pstore panic captured during diagnosis identifies `g4de2d5fcbb2a`, not the
speaker-test kernel, so fresh logs are needed if this image fails to boot.
