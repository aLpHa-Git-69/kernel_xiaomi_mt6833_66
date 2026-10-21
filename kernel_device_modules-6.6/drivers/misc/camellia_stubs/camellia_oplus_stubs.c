// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: neutral stand-ins for Oppo (oplus) helper symbols.
 *
 * Oppo's MT6833 6.6 release calls a few functions from Oppo modules that are
 * not published (oplus_project, device_info) from inside MediaTek drivers
 * (display, keypad, thermal, modem, flashlight, SD card). These stubs answer
 * "retail build, not an Oppo project, nothing registered", so every caller
 * takes MediaTek's standard path.
 */
#include <linux/module.h>

unsigned int get_eng_version(void)
{
	return 0;	/* OEM_RELEASE */
}
EXPORT_SYMBOL(get_eng_version);

unsigned int is_project(int project)
{
	return 0;
}
EXPORT_SYMBOL(is_project);

unsigned int get_PCB_Version(void)
{
	return 0;
}
EXPORT_SYMBOL(get_PCB_Version);

int register_device_proc_for_emmc(char *emmc, char *emmc_fw, void *mmc)
{
	return 0;
}
EXPORT_SYMBOL(register_device_proc_for_emmc);

MODULE_DESCRIPTION("camellia: neutral stubs for unpublished Oppo helper symbols");
MODULE_LICENSE("GPL");
