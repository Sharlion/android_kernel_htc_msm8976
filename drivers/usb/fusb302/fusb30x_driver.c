
#include <linux/init.h>															
#include <linux/module.h>														
#include <linux/kernel.h>														
#include <linux/i2c.h>															
#include <linux/slab.h>															
#include <linux/types.h>														
#include <linux/errno.h>														
#include <linux/of_device.h>													
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#include "fusb30x_global.h"														
#include "platform_helpers.h"													

#include "core/core.h"														 
#include "core/fusb30X.h"
#include "core/TypeC.h"


#include "fusb30x_driver.h"
#include <linux/htc_flags.h>
#include <linux/usb/class-dual-role.h>



extern bool VCONN_enabled;
extern FSC_BOOL blnSrcPreferred;
extern FSC_BOOL blnSnkPreferred;
extern FSC_U8 TypeCSubState;
bool flag_DualRoleSet = FALSE;
bool needDelayHostMode = TRUE;

ssize_t dump_all_register(char *buf)
{
	int i = 0, len = 0;
	FSC_U8 temp = 0;

	for (i = 0; i < 67; i++){
		switch (i)
		{
			
			case regInterrupta:
				temp = Registers.Status2.byte[2];
				break;
			case regInterruptb:
				temp = Registers.Status2.byte[3];
				break;
			case regInterrupt:
				temp = Registers.Status2.byte[6];
				break;
			default:
				DeviceRead(i, 1, &temp);
				break;
		}
		if ((i % 0x10 == 0) && i != 0)
			len += snprintf(buf + len, PAGE_SIZE, "\n");
		len += snprintf(buf + len, 4096, "0x%.2x ", temp);
	}
	return len;
}
EXPORT_SYMBOL(dump_all_register);

FSC_U8 regOffset = 0;
static ssize_t dump_register_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	FSC_U8 temp = 0;

	switch (regOffset)
	{
		
		case regControl0:
			temp = 0xff;
			break;
		
		case regControl1:
			temp = 0xff;
			break;
		case regInterrupta:
			temp = Registers.Status2.byte[2];
			break;
		case regInterruptb:
			temp = Registers.Status2.byte[3];
			break;
		case regInterrupt:
			temp = Registers.Status2.byte[6];
			break;
		default:
			DeviceRead(regOffset, 1, &temp);
			break;
	}

	printk("FUSB  %s : Read dump_register offset:0x%.2x(%d), data:0x%.2x(%d)\n", __func__, regOffset, regOffset, temp, temp);

	return snprintf(buf, PAGE_SIZE, "0x%.2x (%d)\n", temp, temp);
}

static ssize_t dump_register_store(struct device *pdev, struct device_attribute *attr,
				const char *buff, size_t size)
{
	unsigned int store_val = 0;
	sscanf(buff, "%d", &store_val);

	regOffset = store_val;
	printk("FUSB  %s : Store dump_register: 0x%2x (%d)\n", __func__, regOffset, regOffset);

	return size;
}



int fusb_debug_level = 0;
static ssize_t fusb_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned length;

	length = sprintf(buf, "%d\n", fusb_debug_level);
	printk("FUSB  %s : Show fusb_debug_level: %d\n", __func__, fusb_debug_level);
	return length;
}

static ssize_t fusb_debug_store(struct device *pdev, struct device_attribute *attr,
				const char *buff, size_t size)
{
	sscanf(buff, "%d", &fusb_debug_level);

	printk("FUSB  %s : Store fusb_debug_level: %d\n", __func__, fusb_debug_level);

	return size;
}

static ssize_t PD_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned length;

	length = sprintf(buf, "%d\n", USBPDEnabled);
	printk("FUSB  %s : Show PD_on(USBPDEnabled): %d\n", __func__, USBPDEnabled);
	return length;
}

static ssize_t PD_on_store(struct device *pdev, struct device_attribute *attr,
				const char *buff, size_t size)
{
	int PD_on = 0;
	sscanf(buff, "%d", &PD_on);

	printk("FUSB  %s : Store PD_on(USBPDEnabled): %d, and trigger %s PD detection\n", __func__, PD_on, ((PD_on == 1) ? "Enable" : "Disable"));
	if (PD_on == 1) {
		core_enable_pd(true);
	} else if (PD_on == 0) {
		core_enable_pd(false);
	}

	return size;
}

static ssize_t vconn_en_store(struct device *pdev, struct device_attribute *attr,
				const char *buff, size_t size)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int vconn_input = 0;
	sscanf(buff, "%d", &vconn_input);

	printk("FUSB  [%s]: input: %d\n", __func__, vconn_input);
	switch (vconn_input) {
		case 0: 
			if (VCONN_enabled) {
				if (fusb_PowerVconn(false))
					printk("FUSB  [%s]: Error: Unable to force power off VCONN!\n", __func__);
				else {
					Registers.Switches.byte[0] = Registers.Switches_temp.byte[0];
					Registers.Switches.VCONN_CC1 = 0;
					Registers.Switches.VCONN_CC2 = 0;
					DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
					VCONN_enabled = false;
					chip->vconn = VCONN_SUPPLY_NO;
					printk("FUSB  [%s]: Force power off the VCONN done, and now Registers.Switches(0x02): 0x%x\n", __func__, Registers.Switches.byte[0]);
				}
			} else {
				printk("FUSB  [%s]: VCONN is already disabled, so we just break\n", __func__);
			}
			break;
		case 1: 
			if (!fusb_PowerVconn(true))
				printk("FUSB  [%s]: Error: Unable to force power on VCONN %d\n", __func__, vconn_input);
			else {
				DeviceRead(regSwitches0, 1, &Registers.Switches_temp.byte[0]);
				Registers.Switches.VCONN_CC1 = 1;
				Registers.Switches.VCONN_CC2 = 0;
				DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
				VCONN_enabled = true;
				chip->vconn = VCONN_SUPPLY_YES;
				printk("FUSB  [%s]: Force power on the VCONN %d done, and now Registers.Switches(0x02): 0x%x\n", __func__, vconn_input, Registers.Switches.byte[0]);
			}
			break;
		case 2: 
			if (!fusb_PowerVconn(true))
				printk("FUSB  [%s]: Error: Unable to force power on VCONN %d\n", __func__, vconn_input);
			else {
				DeviceRead(regSwitches0, 1, &Registers.Switches_temp.byte[0]);
				Registers.Switches.VCONN_CC1 = 0;
				Registers.Switches.VCONN_CC2 = 1;
				DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
				VCONN_enabled = true;
				chip->vconn = VCONN_SUPPLY_YES;
				printk("FUSB  [%s]: Force power on the VCONN %d done, and now Registers.Switches(0x02): 0x%x\n", __func__, vconn_input, Registers.Switches.byte[0]);
			}
			break;
		default:
			printk("FUSB  [%s]: Error: Unable to handle the input: %d\n", __func__, vconn_input);
			break;
	}

	return size;
}

static DEVICE_ATTR(dump_register, S_IWUSR|S_IRUGO, dump_register_show, dump_register_store);
static DEVICE_ATTR(fusb_debug_level, S_IWUSR|S_IRUGO, fusb_debug_show, fusb_debug_store);
static DEVICE_ATTR(PD_on, S_IWUSR|S_IRUGO, PD_on_show, PD_on_store);
static DEVICE_ATTR(vconn_en, S_IWUSR|S_IRUGO, NULL, vconn_en_store);


static struct device_attribute *fusb_attributes[] = {
	&dev_attr_dump_register,
	&dev_attr_fusb_debug_level,
	&dev_attr_PD_on,
	&dev_attr_vconn_en,
	NULL
};

extern bool ReStartIrq_flag;
extern void core_reset(void);
void fusb_debounce_work_func(struct work_struct* delayed_work)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	core_reset();

	if (flag_DualRoleSet) {
		
		PortType = USBTypeC_DRP;
		blnSrcPreferred = FALSE;
		blnSnkPreferred = TRUE;
		flag_DualRoleSet = FALSE;
	}

	enable_irq(chip->gpio_IntN_irq);
	ReStartIrq_flag = false;
	printk("FUSB  [%s] - Enable chip->gpio_IntN_irq IRQ\n", __func__);
}
void fusb_delayHost_func(struct work_struct* delayed_work)
{
	needDelayHostMode = FALSE;
	pr_debug("FUSB	%s - set needDelayHostMode= %d\n", __func__, needDelayHostMode);
}


int fusb_dual_role_get_property(struct dual_role_phy_instance *dual_role,
			     enum dual_role_property prop,
			     unsigned int *val)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	switch (prop) {
		case DUAL_ROLE_PROP_SUPPORTED_MODES:
			break;
		case DUAL_ROLE_PROP_MODE:
			*val = (unsigned int)chip->pmode;
			break;
		case DUAL_ROLE_PROP_PR:
			*val = (unsigned int)chip->prole;
			break;
		case DUAL_ROLE_PROP_DR:
			*val = (unsigned int)chip->drole;
			break;
		case DUAL_ROLE_PROP_VCONN_SUPPLY:
			*val = (unsigned int)chip->vconn;
			break;
		default:
			break;
	}
	return 0;
}

int fusb_dual_role_set_property(struct dual_role_phy_instance *dual_role,
			     enum dual_role_property prop,
			     const unsigned int *val)
{
	flag_DualRoleSet = FALSE;
	switch (prop) {
		case DUAL_ROLE_PROP_SUPPORTED_MODES:
			printk("FUSB  %s : prop:%d, not supported case so far\n", __func__, prop);
			break;
		case DUAL_ROLE_PROP_MODE:
			switch (*val) {
				case PMODE_UFP:
					flag_DualRoleSet = TRUE;
					PortType = USBTypeC_DRP;
					blnSrcPreferred = FALSE;
					blnSnkPreferred = TRUE;
					break;
				case PMODE_DFP:
					flag_DualRoleSet = TRUE;
					PortType = USBTypeC_DRP;
					blnSrcPreferred = TRUE;
					blnSnkPreferred = FALSE;
					break;
				case PMODE_UNKNOWN:
				default:
					PortType = USBTypeC_DRP;
					blnSrcPreferred = FALSE;
					blnSnkPreferred = TRUE;
					break;
			}

			if (flag_DualRoleSet) {
				printk("FUSB  %s :We want to change mode, so we force trigger StateMachineTypeC() with new PortType: %d, blnSrcPreferred: %d, blnSnkPreferred: %d\n",
						__func__, PortType, blnSrcPreferred, blnSnkPreferred);
				if (ConnState == AttachedSource) {
					TypeCSubState = 2;
					StateMachineAttachedSource();
				} else {
					SetStateDelayUnattached();
				}
				StateMachineTypeC();
			}
			break;
		case DUAL_ROLE_PROP_PR:
			printk("FUSB  %s : prop:%d, not supported case so far\n", __func__, prop);
			break;
		case DUAL_ROLE_PROP_DR:
			printk("FUSB  %s : prop:%d, not supported case so far\n", __func__, prop);
			break;
		case DUAL_ROLE_PROP_VCONN_SUPPLY:
			printk("FUSB  %s : prop:%d, not supported case so far\n", __func__, prop);
			break;
		default:
			printk("FUSB  %s : the input(prop:%d) is not supported\n", __func__, prop);
			break;
	}
	return 0;
}

int fusb_dual_role_property_is_writeable(struct dual_role_phy_instance *dual_role,
			     enum dual_role_property prop)
{
	int val = 0;
	switch (prop) {
		case DUAL_ROLE_PROP_SUPPORTED_MODES:
			val = 0;
			break;
		case DUAL_ROLE_PROP_MODE:
			val = 1;
			break;
		case DUAL_ROLE_PROP_PR:
		case DUAL_ROLE_PROP_DR:
			val = 0;
			break;
		case DUAL_ROLE_PROP_VCONN_SUPPLY:
			val = 1;
			break;
		default:
			break;
	}
	return val;
}

enum dual_role_property fusb_properties[5] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	DUAL_ROLE_PROP_VCONN_SUPPLY,
};

static const struct dual_role_phy_desc fusb_desc = {
	.name = "otg_default",
	.properties = fusb_properties,
	.num_properties = 5,
	.get_property = fusb_dual_role_get_property,
	.set_property = fusb_dual_role_set_property,
	.property_is_writeable = fusb_dual_role_property_is_writeable,
};

static int fusb30x_pm_suspend(struct device *dev)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		if (get_debug_flag() & 0x200) {
			
			return 0;
		} else {
			printk("FUSB  [%s] - Error: Chip structure is NULL!\n", __func__);
			return 0;
		}
	}
	dev_dbg(dev, "FUSB PM suspend\n");
	atomic_set(&chip->pm_suspended, 1);
	return 0;
}

static int fusb30x_pm_resume(struct device *dev)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		if (get_debug_flag() & 0x200) {
			
			return 0;
		} else {
			printk("FUSB  [%s] - Error: Chip structure is NULL!\n", __func__);
			return 0;
		}
	}
	dev_dbg(dev, "FUSB PM resume\n");
	atomic_set(&chip->pm_suspended, 0);
	return 0;
}

static int __init fusb30x_init(void)
{
	pr_debug("FUSB	%s - Start driver initialization...\n", __func__);

	return i2c_add_driver(&fusb30x_driver);
}

static void __exit fusb30x_exit(void)
{
	i2c_del_driver(&fusb30x_driver);
	pr_debug("FUSB	%s - Driver deleted...\n", __func__);
}

static int fusb30x_probe (struct i2c_client* client,
						  const struct i2c_device_id* id)
{
	int ret = 0;
	struct fusb30x_chip* chip;
	struct i2c_adapter* adapter;
	struct device_attribute **attrs = fusb_attributes;
	struct device_attribute *attr;

	if (get_debug_flag() & 0x200) {
		printk("FUSB  [%s] -(Workaround for ATS) Do not probe FUSB driver since it is in Debug mode with 5 200\n", __func__);
		return 0;
	}

	if (!client)
	{
		pr_err("FUSB  %s - Error: Client structure is NULL!\n", __func__);
		return -EINVAL;
	}
	dev_info(&client->dev, "%s\n", __func__);

	
	if (!of_match_device(fusb30x_dt_match, &client->dev))
	{
		dev_err(&client->dev, "FUSB  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}
	pr_debug("FUSB	%s - Device tree matched!\n", __func__);

	
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to allocate memory for fg_chip!\n", __func__);
		return -ENOMEM;
	}
	chip->client = client;														
	fusb30x_SetChip(chip);														
	pr_debug("FUSB	%s - Chip structure is set! Chip: %p ... fg_chip: %p\n", __func__, chip, fusb30x_GetChip());

	
	mutex_init(&chip->lock);

	chip->fusb_instance = devm_dual_role_instance_register(&client->dev, &fusb_desc);

	
	fusb_InitChipData();
	pr_debug("FUSB	%s - Chip struct data initialized!\n", __func__);

	
	adapter = to_i2c_adapter(client->dev.parent);
	if (i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_BLOCK_REQUIRED_FUNC))
	{
		chip->use_i2c_blocks = true;
	}
	else
	{
		
		
		dev_warn(&client->dev, "FUSB  %s - Warning: I2C/SMBus block read/write functionality not supported, checking single-read mode...\n", __func__);
		if (!i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_REQUIRED_FUNC))
		{
			dev_err(&client->dev, "FUSB  %s - Error: Required I2C/SMBus functionality not supported!\n", __func__);
			dev_err(&client->dev, "FUSB  %s - I2C Supported Functionality Mask: 0x%x\n", __func__, i2c_get_functionality(adapter));
			return -EIO;
		}
	}
	pr_debug("FUSB	%s - I2C Functionality check passed! Block reads: %s\n", __func__, chip->use_i2c_blocks ? "YES" : "NO");

	
	i2c_set_clientdata(client, chip);
	pr_debug("FUSB	%s - I2C client data set!\n", __func__);

	if (!fusb_PowerFusb302())
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to power on FUSB302!\n", __func__);
	}

	
	if (!fusb_IsDeviceValid())
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to communicate with device!\n", __func__);
		return -EIO;
	}
	pr_debug("FUSB	%s - Device check passed!\n", __func__);

	chip->fusb_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(chip->fusb_pinctrl)) {
		dev_err(&client->dev, "FUSB  %s - Error: pinctrl not ready\n", __func__);
		return PTR_ERR(chip->fusb_pinctrl);
	}

	chip->fusb_default_state = pinctrl_lookup_state(chip->fusb_pinctrl, "default");
	if (IS_ERR(chip->fusb_default_state)) {
		dev_err(&client->dev, "FUSB  %s - Error: no default pinctrl state\n", __func__);
		return PTR_ERR(chip->fusb_default_state);
	}

	ret = pinctrl_select_state(chip->fusb_pinctrl, chip->fusb_default_state);
	if (ret) {
		dev_err(&client->dev, "FUSB  %s - Error: pinctrl enable state failed\n", __func__);
		return ret;
	}

	
	ret = fusb_InitializeGPIO();
	if (ret)
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to initialize GPIO!\n", __func__);
		return ret;
	}
	pr_debug("FUSB	%s - GPIO initialized!\n", __func__);


	
	fusb_InitializeTimer();
	pr_debug("FUSB	%s - Timers initialized!\n", __func__);

	
	fusb_InitializeCore();
	pr_debug("FUSB	%s - Core is initialized!\n", __func__);

#ifdef FSC_DEBUG
	
	fusb_Sysfs_Init();
	pr_debug("FUSB	%s - Sysfs device file created!\n", __func__);
#endif 

	while ((attr = *attrs++))
		ret = device_create_file(&client->dev, attr);

#ifdef FSC_INTERRUPT_TRIGGERED
	
	ret = fusb_EnableInterrupts();
	if (ret)
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to enable interrupts! Error code: %d\n", __func__, ret);
		return -EIO;
	}
	
	INIT_DELAYED_WORK(&chip->debounce_work, fusb_debounce_work_func);
	INIT_DELAYED_WORK(&chip->delayHost_work, fusb_delayHost_func);
#else
	
	fusb_InitializeWorkers();
	
	fusb_ScheduleWork();
	pr_debug("FUSB	%s - Workers initialized and scheduled!\n", __func__);
#endif	

	schedule_delayed_work(&chip->delayHost_work, 1 * HZ);

	dev_info(&client->dev, "FUSB  %s - FUSB30X Driver loaded successfully!\n", __func__);
	return ret;
}

static int fusb30x_remove(struct i2c_client* client)
{
	pr_debug("FUSB	%s - Removing fusb30x device!\n", __func__);

#ifndef FSC_INTERRUPT_TRIGGERED 
	fusb_StopThreads();
#endif	

	fusb_StopTimers();
	fusb_GPIO_Cleanup();
	pr_debug("FUSB	%s - FUSB30x device removed from driver...\n", __func__);
	return 0;
}


module_init(fusb30x_init);														
module_exit(fusb30x_exit);														

MODULE_LICENSE("GPL");															
MODULE_DESCRIPTION("Fairchild FUSB30x Driver");									
MODULE_AUTHOR("Tim Bremm<tim.bremm@fairchildsemi.com>");						
