// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * camellia: /dev/irtx char device as on 4.14. The vendor consumerir HAL
 * (consumerir.common) builds the PWM bit pattern itself and drives it with
 * the IRTX_IOC_* ioctls; it does not speak LIRC.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#include <mt-plat/mtk_pwm.h>
#include <mt-plat/mtk_pwm_hal.h>

#define IRTX_PWM_CLOCK (26000000)
#define IRTX_MAX_LEN SZ_1M

#define IRTX_IOC_SET_CARRIER_FREQ   _IOW('R', 0, unsigned int)
#define IRTX_IOC_GET_SOLUTTION_TYPE _IOR('R', 1, unsigned int)
#define IRTX_IOC_SET_DUTY_CYCLE     _IOW('R', 2, unsigned int)
#define IRTX_IOC_SET_IRTX_LED_EN    _IOW('R', 10, unsigned int)

static const char * const irtx_gpio_cfg[] = {
	"irtx_gpio_led_default", "irtx_gpio_led_set" };

static struct {
	struct device *dev;
	struct pinctrl *pinctrl;
	unsigned int pwm_data_invert;
	unsigned int carrier;
	unsigned int cycle;
	unsigned int duty_cycle;
	atomic_t usage_cnt;
} irtx = {
	.carrier = 38009,
	.cycle = 3,
	.duty_cycle = 1,
};

static struct pwm_spec_config irtx_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_MEMORY,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
	.PWM_MODE_MEMORY_REGS.GDURATION = 0,
	.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,
};

static void switch_irtx_gpio(int mode)
{
	struct pinctrl_state *st = pinctrl_lookup_state(irtx.pinctrl,
							irtx_gpio_cfg[mode]);

	if (IS_ERR(st) || pinctrl_select_state(irtx.pinctrl, st))
		dev_notice(irtx.dev, "pinctrl %s failed\n", irtx_gpio_cfg[mode]);
}

static int irtx_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&irtx.usage_cnt, 0, 1) != 0)
		return -EBUSY;
	return nonseekable_open(inode, file);
}

static int irtx_release(struct inode *inode, struct file *file)
{
	atomic_set(&irtx.usage_cnt, 0);
	return 0;
}

/* Buffer: PWM bit pattern in 32-bit words, last word = total time in us */
static ssize_t irtx_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	int buf_size = (count + 3) / 4;
	dma_addr_t wave_phy;
	u32 *wave;
	int h_l_period, total_time, i;
	ssize_t ret;

	if (count < 3 * sizeof(u32) || count > IRTX_MAX_LEN)
		return -EINVAL;

	wave = dma_alloc_coherent(irtx.dev, count, &wave_phy, GFP_KERNEL);
	if (!wave)
		return -ENOMEM;

	if (copy_from_user(wave, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	if (irtx.pwm_data_invert)
		for (i = 0; i < count; i++)
			((u8 *)wave)[i] = ~((u8 *)wave)[i];

	/* cycle: whole carrier period, duty_cycle: high part, in PWM bits */
	/* 64-bit: 26 MHz * duty overflows u32 for duty >= 166; HDURATION is u16 */
	h_l_period = DIV64_U64_ROUND_CLOSEST((u64)IRTX_PWM_CLOCK * irtx.duty_cycle,
					     (u64)irtx.carrier * irtx.cycle);
	if (h_l_period < 1 || h_l_period > 0x10000) {
		ret = -EINVAL;
		goto out;
	}
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.HDURATION = h_l_period - 1;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.LDURATION = h_l_period - 1;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = wave_phy;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = buf_size - 2;

	switch_irtx_gpio(1);
	ret = pwm_set_spec_config(&irtx_pwm_config);
	if (ret < 0) {
		dev_err(irtx.dev, "pwm config failed %zd\n", ret);
		switch_irtx_gpio(0);
		goto out;
	}

	total_time = wave[buf_size - 1];
	if (total_time <= 0)
		total_time = (u64)(count - 4) * 8 * h_l_period * USEC_PER_SEC /
			     IRTX_PWM_CLOCK;
	dev_dbg(irtx.dev, "ret %zd period %d count %zu total %dus duty %u/%u\n",
		ret, h_l_period, count, total_time, irtx.duty_cycle, irtx.cycle);
	usleep_range(total_time, total_time + 100);

	mt_pwm_disable(irtx_pwm_config.pwm_no, irtx_pwm_config.pmic_pad);
	switch_irtx_gpio(0);
	ret = count;
out:
	dma_free_coherent(irtx.dev, count, wave, wave_phy);
	return ret;
}

static long irtx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int para;

	if (cmd == IRTX_IOC_GET_SOLUTTION_TYPE)
		return put_user(1, (unsigned int __user *)arg);

	if (get_user(para, (unsigned int __user *)arg))
		return -EFAULT;

	switch (cmd) {
	case IRTX_IOC_SET_IRTX_LED_EN:
		switch_irtx_gpio((para & 0xF) ? 1 : 0);
		return 0;
	case IRTX_IOC_SET_CARRIER_FREQ:
		if (!para)
			return -EINVAL;
		irtx.carrier = para;
		return 0;
	case IRTX_IOC_SET_DUTY_CYCLE:
		if (!(para & 0xFFFF))
			return -EINVAL;
		irtx.cycle = para & 0xFFFF;
		irtx.duty_cycle = (para >> 16) & 0xFFFF;
		return 0;
	}
	return -ENOTTY;
}

static const struct file_operations irtx_fops = {
	.owner = THIS_MODULE,
	.open = irtx_open,
	.release = irtx_release,
	.write = irtx_write,
	.unlocked_ioctl = irtx_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.llseek = no_llseek,
};

static struct miscdevice irtx_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "irtx",
	.fops = &irtx_fops,
};

static int irtx_probe(struct platform_device *pdev)
{
	int ret;

	irtx.dev = &pdev->dev;
	of_property_read_u32(pdev->dev.of_node, "pwm_ch",
			     &irtx_pwm_config.pwm_no);
	of_property_read_u32(pdev->dev.of_node, "pwm_data_invert",
			     &irtx.pwm_data_invert);

	irtx.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(irtx.pinctrl))
		return PTR_ERR(irtx.pinctrl);
	switch_irtx_gpio(0);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	return misc_register(&irtx_misc);
}

static int irtx_remove(struct platform_device *pdev)
{
	misc_deregister(&irtx_misc);
	return 0;
}

static const struct of_device_id irtx_of_ids[] = {
	{ .compatible = "mediatek,irtx-pwm", },
	{}
};
MODULE_DEVICE_TABLE(of, irtx_of_ids);

static struct platform_driver irtx_driver = {
	.probe = irtx_probe,
	.remove = irtx_remove,
	.driver = {
		.name = "mt_irtx",
		.of_match_table = irtx_of_ids,
	},
};
module_platform_driver(irtx_driver);

MODULE_DESCRIPTION("Consumer IR transmitter driver");
MODULE_LICENSE("GPL");
