/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/* ALPS header files */
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#ifndef CONFIG_DEVICE_MODULES_RTC_DRV_MT6397
#include <mtk_rtc.h>
#else
#include <linux/mfd/mt6397/rtc_misc.h>
#endif
#endif

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-DETECT]"

#include "wmt_detect.h"
#include "wmt_gpio.h"

#define INVALID_PIN_ID (0xFFFFFFFF)

/*copied form WMT module*/

int _wmt_detect_output_low(unsigned int id)
{
	if (gpio_ctrl_info.gpio_ctrl_state[id].gpio_num != INVALID_PIN_ID) {
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num, 0);
		WMT_DETECT_PR_INFO("WMT-DETECT: set GPIO%d to output %d\n",
				gpio_ctrl_info.gpio_ctrl_state[id].gpio_num-280,
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num));
	}

	return 0;
}

int _wmt_detect_output_high(unsigned int id)
{
	if (gpio_ctrl_info.gpio_ctrl_state[id].gpio_num != INVALID_PIN_ID) {
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num, 1);
		WMT_DETECT_PR_INFO("WMT-DETECT: set GPIO%d to output %d\n",
				gpio_ctrl_info.gpio_ctrl_state[id].gpio_num-280,
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num));
	}

	return 0;
}

int _wmt_detect_read_gpio_input(unsigned int id)
{
	int retval = 0;

	if (gpio_ctrl_info.gpio_ctrl_state[id].gpio_num != INVALID_PIN_ID) {
		retval = gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num);
		WMT_DETECT_PR_DBG("WMT-DETECT: get GPIO%d val%d\n",
				  gpio_ctrl_info.gpio_ctrl_state[id].gpio_num, retval);
	} else
		WMT_DETECT_PR_ERR("WMT-DETECT: GPIO%d invalid\n",
				  gpio_ctrl_info.gpio_ctrl_state[id].gpio_num);

	return retval;
}

/*This power on sequence must support all combo chip's basic power on sequence
 * 1. LDO control is a must, if external LDO exist
 * 2. PMU control is a must
 * 3. RST control is a must
 * 4. WIFI_EINT pin control is a must, used for GPIO mode for EINT status checkup
 * 5. RTC32k clock control is a must
 *
 */

int wmt_detect_read_ext_cmb_status(void)
{
	int retval = 0;
	/*read WIFI_EINT pin status*/
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num == INVALID_PIN_ID) {
		retval = 0;
		WMT_DETECT_PR_ERR("WMT-DETECT: no WIFI_EINT pin set\n");
	} else {
		retval = _wmt_detect_read_gpio_input(GPIO_WIFI_EINT_PIN);
		WMT_DETECT_PR_INFO("WMT-DETECT: WIFI_EINT input status:%d\n", retval);
	}
	return retval;
}

int wmt_detect_sdio_pwr_ctrl(int on)
{
	int retval = -1;
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	if (on == 0) {
		/*power off SDIO slot */
		retval = board_sdio_ctrl(1, 0);
	} else {
		/*power on SDIO slot */
		retval = board_sdio_ctrl(1, 1);
	}
#else
	WMT_DETECT_PR_WARN("WMT-DETECT: MTK_WCN_COMBO_CHIP_SUPPORT is not set\n");
#endif
	return retval;
}
