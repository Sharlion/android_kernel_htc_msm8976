/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/debug_display.h>
#include "mdss_htc_util.h"
#include "mdss_dsi.h"
#include "mdss_mdp.h"

#define CABC_INDEX	 0
#define FLASH_INDEX	 1
#define BURST_INDEX	 2
#define COLOR_TEMP_INDEX 3
#define COLOR_PROFILE_INDEX 4

struct attribute_status htc_attr_stat[] = {
	{"cabc_level_ctl", 1, 1, 1},
	{"flash", 1, 1, 1},
	{"burst_switch", 0, 0, 0},
	{"color_temp_ctl", 0, 0, 0},
	{"color_profile_ctl", 0, 0, 0},
};


static void dimming_do_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(dimming_work, dimming_do_work);
static DEFINE_MUTEX(dimming_wq_lock);

struct msm_fb_data_type *mfd_instance;
#define DEBUG_BUF   2048
#define MIN_COUNT   9
#define DCS_MAX_CNT   128

static char debug_buf[DEBUG_BUF];
struct mdss_dsi_ctrl_pdata *ctrl_instance = NULL;
static char dcs_cmds[DCS_MAX_CNT];
static char *tmp;

static struct dsi_cmd_desc debug_cmd = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, 1}, dcs_cmds
};
static ssize_t dsi_cmd_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	u32 type, value;
	int cnt, i;
	struct dcs_cmd_req cmdreq;

	if (count >= sizeof(debug_buf) || count < MIN_COUNT)
		return -EFAULT;

	if (copy_from_user(debug_buf, buff, count))
		return -EFAULT;

	if (!ctrl_instance)
		return count;

	
	debug_buf[count] = 0;

	
	cnt = (count) / 3 - 1;
	debug_cmd.dchdr.dlen = cnt;

	
	sscanf(debug_buf, "%x", &type);

	if (type == DTYPE_DCS_LWRITE)
		debug_cmd.dchdr.dtype = DTYPE_DCS_LWRITE;
	else if (type == DTYPE_GEN_LWRITE)
		debug_cmd.dchdr.dtype = DTYPE_GEN_LWRITE;
	else
		return -EFAULT;

	PR_DISP_INFO("%s: cnt=%d, type=0x%x\n", __func__, cnt, type);

	
	for (i = 0; i < cnt; i++) {
		if (i >= DCS_MAX_CNT) {
			PR_DISP_INFO("%s: DCS command count over DCS_MAX_CNT, Skip these commands.\n", __func__);
			break;
		}
		tmp = debug_buf + (3 * (i + 1));
		sscanf(tmp, "%x", &value);
		dcs_cmds[i] = value;
		PR_DISP_INFO("%s: value=0x%x\n", __func__, dcs_cmds[i]);
	}

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &debug_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl_instance, &cmdreq);
	PR_DISP_INFO("%s %d\n", __func__, (int)count);
	return count;
}

static const struct file_operations dsi_cmd_fops = {
        .write = dsi_cmd_write,
};

static struct kobject *android_disp_kobj;
static char panel_name[MDSS_MAX_PANEL_LEN] = {0};
static ssize_t disp_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%s\n", panel_name);
	return ret;
}
static DEVICE_ATTR(vendor, S_IRUGO, disp_vendor_show, NULL);

char *disp_vendor(void){
	return panel_name;
}
EXPORT_SYMBOL(disp_vendor);

void htc_panel_info(const char *panel)
{
	if (strcmp(&panel_name[0], "") != 0) {
		PR_DISP_INFO("%s: Created already!\n", __func__);
		return;
	}

	android_disp_kobj = kobject_create_and_add("android_display", NULL);
	if (!android_disp_kobj) {
		PR_DISP_ERR("%s: subsystem register failed\n", __func__);
		return ;
	}
	if (sysfs_create_file(android_disp_kobj, &dev_attr_vendor.attr)) {
		PR_DISP_ERR("Fail to create sysfs file (vendor)\n");
		return ;
	}
	strlcpy(panel_name, panel, MDSS_MAX_PANEL_LEN);
}

void htc_debugfs_init(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct dentry *dent = debugfs_create_dir("htc_debug", NULL);

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_instance = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (IS_ERR(dent)) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_dir fail, error %ld\n",
			__FILE__, __LINE__, PTR_ERR(dent));
		return;
	}

	if (debugfs_create_file("dsi_cmd", 0644, dent, 0, &dsi_cmd_fops)
			== NULL) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return;
	}
	return;
}

static unsigned backlightvalue = 0;
static ssize_t camera_bl_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	ssize_t ret =0;
	snprintf(buf, PAGE_SIZE,"%s%u\n", "BL_CAM_MIN=", backlightvalue);
	ret = strlen(buf) + 1;
	return ret;
}

static ssize_t attrs_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	for (i = 0 ; i < ARRAY_SIZE(htc_attr_stat); i++) {
		if (strcmp(attr->attr.name, htc_attr_stat[i].title) == 0) {
			snprintf(buf, PAGE_SIZE,"%d\n", htc_attr_stat[i].cur_value);
		        ret = strlen(buf) + 1;
			break;
		}
	}
	return ret;
}


#define SLEEPMS_OFFSET(strlen) (strlen+1) 
#define CMDLEN_OFFSET(strlen)  (SLEEPMS_OFFSET(strlen)+sizeof(const __be32))
#define CMD_OFFSET(strlen)     (CMDLEN_OFFSET(strlen)+sizeof(const __be32))

static struct __dsi_cmd_map{
	char *cmdtype_str;
	int  cmdtype_strlen;
	int  dtype;
} dsi_cmd_map[] = {
	{ "DTYPE_DCS_WRITE", 0, DTYPE_DCS_WRITE },
	{ "DTYPE_DCS_WRITE1", 0, DTYPE_DCS_WRITE1 },
	{ "DTYPE_DCS_LWRITE", 0, DTYPE_DCS_LWRITE },
	{ "DTYPE_GEN_WRITE", 0, DTYPE_GEN_WRITE },
	{ "DTYPE_GEN_WRITE1", 0, DTYPE_GEN_WRITE1 },
	{ "DTYPE_GEN_WRITE2", 0, DTYPE_GEN_WRITE2 },
	{ "DTYPE_GEN_LWRITE", 0, DTYPE_GEN_LWRITE },
	{ NULL, 0, 0 }
};

int htc_mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len = 0;
	char *buf;
	struct property *prop;
	struct dsi_ctrl_hdr *pdchdr;
	int i, cnt;
	int curcmdtype;

	i = 0;
	while(dsi_cmd_map[i].cmdtype_str){
		if(!dsi_cmd_map[i].cmdtype_strlen){
			dsi_cmd_map[i].cmdtype_strlen = strlen(dsi_cmd_map[i].cmdtype_str);
			pr_err("%s: parsing, key=%s/%d  [%d]\n", __func__,
				dsi_cmd_map[i].cmdtype_str, dsi_cmd_map[i].cmdtype_strlen, dsi_cmd_map[i].dtype );
		}
		i++;
	}

	prop = of_find_property( np, cmd_key, &len);
	if (!prop || !len || !(prop->length) || !(prop->value)) {
		pr_err("%s: failed, key=%s  [%d : %d : %p]\n", __func__, cmd_key,
			len, (prop ? prop->length : -1), (prop ? prop->value : 0) );
		
		return -ENOMEM;
	}

	data = prop->value;
	blen = 0;
	cnt = 0;
	while(len > 0){
		curcmdtype = 0;
		while(dsi_cmd_map[curcmdtype].cmdtype_strlen){
			if( !strncmp( data,
						  dsi_cmd_map[curcmdtype].cmdtype_str,
						  dsi_cmd_map[curcmdtype].cmdtype_strlen ) &&
				data[dsi_cmd_map[curcmdtype].cmdtype_strlen] == '\0' )
				break;
			curcmdtype++;
		};
		if( !dsi_cmd_map[curcmdtype].cmdtype_strlen ) 
			break;

		i = be32_to_cpup((__be32 *)&data[CMDLEN_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen)]);
		blen += i;
		cnt++;

		data = data + CMD_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen) + i;
		len = len - CMD_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen) - i;
	}

	if(len || !cnt || !blen){
		pr_err("%s: failed, key[%s] : %d cmds, remain=%d bytes \n", __func__, cmd_key, cnt, len);
		return -ENOMEM;
	}

	i = (sizeof(char)*blen+sizeof(struct dsi_ctrl_hdr)*cnt);
	buf = kzalloc( i, GFP_KERNEL);
	if (!buf){
		pr_err("%s: create dsi ctrl oom failed \n", __func__);
		return -ENOMEM;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
	if (!pcmds->cmds){
		pr_err("%s: create dsi commands oom failed \n", __func__);
		goto exit_free;
	}

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = i;
	data = prop->value;
	for(i=0; i<cnt; i++){
		pdchdr = &pcmds->cmds[i].dchdr;

		curcmdtype = 0;
		while(dsi_cmd_map[curcmdtype].cmdtype_strlen){
			if( !strncmp( data,
						 dsi_cmd_map[curcmdtype].cmdtype_str,
						 dsi_cmd_map[curcmdtype].cmdtype_strlen ) &&
				data[dsi_cmd_map[curcmdtype].cmdtype_strlen] == '\0' ){
				pdchdr->dtype = dsi_cmd_map[curcmdtype].dtype;
				break;
			}
			curcmdtype ++;
		}

		pdchdr->last = 0x01;
		pdchdr->vc = 0x00;
		pdchdr->ack = 0x00;
		pdchdr->wait = be32_to_cpup((__be32 *)&data[SLEEPMS_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen)]) & 0xff;
		pdchdr->dlen = be32_to_cpup((__be32 *)&data[CMDLEN_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen)]);
		memcpy( buf, pdchdr, sizeof(struct dsi_ctrl_hdr) );
		buf += sizeof(struct dsi_ctrl_hdr);
		memcpy( buf, &data[CMD_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen)], pdchdr->dlen);
		pcmds->cmds[i].payload = buf;
		buf += pdchdr->dlen;
		data = data + CMD_OFFSET(dsi_cmd_map[curcmdtype].cmdtype_strlen) + pdchdr->dlen;
	}

	
	pcmds->link_state = DSI_LP_MODE;

	if (link_key) {
		data = of_get_property(np, link_key, NULL);
		if (data && !strcmp(data, "dsi_hs_mode"))
			pcmds->link_state = DSI_HS_MODE;
		else
			pcmds->link_state = DSI_LP_MODE;
	}
	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

exit_free:
	kfree(buf);
	return -ENOMEM;
}

static ssize_t attr_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
	unsigned long res;
	int rc, i;

	rc = strict_strtoul(buf, 10, &res);
	if (rc) {
		pr_err("invalid parameter, %s %d\n", buf, rc);
		count = -EINVAL;
		goto err_out;
	}

	for (i = 0 ; i < ARRAY_SIZE(htc_attr_stat); i++) {
		if (strcmp(attr->attr.name, htc_attr_stat[i].title) == 0) {
			htc_attr_stat[i].req_value = res;
			break;
		}
	}
err_out:
	return count;
}

static ssize_t burst_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t count)
{
	ssize_t retval = attr_store(dev, attr, buf, count);

	if(mfd_instance == NULL) {
		pr_info("skip htc_set_burst due to mfd is null\n");
		return -EPERM;
	}
	htc_set_burst(mfd_instance);
	return retval;
}

static DEVICE_ATTR(backlight_info, S_IRUGO, camera_bl_show, NULL);
static DEVICE_ATTR(cabc_level_ctl, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static DEVICE_ATTR(burst_switch, S_IRUGO | S_IWUSR, attrs_show, burst_store);
static DEVICE_ATTR(color_temp_ctl, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static DEVICE_ATTR(color_profile_ctl, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static struct attribute *htc_extend_attrs[] = {
	&dev_attr_backlight_info.attr,
	&dev_attr_cabc_level_ctl.attr,
	&dev_attr_burst_switch.attr,
	&dev_attr_color_temp_ctl.attr,
	&dev_attr_color_profile_ctl.attr,
	NULL,
};

static struct attribute_group htc_extend_attr_group = {
	.attrs = htc_extend_attrs,
};

void htc_register_attrs(struct kobject *led_kobj, struct msm_fb_data_type *mfd)
{
	int rc;

	rc = sysfs_create_group(led_kobj, &htc_extend_attr_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);

	return;
}

void htc_reset_status(void)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(htc_attr_stat); i++) {
		htc_attr_stat[i].cur_value = htc_attr_stat[i].def_value;
	}
	
	mutex_lock(&dimming_wq_lock);
	cancel_delayed_work_sync(&dimming_work);
	mutex_unlock(&dimming_wq_lock);

	return;
}

void htc_register_camera_bkl(int level)
{
	backlightvalue = level;
}

void htc_set_cabc(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if ((htc_attr_stat[CABC_INDEX].req_value > 2) || (htc_attr_stat[CABC_INDEX].req_value < 0))
		return;

	if (!ctrl_pdata->cabc_off_cmds.cmds)
		return;

	if (!ctrl_pdata->cabc_ui_cmds.cmds)
		return;

	if (!ctrl_pdata->cabc_video_cmds.cmds)
		return;

	if (htc_attr_stat[CABC_INDEX].req_value == htc_attr_stat[CABC_INDEX].cur_value)
		return;

	if (htc_attr_stat[CABC_INDEX].req_value == 0) {
		mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->cabc_off_cmds);
	} else if (htc_attr_stat[CABC_INDEX].req_value == 1) {
		mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->cabc_ui_cmds);
	} else if (htc_attr_stat[CABC_INDEX].req_value == 2) {
		mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->cabc_video_cmds);
	} else {
		mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->cabc_ui_cmds);
	}

	htc_attr_stat[CABC_INDEX].cur_value = htc_attr_stat[CABC_INDEX].req_value;
	PR_DISP_INFO("%s cabc mode=%d\n", __func__, htc_attr_stat[CABC_INDEX].cur_value);
	return;
}

void htc_set_burst(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;
	static int burst_mode = 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (ctrl_pdata->burst_on_level == 0 || ctrl_pdata->burst_off_level == 0)
		return;

	if (htc_attr_stat[BURST_INDEX].req_value < 0)
		return;

	if (!ctrl_pdata->burst_off_cmds.cmds)
		return;

	if (!ctrl_pdata->burst_on_cmds.cmds)
		return;

	if (htc_attr_stat[BURST_INDEX].req_value == htc_attr_stat[BURST_INDEX].cur_value)
		return;

	if(burst_mode == 1 && htc_attr_stat[BURST_INDEX].req_value <= ctrl_pdata->burst_off_level) {
		burst_mode = 0;
	} else if (burst_mode == 0 && htc_attr_stat[BURST_INDEX].req_value >= ctrl_pdata->burst_on_level){
		burst_mode = 1;
	} else {
		htc_attr_stat[BURST_INDEX].cur_value = htc_attr_stat[BURST_INDEX].req_value;
		return;
	}

	if(!mdss_fb_is_power_on(mfd))
		return;

	memset(&cmdreq, 0, sizeof(cmdreq));

	if (burst_mode) {
		cmdreq.cmds = ctrl_pdata->burst_on_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->burst_on_cmds.cmd_cnt;
	} else {
		cmdreq.cmds = ctrl_pdata->burst_off_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->burst_off_cmds.cmd_cnt;
	}

	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mutex_lock(&mfd->bl_lock);
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	mutex_unlock(&mfd->bl_lock);

	htc_attr_stat[BURST_INDEX].cur_value = htc_attr_stat[BURST_INDEX].req_value;
	PR_DISP_INFO("%s burst mode=%d\n", __func__, htc_attr_stat[BURST_INDEX].cur_value);
	return;
}

static void dimming_do_work(struct work_struct *work)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;

	pdata = dev_get_platdata(&mfd_instance->pdev->dev);

	
	if (mfd_instance->bl_level == 0) {
		mutex_lock(&mfd_instance->bl_lock);
		MDSS_BRIGHT_TO_BL(mfd_instance->bl_level, DEFAULT_BRIGHTNESS,
			mfd_instance->panel_info->bl_max, MDSS_MAX_BL_BRIGHTNESS);

		pdata->set_backlight(pdata, mfd_instance->bl_level);
		mfd_instance->bl_level_scaled = mfd_instance->bl_level;
		mfd_instance->bl_updated = 1;
		mutex_unlock(&mfd_instance->bl_lock);
		PR_DISP_INFO("%s set default backlight!\n", __func__);
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = ctrl_pdata->dimming_on_cmds.cmds;
	cmdreq.cmds_cnt = ctrl_pdata->dimming_on_cmds.cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	PR_DISP_INFO("dimming on\n");
}

void htc_dimming_on(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (!ctrl_pdata->dimming_on_cmds.cmds)
		return;

	mfd_instance = mfd;
	mutex_lock(&dimming_wq_lock);
	schedule_delayed_work(&dimming_work, msecs_to_jiffies(1000));
	mutex_unlock(&dimming_wq_lock);

	return;
}

void htc_dimming_switch(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (!ctrl_pdata->dimming_switch_cmds.cmds || !ctrl_pdata->dimming_off_cmds.cmds)
		return;

	memset(&cmdreq, 0, sizeof(cmdreq));

	if (pdata->panel_info.first_power_on) {
		cmdreq.cmds = ctrl_pdata->dimming_switch_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->dimming_switch_cmds.cmd_cnt;
	} else {
		cmdreq.cmds = ctrl_pdata->dimming_off_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->dimming_off_cmds.cmd_cnt;
	}

	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	PR_DISP_INFO("dimming switch\n");
	return;
}
#if 0
#define HUE_MAX   4096
void htc_set_pp_pa(struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata;
	struct mdss_mdp_mixer *mixer;
	u32 opmode;
	char __iomem *base;

	
	if (htc_mdss_pp_pa[HUE_INDEX].req_value == htc_mdss_pp_pa[HUE_INDEX].cur_value)
		return;

	if (htc_mdss_pp_pa[HUE_INDEX].req_value >= HUE_MAX)
		return;

	mdata = mdss_mdp_get_mdata();
	mixer = mdata->mixer_intf;
	base = mixer->dspp_base;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	
	writel_relaxed(htc_mdss_pp_pa[HUE_INDEX].req_value, base + MDSS_MDP_REG_DSPP_PA_BASE);

	
	opmode = readl_relaxed(base);
	opmode |= MDSS_MDP_DSPP_OP_PA_EN | MDSS_MDP_DSPP_OP_PA_HUE_MASK; 
	writel_relaxed(opmode, base + MDSS_MDP_REG_DSPP_OP_MODE);

	ctl->flush_bits |= BIT(13);
	wmb();

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	htc_mdss_pp_pa[HUE_INDEX].cur_value = htc_mdss_pp_pa[HUE_INDEX].req_value;

	PR_DISP_INFO("%s pp_hue = 0x%x\n", __func__, htc_mdss_pp_pa[HUE_INDEX].req_value);
}
extern struct mdss_dsi_pwrctrl pwrctrl_pdata;

void htc_bkl_ctrl(struct msm_fb_data_type *mfd,int enable)
{
	struct mdss_panel_data *pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if(pwrctrl_pdata.bkl_config)
		pwrctrl_pdata.bkl_config(pdata, enable);

	return;
}
#endif

void htc_enable_backlight_when_flashlight(int enable)
{
	int i;
	for (i = 0 ; i < ARRAY_SIZE(htc_attr_stat); i++) {
		if (strcmp("flash", htc_attr_stat[i].title) == 0) {
			htc_attr_stat[i].req_value = enable;
			break;
		}
	}
}

void htc_set_enable_backlight_when_flashlight(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;
	int enable;

	if (htc_attr_stat[FLASH_INDEX].req_value == htc_attr_stat[FLASH_INDEX].cur_value)
		return;
	enable = htc_attr_stat[FLASH_INDEX].req_value;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);
	if(enable){
		
		mutex_lock(&mfd->bl_lock);
		pdata->set_backlight(pdata, mfd->bl_level_scaled);
		mutex_unlock(&mfd->bl_lock);
		PR_DISP_INFO("%s restore backlight!\n", __func__);

		
		memset(&cmdreq, 0, sizeof(cmdreq));

		cmdreq.cmds = ctrl_pdata->dimming_on_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->dimming_on_cmds.cmd_cnt;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

		PR_DISP_INFO("dimming on\n");
	}else{
		
		memset(&cmdreq, 0, sizeof(cmdreq));

		cmdreq.cmds = ctrl_pdata->dimming_off_cmds.cmds;
		cmdreq.cmds_cnt = ctrl_pdata->dimming_off_cmds.cmd_cnt;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;

		mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
		PR_DISP_INFO("dimming off\n");

		
		mutex_lock(&mfd->bl_lock);
		pdata->set_backlight(pdata, 0);
		mutex_unlock(&mfd->bl_lock);
		PR_DISP_INFO("%s set backlight off!\n", __func__);
	}
	htc_attr_stat[FLASH_INDEX].cur_value = htc_attr_stat[FLASH_INDEX].req_value;
}

int shrink_pwm(struct msm_fb_data_type *mfd, int val)
{
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	unsigned int shrink_br = BRI_SETTING_DEF;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (val <= 0) {
		shrink_br = 0;
	} else if (val > 0 && (val < BRI_SETTING_MIN)) {
		shrink_br = ctrl->pwm_min;
	} else if ((val >= BRI_SETTING_MIN) && (val <= BRI_SETTING_DEF)) {
		shrink_br = (val - BRI_SETTING_MIN) * (ctrl->pwm_default - ctrl->pwm_min) /
				(BRI_SETTING_DEF - BRI_SETTING_MIN) + ctrl->pwm_min;
	} else if (val > BRI_SETTING_DEF && val <= BRI_SETTING_MAX) {
		shrink_br = (val - BRI_SETTING_DEF) * (ctrl->pwm_max - ctrl->pwm_default) /
				(BRI_SETTING_MAX - BRI_SETTING_DEF) + ctrl->pwm_default;
	} else if (val > BRI_SETTING_MAX)
		shrink_br = ctrl->pwm_max;

	PR_DISP_INFO("brightness orig=%d, transformed=%d\n", val, shrink_br);

	return shrink_br;
}

void htc_set_color_temp(struct msm_fb_data_type *mfd, bool force)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int req_mode = 0;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (!ctrl_pdata->color_temp_cnt)
		return;

	if (htc_attr_stat[COLOR_TEMP_INDEX].req_value >= ctrl_pdata->color_temp_cnt)
		return;

	if (!force && (htc_attr_stat[COLOR_TEMP_INDEX].req_value == htc_attr_stat[COLOR_TEMP_INDEX].cur_value))
		return;


	req_mode = htc_attr_stat[COLOR_TEMP_INDEX].req_value;
	mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->color_temp_cmds[req_mode]);

	htc_attr_stat[COLOR_TEMP_INDEX].cur_value = htc_attr_stat[COLOR_TEMP_INDEX].req_value;
	pr_info("%s: color temp mode=%d\n", __func__, htc_attr_stat[COLOR_TEMP_INDEX].cur_value);
	return;
}

void htc_set_color_profile(struct msm_fb_data_type *mfd, bool force)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pdata = dev_get_platdata(&mfd->pdev->dev);

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (!ctrl_pdata->color_default_cmds.cmd_cnt || !ctrl_pdata->color_srgb_cmds.cmd_cnt)
		return;

	if ((htc_attr_stat[COLOR_PROFILE_INDEX].req_value >= MAX_MODE) || (htc_attr_stat[COLOR_PROFILE_INDEX].req_value < DEFAULT_MODE))
		return;

	if (!force && (htc_attr_stat[COLOR_PROFILE_INDEX].req_value == htc_attr_stat[COLOR_PROFILE_INDEX].cur_value))
		return;

	if (htc_attr_stat[COLOR_PROFILE_INDEX].req_value == SRGB_MODE) {
		mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->color_srgb_cmds);
	} else {
		mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->color_default_cmds);
	}

	htc_attr_stat[COLOR_PROFILE_INDEX].cur_value = htc_attr_stat[COLOR_PROFILE_INDEX].req_value;
	pr_info("%s: color profile mode=%d\n", __func__, htc_attr_stat[COLOR_PROFILE_INDEX].cur_value);
	return;
}
