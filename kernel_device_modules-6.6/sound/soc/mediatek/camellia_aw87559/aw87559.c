// SPDX-License-Identifier: GPL-2.0
/*
 * Camellia's alternative analog speaker amplifier. Reset sequencing, scene
 * names and the no-DSP vmax ABI follow Xiaomi's 4.14 AW87xxx driver. Register
 * settings come exclusively from the device's existing /vendor/firmware.
 */

#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include "aw87559.h"

#define AW87559_CHIP_ID		0x5a
#define AW87559_CHIP_ID_REG	0x00
#define AW87559_SOFT_RESET	0xaa
#define AW87559_FW_HEADER_SIZE	60
#define AW87559_FW_HEADER_VERSION	0x01000000
#define AW87559_MAX_FW_SIZE	4096
#define AW87559_I2C_RETRIES	5
#define AW87559_XIAOMI_MUSIC_CRC32	0x8c9320cc

struct aw87559_scene {
	u8 *regs;
	size_t size;
};

struct camellia_aw87559 {
	struct i2c_client *client;
	struct gpio_desc *reset;
	struct aw87559_scene scenes[CAMELLIA_AW87559_MODE_COUNT];
	u8 *vmax_table;
	size_t vmax_size;
	u32 channel;
	unsigned int mode;
	int last_error;
};

static DEFINE_MUTEX(aw87559_lock);
static struct camellia_aw87559 *aw87559_amp;
static const char * const aw87559_scene_names[] = {
	"off", "music", "voice", "fm", "rcv",
};

static int aw87559_write(struct camellia_aw87559 *amp, u8 reg, u8 value)
{
	int ret, retry;

	for (retry = 0; retry < AW87559_I2C_RETRIES; retry++) {
		ret = i2c_smbus_write_byte_data(amp->client, reg, value);
		if (!ret)
			return 0;
		usleep_range(2000, 2500);
	}
	dev_err(&amp->client->dev, "write register 0x%02x failed: %d\n",
		reg, ret);
	return ret;
}

static void aw87559_reset(struct camellia_aw87559 *amp, bool enabled)
{
	gpiod_set_value_cansleep(amp->reset, 0);
	usleep_range(2000, 2500);
	if (enabled) {
		gpiod_set_value_cansleep(amp->reset, 1);
		usleep_range(2000, 2500);
	}
}

/* Awinic v1 register files: 60-byte header, LE32 count, byte/byte pairs. */
static int aw87559_parse_scene(struct camellia_aw87559 *amp,
			       const struct firmware *fw,
			       struct aw87559_scene *scene, unsigned int mode)
{
	const u8 *data = fw->data;
	u32 checksum = 0, payload_size, register_count;
	size_t i;

	if (fw->size < AW87559_FW_HEADER_SIZE + 6 ||
	    fw->size > AW87559_MAX_FW_SIZE)
		return -EINVAL;
	if (get_unaligned_le32(data + 4) != AW87559_FW_HEADER_VERSION ||
	    get_unaligned_le32(data + 8) != 0 ||
	    get_unaligned_le32(data + 12) != 1 ||
	    get_unaligned_le32(data + 32) != 1 ||
	    get_unaligned_le32(data + 36) != 1)
		return -EINVAL;
	payload_size = get_unaligned_le32(data + 16);
	if (payload_size != fw->size - AW87559_FW_HEADER_SIZE ||
	    (payload_size - 4) % 2)
		return -EINVAL;
	register_count = get_unaligned_le32(data + AW87559_FW_HEADER_SIZE);
	if (register_count != (payload_size - 4) / 2)
		return -EINVAL;
	for (i = 4; i < fw->size; i++)
		checksum += data[i];
	if (checksum != get_unaligned_le32(data)) {
		/* Xiaomi ships this exact music preset with a stale checksum. */
		if (mode != CAMELLIA_AW87559_MUSIC || fw->size != 142 ||
		    (crc32_le(~0U, data, fw->size) ^ ~0U) != AW87559_XIAOMI_MUSIC_CRC32)
			return -EBADMSG;
		dev_info(&amp->client->dev,
			 "using verified Xiaomi music preset with stale checksum\n");
	}

	/* Stock music firmware has AW87579/0x5b metadata; 4.14 ignores it too. */
	scene->size = payload_size - 4;
	scene->regs = devm_kmemdup(&amp->client->dev,
				   data + AW87559_FW_HEADER_SIZE + 4,
				   scene->size, GFP_KERNEL);
	return scene->regs ? 0 : -ENOMEM;
}

static int aw87559_load_scene(struct camellia_aw87559 *amp, unsigned int mode)
{
	const struct firmware *fw;
	char name[64];
	int ret;

	if (amp->scenes[mode].regs)
		return 0;
	snprintf(name, sizeof(name), "aw87559_pid_5a_%s_%u.bin",
		 aw87559_scene_names[mode], amp->channel);
	ret = request_firmware(&fw, name, &amp->client->dev);
	if (ret)
		return dev_err_probe(&amp->client->dev, ret,
				     "cannot load %s\n", name);
	ret = aw87559_parse_scene(amp, fw, &amp->scenes[mode], mode);
	release_firmware(fw);
	if (ret)
		dev_err(&amp->client->dev, "invalid preset %s: %d\n", name, ret);
	else
		dev_info(&amp->client->dev, "loaded %s (%zu registers)\n",
			 name, amp->scenes[mode].size / 2);
	return ret;
}

static int aw87559_write_scene(struct camellia_aw87559 *amp, unsigned int mode)
{
	struct aw87559_scene *scene = &amp->scenes[mode];
	size_t i;
	int ret;

	for (i = 0; i < scene->size; i += 2) {
		ret = aw87559_write(amp, scene->regs[i], scene->regs[i + 1]);
		if (ret)
			return ret;
	}
	return 0;
}

/* Caller holds aw87559_lock, including while loading firmware or removing. */
static int aw87559_set_mode_locked(struct camellia_aw87559 *amp,
				   unsigned int mode)
{
	int ret = 0;

	if (mode == CAMELLIA_AW87559_OFF) {
		if (amp->mode != CAMELLIA_AW87559_OFF)
			ret = aw87559_write_scene(amp, CAMELLIA_AW87559_OFF);
		aw87559_reset(amp, false);
		amp->mode = CAMELLIA_AW87559_OFF;
	} else {
		/* As on 4.14, require the off preset before enabling the PA. */
		ret = aw87559_load_scene(amp, CAMELLIA_AW87559_OFF);
		if (!ret)
			ret = aw87559_load_scene(amp, mode);
		if (!ret) {
			if (amp->mode == CAMELLIA_AW87559_OFF)
				aw87559_reset(amp, true);
			ret = aw87559_write_scene(amp, mode);
		}
		if (ret) {
			aw87559_reset(amp, false);
			amp->mode = CAMELLIA_AW87559_OFF;
		} else {
			amp->mode = mode;
		}
	}
	amp->last_error = ret;
	dev_info(&amp->client->dev, "scene %s: %d\n",
		 aw87559_scene_names[mode], ret);
	return ret;
}

bool camellia_aw87559_is_present(void)
{
	bool present;

	mutex_lock(&aw87559_lock);
	present = aw87559_amp;
	mutex_unlock(&aw87559_lock);
	return present;
}
EXPORT_SYMBOL_GPL(camellia_aw87559_is_present);

int camellia_aw87559_set_mode(unsigned int mode)
{
	int ret;

	if (mode >= CAMELLIA_AW87559_MODE_COUNT)
		return -EINVAL;
	mutex_lock(&aw87559_lock);
	ret = aw87559_amp ? aw87559_set_mode_locked(aw87559_amp, mode) : -ENODEV;
	mutex_unlock(&aw87559_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(camellia_aw87559_set_mode);

static ssize_t chip_id_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sysfs_emit(buf, "0x%02x\n", AW87559_CHIP_ID);
}
static DEVICE_ATTR_RO(chip_id);

static ssize_t last_error_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct camellia_aw87559 *amp = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&aw87559_lock);
	ret = amp->last_error;
	mutex_unlock(&aw87559_lock);
	return sysfs_emit(buf, "%d\n", ret);
}
static DEVICE_ATTR_RO(last_error);

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct camellia_aw87559 *amp = dev_get_drvdata(dev);
	unsigned int mode;

	mutex_lock(&aw87559_lock);
	mode = amp->mode;
	mutex_unlock(&aw87559_lock);
	return sysfs_emit(buf, "0: off mode\n1: music mode\n2: voice mode\n"
			 "3: fm mode\n4: rcv mode\ncurrent mode:%u\n", mode);
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	unsigned int mode;
	int ret;

	ret = kstrtouint(buf, 0, &mode);
	if (ret)
		return ret;
	ret = camellia_aw87559_set_mode(mode);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(mode);

static int aw87559_load_vmax(struct camellia_aw87559 *amp)
{
	const struct firmware *fw;
	char name[32];
	size_t i;
	u32 threshold, previous = 101;
	int ret;

	if (amp->vmax_table)
		return 0;
	snprintf(name, sizeof(name), "aw87xxx_vmax_%u.bin", amp->channel);
	ret = request_firmware(&fw, name, &amp->client->dev);
	if (ret)
		return ret;
	if (!fw->size || fw->size > AW87559_MAX_FW_SIZE || fw->size % 8) {
		ret = -EINVAL;
		goto out;
	}
	for (i = 0; i < fw->size; i += 8) {
		threshold = get_unaligned_le32(fw->data + i);
		if (threshold > 100 || threshold >= previous) {
			ret = -EINVAL;
			goto out;
		}
		previous = threshold;
	}
	amp->vmax_table = devm_kmemdup(&amp->client->dev, fw->data, fw->size, GFP_KERNEL);
	if (!amp->vmax_table) {
		ret = -ENOMEM;
		goto out;
	}
	amp->vmax_size = fw->size;
out:
	release_firmware(fw);
	return ret;
}

/* The stock audio HAL reads /sys/bus/i2c/devices/6-0058/vmax. */
static ssize_t vmax_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct camellia_aw87559 *amp = dev_get_drvdata(dev);
	union power_supply_propval capacity;
	struct power_supply *battery;
	u32 vmax = 0;
	size_t i;
	int ret;

	battery = power_supply_get_by_name("battery");
	if (!battery)
		return -ENODEV;
	ret = power_supply_get_property(battery, POWER_SUPPLY_PROP_CAPACITY,
					&capacity);
	power_supply_put(battery);
	if (ret)
		return ret;
	if (capacity.intval < 0 || capacity.intval > 100)
		return -ERANGE;
	mutex_lock(&aw87559_lock);
	ret = aw87559_load_vmax(amp);
	if (!ret) {
		for (i = 0; i < amp->vmax_size; i += 8) {
			if (capacity.intval > get_unaligned_le32(amp->vmax_table + i)) {
				vmax = get_unaligned_le32(amp->vmax_table + i + 4);
				break;
			}
		}
	}
	mutex_unlock(&aw87559_lock);
	return ret ? ret : sysfs_emit(buf, "%x\n", vmax);
}

static ssize_t vmax_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u32 value;
	int ret;

	/* On MT6833 the HAL applies vmax in software, just as on stock 4.14. */
	ret = kstrtou32(buf, 0, &value);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(vmax);

static struct attribute *aw87559_attrs[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_last_error.attr,
	&dev_attr_mode.attr,
	&dev_attr_vmax.attr,
	NULL,
};
ATTRIBUTE_GROUPS(aw87559);

static int aw87559_probe(struct i2c_client *client)
{
	struct camellia_aw87559 *amp;
	int ret, retry;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EOPNOTSUPP;
	amp = devm_kzalloc(&client->dev, sizeof(*amp), GFP_KERNEL);
	if (!amp)
		return -ENOMEM;
	amp->client = client;
	/* gpiolib also accepts the stock singular "reset-gpio" property. */
	amp->reset = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(amp->reset))
		return dev_err_probe(&client->dev, PTR_ERR(amp->reset),
				     "cannot request amplifier reset\n");
	of_property_read_u32(client->dev.of_node, "pa-channel", &amp->channel);
	aw87559_reset(amp, true);
	for (retry = 0; retry < AW87559_I2C_RETRIES; retry++) {
		ret = i2c_smbus_read_byte_data(client, AW87559_CHIP_ID_REG);
		if (ret == AW87559_CHIP_ID)
			break;
		usleep_range(2000, 2500);
	}
	if (ret != AW87559_CHIP_ID) {
		aw87559_reset(amp, false);
		return dev_err_probe(&client->dev, ret < 0 ? ret : -ENODEV,
				     "AW87559 not detected (ID/error %d)\n", ret);
	}
	ret = aw87559_write(amp, AW87559_CHIP_ID_REG, AW87559_SOFT_RESET);
	aw87559_reset(amp, false);
	if (ret)
		return ret;
	i2c_set_clientdata(client, amp);
	ret = devm_device_add_groups(&client->dev, aw87559_groups);
	if (ret)
		return ret;
	mutex_lock(&aw87559_lock);
	if (aw87559_amp) {
		mutex_unlock(&aw87559_lock);
		return -EBUSY;
	}
	aw87559_amp = amp;
	mutex_unlock(&aw87559_lock);
	dev_info(&client->dev, "AW87559 detected, chip ID 0x5a, channel %u\n",
		 amp->channel);
	return 0;
}

static void aw87559_remove(struct i2c_client *client)
{
	struct camellia_aw87559 *amp = i2c_get_clientdata(client);

	mutex_lock(&aw87559_lock);
	aw87559_set_mode_locked(amp, CAMELLIA_AW87559_OFF);
	if (aw87559_amp == amp)
		aw87559_amp = NULL;
	mutex_unlock(&aw87559_lock);
}

static void aw87559_shutdown(struct i2c_client *client)
{
	struct camellia_aw87559 *amp = i2c_get_clientdata(client);

	mutex_lock(&aw87559_lock);
	aw87559_set_mode_locked(amp, CAMELLIA_AW87559_OFF);
	mutex_unlock(&aw87559_lock);
}

static const struct of_device_id aw87559_of_match[] = {
	{ .compatible = "awinic,aw87559_pa" },
	{ }
};
MODULE_DEVICE_TABLE(of, aw87559_of_match);

static const struct i2c_device_id aw87559_i2c_ids[] = {
	{ "aw87559_pa", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw87559_i2c_ids);

static struct i2c_driver aw87559_driver = {
	.driver = {
		.name = "camellia-aw87559",
		.of_match_table = aw87559_of_match,
	},
	.probe = aw87559_probe,
	.remove = aw87559_remove,
	.shutdown = aw87559_shutdown,
	.id_table = aw87559_i2c_ids,
};
module_i2c_driver(aw87559_driver);

MODULE_DESCRIPTION("Camellia AW87559 analog speaker amplifier");
MODULE_LICENSE("GPL");
