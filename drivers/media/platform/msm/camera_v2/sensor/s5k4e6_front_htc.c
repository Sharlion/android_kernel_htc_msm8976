/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include "msm_cci.h"
#include "lc898123_htc.h"

#if 0
#if 1
#define HTC_OIS_HW_WORKAROUND 1
#else
#undef HTC_OIS_HW_WORKAROUND
#endif
#endif

#define S5K4E6_FRONT_HTC_SENSOR_NAME "s5k4e6_front_htc"
#define s5k4e6_front_htc_obj s5k4e6_front_htc_##obj

DEFINE_MSM_MUTEX(s5k4e6_front_htc_mut);

static struct msm_sensor_ctrl_t s5k4e6_front_htc_s_ctrl;

static struct msm_sensor_power_setting s5k4e6_front_htc_power_setting[] = {
    {
        .seq_type = SENSOR_GPIO,
        .seq_val = SENSOR_GPIO_STANDBY,
        .config_val = GPIO_OUT_HIGH,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_VREG,
        .seq_val = CAM_VAF,
        .config_val = 1,
        .delay = 3,
    },
    {
        .seq_type = SENSOR_VREG,
        .seq_val = CAM_VIO,
        .config_val = 1,
        .delay = 1,
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
        .delay = 2,
    },
    {
        .seq_type = SENSOR_GPIO,
        .seq_val = SENSOR_GPIO_RESET,
        .config_val = GPIO_OUT_HIGH,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_CLK,
        .seq_val = SENSOR_CAM_MCLK,
        .config_val = 0,
        .delay = 1
    },
    {
        .seq_type = SENSOR_I2C_MUX,
        .seq_val = 0,
        .config_val = 0,
        .delay = 0,
    },
};

static struct v4l2_subdev_info s5k4e6_front_htc_subdev_info[] = {
    {
        .code = V4L2_MBUS_FMT_SBGGR10_1X10,
        .colorspace = V4L2_COLORSPACE_JPEG,
        .fmt = 1,
        .order = 0,
    },
};

static const struct i2c_device_id s5k4e6_front_htc_i2c_id[] = {
    {S5K4E6_FRONT_HTC_SENSOR_NAME, (kernel_ulong_t)&s5k4e6_front_htc_s_ctrl},
    {}
};

static int32_t msm_s5k4e6_front_htc_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    return msm_sensor_i2c_probe(client, id, &s5k4e6_front_htc_s_ctrl);
}

static struct i2c_driver s5k4e6_front_htc_i2c_driver = {
    .id_table = s5k4e6_front_htc_i2c_id,
    .probe = msm_s5k4e6_front_htc_i2c_probe,
    .driver = {
        .name = S5K4E6_FRONT_HTC_SENSOR_NAME,
    },
};

static struct msm_camera_i2c_client s5k4e6_front_htc_sensor_i2c_client = {
    .addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k4e6_front_htc_dt_match[] = {
    {.compatible = "htc,s5k4e6_front_htc", .data = &s5k4e6_front_htc_s_ctrl},
    {}
};

MODULE_DEVICE_TABLE(of, s5k4e6_front_htc_dt_match);

static struct platform_driver s5k4e6_front_htc_platform_driver = {
    .driver = {
        .name = "htc,s5k4e6_front_htc",
        .owner = THIS_MODULE,
        .of_match_table = s5k4e6_front_htc_dt_match,
    },
};

static const char *s5k4e6_front_htcVendor = "Samsung";
static const char *s5k4e6_front_htcNAME = "s5k4e6_front_htc";
static const char *s5k4e6_front_htcSize = "5M";

static ssize_t sensor_vendor_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;

    sprintf(buf, "%s %s %s\n", s5k4e6_front_htcVendor, s5k4e6_front_htcNAME, s5k4e6_front_htcSize);
    ret = strlen(buf) + 1;

    return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);

static struct kobject *android_s5k4e6_front_htc;

static int s5k4e6_front_htc_sysfs_init(void)
{
    int ret ;
    pr_info("s5k4e6_front_htc:kobject creat and add\n");
    android_s5k4e6_front_htc = kobject_create_and_add("android_camera2", NULL);
    if (android_s5k4e6_front_htc == NULL) {
        pr_err("s5k4e6_front_htc_sysfs_init: subsystem_register " \
        "failed\n");
        ret = -ENOMEM;
        return ret ;
    }
    pr_info("s5k4e6_front_htc:sysfs_create_file\n");
    ret = sysfs_create_file(android_s5k4e6_front_htc, &dev_attr_sensor.attr);
    if (ret) {
        pr_err("s5k4e6_front_htc_sysfs_init: sysfs_create_file " \
        "failed\n");
        kobject_del(android_s5k4e6_front_htc);
    }

    return 0 ;
}

static uint8_t otp[20] = {0};

static int s5k4e6_front_htc_read_fuseid(struct sensorb_cfg_data *cdata,
    struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    int32_t i = 0, j = 0;
    uint16_t read_data = 0;
    static int first = true;
    int valid_layer = -1;
    uint8_t OTP_User1Page[3] = {51,52,53};
    uint8_t OTP_User2Page[3] = {48,49,50};
    short data_addr[10] = {0x000D,0x000E,0x000F,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016};

    if (first)
    {
        first = false;
        for(j=2; j>=0; j--)
        {
            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0xFCFC, 0x4000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(10);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6028, 0x2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0x000B, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, OTP_User1Page[j], MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0X0009, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, 0X01, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0X0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(5);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602C, 0X2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            for (i = 0 ; i < 4; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602E, data_addr[i], MSM_CAMERA_I2C_WORD_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x6F12, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
                } else {
                    otp[i] = read_data & 0xff;
                    if (read_data)
                        valid_layer = OTP_User1Page[j];
                    read_data = 0;
                }
            }
            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n",__func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;

        for (j=2; j>=0; j--)
        {
            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0xFCFC, 0x4000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(10);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6028, 0x2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0x000B, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, OTP_User2Page[j], MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0X0009, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, 0X01, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0X0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(5);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602C, 0X2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            for (i=0 ; i<10; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602E, data_addr[i], MSM_CAMERA_I2C_WORD_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x6F12, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
                } else {
                    otp[4+i] = read_data & 0xff;
                    if (read_data)
                        valid_layer = OTP_User2Page[j];
                    read_data = 0;
                }
            }
            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n",__func__,valid_layer);
                break;
            }
        }
    }

    if (cdata != NULL) {
        cdata->cfg.fuse.fuse_id_word1 = otp[0];
        cdata->cfg.fuse.fuse_id_word2 = otp[1];
        cdata->cfg.fuse.fuse_id_word3 = otp[2];
        cdata->cfg.fuse.fuse_id_word4 = otp[3];
        pr_info("[OTP]%s: s5k4e6_front_htc: fuse->fuse_id : 0x%x 0x%x 0x%x 0x%x\n",__func__,
                cdata->cfg.fuse.fuse_id_word1,
                cdata->cfg.fuse.fuse_id_word2,
                cdata->cfg.fuse.fuse_id_word3,
                cdata->cfg.fuse.fuse_id_word4);

        
        
        cdata->af_value.VCM_VENDOR = otp[4+0];
        cdata->af_value.MODULE_ID_AB = cdata->cfg.fuse.fuse_id_word1;
        cdata->af_value.VCM_VENDOR_ID_VERSION = otp[4+4];
        cdata->af_value.AF_INF_MSB = otp[4+5];
        cdata->af_value.AF_INF_LSB = otp[4+6];
        cdata->af_value.AF_MACRO_MSB = otp[4+7];
        cdata->af_value.AF_MACRO_LSB = otp[4+8];
        strlcpy(cdata->af_value.ACT_NAME, "lc898123_act", sizeof("lc898123_act"));
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Actuator Name = %s\n",__func__,cdata->af_value.ACT_NAME);

        pr_info("[OTP]%s: s5k4e6_front_htc OTP Module vendor = 0x%x\n",               __func__, otp[4+0]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP LENS = 0x%x\n",                        __func__, otp[4+1]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Sensor Version = 0x%x\n",              __func__, otp[4+2]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Driver IC Vendor & Version = 0x%x\n",  __func__, otp[4+3]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Actuator vender ID & Version = 0x%x\n",__func__, otp[4+4]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Infinity position code (MSByte) = 0x%x\n",__func__,cdata->af_value.AF_INF_MSB);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Infinity position code (LSByte) = 0x%x\n",__func__,cdata->af_value.AF_INF_LSB);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Macro position code (MSByte) = 0x%x\n",__func__,cdata->af_value.AF_MACRO_MSB);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Macro position code (LSByte) = 0x%x\n",__func__,cdata->af_value.AF_MACRO_LSB);
    }
    else {
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Module vendor = 0x%x\n",               __func__, otp[4+0]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP LENS = 0x%x\n",                        __func__, otp[4+1]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Sensor Version = 0x%x\n",              __func__, otp[4+2]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Driver IC Vendor & Version = 0x%x\n",  __func__, otp[4+3]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Actuator vender ID & Version = 0x%x\n",__func__, otp[4+4]);
    }
    return rc;
}

#ifdef CONFIG_COMPAT
static int s5k4e6_front_htc_read_fuseid32(struct sensorb_cfg_data32 *cdata,
    struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    int32_t i = 0, j = 0;
    uint16_t read_data = 0;
    static int first = true;
    int valid_layer = -1;
    uint8_t OTP_User1Page[3] = {51,52,53}; 
    uint8_t OTP_User2Page[3] = {48,49,50}; 
    short data_addr[10] = {0x000D,0x000E,0x000F,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016};

    if (first)
    {
        first = false;
        for (j=2; j>=0; j--)
        {
            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0xFCFC, 0x4000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(10);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6028, 0x2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0x000B, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, OTP_User1Page[j], MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0X0009, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, 0X01, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0X0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(5);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602C, 0X2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            for (i=0; i<4; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602E, data_addr[i], MSM_CAMERA_I2C_WORD_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x6F12, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
                } else {
                    otp[i] = read_data & 0xff;

                    if(read_data)
                        valid_layer = OTP_User1Page[j];

                    read_data = 0;
                }
            }

            if (valid_layer != -1) {
                pr_info("[OTP]%s: valid_layer:%d \n",__func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;

        for (j=2; j>=0; j--)
        {
            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0xFCFC, 0x4000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0100, 0x0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(10);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6028, 0x2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0x000B, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, OTP_User2Page[j], MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602A, 0X0009, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x6F12, 0X01, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0X0100, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            msleep(5);

            
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602C, 0X2000, MSM_CAMERA_I2C_WORD_DATA);
            if (rc < 0)
                pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

            
            for (i=0 ; i<10; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x602E, data_addr[i], MSM_CAMERA_I2C_WORD_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x6F12, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0) {
                    pr_err("[OTP]%s: i2c_write failed Line:%d\n",__func__,__LINE__);
                } else {
                    otp[4+i] = read_data & 0xff;

                    if (read_data)
                        valid_layer = OTP_User2Page[j];

                    read_data = 0;
                }
            }

            if (valid_layer != -1) {
                pr_info("[OTP]%s: valid_layer:%d \n",__func__,valid_layer);
                break;
            }
        }
    }

    if (cdata != NULL) {
        cdata->cfg.fuse.fuse_id_word1 = otp[0];
        cdata->cfg.fuse.fuse_id_word2 = otp[1];
        cdata->cfg.fuse.fuse_id_word3 = otp[2];
        cdata->cfg.fuse.fuse_id_word4 = otp[3];
        pr_info("[OTP]%s: s5k4e6_front_htc: fuse->fuse_id : 0x%x 0x%x 0x%x 0x%x\n",__func__,
                cdata->cfg.fuse.fuse_id_word1,
                cdata->cfg.fuse.fuse_id_word2,
                cdata->cfg.fuse.fuse_id_word3,
                cdata->cfg.fuse.fuse_id_word4);

        
        
        cdata->af_value.VCM_VENDOR = otp[4+0];
        cdata->af_value.MODULE_ID_AB = cdata->cfg.fuse.fuse_id_word1;
        cdata->af_value.VCM_VENDOR_ID_VERSION = otp[4+4];
        cdata->af_value.AF_INF_MSB = otp[4+5];
        cdata->af_value.AF_INF_LSB = otp[4+6];
        cdata->af_value.AF_MACRO_MSB = otp[4+7];
        cdata->af_value.AF_MACRO_LSB = otp[4+8];
        strlcpy(cdata->af_value.ACT_NAME, "lc898123_act", sizeof("lc898123_act"));
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Actuator Name = %s\n",__func__,cdata->af_value.ACT_NAME);

        pr_info("[OTP]%s: s5k4e6_front_htc OTP Module vendor = 0x%x\n",               __func__, otp[4+0]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP LENS = 0x%x\n",                        __func__, otp[4+1]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Sensor Version = 0x%x\n",              __func__, otp[4+2]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Driver IC Vendor & Version = 0x%x\n",  __func__, otp[4+3]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Actuator vender ID & Version = 0x%x\n",__func__, otp[4+4]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Infinity position code (MSByte) = 0x%x\n",__func__,cdata->af_value.AF_INF_MSB);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Infinity position code (LSByte) = 0x%x\n",__func__,cdata->af_value.AF_INF_LSB);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Macro position code (MSByte) = 0x%x\n",__func__,cdata->af_value.AF_MACRO_MSB);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Macro position code (LSByte) = 0x%x\n",__func__,cdata->af_value.AF_MACRO_LSB);
    }
    else {
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Module vendor = 0x%x\n",               __func__, otp[4+0]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP LENS = 0x%x\n",                        __func__, otp[4+1]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Sensor Version = 0x%x\n",              __func__, otp[4+2]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Driver IC Vendor & Version = 0x%x\n",  __func__, otp[4+3]);
        pr_info("[OTP]%s: s5k4e6_front_htc OTP Actuator vender ID & Version = 0x%x\n",__func__, otp[4+4]);
    }

    return rc;
}
#endif

#if 0
#ifdef HTC_OIS_HW_WORKAROUND
#define OTP_NVR0_DATA_SIZE 64
#define OTP_NVR1_DATA_SIZE 162
static uint8_t otp_NVR0_data[OTP_NVR0_DATA_SIZE];
static uint8_t otp_NVR1_data[OTP_NVR1_DATA_SIZE];
static int valid_otp_ois = 0;

static int s5k4e6_front_htc_read_otp_ois_data(struct sensorb_cfg_data *cdata,
    struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    int32_t i = 0, j = 0;
    uint16_t read_data = 0;
    static int first = true;
    int valid_layer = -1;
    int retry_count = 0;

    if (first)
    {
        first = false;

        for(j = 5; j >=3; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            } while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );

            for(i = 0; i < 64; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR0_data[i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;
        for(j = 8; j >=6; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            }while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );

            for(i = 0; i < 64; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;
        for(j = 11; j >=9; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            } while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );


            for(i = 0; i < 64; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[64+i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;
        for(j = 14; j >=12; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            } while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );


            for(i = 0; i < 2; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[128+i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            for(i = 0; i < 32; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, (0x0A14 + i), &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A14 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[130+i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }
    }

    
    valid_otp_ois = 0;
    for(i=0; i < OTP_NVR0_DATA_SIZE; i++) {
        if (otp_NVR0_data[i])
            valid_otp_ois = 1;
        pr_info("[OTP]%s : otp_NVR0_data[%d] = 0x%x\n", __func__, i, otp_NVR0_data[i]);
    }

    for(i=0; i < OTP_NVR1_DATA_SIZE; i++) {
        if (otp_NVR1_data[i])
            valid_otp_ois = 1;
        pr_info("[OTP]%s : otp_NVR1_data[%d] = 0x%x\n", __func__, i, otp_NVR1_data[i]);
    }

    return rc;

read_otp_ois_error:
    valid_otp_ois = 0;
    return rc;
}


#define OIS_COMPONENT_I2C_ADDR_WRITE 0x7C

int htc_check_ois_component(struct msm_sensor_ctrl_t *s_ctrl)
{
    int rc = 0;
    uint16_t cci_client_sid_backup;

    
    cci_client_sid_backup = s_ctrl->sensor_i2c_client->cci_client->sid;

    
    s_ctrl->sensor_i2c_client->cci_client->sid = OIS_COMPONENT_I2C_ADDR_WRITE >> 1;

    
    lc898123_check_ois_component(s_ctrl, valid_otp_ois, otp_NVR0_data, otp_NVR1_data);

    
    s_ctrl->sensor_i2c_client->cci_client->sid = cci_client_sid_backup;

    return rc;
}
#endif
#endif


int32_t s5k4e6_front_htc_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    static int first = 0;

    rc = msm_sensor_match_id(s_ctrl);
    if(rc == 0)
    {
        if(first == 0)
        {
            pr_info("%s : read_fuseid\n", __func__);

#ifdef CONFIG_COMPAT
            s5k4e6_front_htc_read_fuseid32(NULL, s_ctrl);
#else
            s5k4e6_front_htc_read_fuseid(NULL, s_ctrl);
#endif
            first = 1;

#if 0
#ifdef HTC_OIS_HW_WORKAROUND
            pr_info("%s : read otp ois data\n", __func__);
            s5k4e6_front_htc_read_otp_ois_data(NULL, s_ctrl);

            pr_info("%s : check ois component\n", __func__);
            htc_check_ois_component(s_ctrl);
#endif
#endif
        }
    }
    return rc;
}

static int32_t s5k4e6_front_htc_platform_probe(struct platform_device *pdev)
{
    int32_t rc = 0;
    const struct of_device_id *match;
    match = of_match_device(s5k4e6_front_htc_dt_match, &pdev->dev);
    if (match) {
        rc = msm_sensor_platform_probe(pdev, match->data);
    }
    else {
        pr_err("%s:%d, failed to match sensor\n", __func__, __LINE__);
        rc = -EINVAL;
    }
    return rc;
}

static void __init s5k4e6_front_htc_init_module_async(void *unused, async_cookie_t cookie)
{
    int32_t rc = 0;
    async_synchronize_cookie(cookie);
    pr_info("%s:%d\n", __func__, __LINE__);
    rc = platform_driver_probe(&s5k4e6_front_htc_platform_driver,
    s5k4e6_front_htc_platform_probe);
    if (!rc) {
        s5k4e6_front_htc_sysfs_init();
        return;
    }
    pr_info("%s:%d rc %d\n", __func__, __LINE__, rc);
    i2c_add_driver(&s5k4e6_front_htc_i2c_driver);
}

static int __init s5k4e6_front_htc_init_module(void)
{
    pr_info("%s:%d\n", __func__, __LINE__);
    async_schedule(s5k4e6_front_htc_init_module_async, NULL);
    return 0;
}

static void __exit s5k4e6_front_htc_exit_module(void)
{
    pr_info("%s:%d\n", __func__, __LINE__);
    if (s5k4e6_front_htc_s_ctrl.pdev) {
        msm_sensor_free_sensor_data(&s5k4e6_front_htc_s_ctrl);
        platform_driver_unregister(&s5k4e6_front_htc_platform_driver);
    } else
        i2c_del_driver(&s5k4e6_front_htc_i2c_driver);
    return;
}

static struct msm_sensor_fn_t s5k4e6_front_htc_sensor_func_tbl = {
    .sensor_config = msm_sensor_config,
#ifdef CONFIG_COMPAT
    .sensor_config32 = msm_sensor_config32,
#endif
    .sensor_power_up = msm_sensor_power_up,
    .sensor_power_down = msm_sensor_power_down,
    .sensor_match_id = s5k4e6_front_htc_sensor_match_id,
    .sensor_i2c_read_fuseid = s5k4e6_front_htc_read_fuseid,
#ifdef CONFIG_COMPAT
    .sensor_i2c_read_fuseid32 = s5k4e6_front_htc_read_fuseid32,
#endif
};

static struct msm_sensor_ctrl_t s5k4e6_front_htc_s_ctrl = {
    .sensor_i2c_client = &s5k4e6_front_htc_sensor_i2c_client,
    .power_setting_array.power_setting = s5k4e6_front_htc_power_setting,
    .power_setting_array.size = ARRAY_SIZE(s5k4e6_front_htc_power_setting),
    .msm_sensor_mutex = &s5k4e6_front_htc_mut,
    .sensor_v4l2_subdev_info = s5k4e6_front_htc_subdev_info,
    .sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4e6_front_htc_subdev_info),
    .func_tbl = &s5k4e6_front_htc_sensor_func_tbl
};

module_init(s5k4e6_front_htc_init_module);
module_exit(s5k4e6_front_htc_exit_module);
MODULE_DESCRIPTION("s5k4e6_front_htc");
MODULE_LICENSE("GPL v2");
