/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __SDHCI_MSM_H__
#define __SDHCI_MSM_H__

#include <linux/mmc/mmc.h>
#include "sdhci-pltfm.h"

struct sdhci_msm_reg_data {
	
	struct regulator *reg;
	
	const char *name;
	
	u32 low_vol_level;
	u32 high_vol_level;
	
	u32 lpm_uA;
	u32 hpm_uA;

	
	bool is_enabled;
	
	bool is_always_on;
	
	bool lpm_sup;
	bool set_voltage_sup;
};

struct sdhci_msm_slot_reg_data {
	
	struct sdhci_msm_reg_data *vdd_data;
	 
	struct sdhci_msm_reg_data *vdd_io_data;
};

struct sdhci_msm_gpio {
	u32 no;
	const char *name;
	bool is_enabled;
};

struct sdhci_msm_gpio_data {
	struct sdhci_msm_gpio *gpio;
	u8 size;
};

struct sdhci_msm_pin_data {
	u8 is_gpio;
	struct sdhci_msm_gpio_data *gpio_data;
};

struct sdhci_pinctrl_data {
	struct pinctrl          *pctrl;
	struct pinctrl_state    *pins_active;
	struct pinctrl_state    *pins_active_sdr104;
	struct pinctrl_state    *pins_sleep;
};

struct sdhci_msm_bus_voting_data {
	struct msm_bus_scale_pdata *bus_pdata;
	unsigned int *bw_vecs;
	unsigned int bw_vecs_size;
};

struct sdhci_msm_pltfm_data {
	
	u32 caps;

	
	u32 caps2;

	unsigned long mmc_bus_width;
	struct sdhci_msm_slot_reg_data *vreg_data;
	bool nonremovable;
	bool use_mod_dynamic_qos;
	bool nonhotplug;
	bool no_1p8v;
	bool pin_cfg_sts;
	struct sdhci_msm_pin_data *pin_data;
	struct sdhci_pinctrl_data *pctrl_data;
	u32 *cpu_dma_latency_us;
	unsigned int cpu_dma_latency_tbl_sz;
	int status_gpio; 
	struct sdhci_msm_bus_voting_data *voting_data;
	u32 *sup_clk_table;
	unsigned char sup_clk_cnt;
	int mpm_sdiowakeup_int;
	int sdiowakeup_irq;
	int slot_type;
	enum pm_qos_req_type cpu_affinity_type;
	cpumask_t cpu_affinity_mask;
	u32 *sup_ice_clk_table;
	unsigned char sup_ice_clk_cnt;
	u32 ice_clk_max;
	u32 ice_clk_min;
	bool core_3_0v_support;
};

struct sdhci_msm_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	bool is_max_bw_needed;
	struct delayed_work vote_work;
	struct device_attribute max_bus_bw;
};

struct sdhci_msm_ice_data {
	struct qcom_ice_variant_ops *vops;
	struct platform_device *pdev;
	int state;
};

struct sdhci_msm_host {
	struct platform_device	*pdev;
	void __iomem *core_mem;    
	int	pwr_irq;	
	struct clk	 *clk;     
	struct clk	 *pclk;    
	struct clk	 *bus_clk; 
	struct clk	 *ff_clk; 
	struct clk	 *sleep_clk; 
	struct clk	 *ice_clk; 
	atomic_t clks_on; 
	struct sdhci_msm_pltfm_data *pdata;
	struct mmc_host  *mmc;
	struct sdhci_pltfm_data sdhci_msm_pdata;
	u32 curr_pwr_state;
	u32 curr_io_level;
	struct completion pwr_irq_completion;
	struct sdhci_msm_bus_vote msm_bus_vote;
	struct device_attribute	polling;
	u32 clk_rate; 
	bool tuning_done;
	bool calibration_done;
	struct proc_dir_entry   *speed_class;
	struct proc_dir_entry   *sd_tray_state;
	u8 saved_tuning_phase;
	bool en_auto_cmd21;
	struct device_attribute auto_cmd21_attr;
	bool is_sdiowakeup_enabled;
	atomic_t controller_clock;
	bool use_cdclp533;
	bool use_updated_dll_reset;
	u32 caps_0;
	enum dev_state mmc_dev_state;
	struct sdhci_msm_ice_data ice;
	u32 ice_clk_rate;
	bool tuning_in_progress;
	bool enhanced_strobe;
};
#endif 
