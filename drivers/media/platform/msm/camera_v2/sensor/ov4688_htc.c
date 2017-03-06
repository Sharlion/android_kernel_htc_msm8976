/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include <linux/async.h>
#define ov4688_SENSOR_NAME "ov4688_htc"
#define PLATFORM_DRIVER_NAME "msm_camera_ov4688"
#define ov4688_obj ov4688_htc_##obj

static struct msm_camera_i2c_reg_array init0_reg_array[] =
{
  {0x0103, 0x01, 0x00},
  {0x3638, 0x00, 0x00},
  {0x0300, 0x00, 0x00},
  {0x0302, 0x2f, 0x00},
  {0x0303, 0x01, 0x00},
  {0x0304, 0x03, 0x00},
  {0x0305, 0x03, 0x00},
  {0x0306, 0x01, 0x00},
  {0x030a, 0x00, 0x00},
  {0x030b, 0x00, 0x00},
  {0x030d, 0x1e, 0x00},
  {0x030e, 0x04, 0x00},
  {0x030f, 0x01, 0x00},
  {0x0312, 0x01, 0x00},
  {0x031e, 0x00, 0x00},
  {0x3000, 0x20, 0x00},
  {0x3002, 0x00, 0x00},
  {0x3018, 0x72, 0x00},
  {0x3020, 0x93, 0x00},
  {0x3021, 0x03, 0x00},
  {0x3022, 0x01, 0x00},
  {0x3031, 0x0a, 0x00},
  {0x3305, 0xf1, 0x00},
  {0x3307, 0x04, 0x00},
  {0x3309, 0x29, 0x00},
  {0x3500, 0x00, 0x00},
  {0x3501, 0x60, 0x00},
  {0x3502, 0x00, 0x00},
  {0x3503, 0x04, 0x00},
  {0x3504, 0x00, 0x00},
  {0x3505, 0x00, 0x00},
  {0x3506, 0x00, 0x00},
  {0x3507, 0x00, 0x00},
  {0x3508, 0x00, 0x00},
  {0x3509, 0x80, 0x00},
  {0x350a, 0x00, 0x00},
  {0x350b, 0x00, 0x00},
  {0x350c, 0x00, 0x00},
  {0x350d, 0x00, 0x00},
  {0x350e, 0x00, 0x00},
  {0x350f, 0x80, 0x00},
  {0x3510, 0x00, 0x00},
  {0x3511, 0x00, 0x00},
  {0x3512, 0x00, 0x00},
  {0x3513, 0x00, 0x00},
  {0x3514, 0x00, 0x00},
  {0x3515, 0x80, 0x00},
  {0x3516, 0x00, 0x00},
  {0x3517, 0x00, 0x00},
  {0x3518, 0x00, 0x00},
  {0x3519, 0x00, 0x00},
  {0x351a, 0x00, 0x00},
  {0x351b, 0x80, 0x00},
  {0x351c, 0x00, 0x00},
  {0x351d, 0x00, 0x00},
  {0x351e, 0x00, 0x00},
  {0x351f, 0x00, 0x00},
  {0x3520, 0x00, 0x00},
  {0x3521, 0x80, 0x00},
  {0x3522, 0x08, 0x00},
  {0x3524, 0x08, 0x00},
  {0x3526, 0x08, 0x00},
  {0x3528, 0x08, 0x00},
  {0x352a, 0x08, 0x00},
  {0x3602, 0x00, 0x00},
  {0x3604, 0x02, 0x00},
  {0x3605, 0x00, 0x00},
  {0x3606, 0x00, 0x00},
  {0x3607, 0x00, 0x00},
  {0x3609, 0x12, 0x00},
  {0x360a, 0x40, 0x00},
  {0x360c, 0x08, 0x00},
  {0x360f, 0xe5, 0x00},
  {0x3608, 0x8f, 0x00},
  {0x3611, 0x00, 0x00},
  {0x3613, 0xf7, 0x00},
  {0x3616, 0x58, 0x00},
  {0x3619, 0x99, 0x00},
  {0x361b, 0x60, 0x00},
  {0x361c, 0x7a, 0x00},
  {0x361e, 0x79, 0x00},
  {0x361f, 0x02, 0x00},
  {0x3632, 0x00, 0x00},
  {0x3633, 0x10, 0x00},
  {0x3634, 0x10, 0x00},
  {0x3635, 0x10, 0x00},
  {0x3636, 0x15, 0x00},
  {0x3646, 0x86, 0x00},
  {0x364a, 0x0b, 0x00},
  {0x3700, 0x17, 0x00},
  {0x3701, 0x22, 0x00},
  {0x3703, 0x10, 0x00},
  {0x370a, 0x37, 0x00},
  {0x3705, 0x00, 0x00},
  {0x3706, 0x63, 0x00},
  {0x3709, 0x3c, 0x00},
  {0x370b, 0x01, 0x00},
  {0x370c, 0x30, 0x00},
  {0x3710, 0x24, 0x00},
  {0x3711, 0x0c, 0x00},
  {0x3716, 0x00, 0x00},
  {0x3720, 0x28, 0x00},
  {0x3729, 0x7b, 0x00},
  {0x372a, 0x84, 0x00},
  {0x372b, 0xbd, 0x00},
  {0x372c, 0xbc, 0x00},
  {0x372e, 0x52, 0x00},
  {0x373c, 0x0e, 0x00},
  {0x373e, 0x33, 0x00},
  {0x3743, 0x10, 0x00},
  {0x3744, 0x88, 0x00},
  {0x374a, 0x43, 0x00},
  {0x374c, 0x00, 0x00},
  {0x374e, 0x23, 0x00},
  {0x3751, 0x7b, 0x00},
  {0x3752, 0x84, 0x00},
  {0x3753, 0xbd, 0x00},
  {0x3754, 0xbc, 0x00},
  {0x3756, 0x52, 0x00},
  {0x375c, 0x00, 0x00},
  {0x3760, 0x00, 0x00},
  {0x3761, 0x00, 0x00},
  {0x3762, 0x00, 0x00},
  {0x3763, 0x00, 0x00},
  {0x3764, 0x00, 0x00},
  {0x3767, 0x04, 0x00},
  {0x3768, 0x04, 0x00},
  {0x3769, 0x08, 0x00},
  {0x376a, 0x08, 0x00},
  {0x376b, 0x20, 0x00},
  {0x376c, 0x00, 0x00},
  {0x376d, 0x00, 0x00},
  {0x376e, 0x00, 0x00},
  {0x3773, 0x00, 0x00},
  {0x3774, 0x51, 0x00},
  {0x3776, 0xbd, 0x00},
  {0x3777, 0xbd, 0x00},
  {0x3781, 0x18, 0x00},
  {0x3783, 0x25, 0x00},
  {0x3800, 0x00, 0x00},
  {0x3801, 0x08, 0x00},
  {0x3802, 0x00, 0x00},
  {0x3803, 0x04, 0x00},
  {0x3804, 0x0a, 0x00},
  {0x3805, 0x97, 0x00},
  {0x3806, 0x05, 0x00},
  {0x3807, 0xfb, 0x00},
  {0x3808, 0x0a, 0x00},
  {0x3809, 0x80, 0x00},
  {0x380a, 0x05, 0x00},
  {0x380b, 0xf0, 0x00},
  {0x380c, 0x0a, 0x00},
  {0x380d, 0x0c, 0x00},
  {0x380e, 0x06, 0x00},
  {0x380f, 0x12, 0x00},
  {0x3810, 0x00, 0x00},
  {0x3811, 0x08, 0x00},
  {0x3812, 0x00, 0x00},
  {0x3813, 0x04, 0x00},
  {0x3814, 0x01, 0x00},
  {0x3815, 0x01, 0x00},
  {0x3819, 0x01, 0x00},
  {0x3820, 0x00, 0x00},
  {0x3821, 0x06, 0x00},
  {0x3829, 0x00, 0x00},
  {0x382a, 0x01, 0x00},
  {0x382b, 0x01, 0x00},
  {0x382d, 0x7f, 0x00},
  {0x3830, 0x04, 0x00},
  {0x3836, 0x01, 0x00},
  {0x3841, 0x02, 0x00},
  {0x3846, 0x08, 0x00},
  {0x3847, 0x07, 0x00},
  {0x3d85, 0x36, 0x00},
  {0x3d8c, 0x71, 0x00},
  {0x3d8d, 0xcb, 0x00},
  {0x3f0a, 0x00, 0x00},
  {0x4000, 0x71, 0x00},
  {0x4001, 0x40, 0x00},
  {0x4002, 0x04, 0x00},
  {0x4003, 0x14, 0x00},
  {0x400e, 0x00, 0x00},
  {0x4011, 0x00, 0x00},
  {0x401a, 0x00, 0x00},
  {0x401b, 0x00, 0x00},
  {0x401c, 0x00, 0x00},
  {0x401d, 0x00, 0x00},
  {0x401f, 0x00, 0x00},
  {0x4020, 0x00, 0x00},
  {0x4021, 0x10, 0x00},
  {0x4022, 0x07, 0x00},
  {0x4023, 0xcf, 0x00},
  {0x4024, 0x09, 0x00},
  {0x4025, 0x60, 0x00},
  {0x4026, 0x09, 0x00},
  {0x4027, 0x6f, 0x00},
  {0x4028, 0x00, 0x00},
  {0x4029, 0x02, 0x00},
  {0x402a, 0x06, 0x00},
  {0x402b, 0x04, 0x00},
  {0x402c, 0x02, 0x00},
  {0x402d, 0x02, 0x00},
  {0x402e, 0x0e, 0x00},
  {0x402f, 0x04, 0x00},
  {0x4302, 0xff, 0x00},
  {0x4303, 0xff, 0x00},
  {0x4304, 0x00, 0x00},
  {0x4305, 0x00, 0x00},
  {0x4306, 0x00, 0x00},
  {0x4308, 0x02, 0x00},
  {0x4500, 0x6c, 0x00},
  {0x4501, 0xc4, 0x00},
  {0x4502, 0x40, 0x00},
  {0x4503, 0x02, 0x00},
  {0x4601, 0xa7, 0x00},
  {0x4800, 0x04, 0x00},
  {0x4813, 0x08, 0x00},
  {0x481f, 0x40, 0x00},
  {0x4829, 0x78, 0x00},
  {0x4837, 0x1c, 0x00},
  {0x4b00, 0x2a, 0x00},
  {0x4b0d, 0x00, 0x00},
  {0x4d00, 0x04, 0x00},
  {0x4d01, 0x42, 0x00},
  {0x4d02, 0xd1, 0x00},
  {0x4d03, 0x93, 0x00},
  {0x4d04, 0xf5, 0x00},
  {0x4d05, 0xc1, 0x00},
  {0x5000, 0xf3, 0x00},
  {0x5001, 0x11, 0x00},
  {0x5004, 0x00, 0x00},
  {0x500a, 0x00, 0x00},
  {0x500b, 0x00, 0x00},
  {0x5032, 0x00, 0x00},
  {0x5040, 0x00, 0x00},
  {0x5050, 0x0c, 0x00},
  {0x5500, 0x00, 0x00},
  {0x5501, 0x10, 0x00},
  {0x5502, 0x01, 0x00},
  {0x5503, 0x0f, 0x00},
  {0x8000, 0x00, 0x00},
  {0x8001, 0x00, 0x00},
  {0x8002, 0x00, 0x00},
  {0x8003, 0x00, 0x00},
  {0x8004, 0x00, 0x00},
  {0x8005, 0x00, 0x00},
  {0x8006, 0x00, 0x00},
  {0x8007, 0x00, 0x00},
  {0x8008, 0x00, 0x00},
  {0x3638, 0x00, 0x00},
  {0x3105, 0x31, 0x00},
  {0x301a, 0xf9, 0x00},
  {0x3508, 0x07, 0x00},
  {0x484b, 0x05, 0x00},
  {0x4805, 0x03, 0x00},
  {0x3601, 0x00, 0x00},
  {0x4805, 0x00, 0x00},
  {0x301a, 0xf0, 0x00},
  {0x3638, 0x00, 0x00},
  {0x0100, 0x01, 10000},
};

static  struct msm_camera_i2c_reg_setting init_settings = {
  .reg_setting = init0_reg_array,
  .size = ARRAY_SIZE(init0_reg_array),
  .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
  .data_type = MSM_CAMERA_I2C_BYTE_DATA,
  .delay = 0,
};

DEFINE_MSM_MUTEX(ov4688_mut);

static struct msm_sensor_ctrl_t ov4688_s_ctrl;

static struct msm_sensor_power_setting ov4688_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 1,
		.delay = 3,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 1,
		.delay = 3,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 1,
		.delay = 3,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 5,
	},

};

static struct v4l2_subdev_info ov4688_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ov4688_i2c_id[] = {
	{ov4688_SENSOR_NAME, (kernel_ulong_t)&ov4688_s_ctrl},
	{ }
};

static int32_t msm_ov4688_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov4688_s_ctrl);
}

static struct i2c_driver ov4688_i2c_driver = {
	.id_table = ov4688_i2c_id,
	.probe  = msm_ov4688_i2c_probe,
	.driver = {
		.name = ov4688_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov4688_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov4688_dt_match[] = {
	{.compatible = "htc,ov4688_htc", .data = &ov4688_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov4688_dt_match);

static struct platform_driver ov4688_platform_driver = {
	.driver = {
		.name = "htc,ov4688_htc",
		.owner = THIS_MODULE,
		.of_match_table = ov4688_dt_match,
	},
};

static const char *ov4688Vendor = "OmniVision";
static const char *ov4688NAME = "ov4688";
static const char *ov4688Size = "4.0M";

static ssize_t sensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", ov4688Vendor, ov4688NAME, ov4688Size);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);

static struct kobject *android_ov4688;

static int ov4688_sysfs_init(void)
{
	int ret ;
	pr_info("ov4688:kobject creat and add\n");
	android_ov4688 = kobject_create_and_add("android_camera2", NULL);
	if (android_ov4688 == NULL) {
		pr_info("ov4688_sysfs_init: subsystem_register " \
		"failed\n");
		ret = -ENOMEM;
		return ret ;
	}
	pr_info("ov4688:sysfs_create_file\n");
	ret = sysfs_create_file(android_ov4688, &dev_attr_sensor.attr);
	if (ret) {
		pr_info("ov4688_sysfs_init: sysfs_create_file " \
		"failed\n");
		kobject_del(android_ov4688);
	}

	return 0 ;
}

static int32_t ov4688_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(ov4688_dt_match, &pdev->dev);
	if (match)
		rc = msm_sensor_platform_probe(pdev, match->data);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	return rc;
}

static void __init ov4688_init_module_async(void *unused, async_cookie_t cookie)
{
	int32_t rc = 0;
	async_synchronize_cookie(cookie);
	pr_info("%s_front:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ov4688_platform_driver,
		ov4688_platform_probe);
	if (!rc) {
		ov4688_sysfs_init();
		return;
	}
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	i2c_add_driver(&ov4688_i2c_driver);
}

static int __init ov4688_init_module(void)
{
	async_schedule(ov4688_init_module_async, NULL);
	return 0;
}

static void __exit ov4688_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ov4688_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov4688_s_ctrl);
		platform_driver_unregister(&ov4688_platform_driver);
	} else
		i2c_del_driver(&ov4688_i2c_driver);
	return;
}

static int ov4688_read_fuseid(struct sensorb_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl)
{
    #define OV4688_OTP_SIZE 0x07
    /*
    OTP Location

    Burn in times						1st(Layer 0)	2nd(Layer 1)	3rd(Layer 2)

	0 Module vendor 					0x126			0x144			0x162
	1 LENS							0x127			0x145			0x163
	2 Sensor Version					0x128			0x146			0x164
	3 Module ID 						0x110			0x12e			0x14c
	4 Module ID 						0x111			0x12f			0x14d
	5 Module ID 						0x112			0x130			0x14e

	6 Checksum of Information		0x12d			0x14b			0x169
    */

    const short addr[3][OV4688_OTP_SIZE] = {
        {0x126, 0x127, 0x128, 0x110, 0x111, 0x112, 0x12d}, // layer 1
        {0x144, 0x145, 0x146, 0x12e, 0x12f, 0x130, 0x14b}, // layer 2
        {0x162, 0x163, 0x164, 0x14c, 0x14d, 0x14e, 0x169}, // layer 3
    };
    static uint8_t otp[OV4688_OTP_SIZE];
	static int first= true;
	uint16_t read_data = 0;

    int32_t i,j;
    int32_t rc = 0;
    const int32_t offset = 0x7000;
    static int32_t valid_layer=-1;

	if (first) {
	    first = false;

        /*Must finish the recommend settings before otp read*/
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write b 0x0100 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d84, 0x40, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d84 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d88, 0x71, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d88 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d89, 0x10, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d89 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d8a, 0x71, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d8a fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d8b, 0xca, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d8a fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d81, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write b 0x3d81 fail\n", __func__);

        msleep(10);

        // start from layer 2
        for (j=2; j>=0; j--)
        {
            for (i=0; i<OV4688_OTP_SIZE; ++i)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
                if (rc < 0)
                {
                    pr_err("%s: i2c_read 0x%x failed\n", __func__, addr[j][i]);
                    return rc;
                }
                else
                {
                    otp[i]= read_data;
                    if (read_data)
                        valid_layer = j;
                }
            }
            if (valid_layer != -1)
            {
                pr_info("[CAM]%s: valid_layer of module info:%d \n", __func__, valid_layer);
                break;
            }
        }

        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_info("%s: i2c_write b 0x0100 fail\n", __func__);
    }

	if(cdata != NULL)
	{
		cdata->cfg.fuse.fuse_id_word1 = 0;
		cdata->cfg.fuse.fuse_id_word2 = otp[3];
		cdata->cfg.fuse.fuse_id_word3 = otp[4];
		cdata->cfg.fuse.fuse_id_word4 = otp[5];

		pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
		pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
		pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);

		pr_info("%s: OTP fuse 0 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word1);
		pr_info("%s: OTP fuse 1 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word2);
		pr_info("%s: OTP fuse 2 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word3);
		pr_info("%s: OTP fuse 3 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word4);
	}
	else
	{
		pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
		pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
		pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
	}
	return rc;
}

static int ov4688_read_fuseid32(struct sensorb_cfg_data32 *cdata,
	struct msm_sensor_ctrl_t *s_ctrl)
{
	#define OV4688_OTP_SIZE 0x07
	/*
	OTP Location

	Burn in times						1st(Layer 0)	2nd(Layer 1)	3rd(Layer 2)

	0 Module vendor 					0x126			0x144			0x162
	1 LENS								0x127			0x145			0x163
	2 Sensor Version					0x128			0x146			0x164
	3 Module ID 						0x110			0x12e			0x14c
	4 Module ID 						0x111			0x12f			0x14d
	5 Module ID 						0x112			0x130			0x14e

	6 Checksum of Information		0x12d			0x14b			0x169
	*/

	const short addr[3][OV4688_OTP_SIZE] = {
      {0x126, 0x127, 0x128, 0x110, 0x111, 0x112, 0x12d}, // layer 1
      {0x144, 0x145, 0x146, 0x12e, 0x12f, 0x130, 0x14b}, // layer 2
      {0x162, 0x163, 0x164, 0x14c, 0x14d, 0x14e, 0x169}, // layer 3
	};
	static uint8_t otp[OV4688_OTP_SIZE];
	static int first= true;
	uint16_t read_data = 0;

	int32_t i,j;
	int32_t rc = 0;
	const int32_t offset = 0x7000;
	static int32_t valid_layer=-1;

	if (first) {
        first = false;

        /*Must finish the recommend settings before otp read*/
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write b 0x0100 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d84, 0x40, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d84 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d88, 0x71, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d88 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d89, 0x10, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d89 fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d8a, 0x71, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d8a fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d8b, 0xca, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write w 0x3d8a fail\n", __func__);
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x3d81, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
          pr_err("%s: i2c_write b 0x3d81 fail\n", __func__);

		msleep(10);

		// start from layer 2
		for (j=2; j>=0; j--)
		{
			for (i=0; i<OV4688_OTP_SIZE; ++i)
			{
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
					s_ctrl->sensor_i2c_client, addr[j][i]+offset, &read_data, MSM_CAMERA_I2C_BYTE_DATA);//0x37c2
				if (rc < 0)
				{
					pr_err("%s: i2c_read 0x%x failed\n", __func__, addr[j][i]);
					return rc;
				}
				else
				{
					otp[i]= read_data;
					if (read_data)
					valid_layer = j;
				}
			}
			if (valid_layer != -1)
			{
				pr_info("[CAM]%s: valid_layer of module info:%d \n", __func__, valid_layer);
				break;
			}
		}

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
			pr_info("%s: i2c_write b 0x0100 fail\n", __func__);
	}

	if(cdata != NULL)
	{
		cdata->cfg.fuse.fuse_id_word1 = 0;
		cdata->cfg.fuse.fuse_id_word2 = otp[3];
		cdata->cfg.fuse.fuse_id_word3 = otp[4];
		cdata->cfg.fuse.fuse_id_word4 = otp[5];

		pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
		pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
		pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);

		pr_info("%s: OTP fuse 0 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word1);
		pr_info("%s: OTP fuse 1 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word2);
		pr_info("%s: OTP fuse 2 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word3);
		pr_info("%s: OTP fuse 3 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word4);
	}
	else
	{
		pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
		pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
		pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
	}
	return rc;
}

int32_t ov4688_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int32_t rc1 = 0;
	static int first = 0;
	rc = msm_sensor_match_id(s_ctrl);
	if(rc == 0) {
		if(first == 0) {
			pr_info("%s read_fuseid\n",__func__);
            rc1 = s_ctrl->sensor_i2c_client->i2c_func_tbl->
                i2c_write_table(s_ctrl->sensor_i2c_client, &init_settings);
#ifdef CONFIG_COMPAT
			rc1 = ov4688_read_fuseid32(NULL, s_ctrl);
#else
			rc1 = ov4688_read_fuseid(NULL, s_ctrl);
#endif
			first = 1;
		}
	}
	return rc;
}

static struct msm_sensor_fn_t ov4688_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_config32 = msm_sensor_config32,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = ov4688_sensor_match_id,
	.sensor_i2c_read_fuseid = ov4688_read_fuseid,
#ifdef CONFIG_COMPAT
	.sensor_i2c_read_fuseid32 = ov4688_read_fuseid32,
#endif
};

static struct msm_sensor_ctrl_t ov4688_s_ctrl = {
	.sensor_i2c_client = &ov4688_sensor_i2c_client,
	.power_setting_array.power_setting = ov4688_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov4688_power_setting),
    .msm_sensor_mutex = &ov4688_mut,
	.sensor_v4l2_subdev_info = ov4688_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov4688_subdev_info),
	.func_tbl = &ov4688_sensor_func_tbl,
};

module_init(ov4688_init_module);
module_exit(ov4688_exit_module);
MODULE_DESCRIPTION("ov4688");
MODULE_LICENSE("GPL v2");
