/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CAMELLIA_THERMAL_MESSAGE_H__
#define __CAMELLIA_THERMAL_MESSAGE_H__

struct thermal_cooling_device;

int camellia_thermal_message_init(struct thermal_cooling_device *cdev);
void camellia_thermal_message_exit(void);

#endif
