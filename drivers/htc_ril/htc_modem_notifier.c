#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <asm/io.h>

#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/smsm.h>
#include "htc_radio_smem.h"

#define OEM_BASE 0x6F656D00
static int modem_is_load = 0;

static struct miscdevice modem_notifier_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "modem_notifier",
};

static uint32_t check_modem_smsm_stat(uint32_t modem_state)
{
	return (smsm_get_state(SMSM_MODEM_STATE) & modem_state);
}

static void check_modem_smsm_stat_timeout(unsigned timeout, uint32_t modem_state)
{
	while (timeout > 0 && !check_modem_smsm_stat(modem_state)) {
		msleep(100);
		timeout--;
	}
	if (timeout <= 0)
		pr_notice("[modem_notifier]%s: modem smsm stat timeout.\n", __func__);
	else
		pr_info("[modem_notifier]%s: modem smsm stat done.\n", __func__);
}

static void set_oem_reboot_reason(unsigned long code)
{
	struct htc_smem_type *smem;

	smem = ioremap(get_smem_base(), sizeof(struct htc_smem_type));

	if(!smem){
		pr_err("[modem_notifier]%s: ioremap fail.\n", __func__);
		return;
	}

	code |= OEM_BASE;
	memcpy(&smem->htc_smem_ce_radio_dbg_ril_fatal, &code, 4);
	pr_info("[modem_notifier]%s: reboot reason: 0x%x.\n", __func__, smem->htc_smem_ce_radio_dbg_ril_fatal);

	iounmap(smem);
}

static int notify_modem_app_reboot_call(struct notifier_block *this,
								unsigned long code, void *_cmd)
{
	unsigned long oem_code = 0;
	pr_info("[modem_notifier]%s: notify modem app is rebooting/powering off\n", __func__);

	switch (code) {
		case SYS_RESTART:
		case SYS_POWER_OFF:
			
			if(modem_is_load && _cmd && (strlen(_cmd)==6) && !strncmp(_cmd, "oem-9", 5) ) {
				pr_info("[modem_notifier]%s: send reboot reason to modem.\n", __func__);
				oem_code = simple_strtoul(_cmd + 4, 0, 16) & 0xff;
				set_oem_reboot_reason(oem_code);
				smsm_change_state(SMSM_APPS_STATE, 0, SMSM_APPS_REBOOT);
				check_modem_smsm_stat_timeout(100, SMSM_RESET);
			}
			else{
				pr_info("[modem_notifier]%s: Ignoring send smsm.\n", __func__);
			}
                        break;
               default:
                        break;
	}

	return NOTIFY_DONE;
}

static int modem_notifier_cb(struct notifier_block *this,
				unsigned long code, void *data)
{
	switch(code) {
	    case SUBSYS_AFTER_POWERUP:
		modem_is_load = 1;
        	break;
            case SUBSYS_AFTER_SHUTDOWN:
                modem_is_load = 0;
                break;
	    case SUBSYS_SOC_RESET:  
	        pr_info("[modem_notifier]%s: notify modem to do cache flush [Before]\n", __func__);
	        if (!check_modem_smsm_stat(SMSM_RESET)){
	            
	            pr_info("[modem_notifier]%s: notify modem to do cache flush [Start]\n", __func__);
	            
	            set_oem_reboot_reason(0x98);
	            smsm_change_state(SMSM_APPS_STATE, 0, SMSM_APPS_REBOOT);
	            check_modem_smsm_stat_timeout(100, SMSM_RESET);
	            smsm_change_state(SMSM_APPS_STATE, SMSM_APPS_REBOOT, 0);
	            pr_info("[modem_notifier]%s: notify modem to do cache flush [End]\n", __func__);
		    }
		    break;
		default:
			break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block modem_notifier = {
	.notifier_call = notify_modem_app_reboot_call,
};

static struct notifier_block modem_state_notifier_block = {
	.notifier_call = modem_notifier_cb,
};

int __init htc_modem_notifier_init(void)
{
    int ret = -1;

    ret = misc_register(&modem_notifier_misc);
    if(ret < 0)
        pr_err("[modem_notifier]%s register misc fail.\n", __func__);

    subsys_notif_register_notifier("modem", &modem_state_notifier_block);
    register_reboot_notifier(&modem_notifier);
    return ret;
}

static void __exit htc_modem_notifier_exit(void)
{
	int ret;

    ret = misc_deregister(&modem_notifier_misc);
	if (ret < 0)
		pr_err("[modem_notifier]%s: failed to unregister misc device!\n", __func__);
}

module_init(htc_modem_notifier_init);
module_exit(htc_modem_notifier_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HTC modem notifier");
