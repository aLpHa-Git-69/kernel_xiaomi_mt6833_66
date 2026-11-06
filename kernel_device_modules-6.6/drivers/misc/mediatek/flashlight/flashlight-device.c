// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "flashlight-core.h"

const struct flashlight_device_id flashlight_id_dual[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-mt6360", 0, 0},
	{0, 1, 0, "flashlights-mt6360", 1, 0},
};
const struct flashlight_device_id flashlight_id_single[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-mt6360", 0, 1},
};
const struct flashlight_device_id flashlight_id_orisa[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "sc6607_24700", 0, 1},
};

const struct flashlight_device_id flashlight_id_aladdin[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-aladdin", 0, 1},
};

const struct flashlight_device_id flashlight_id_sy6560[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-sy6560", 0, 1},
};

const struct flashlight_device_id flashlight_id_atom[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-atom", 0, 1},
};

const struct flashlight_device_id flashlight_id_miami[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights_miami", 0, 1},
};
const struct flashlight_device_id flashlight_id_ark[] = {
	/* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
	{0, 0, 0, "flashlights-ark", 0, 1},
};

