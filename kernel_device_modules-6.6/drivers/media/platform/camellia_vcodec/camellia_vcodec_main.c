// SPDX-License-Identifier: GPL-2.0
/*
 * camellia: one module for the 4.14 MediaTek VCU-mode codec stack. In 4.14
 * the VCU, the common code and the decoder/encoder drivers were built in and
 * call each other both ways (the VCU calls back into vdec/venc locking and
 * the GCE timeout dump), so they cannot be separate modules.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

extern struct platform_driver mtk_vcu_driver;
extern struct platform_driver mtk_vcodec_dec_driver;
extern struct platform_driver mtk_vcodec_enc_driver;

static struct platform_driver * const camellia_vcodec_drivers[] = {
	&mtk_vcu_driver,
	&mtk_vcodec_dec_driver,
	&mtk_vcodec_enc_driver,
};

static int __init camellia_vcodec_init(void)
{
	return platform_register_drivers(camellia_vcodec_drivers,
					 ARRAY_SIZE(camellia_vcodec_drivers));
}
module_init(camellia_vcodec_init);

static void __exit camellia_vcodec_exit(void)
{
	platform_unregister_drivers(camellia_vcodec_drivers,
				    ARRAY_SIZE(camellia_vcodec_drivers));
}
module_exit(camellia_vcodec_exit);

MODULE_DESCRIPTION("camellia: 4.14 MediaTek VCU, video decoder and encoder (stock vpud protocol)");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
