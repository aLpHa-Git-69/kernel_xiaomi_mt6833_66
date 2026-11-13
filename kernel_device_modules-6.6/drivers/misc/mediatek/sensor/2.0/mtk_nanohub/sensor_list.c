// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "<sensorlist> " fmt

#include <linux/module.h>

#include "sensor_list.h"
#include "hf_sensor_type.h"

enum sensorlist {
	accel,
	gyro,
	mag,
	als,
	ps,
	baro,
	sar,
	ois,
	front_cct,
	pwm_rgb,
	flicker,
	rear_als,
	maxhandle,
};

int sensorlist_sensor_to_handle(int sensor)
{
	int handle = -1;

	switch (sensor) {
	case SENSOR_TYPE_ACCELEROMETER:
		handle = accel;
		break;
	case SENSOR_TYPE_GYROSCOPE:
		handle = gyro;
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD:
		handle = mag;
		break;
	case SENSOR_TYPE_LIGHT:
		handle = als;
		break;
	case SENSOR_TYPE_PROXIMITY:
		handle = ps;
		break;
	case SENSOR_TYPE_PRESSURE:
		handle = baro;
		break;
	case SENSOR_TYPE_SAR:
		handle = sar;
		break;
	case SENSOR_TYPE_OIS:
		handle = ois;
		break;
	case SENSOR_TYPE_FRONT_CCT:
                handle = front_cct;
                break;
        case SENSOR_TYPE_PWM_RGB:
                handle = pwm_rgb;
                break;
	case SENSOR_TYPE_FLICKER:
		handle = flicker;
		break;
	case SENSOR_TYPE_REAR_LIGHT:
		handle = rear_als;
		break;
	}
	return handle;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dynamic sensorlist driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
