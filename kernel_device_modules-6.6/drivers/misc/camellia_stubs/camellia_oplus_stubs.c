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
#include <linux/errno.h>

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

/* camera (imgsensor ISP6S_MON): Oppo's external camera LDOs, charger and devinfo hooks */
int register_device_proc(char *name, char *version, char *vendor)
{
	return 0;
}
EXPORT_SYMBOL(register_device_proc);

int is_fan53870_pmic(void)
{
	return 0;	/* no FAN53870 */
}
EXPORT_SYMBOL(is_fan53870_pmic);

int fan53870_cam_ldo_set_voltage(int LDO_NUM, int set_mv)
{
	return -ENODEV;
}
EXPORT_SYMBOL(fan53870_cam_ldo_set_voltage);

int fan53870_cam_ldo_disable(int LDO_NUM)
{
	return -ENODEV;
}
EXPORT_SYMBOL(fan53870_cam_ldo_disable);

int pmic_ldo_get_type(void)
{
	return -1;	/* no Oppo camera PMIC */
}
EXPORT_SYMBOL(pmic_ldo_get_type);

int pmic_ldo_set_voltage_mv(unsigned int ldo_num, int set_mv)
{
	return -ENODEV;
}
EXPORT_SYMBOL(pmic_ldo_set_voltage_mv);

int pmic_ldo_set_disable(unsigned int ldo_num)
{
	return -ENODEV;
}
EXPORT_SYMBOL(pmic_ldo_set_disable);

void pmic_gpio_enable(int pwr_status)
{
}
EXPORT_SYMBOL(pmic_gpio_enable);

void oplus_chg_set_camera_on(bool val)
{
}
EXPORT_SYMBOL(oplus_chg_set_camera_on);

void oplus_chg_set_camera_status(bool val)
{
}
EXPORT_SYMBOL(oplus_chg_set_camera_status);

MODULE_DESCRIPTION("camellia: neutral stubs for unpublished Oppo helper symbols");
MODULE_LICENSE("GPL");
