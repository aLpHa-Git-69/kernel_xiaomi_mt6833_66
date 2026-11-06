// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: neutral stand-ins for vendor helper symbols.
 *
 * Some MediaTek drivers in this tree (display, keypad, thermal, modem,
 * flashlight, SD card, camera) call project-info, device-info, camera-LDO
 * and charger hooks whose providers are not part of the camellia build.
 * These stubs answer "retail build, no project match, nothing registered",
 * so every caller takes MediaTek's standard path.
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

#if 0 /* camellia: unused: register_device_proc_for_emmc */
int register_device_proc_for_emmc(char *emmc, char *emmc_fw, void *mmc)
{
	return 0;
}
EXPORT_SYMBOL(register_device_proc_for_emmc);
#endif /* register_device_proc_for_emmc */

/* camera (imgsensor ISP6S_MON): external camera LDOs, charger and devinfo hooks */
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
	return -1;	/* no external camera PMIC */
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

void camellia_chg_set_camera_on(bool val)
{
}
EXPORT_SYMBOL(camellia_chg_set_camera_on);

void camellia_chg_set_camera_status(bool val)
{
}
EXPORT_SYMBOL(camellia_chg_set_camera_status);

MODULE_DESCRIPTION("camellia: neutral stubs for vendor helper symbols");
MODULE_LICENSE("GPL");
