// SPDX-License-Identifier: GPL-2.0
/*
 * camellia LCD bias regulator (I2C 0x3e, "mediatek,i2c_lcd_bias").
 *
 * Split out of Xiaomi's camellia 4.14 panel driver
 * (panel-nt36672d-tianma-fhdp-dsi-vdo-90hz.c), where it lived; all camellia
 * panel drivers call _lcm_i2c_write_bytes() to program the +/- bias voltage.
 *
 * Also holds what camellia's display drivers shared through the 4.14 kernel:
 * - camellia_bootargs_contains(): 4.14 panel/backlight drivers matched LK's
 *   panel and backlight-IC detection with strstr(saved_command_line, ...),
 *   which 6.6 does not export to modules; LK passes the same text in
 *   /chosen bootargs (panel name also in atag,videolfb-lcmname).
 * - real_refresh/temp_refresh: refresh-rate globals 4.14 kept in mtk_dsi.c.
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/string.h>

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

static struct i2c_client *_lcm_i2c_client;

int real_refresh;
EXPORT_SYMBOL(real_refresh);
int temp_refresh;
EXPORT_SYMBOL(temp_refresh);

bool camellia_bootargs_contains(const char *needle)
{
	struct device_node *chosen = of_find_node_by_path("/chosen");
	const char *str = NULL;
	bool found = false;

	if (!chosen)
		return false;
	if (!of_property_read_string(chosen, "bootargs", &str) && str && strstr(str, needle))
		found = true;
	else if (!of_property_read_string(chosen, "atag,videolfb-lcmname", &str) && str &&
		 strstr(str, needle))
		found = true;
	of_node_put(chosen);
	return found;
}
EXPORT_SYMBOL(camellia_bootargs_contains);

/*
 * Backlight IC chosen by LK, ":bklic=N" in bootargs (24 = KTD3136/3137,
 * 1 = LM3697). -1 if absent. (4.14 drivers parsed saved_command_line
 * without a NULL check.)
 */
int camellia_bklic_id(void)
{
	struct device_node *chosen = of_find_node_by_path("/chosen");
	const char *str = NULL, *p;
	int id = -1;

	if (!chosen)
		return -1;
	if (!of_property_read_string(chosen, "bootargs", &str) && str) {
		p = strstr(str, ":bklic=");
		if (p && kstrtoint(p + strlen(":bklic="), 10, &id))
			id = (int)simple_strtol(p + strlen(":bklic="), NULL, 10);
	}
	of_node_put(chosen);
	return id;
}
EXPORT_SYMBOL_GPL(camellia_bklic_id);

int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { addr, value };
	int ret;

	if (!client) {
		pr_info("[LCM][I2C] bias client not probed, skip 0x%02x=0x%02x\n",
			addr, value);
		return 0;
	}
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][I2C] bias write 0x%02x=0x%02x failed %d\n",
			addr, value, ret);
	return ret;
}
EXPORT_SYMBOL(_lcm_i2c_write_bytes);

static int camellia_lcm_bias_probe(struct i2c_client *client)
{
	pr_info("[LCM][I2C] bias: name=%s addr=0x%x\n", client->name, client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static void camellia_lcm_bias_remove(struct i2c_client *client)
{
	_lcm_i2c_client = NULL;
}

static const struct of_device_id camellia_lcm_bias_of_match[] = {
	{ .compatible = "mediatek,I2C_LCD_BIAS" },	/* matched case-insensitively */
	{},
};
MODULE_DEVICE_TABLE(of, camellia_lcm_bias_of_match);

static const struct i2c_device_id camellia_lcm_bias_id[] = {
	{ LCM_I2C_ID_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, camellia_lcm_bias_id);

static struct i2c_driver camellia_lcm_bias_driver = {
	.id_table = camellia_lcm_bias_id,
	.probe = camellia_lcm_bias_probe,
	.remove = camellia_lcm_bias_remove,
	.driver = {
		.name = LCM_I2C_ID_NAME,
		.of_match_table = camellia_lcm_bias_of_match,
	},
};
module_i2c_driver(camellia_lcm_bias_driver);

MODULE_DESCRIPTION("camellia LCD bias I2C regulator");
MODULE_LICENSE("GPL");
