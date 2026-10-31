// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_16K 0x4000
#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_8K 0x2000

/*
 * camellia: the GC02M1B depth sensor has no EEPROM; its imgsensor driver reads
 * the OTP (vendor + fuse id) at probe and registers a reader here, since that
 * module loads after this one.
 */
static cam_cal_cmd_func camellia_gc02m1b_reader[2]; /* [0] QTech, [1] AAC */

void camellia_cam_cal_set_gc02m1b_reader(unsigned int sensor_id,
					 cam_cal_cmd_func reader)
{
	camellia_gc02m1b_reader[sensor_id == GC02M1B_AAC_DEPTH_SENSOR_ID] = reader;
}
EXPORT_SYMBOL(camellia_cam_cal_set_gc02m1b_reader);

static unsigned int camellia_gc02m1b_qtech_read(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	cam_cal_cmd_func reader = camellia_gc02m1b_reader[0];

	return reader ? reader(client, addr, data, size) : 0;
}

static unsigned int camellia_gc02m1b_aac_read(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	cam_cal_cmd_func reader = camellia_gc02m1b_reader[1];

	return reader ? reader(client, addr, data, size) : 0;
}

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/* camellia (stock 4.14 mt6853 list) */
	{OV48B_QTECH_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV48B_AAC_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV8856_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{OV8856_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{HI259H_QTECH_MACRO_SENSOR_ID, 0xA4, Common_read_region},
	{HI259H_AAC_MACRO_SENSOR_ID, 0xA4, Common_read_region},
	{OV02B10_AAC_MACRO_SENSOR_ID, 0xA4, Common_read_region},
	{GC02M1B_QTECH_DEPTH_SENSOR_ID, 0x37, camellia_gc02m1b_qtech_read},
	{GC02M1B_AAC_DEPTH_SENSOR_ID, 0x37, camellia_gc02m1b_aac_read},
	{S5K3L6_SENSOR_ID_LIJING, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{SC800CS_SENSOR_ID_LIJING, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID_LIJING, 0x40, Hi846_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID_LIJINGA, 0x40, Hi846a_read_region, MAX_EEPROM_SIZE_16K},
	{HI556_SENSOR_ID_LIJINGA, 0x40, Hi556a_read_region, MAX_EEPROM_SIZE_16K},
	{S5KJN103_SENSOR_ID_LIJING, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	/*Below is commom sensor */
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B. No Cal data for main2 OV8856*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
#ifdef SUPPORT_S5K4H7
	{S5K4H7_SENSOR_ID, 0xA0, zte_s5k4h7_read_region},
	{S5K4H7SUB_SENSOR_ID, 0xA0, zte_s5k4h7_sub_read_region},
#endif
    {IMX882_SENSOR_ID23231, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV32C_SENSOR_ID23231, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID23231, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX709_SENSOR_ID23231, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX882PD_SENSOR_ID23231, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},

	{OV64B_SENSOR_ID23051, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID23051, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX615_SENSOR_ID23051, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV02B10_SENSOR_ID23051, 0xA4, Common_read_region, MAX_EEPROM_SIZE_16K},

	{OV64B_SENSOR_ID22277, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV32C_SENSOR_ID22277, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID22277, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX709_SENSOR_ID22277, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},

	{S5KHP3SP_SENSOR_ID22629, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{IMX615_SENSOR_ID22629, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID22629, 0xA2, Common_read_region},
	{GC02M1_SENSOR_ID22629, 0xA4, Common_read_region},

	{OVA0B4_SENSOR_ID22633, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID22633, 0xA8, Common_read_region},

	{OV64B_SENSOR_ID23035, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV08D_SENSOR_ID23035, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},

	{S5K3L6_SENSOR_ID_FANLI, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KJN103_SENSOR_ID_FANLI, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV08D10_SENSOR_ID_FANLI, 0xA0, Common_read_region},
	{GC02M1_SENSOR_ID_FANLI, 0xA4, Common_read_region},
	{OV64B40_SENSOR_ID_HAWAII,0xA0,Common_read_region,MAX_EEPROM_SIZE_16K},
	{OV08D10_SENSOR_ID_HAWAII, 0xA0, Common_read_region},
	{OV16A1Q_SENSOR_ID_HAWAII, 0xA8, Common_read_region},
	{OV02B1B_SENSOR_ID_HAWAII, 0xA4, Common_read_region},

	{S5K3L6_SENSOR_ID_LIJINGA, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{SC520CS_SENSOR_ID_LIJINGA, 0x20, sc520cs_read_region, MAX_EEPROM_SIZE_16K},
	{IMX882_SENSOR_ID23687, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX890_SENSOR_ID23689, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI1634Q_SENSOR_ID23687, 0xA2, Common_read_region, MAX_EEPROM_SIZE_8K},
	{HI846W_SENSOR_ID23687, 0x42, hi846w_read_region, MAX_EEPROM_SIZE_8K},
	{GC02M1_SENSOR_ID23687, 0xA4, Common_read_region},
	{IMX882PD_SENSOR_ID23687, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KJN1_SENSOR_ID23707, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI1634Q_SENSOR_ID23707, 0xA2, Common_read_region, MAX_EEPROM_SIZE_8K},
	{IMX766_SENSOR_ID22693, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{S5KHM6SP_SENSOR_ID22693, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID22693, 0xA8, Common_read_region},
	{IMX355_SENSOR_ID22693, 0xA2, Common_read_region},
	{GC02M1_SENSOR_ID22693, 0xA4, Common_read_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


