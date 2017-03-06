#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <asm/debug_display.h>
#include "../../../drivers/video/msm/mdss/mdss_dsi.h"

struct dsi_power_data {
	uint32_t sysrev;         
	struct regulator *vci; 	 
	struct regulator *lab; 	 
	struct regulator *ibb; 	 
	int vdd1v8;

	int vci_post_on_sleep;
	int vci_post_off_sleep;

	int lab_post_on_sleep;
	int lab_post_off_sleep;

	int ibb_pre_off_sleep;
	int ibb_post_on_sleep;
	int ibb_post_off_sleep;

	int vdd1v8_pre_off_sleep;
	int vdd1v8_post_on_sleep;
	int vdd1v8_post_off_sleep;

	int rst_post_off_sleep;
};

static int htc_8952_76_regulator_init(struct platform_device *pdev)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;
	u32 tmp = 0;
	struct mdss_panel_info *pinfo = NULL;

	PR_DISP_INFO("%s\n", __func__);
	if (!pdev) {
		PR_DISP_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		PR_DISP_ERR("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	pwrdata = devm_kzalloc(&pdev->dev,
				sizeof(struct dsi_power_data), GFP_KERNEL);
	if (!pwrdata) {
		PR_DISP_ERR("%s: FAILED to alloc pwrdata\n", __func__);
		return -ENOMEM;
	}

	ctrl_pdata->dsi_pwrctrl_data = pwrdata;

	pwrdata->vci = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(pwrdata->vci)) {
		PR_DISP_ERR("%s: could not get vdda vreg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vci));
		pwrdata->vci = 0;
	} else {

		
		ret = regulator_set_voltage(pwrdata->vci, 3000000, 3000000);
		if (ret) {
			PR_DISP_ERR("%s: set voltage failed on vdda vreg, rc=%d\n",
			__func__, ret);
			return ret;
		}
		ret = of_property_read_u32(pdev->dev.of_node,"htc,vci-post-on-sleep", &tmp);
		pwrdata->vci_post_on_sleep = (!ret ? tmp : 0);

		ret = of_property_read_u32(pdev->dev.of_node,"htc,vci-post-off-sleep", &tmp);
		pwrdata->vci_post_off_sleep = (!ret ? tmp : 0);
		PR_DISP_INFO("%s vci_post_on_sleep=%d,vci_post_off_sleep=%d\n", __func__,
					pwrdata->vci_post_on_sleep,
					pwrdata->vci_post_off_sleep);

	}

	pwrdata->lab = devm_regulator_get(&pdev->dev, "lab");
	if (IS_ERR(pwrdata->lab)) {
		PR_DISP_ERR("%s: could not get lab vreg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->lab));
		pwrdata->lab = 0;
	} else {
		
		pinfo = &ctrl_pdata->panel_data.panel_info;
		PR_DISP_INFO("lab_voltage=%d\n",pinfo->mdss_lab_voltage);

		ret = regulator_set_voltage(pwrdata->lab, pinfo->mdss_lab_voltage, pinfo->mdss_lab_voltage);
		if (ret) {
			PR_DISP_ERR("%s: set voltage failed on lab vreg, rc=%d\n",
			__func__, ret);
			return ret;
		}

		ret = of_property_read_u32(pdev->dev.of_node,"htc,lab-post-on-sleep", &tmp);
		pwrdata->lab_post_on_sleep = (!ret ? tmp : 0);

		ret = of_property_read_u32(pdev->dev.of_node,"htc,lab-post-off-sleep", &tmp);
		pwrdata->lab_post_off_sleep = (!ret ? tmp : 0);
		PR_DISP_INFO("%s lab_post_on_sleep=%d,lab_post_off_sleep=%d\n", __func__,
					pwrdata->lab_post_on_sleep,
					pwrdata->lab_post_off_sleep);
	}

	pwrdata->ibb = devm_regulator_get(&pdev->dev, "ibb");
	if (IS_ERR(pwrdata->ibb)) {
		PR_DISP_ERR("%s: could not get ibb vreg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->ibb));
		pwrdata->ibb = 0;
	} else {
		
		pinfo = &ctrl_pdata->panel_data.panel_info;
		PR_DISP_INFO("ibb_voltage=%d\n",pinfo->mdss_ibb_voltage);

		ret = regulator_set_voltage(pwrdata->ibb, pinfo->mdss_ibb_voltage, pinfo->mdss_ibb_voltage);
		if (ret) {
			PR_DISP_ERR("%s: set voltage failed on ibb vreg, rc=%d\n",
			__func__, ret);
			return ret;
		}
		ret = of_property_read_u32(pdev->dev.of_node,"htc,ibb-pre-off-sleep", &tmp);
		pwrdata->ibb_pre_off_sleep = (!ret ? tmp : 0);

		ret = of_property_read_u32(pdev->dev.of_node,"htc,ibb-post-on-sleep", &tmp);
		pwrdata->ibb_post_on_sleep = (!ret ? tmp : 0);

		ret = of_property_read_u32(pdev->dev.of_node,"htc,ibb-post-off-sleep", &tmp);
		pwrdata->ibb_post_off_sleep = (!ret ? tmp : 0);
		PR_DISP_INFO("%s ibb_pre_off_sleep=%d,ibb_post_on_sleep=%d,ibb_post_off_sleep=%d\n", __func__,
					pwrdata->ibb_pre_off_sleep,
					pwrdata->ibb_post_on_sleep,
					pwrdata->ibb_post_off_sleep);
	}

	pwrdata->vdd1v8 = of_get_named_gpio(pdev->dev.of_node,
						"htc,vdd1v8-gpio", 0);
	if (!gpio_is_valid(pwrdata->vdd1v8)) {
		pr_err("%s pwrdata->vdd1v8 fail\n", __func__);
		pwrdata->vdd1v8 = 0;
	} else {
		ret = of_property_read_u32(pdev->dev.of_node,"htc,vdd1v8-pre-off-sleep", &tmp);
		pwrdata->vdd1v8_pre_off_sleep = (!ret ? tmp : 0);

		ret = of_property_read_u32(pdev->dev.of_node,"htc,vdd1v8-post-on-sleep", &tmp);
		pwrdata->vdd1v8_post_on_sleep = (!ret ? tmp : 0);

		ret = of_property_read_u32(pdev->dev.of_node,"htc,vdd1v8-post-off-sleep", &tmp);
		pwrdata->vdd1v8_post_off_sleep = (!ret ? tmp : 0);
		PR_DISP_INFO("%s vdd1v8_pre_off_sleep=%d,vdd1v8_post_on_sleep=%d,vdd1v8_post_off_sleep=%d\n", __func__,
					pwrdata->vdd1v8_pre_off_sleep,
					pwrdata->vdd1v8_post_on_sleep,
					pwrdata->vdd1v8_post_off_sleep);
	}

	ret = of_property_read_u32(pdev->dev.of_node,"htc,rst-post-off-sleep", &tmp);
	pwrdata->rst_post_off_sleep = (!ret ? tmp : 0);
	PR_DISP_INFO("%s rst_post_off_sleep=%d\n", __func__,
					pwrdata->rst_post_off_sleep);


	return 0;
}

static int htc_8952_76_regulator_deinit(struct platform_device *pdev)
{
	
	return 0;
}

void htc_8952_76_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i;

	if (pdata == NULL) {
		PR_DISP_ERR("%s: Invalid input data\n", __func__);
		return;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (enable && (mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data))) {
		pr_info("%s not need for dsi right!\n", __func__);
		return;
	}
	if (!enable && (mdss_dsi_is_left_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data))) {
		pr_info("%s not need for dsi left!\n", __func__);
		return;
	}


	pwrdata = ctrl_pdata->dsi_pwrctrl_data;
	pinfo = &ctrl_pdata->panel_data.panel_info;

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		PR_DISP_DEBUG("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	PR_DISP_INFO("%s: enable = %d\n", __func__, enable);

	if (enable) {
		if (pdata->panel_info.first_power_on == 1) {
			PR_DISP_INFO("reset already on in first time\n");
			return;
		}
		for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
			gpio_set_value((ctrl_pdata->rst_gpio),
				pinfo->rst_seq[i]);
			if (pdata->panel_info.rst_seq[++i])
				usleep_range((pinfo->rst_seq[i] * 1000), (pinfo->rst_seq[i] * 1000) + 500);
		}

	} else {
		gpio_set_value(ctrl_pdata->rst_gpio, 0);
		if (pwrdata->rst_post_off_sleep)
			usleep_range((pwrdata->rst_post_off_sleep * 1000),(pwrdata->rst_post_off_sleep * 1000) + 500);
	}

	PR_DISP_INFO("%s: enable = %d done\n", __func__, enable);
}

static int htc_8952_76_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	int ret;

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;

	PR_DISP_INFO("%s: en=%d\n", __func__, enable);
	if (pdata == NULL) {
		PR_DISP_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (enable && (mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data))) {
		pr_info("%s not need for dsi right!\n", __func__);
		return 0;
	}
	if (!enable && (mdss_dsi_is_left_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data))) {
		pr_info("%s not need for dsi left!\n", __func__);
		return 0;
	}

	pwrdata = ctrl_pdata->dsi_pwrctrl_data;

	if (!pwrdata) {
		PR_DISP_ERR("%s: pwrdata not initialized\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (pwrdata->vci) {
			
			ret = regulator_set_optimum_mode(pwrdata->vci, 100000);
			if (ret < 0) {
				PR_DISP_ERR("%s: vdda set opt mode failed.\n",
				__func__);
				return ret;
			}
			ret = regulator_enable(pwrdata->vci);
			if (ret) {
				PR_DISP_ERR("%s: Failed to enable regulator.\n",__func__);
				return ret;
			}
			if (pwrdata->vci_post_on_sleep)
				usleep_range((pwrdata->vci_post_on_sleep * 1000),(pwrdata->vci_post_on_sleep * 1000) + 500);
		}

		if (pwrdata->vdd1v8) {
			
			gpio_set_value(pwrdata->vdd1v8, 1);
			if (pwrdata->vdd1v8_post_on_sleep)
				usleep_range((pwrdata->vdd1v8_post_on_sleep * 1000),(pwrdata->vdd1v8_post_on_sleep * 1000) + 500);
		}
		if (pwrdata->lab) {
			
			ret = regulator_set_optimum_mode(pwrdata->lab, 100000);
			if (ret < 0) {
				PR_DISP_ERR("%s: lab set opt mode failed.\n",
				__func__);
				return ret;
			}

			ret = regulator_enable(pwrdata->lab);
			if (ret) {
				PR_DISP_ERR("%s: Failed to enable lab.\n",__func__);
				return ret;
			}
			if (pwrdata->lab_post_on_sleep)
				usleep_range((pwrdata->lab_post_on_sleep * 1000),(pwrdata->lab_post_on_sleep * 1000) + 500);
		}
		if (pwrdata->ibb) {
			
			ret = regulator_set_optimum_mode(pwrdata->ibb, 100000);
			if (ret < 0) {
				PR_DISP_ERR("%s: ibb set opt mode failed.\n",
				__func__);
				return ret;
			}
			ret = regulator_enable(pwrdata->ibb);
			if (ret) {
				PR_DISP_ERR("%s: Failed to enable ibb.\n",__func__);
				return ret;
			}
			if(pwrdata->ibb_post_on_sleep)
				usleep_range((pwrdata->ibb_post_on_sleep * 1000),(pwrdata->ibb_post_on_sleep * 1000) + 500);
		}
	} else {
		if (pwrdata->ibb) {
			if (pwrdata->ibb_pre_off_sleep)
				usleep_range((pwrdata->ibb_pre_off_sleep * 1000),(pwrdata->ibb_pre_off_sleep * 1000) + 500);
			
			ret = regulator_disable(pwrdata->ibb);
			if (ret) {
				PR_DISP_ERR("%s: Failed to disable ibb regulator.\n", __func__);
				return ret;
			}
			ret = regulator_set_optimum_mode(pwrdata->ibb, 100);
			if (ret < 0) {
				PR_DISP_ERR("%s: ibb set opt mode failed.\n",__func__);
				return ret;
			}
			if (pwrdata->ibb_post_off_sleep)
				usleep_range((pwrdata->ibb_post_off_sleep * 1000),(pwrdata->ibb_post_off_sleep * 1000) + 500);
		}
		if (pwrdata->lab) {
			
			ret = regulator_disable(pwrdata->lab);
			if (ret) {
				PR_DISP_ERR("%s: Failed to disable lab regulator.\n",__func__);
				return ret;
			}
			ret = regulator_set_optimum_mode(pwrdata->lab, 100);
			if (ret < 0) {
				PR_DISP_ERR("%s: lab set opt mode failed.\n",__func__);
				return ret;
			}
			if (pwrdata->lab_post_off_sleep)
				usleep_range((pwrdata->lab_post_off_sleep * 1000),(pwrdata->lab_post_off_sleep * 1000) + 500);
		}

		if (pwrdata->vdd1v8) {
			if (pwrdata->vdd1v8_pre_off_sleep)
				usleep_range((pwrdata->vdd1v8_pre_off_sleep * 1000),(pwrdata->vdd1v8_pre_off_sleep * 1000) + 500);
			
			gpio_set_value(pwrdata->vdd1v8, 0);
			if (pwrdata->vdd1v8_post_off_sleep)
				usleep_range((pwrdata->vdd1v8_post_off_sleep * 1000),(pwrdata->vdd1v8_post_off_sleep * 1000) + 500);
		}

		if (pwrdata->vci) {
			
			ret = regulator_disable(pwrdata->vci);
			if (ret) {
				PR_DISP_ERR("%s: Failed to disable vdda regulator.\n",
					__func__);
				return ret;
			}
			ret = regulator_set_optimum_mode(pwrdata->vci, 100);
			if (ret < 0) {
				PR_DISP_ERR("%s: vddpll_vreg set opt mode failed.\n",
				__func__);
				return ret;
			}
			if (pwrdata->vci_post_off_sleep)
				usleep_range((pwrdata->vci_post_off_sleep * 1000),(pwrdata->vci_post_off_sleep * 1000) + 500);
		}
	}
	PR_DISP_INFO("%s: en=%d done\n", __func__, enable);

	return 0;
}

static struct mdss_dsi_pwrctrl dsi_pwrctrl = {
	.dsi_regulator_init = htc_8952_76_regulator_init,
	.dsi_regulator_deinit = htc_8952_76_regulator_deinit,
	.dsi_power_on = htc_8952_76_panel_power_on,
	.dsi_panel_reset = htc_8952_76_panel_reset,

};

static struct platform_device dsi_pwrctrl_device = {
	.name          = "mdss_dsi_pwrctrl",
	.id            = -1,
	.dev.platform_data = &dsi_pwrctrl,
};

int __init htc_8952_dsi_panel_power_register(void)
{
       int ret;

       ret = platform_device_register(&dsi_pwrctrl_device);
       if (ret) {
               pr_err("[DISP] %s: dsi_pwrctrl_device register failed! ret =%x\n",__func__, ret);
               return ret;
       }
       return 0;
}
arch_initcall(htc_8952_dsi_panel_power_register);
