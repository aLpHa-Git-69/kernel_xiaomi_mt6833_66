// SPDX-License-Identifier: GPL-2.0
/*
 * FPC1022 fingerprint sensor (SPI5, Microtrust FPC TA), ported from Xiaomi
 * 4.14 drivers/input/fingerprint/fpc/fpc1022_tee.c.
 *
 * camellia ships either a Goodix or an FPC sensor on the same SPI5 bus and
 * fingerprint DT node. gf_probe() reads the Goodix chip id and, when it does
 * not match, leaves the bus in spi_fingerprint; fpc1022_init() then reads the
 * FPC hardware id, selects the FPC TA and exposes the sysfs nodes the vendor
 * HAL uses under /sys/bus/platform/devices/fingerprint/.
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include "teei_fp.h"
#include "tee_client_api.h"
#include "gf_spi_tee.h"

#define FPC1022_RESET_LOW_US 5000
#define FPC1022_RESET_HIGH1_US 100

#define FPC_TTW_HOLD_TIME 2000

#define FPC102X_REG_HWID 252
#define FPC1022_CHIP 0x1000
#define FPC1022_CHIP_MASK_SENSOR_TYPE 0xf000

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

static struct TEEC_UUID uuid_ta_fpc = { 0x7778c03f, 0xc30c, 0x4dd0,
	{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };

struct fpc1022_data {
	struct device *dev;
	struct spi_device *spi;
	int irq_gpio;
	int irq_num;
	struct pinctrl *pinctrl;
	struct pinctrl_state *st_irq;
	struct pinctrl_state *st_rst_l;
	struct pinctrl_state *st_rst_h;

	struct input_dev *idev;
	char idev_name[32];
	int event_type;
	int event_code;
	struct mutex lock;
	bool wakeup_enabled;
	struct wakeup_source *ttw_wl;
};

bool fpc1022_fp_exist;
static bool fpc1022_registered;
static struct fpc1022_data *fpc1022;
static u32 spi_speed = 1 * 1000000;

static void fpc1022_get_irqNum(struct fpc1022_data *fpc1022)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,fpc1022_irq");
	if (!node) {
		pr_err("%s can't find compatible node\n", __func__);
		return;
	}

	fpc1022->irq_num = irq_of_parse_and_map(node, 0);
	fpc1022->irq_gpio = of_get_named_gpio(node, "fpc,gpio_irq", 0);
	of_node_put(node);
	pr_info("%s, irq_num = %d, irq_gpio = %d\n",
		__func__, fpc1022->irq_num, fpc1022->irq_gpio);
	if (!fpc1022->irq_num)
		pr_err("irq_of_parse_and_map fail!!\n");
}

static int hw_reset(struct fpc1022_data *fpc1022)
{
	struct device *dev = fpc1022->dev;

	pinctrl_select_state(fpc1022->pinctrl, fpc1022->st_rst_h);
	usleep_range(FPC1022_RESET_HIGH1_US, FPC1022_RESET_HIGH1_US + 100);

	pinctrl_select_state(fpc1022->pinctrl, fpc1022->st_rst_l);
	usleep_range(FPC1022_RESET_LOW_US, FPC1022_RESET_LOW_US + 100);

	pinctrl_select_state(fpc1022->pinctrl, fpc1022->st_rst_h);
	usleep_range(FPC1022_RESET_HIGH1_US, FPC1022_RESET_HIGH1_US + 100);

	dev_info(dev, "IRQ after reset %d\n", gpio_get_value(fpc1022->irq_gpio));

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	int ret;
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset")))
		ret = hw_reset(fpc1022);
	else
		return -EINVAL;
	return ret ? ret : count;
}

static DEVICE_ATTR(hw_reset, 0200, NULL, hw_reset_set);

/* whether the driver may wake up the platform on interrupt */
static ssize_t wakeup_enable_set(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	if (!strncmp(buf, "enable", strlen("enable"))) {
		fpc1022->wakeup_enabled = true;
		smp_wmb();
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		fpc1022->wakeup_enabled = false;
		smp_wmb();
	} else
		return -EINVAL;

	return count;
}

static DEVICE_ATTR(wakeup_enable, 0200, NULL, wakeup_enable_set);

/* send an event to make the system interactive */
static ssize_t do_wakeup_set(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	if (count > 0) {
		input_report_key(fpc1022->idev, KEY_POWER, 1);
		input_report_key(fpc1022->idev, KEY_POWER, 0);
		input_sync(fpc1022->idev);
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(do_wakeup, 0200, NULL, do_wakeup_set);

/* the HAL gates the SPI5 clock around TA calls: '1' on, '0' off */
static ssize_t clk_enable_set(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	if (!fpc1022->spi) {
		dev_err(fpc1022->dev, "spi clk NULL %s\n", __func__);
		return 0;
	}

	if (*buf == '1')
		mt_spi_enable_master_clk(fpc1022->spi);
	if (*buf == '0')
		mt_spi_disable_master_clk(fpc1022->spi);

	return 1;
}

static DEVICE_ATTR(clk_enable, 0200, NULL, clk_enable_set);

/* the irq handler sysfs_notify()s this node; userland polls it */
static ssize_t irq_get(struct device *device,
		       struct device_attribute *attribute, char *buffer)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc1022->irq_gpio);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/* writing just returns success, used for latency measurement */
static ssize_t irq_ack(struct device *device,
		       struct device_attribute *attribute,
		       const char *buffer, size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(device);

	dev_dbg(fpc1022->dev, "%s\n", __func__);
	return count;
}

static DEVICE_ATTR(irq, 0600, irq_get, irq_ack);

static ssize_t fpc_ic_is_exist(struct device *device,
			       struct device_attribute *attribute, char *buffer)
{
	return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc1022_fp_exist ? 1 : 0);
}

static DEVICE_ATTR(fpid_get, 0400, fpc_ic_is_exist, NULL);

static struct attribute *attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_do_wakeup.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	&dev_attr_fpid_get.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static irqreturn_t fpc1022_irq_handler(int irq, void *handle)
{
	struct fpc1022_data *fpc1022 = handle;

	smp_rmb();
	__pm_wakeup_event(fpc1022->ttw_wl, FPC_TTW_HOLD_TIME);

	sysfs_notify(&fpc1022->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}

static int fpc1022_platform_probe(struct platform_device *pldev)
{
	int ret = 0;
	int irqf;
	u32 val;
	const char *idev_name;
	struct device *dev = &pldev->dev;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "no of node found\n");
		return -EINVAL;
	}

	fpc1022 = devm_kzalloc(dev, sizeof(*fpc1022), GFP_KERNEL);
	if (!fpc1022)
		return -ENOMEM;

	fpc1022->dev = dev;
	fpc1022->spi = spi_fingerprint;
	dev_set_drvdata(dev, fpc1022);

	fpc1022->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fpc1022->pinctrl)) {
		dev_err(dev, "Cannot find pinctrl!");
		return PTR_ERR(fpc1022->pinctrl);
	}
	fpc1022->st_irq = pinctrl_lookup_state(fpc1022->pinctrl, "default");
	if (IS_ERR(fpc1022->st_irq))
		dev_err(dev, "pinctrl err, irq\n");

	fpc1022->st_rst_h = pinctrl_lookup_state(fpc1022->pinctrl, "reset_high");
	if (IS_ERR(fpc1022->st_rst_h)) {
		dev_err(dev, "pinctrl err, rst_high\n");
		return PTR_ERR(fpc1022->st_rst_h);
	}

	fpc1022->st_rst_l = pinctrl_lookup_state(fpc1022->pinctrl, "reset_low");
	if (IS_ERR(fpc1022->st_rst_l)) {
		dev_err(dev, "pinctrl err, rst_low\n");
		return PTR_ERR(fpc1022->st_rst_l);
	}
	fpc1022_get_irqNum(fpc1022);

	ret = of_property_read_u32(np, "fpc,event-type", &val);
	fpc1022->event_type = ret < 0 ? EV_MSC : val;

	ret = of_property_read_u32(np, "fpc,event-code", &val);
	fpc1022->event_code = ret < 0 ? MSC_SCAN : val;

	fpc1022->idev = devm_input_allocate_device(dev);
	if (!fpc1022->idev) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	input_set_capability(fpc1022->idev, fpc1022->event_type,
			     fpc1022->event_code);

	if (!of_property_read_string(np, "input-device-name", &idev_name)) {
		fpc1022->idev->name = idev_name;
	} else {
		snprintf(fpc1022->idev_name, sizeof(fpc1022->idev_name),
			 "fpc_irq@%s", dev_name(dev));
		fpc1022->idev->name = fpc1022->idev_name;
	}

	/* Also register the key for wake up */
	set_bit(EV_KEY, fpc1022->idev->evbit);
	set_bit(EV_PWR, fpc1022->idev->evbit);
	set_bit(KEY_WAKEUP, fpc1022->idev->keybit);
	set_bit(KEY_POWER, fpc1022->idev->keybit);
	ret = input_register_device(fpc1022->idev);
	fpc1022->wakeup_enabled = false;
	if (ret) {
		dev_err(dev, "failed to register input device\n");
		return ret;
	}

	irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}

	mutex_init(&fpc1022->lock);

	fpc1022->ttw_wl = wakeup_source_register(dev, "fpc_ttw_wl");

	ret = devm_request_threaded_irq(dev, fpc1022->irq_num,
					NULL, fpc1022_irq_handler, irqf,
					dev_name(dev), fpc1022);
	if (ret) {
		dev_err(dev, "could not request irq %d\n", fpc1022->irq_num);
		goto err_request_irq;
	}
	dev_info(dev, "requested irq %d\n", fpc1022->irq_num);

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(fpc1022->irq_num);

	ret = sysfs_create_group(&dev->kobj, &attribute_group);
	if (ret) {
		dev_err(dev, "could not create sysfs\n");
		goto err_sysfs;
	}

	hw_reset(fpc1022);
	dev_info(dev, "%s: ok\n", __func__);

	return 0;

err_sysfs:
	/* the handler takes ttw_wl: free the IRQ (waits for it) before the wake source */
	disable_irq_wake(fpc1022->irq_num);
	devm_free_irq(dev, fpc1022->irq_num, fpc1022);
err_request_irq:
	wakeup_source_unregister(fpc1022->ttw_wl);
	mutex_destroy(&fpc1022->lock);
	return ret;
}

static int fpc1022_platform_remove(struct platform_device *pldev)
{
	struct device *dev = &pldev->dev;
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	sysfs_remove_group(&dev->kobj, &attribute_group);
	/* the handler takes ttw_wl: free the IRQ (waits for it) before the wake source */
	disable_irq_wake(fpc1022->irq_num);
	devm_free_irq(dev, fpc1022->irq_num, fpc1022);
	device_init_wakeup(dev, 0);

	wakeup_source_unregister(fpc1022->ttw_wl);
	mutex_destroy(&fpc1022->lock);

	return 0;
}

static const struct of_device_id fpc1022_of_match[] = {
	{ .compatible = "mediatek,fpc1022_irq", },
	{}
};
MODULE_DEVICE_TABLE(of, fpc1022_of_match);

static struct platform_driver fpc1022_driver = {
	.driver = {
		.name = "fpc1022_irq",
		.of_match_table = fpc1022_of_match,
	},
	.probe = fpc1022_platform_probe,
	.remove = fpc1022_platform_remove,
};

static void spi_read_hwid(struct spi_device *spi, u8 *rx_buf)
{
	struct spi_message msg;
	struct spi_transfer xfer[2] = { 0 };
	u8 tmp_buf[10] = { 0 };

	spi_message_init(&msg);

	tmp_buf[0] = FPC102X_REG_HWID;
	xfer[0].tx_buf = tmp_buf;
	xfer[0].len = 1;
	xfer[0].delay.value = 5;
	xfer[0].delay.unit = SPI_DELAY_UNIT_USECS;
	xfer[0].speed_hz = spi_speed;
	spi_message_add_tail(&xfer[0], &msg);

	xfer[1].tx_buf = tmp_buf + 2;
	xfer[1].rx_buf = tmp_buf + 4;
	xfer[1].len = 2;
	xfer[1].delay.value = 5;
	xfer[1].delay.unit = SPI_DELAY_UNIT_USECS;
	xfer[1].speed_hz = spi_speed;
	spi_message_add_tail(&xfer[1], &msg);

	if (spi_sync(spi, &msg))
		dev_err(&spi->dev, "%s : spi_sync failed.\n", __func__);

	memcpy(rx_buf, tmp_buf + 4, 2);
}

static int check_hwid(struct spi_device *spi)
{
	u32 time_out = 0;
	u8 tmp_buf[2] = { 0 };
	u16 hardware_id;

	do {
		spi_read_hwid(spi, tmp_buf);
		time_out++;

		hardware_id = (tmp_buf[0] << 8) | tmp_buf[1];
		pr_info("fpc hardware_id = 0x%x\n", hardware_id);

		if ((FPC1022_CHIP_MASK_SENSOR_TYPE & hardware_id) == FPC1022_CHIP) {
			pr_info("fpc1022 chip version check pass, time_out=%d\n", time_out);
			return 0;
		}
	} while (time_out < 2);

	pr_info("%s, fpc1022 chip version read failed, time_out=%d\n",
		__func__, time_out);

	return -1;
}

/* called by gf_init() once gf_probe() has had its go at the sensor */
int fpc1022_init(void)
{
	int ret;

	if (goodix_fp_exist) {
		pr_info("%s goodix sensor has been detected, so exit FPC sensor detect.\n",
			__func__);
		return -ENODEV;
	}

	if (!spi_fingerprint) {
		pr_notice("%s spi device is NULL, cannot spi transfer\n", __func__);
	} else {
		if (check_hwid(spi_fingerprint) < 0) {
			pr_notice("%s get chipid fail, now exit\n", __func__);
			return -ENODEV;
		}
		fpc1022_fp_exist = true;
		memcpy(&uuid_fp, &uuid_ta_fpc, sizeof(struct TEEC_UUID));
		pr_info("%s, detected sensor\n", __func__);
	}

	ret = platform_driver_register(&fpc1022_driver);
	fpc1022_registered = !ret;
	return ret;
}

void fpc1022_exit(void)
{
	if (fpc1022_registered)
		platform_driver_unregister(&fpc1022_driver);
}
