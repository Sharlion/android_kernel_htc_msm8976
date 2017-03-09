#include <linux/kernel.h>
#include <linux/stat.h>															
#include <linux/types.h>														
#include <linux/i2c.h>															
#include <linux/errno.h>														
#include <linux/hrtimer.h>														
#include <linux/workqueue.h>													
#include <linux/delay.h>														
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#include "fusb30x_global.h"														
#include "core/core.h"														 
#include "platform_helpers.h"

#ifdef FSC_DEBUG
#include "hostcomm.h"
#include "core/PD_Types.h"                                                   
#include "core/TypeC_Types.h"                                                
#endif 

const char* FUSB_DT_INTERRUPT_INTN =	"fsc_interrupt_int_n";		
#define FUSB_DT_GPIO_INTN				"fairchild,int_n"			
#define FUSB_DT_GPIO_VBUS_5V			"fairchild,vbus5v"			
#define FUSB_DT_GPIO_VBUS_OTHER			"fairchild,vbusOther"		
#define FUSB_DT_GPIO_POWER		   "fairchild,power-gpio"
#define FUSB_DT_GPIO_VCONN		   "fairchild,vconn-gpio"	
#define FUSB_I2C_RETRY_DELAY	50


#ifdef FSC_DEBUG
#define FUSB_DT_GPIO_DEBUG_SM_TOGGLE	"fairchild,dbg_sm"			
#endif  

#ifdef FSC_INTERRUPT_TRIGGERED
static irqreturn_t _fusb_isr_intn(int irq, void *dev_id);
#endif	

bool fusb_PowerFusb302(void)
{
	struct device_node* node;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int fairchild_power = 0;
	int val = -100;
	printk("FUSB  [%s] - Power on FUSB302\n", __func__);

	if (!chip)
	{
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}

	
	node = chip->client->dev.of_node;
	fairchild_power = of_get_named_gpio(node, FUSB_DT_GPIO_POWER, 0);
	gpio_direction_output( fairchild_power, 1 );

	val = gpio_get_value(fairchild_power);
	if (val == 1)
		return true;
	else
		return false;

}

bool fusb_PowerVconn(bool set)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int val = -100;

	if (!chip)
	{
		printk(KERN_ALERT "FUSB  [%s] - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}

	val = gpio_get_value(chip->gpio_Vconn);
	if (set) {
		if (val == 1) {
			printk("FUSB  [%s] - VCONN is already enabled, we just reurn true directly\n", __func__);
			return true;
		} else {
			printk("FUSB  [%s] - Turn on the VCONN GPIO\n", __func__);
			gpio_direction_output( chip->gpio_Vconn, 1 );
		}
	} else {
		if (val == 0) {
			printk("FUSB  [%s] - VCONN is already disabled, we just reurn false directly\n", __func__);
			return false;
		} else {
			printk("FUSB  [%s] - Turn off the VCONN GPIO\n", __func__);
			gpio_direction_output( chip->gpio_Vconn, 0 );
		}
	}

	val = gpio_get_value(chip->gpio_Vconn);
	if (val == 1)
		return true;
	else
		return false;

}


FSC_S32 fusb_InitializeGPIO(void)
{
    FSC_S32 ret = 0;
	struct device_node* node;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}
	
	node = chip->client->dev.of_node;

    
	chip->gpio_IntN = of_get_named_gpio(node, FUSB_DT_GPIO_INTN, 0);
	if (!gpio_is_valid(chip->gpio_IntN))
	{
        dev_err(&chip->client->dev, "%s - Error: Could not get named GPIO for Int_N! Error code: %d\n", __func__, chip->gpio_IntN);
        return chip->gpio_IntN;
    }

    
    ret = gpio_request(chip->gpio_IntN, FUSB_DT_GPIO_INTN);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Int_N! Error code: %d\n", __func__, ret);
        return ret;
    }

    ret = gpio_direction_input(chip->gpio_IntN);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to input for Int_N! Error code: %d\n", __func__, ret);
        return ret;
    }

#ifdef FSC_DEBUG
	
	gpio_export(chip->gpio_IntN, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_INTN, chip->gpio_IntN);
#endif 

	
	chip->gpio_Vconn = of_get_named_gpio(node, FUSB_DT_GPIO_VCONN, 0);
	if (!gpio_is_valid(chip->gpio_Vconn))
	{
		dev_err(&chip->client->dev, "%s - Error: Could not get GPIO for VCONN! Error code: %d\n", __func__, chip->gpio_Vconn);
		return chip->gpio_Vconn;
	}


#ifdef VBUS_5V_SUPPORTED
	
	chip->gpio_VBus5V = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_5V, 0);
	if (!gpio_is_valid(chip->gpio_VBus5V))
	{
		dev_err(&chip->client->dev, "%s - Error: Could not get GPIO for VBus5V! Error code: %d\n", __func__, chip->gpio_VBus5V);
		fusb_GPIO_Cleanup();
		return chip->gpio_VBus5V;
	}
    
    ret = gpio_request(chip->gpio_VBus5V, FUSB_DT_GPIO_VBUS_5V);
    if (ret < 0)
    {
        dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for VBus5V! Error code: %d\n", __func__, ret);
        return ret;
    }

    ret = gpio_direction_output(chip->gpio_VBus5V, chip->gpio_VBus5V_value);
    if (ret < 0)
	{
		dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for VBus5V! Error code: %d\n", __func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}

#ifdef FSC_DEBUG
	
	gpio_export(chip->gpio_VBus5V, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_VBUS_5V, chip->gpio_VBus5V);
#endif 
	printk(KERN_DEBUG "FUSB  %s - VBus 5V initialized as pin '%d' and is set to '%d'\n", __func__, chip->gpio_VBus5V, chip->gpio_VBus5V_value ? 1 : 0);
#endif

#ifdef VBUS_OTHER_SUPPORTED
	
	
	chip->gpio_VBusOther = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_OTHER, 0);
	chip->gpio_VBusOther = -1;

	if (!gpio_is_valid(chip->gpio_VBusOther))
	{
		
        pr_warning("%s - Warning: Could not get GPIO for VBusOther! Error code: %d\n", __func__, chip->gpio_VBusOther);
	}
	else
	{
        
        ret = gpio_request(chip->gpio_VBusOther, FUSB_DT_GPIO_VBUS_OTHER);
        if (ret < 0)
        {
            dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for VBusOther! Error code: %d\n", __func__, ret);
            return ret;
        }

        ret = gpio_direction_output(chip->gpio_VBusOther, chip->gpio_VBusOther_value);
		if (ret != 0)
		{
            dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for VBusOther! Error code: %d\n", __func__, ret);
			return ret;
		}
        else
        {
            pr_info("FUSB  %s - VBusOther initialized as pin '%d' and is set to '%d'\n", __func__, chip->gpio_VBusOther, chip->gpio_VBusOther_value ? 1 : 0);

        }
#endif

#ifdef FSC_DEBUG
	
	
	chip->dbg_gpio_StateMachine = of_get_named_gpio(node, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, 0);
	if (!gpio_is_valid(chip->dbg_gpio_StateMachine))
	{
		
        pr_warning("%s - Warning: Could not get GPIO for Debug GPIO! Error code: %d\n", __func__, chip->dbg_gpio_StateMachine);
	}
	else
	{
        
        ret = gpio_request(chip->dbg_gpio_StateMachine, FUSB_DT_GPIO_DEBUG_SM_TOGGLE);
        if (ret < 0)
        {
            dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Debug GPIO! Error code: %d\n", __func__, ret);
            return ret;
        }

        ret = gpio_direction_output(chip->dbg_gpio_StateMachine, chip->dbg_gpio_StateMachine_value);
		if (ret != 0)
		{
            dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for Debug GPIO! Error code: %d\n", __func__, ret);
			return ret;
		}
        else
        {
            pr_info("FUSB  %s - Debug GPIO initialized as pin '%d' and is set to '%d'\n", __func__, chip->dbg_gpio_StateMachine, chip->dbg_gpio_StateMachine_value ? 1 : 0);

        }

		
        gpio_export(chip->dbg_gpio_StateMachine, true); 
		gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, chip->dbg_gpio_StateMachine);
	}
#endif  

	return 0;	
}

void fusb_GPIO_Set_VBus5v(FSC_BOOL set)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}
#ifdef VBUS_5V_SUPPORTED
	
    if (gpio_cansleep(chip->gpio_VBus5V))
    {
        gpio_set_value_cansleep(chip->gpio_VBus5V, set ? 1 : 0);
    }
    else
    {
        gpio_set_value(chip->gpio_VBus5V, set ? 1 : 0);
    }
    chip->gpio_VBus5V_value = set;

    pr_debug("FUSB  %s - VBus 5V set to: %d\n", __func__, chip->gpio_VBus5V_value ? 1 : 0);
#endif
}

void fusb_GPIO_Set_VBusOther(FSC_BOOL set)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}

#ifdef VBUS_OTHER_SUPPORTED
	
	if (gpio_is_valid(chip->gpio_VBusOther))
	{
        if (gpio_cansleep(chip->gpio_VBusOther))
        {
            gpio_set_value_cansleep(chip->gpio_VBusOther, set ? 1 : 0);
        }
        else
        {
            gpio_set_value(chip->gpio_VBusOther, set ? 1 : 0);
        }
    }
	chip->gpio_VBusOther_value = set;
#endif
}

FSC_BOOL fusb_GPIO_Get_VBus5v(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}

	return false;
#ifdef VBUS_5V_SUPPORTED
	if (!gpio_is_valid(chip->gpio_VBus5V))
	{
        pr_debug("FUSB  %s - Error: VBus 5V pin invalid! Pin value: %d\n", __func__, chip->gpio_VBus5V);
	}

	return chip->gpio_VBus5V_value;
#endif
}

FSC_BOOL fusb_GPIO_Get_VBusOther(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}

	return false;
#ifdef VBUS_OTHER_SUPPORTED
	return chip->gpio_VBusOther_value;
#endif
}

FSC_BOOL fusb_GPIO_Get_IntN(void)
{
    FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return false;
    }
    else
    {
        if (gpio_cansleep(chip->gpio_IntN))
        {
            ret = !gpio_get_value_cansleep(chip->gpio_IntN);
        }
        else
        {
            ret = !gpio_get_value(chip->gpio_IntN); 
        }
        return (ret != 0);
    }
}

#ifdef FSC_DEBUG
void dbg_fusb_GPIO_Set_SM_Toggle(FSC_BOOL set)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
    }

    if (gpio_is_valid(chip->dbg_gpio_StateMachine))
    {
        if (gpio_cansleep(chip->dbg_gpio_StateMachine))
        {
            gpio_set_value_cansleep(chip->dbg_gpio_StateMachine, set ? 1 : 0);
        }
        else
        {
            gpio_set_value(chip->dbg_gpio_StateMachine, set ? 1 : 0);
        }
        chip->dbg_gpio_StateMachine_value = set;
    }
}

FSC_BOOL dbg_fusb_GPIO_Get_SM_Toggle(void)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return false;
    }
    return chip->dbg_gpio_StateMachine_value;
}
#endif  

void fusb_GPIO_Cleanup(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

#ifdef FSC_INTERRUPT_TRIGGERED
	if (gpio_is_valid(chip->gpio_IntN) && chip->gpio_IntN_irq != -1)	
	{
		devm_free_irq(&chip->client->dev, chip->gpio_IntN_irq, chip);
	}
#endif 

	if (gpio_is_valid(chip->gpio_IntN) >= 0)
	{
#ifdef FSC_DEBUG
		gpio_unexport(chip->gpio_IntN);
#endif 
		gpio_free(chip->gpio_IntN);
	}

#ifdef VBUS_5V_SUPPORTED
	if (gpio_is_valid(chip->gpio_VBus5V) >= 0)
	{
#ifdef FSC_DEBUG
		gpio_unexport(chip->gpio_VBus5V);
#endif 
		gpio_free(chip->gpio_VBus5V);
	}
#endif
#ifdef VBUS_OTHER_SUPPORTED
	if (gpio_is_valid(chip->gpio_VBusOther) >= 0)
	{
		gpio_free(chip->gpio_VBusOther);
	}
#endif
#ifdef FSC_DEBUG
	if (gpio_is_valid(chip->dbg_gpio_StateMachine) >= 0)
	{
		gpio_unexport(chip->dbg_gpio_StateMachine);
		gpio_free(chip->dbg_gpio_StateMachine);
	}
#endif	
}

FSC_BOOL fusb_I2C_WriteData(FSC_U8 address, FSC_U8 length, FSC_U8* data)
{
    FSC_S32 i = 0;
    FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)				
	{
        pr_err("%s - Error: %s is NULL!\n", __func__, (chip == NULL ? "Internal chip structure"
			: (chip->client == NULL ? "I2C Client"
			: "Write data buffer")));
		return false;
	}

	mutex_lock(&chip->lock);
	
	for (i = 0; i <= chip->numRetriesI2C; i++)
	{
		if (atomic_read(&chip->pm_suspended)) {
			pr_debug("FUSB %s: pm_suspended, retry\n", __func__);
			msleep(FUSB_I2C_RETRY_DELAY);
			continue;
		}
		ret = i2c_smbus_write_i2c_block_data(chip->client,						
											 address,							
											 length,							
											 data);								
		if (ret < 0)															
		{
            if (ret == -ERANGE) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINVAL) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EAGAIN) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EAGAIN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EALREADY) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EALREADY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBADE) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EBADE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBADMSG) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EBADMSG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBUSY) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EBUSY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECANCELED) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECANCELED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECOMM) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECOMM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNABORTED) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECONNABORTED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNREFUSED) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECONNREFUSED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNRESET) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECONNRESET.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDEADLK) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EDEADLK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDEADLOCK) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EDEADLOCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDESTADDRREQ) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EDESTADDRREQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EFAULT) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EFAULT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EHOSTDOWN) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EHOSTDOWN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EHOSTUNREACH) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EHOSTUNREACH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EILSEQ) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EILSEQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINPROGRESS) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EINPROGRESS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINTR) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EINTR.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EIO) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBACC) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELIBACC.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBBAD) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELIBBAD.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBMAX) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELIBMAX.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELOOP) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELOOP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EMSGSIZE) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EMSGSIZE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EMULTIHOP) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EMULTIHOP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOBUFS) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOBUFS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENODATA) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENODATA.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENODEV) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENODEV.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOLCK) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOLCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOMEM) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOMEM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOMSG) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOMSG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOPROTOOPT) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOPROTOOPT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOSPC) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOSPC.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOSYS) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOSYS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTBLK) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOTBLK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTTY) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOTTY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTUNIQ) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOTUNIQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENXIO) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENXIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EOVERFLOW) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EOVERFLOW.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPERM) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPERM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPFNOSUPPORT) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPFNOSUPPORT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPIPE) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPIPE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROTO) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPROTO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROTONOSUPPORT) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPROTONOSUPPORT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ERANGE) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMCHG) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EREMCHG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMOTE) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EREMOTE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMOTEIO) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EREMOTEIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ERESTART) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ERESTART.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ESRCH) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ESRCH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETIME) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ETIME.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETIMEDOUT) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ETIMEDOUT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETXTBSY) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ETXTBSY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUCLEAN) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EUCLEAN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUNATCH) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EUNATCH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUSERS) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EUSERS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EWOULDBLOCK) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EWOULDBLOCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EXDEV) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EXDEV.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EXFULL) { dev_err(&chip->client->dev, "%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EXFULL.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EOPNOTSUPP) { dev_err(&chip->client->dev, "%s - I2C Error writing byte data. Address: '0x%02x', Return: -EOPNOTSUPP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROBE_DEFER) { dev_err(&chip->client->dev, "%s - I2C Error writing byte data. Address: '0x%02x', Return: -EPROBE_DEFER.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOENT) { dev_err(&chip->client->dev, "%s - I2C Error writing byte data. Address: '0x%02x', Return: -ENOENT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else { dev_err(&chip->client->dev, "%s - Unexpected I2C error block writing byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__, address, ret, i, chip->numRetriesI2C); }
		}
		else																	
		{
			break;
		}
	}
	mutex_unlock(&chip->lock);

	return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadData(FSC_U8 address, FSC_U8* data)
{
    FSC_S32 i = 0;
    FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)
	{
        pr_err("%s - Error: %s is NULL!\n", __func__, (chip == NULL ? "Internal chip structure"
			: (chip->client == NULL ? "I2C Client"
			: "read data buffer")));
		return false;
	}
	mutex_lock(&chip->lock);
	
	for (i = 0; i <= chip->numRetriesI2C; i++)
	{
		if (atomic_read(&chip->pm_suspended)) {
			pr_debug("FUSB %s: pm_suspended, retry\n", __func__);
			msleep(FUSB_I2C_RETRY_DELAY);
			continue;
		}
		ret = i2c_smbus_read_byte_data(chip->client, (u8)address);		   
		if (ret < 0)															
		{
            if (ret == -ERANGE) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINVAL) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EAGAIN) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EAGAIN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EALREADY) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EALREADY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBADE) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EBADE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBADMSG) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EBADMSG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBUSY) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EBUSY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECANCELED) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECANCELED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECOMM) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECOMM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNABORTED) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECONNABORTED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNREFUSED) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECONNREFUSED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNRESET) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECONNRESET.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDEADLK) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EDEADLK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDEADLOCK) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EDEADLOCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDESTADDRREQ) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EDESTADDRREQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EFAULT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EFAULT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EHOSTDOWN) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EHOSTDOWN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EHOSTUNREACH) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EHOSTUNREACH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EILSEQ) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EILSEQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINPROGRESS) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINPROGRESS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINTR) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINTR.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EIO) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBACC) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELIBACC.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBBAD) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELIBBAD.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBMAX) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELIBMAX.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELOOP) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELOOP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EMSGSIZE) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EMSGSIZE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EMULTIHOP) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EMULTIHOP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOBUFS) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOBUFS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENODATA) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENODATA.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENODEV) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENODEV.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOLCK) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOLCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOMEM) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOMEM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOMSG) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOMSG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOPROTOOPT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOPROTOOPT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOSPC) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOSPC.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOSYS) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOSYS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTBLK) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOTBLK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTTY) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOTTY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTUNIQ) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOTUNIQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENXIO) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENXIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EOVERFLOW) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EOVERFLOW.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPERM) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPERM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPFNOSUPPORT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPFNOSUPPORT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPIPE) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPIPE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROTO) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROTO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROTONOSUPPORT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROTONOSUPPORT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ERANGE) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMCHG) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EREMCHG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMOTE) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EREMOTE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMOTEIO) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EREMOTEIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ERESTART) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERESTART.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ESRCH) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ESRCH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETIME) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ETIME.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETIMEDOUT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ETIMEDOUT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETXTBSY) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ETXTBSY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUCLEAN) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EUCLEAN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUNATCH) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EUNATCH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUSERS) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EUSERS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EWOULDBLOCK) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EWOULDBLOCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EXDEV) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EXDEV.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EXFULL) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EXFULL.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EOPNOTSUPP) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EOPNOTSUPP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROBE_DEFER) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROBE_DEFER.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOENT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOENT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else { dev_err(&chip->client->dev, "%s - Unexpected I2C error reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__, address, ret, i, chip->numRetriesI2C); }
		}
		else																	
		{
            *data = (FSC_U8)ret;
			break;
		}
	}
	mutex_unlock(&chip->lock);

	return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadBlockData(FSC_U8 address, FSC_U8 length, FSC_U8* data)
{
    FSC_S32 i = 0;
    FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)
	{
        pr_err("%s - Error: %s is NULL!\n", __func__, (chip == NULL ? "Internal chip structure"
			: (chip->client == NULL ? "I2C Client"
			: "block read data buffer")));
		return false;
	}

	mutex_lock(&chip->lock);

    
    for (i = 0; i <= chip->numRetriesI2C; i++)
    {
		if (atomic_read(&chip->pm_suspended)) {
			pr_debug("FUSB %s: pm_suspended, retry\n", __func__);
			msleep(FUSB_I2C_RETRY_DELAY);
			continue;
		}
	ret = i2c_smbus_read_i2c_block_data(chip->client, (u8)address, (u8)length, (u8*)data);			
	if (ret < 0)																					
	{
            if (ret == -ERANGE) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINVAL) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EAGAIN) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EAGAIN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EALREADY) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EALREADY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBADE) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EBADE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBADMSG) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EBADMSG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EBUSY) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EBUSY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECANCELED) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECANCELED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECOMM) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECOMM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNABORTED) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECONNABORTED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNREFUSED) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECONNREFUSED.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ECONNRESET) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECONNRESET.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDEADLK) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EDEADLK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDEADLOCK) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EDEADLOCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EDESTADDRREQ) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EDESTADDRREQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EFAULT) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EFAULT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EHOSTDOWN) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EHOSTDOWN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EHOSTUNREACH) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EHOSTUNREACH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EILSEQ) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EILSEQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINPROGRESS) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINPROGRESS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EINTR) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINTR.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EIO) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBACC) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELIBACC.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBBAD) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELIBBAD.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELIBMAX) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELIBMAX.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ELOOP) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELOOP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EMSGSIZE) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EMSGSIZE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EMULTIHOP) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EMULTIHOP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOBUFS) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOBUFS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENODATA) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENODATA.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENODEV) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENODEV.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOLCK) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOLCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOMEM) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOMEM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOMSG) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOMSG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOPROTOOPT) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOPROTOOPT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOSPC) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOSPC.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOSYS) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOSYS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTBLK) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOTBLK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTTY) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOTTY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOTUNIQ) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOTUNIQ.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENXIO) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENXIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EOVERFLOW) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EOVERFLOW.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPERM) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPERM.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPFNOSUPPORT) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPFNOSUPPORT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPIPE) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPIPE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROTO) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPROTO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROTONOSUPPORT) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPROTONOSUPPORT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ERANGE) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMCHG) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EREMCHG.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMOTE) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EREMOTE.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EREMOTEIO) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EREMOTEIO.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ERESTART) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERESTART.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ESRCH) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ESRCH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETIME) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ETIME.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETIMEDOUT) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ETIMEDOUT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ETXTBSY) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ETXTBSY.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUCLEAN) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EUCLEAN.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUNATCH) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EUNATCH.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EUSERS) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EUSERS.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EWOULDBLOCK) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EWOULDBLOCK.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EXDEV) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EXDEV.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EXFULL) { dev_err(&chip->client->dev, "%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EXFULL.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EOPNOTSUPP) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EOPNOTSUPP.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -EPROBE_DEFER) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROBE_DEFER.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else if (ret == -ENOENT) { dev_err(&chip->client->dev, "%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOENT.  Attempt #%d / %d...\n", __func__, address, i, chip->numRetriesI2C); }
            else { dev_err(&chip->client->dev, "%s - Unexpected I2C error block reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__, address, ret, i, chip->numRetriesI2C); }
	}
	else if (ret != length) 
	{
            dev_err(&chip->client->dev, "%s - Error: Block read request of %u bytes truncated to %u bytes.\n", __func__, length, I2C_SMBUS_BLOCK_MAX);
	}
        else
        {
            break;  
        }
    }

	mutex_unlock(&chip->lock);

	return (ret == length);
}


static const unsigned long g_fusb_timer_tick_period_ns = 100000;	

enum hrtimer_restart _fusb_TimerHandler(struct hrtimer* timer)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return HRTIMER_NORESTART;
	}

	if (!timer)
	{
        pr_err("FUSB  %s - Error: High-resolution timer is NULL!\n", __func__);
		return HRTIMER_NORESTART;
	}

	core_tick_at_100us();

#ifdef FSC_DEBUG
	if (chip->dbgTimerTicks++ >= U8_MAX)
	{
		chip->dbgTimerRollovers++;
	}
#endif  

	
	hrtimer_forward(timer, ktime_get(), ktime_set(0, g_fusb_timer_tick_period_ns));

	return HRTIMER_RESTART;			
}

void fusb_InitializeTimer(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	hrtimer_init(&chip->timer_state_machine, CLOCK_MONOTONIC, HRTIMER_MODE_REL);			
	chip->timer_state_machine.function = _fusb_TimerHandler;								

    pr_debug("FUSB  %s - Timer initialized!\n", __func__);
}

void fusb_StartTimers(void)
{
	ktime_t ktime;
	struct fusb30x_chip* chip;

	chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
#ifdef FSC_DEBUG
    
    chip->dbgTimerTicks = 0;
    chip->dbgTimerRollovers = 0;
#endif  

	if (hrtimer_active(&chip->timer_state_machine) != 0)
	{
		hrtimer_cancel(&chip->timer_state_machine);
		pr_debug("FUSB %s - Force cancel hrtimer before re-start it becuase hrtimer is already Active state\n", __func__);
	}
	if (hrtimer_is_queued(&chip->timer_state_machine) != 0)
	{
		hrtimer_cancel(&chip->timer_state_machine);
		pr_debug("FUSB %s - Force cancel hrtimer before re-start it becuase hrtimer is already Queued state\n", __func__);
	}
	ktime = ktime_set(0, g_fusb_timer_tick_period_ns);										
	hrtimer_start(&chip->timer_state_machine, ktime, HRTIMER_MODE_REL);						
}

void fusb_StopTimers(void)
{
    FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	mutex_lock(&chip->lock);
	if (hrtimer_active(&chip->timer_state_machine) != 0)
	{
		ret = hrtimer_cancel(&chip->timer_state_machine);
        pr_debug("%s - Active state machine hrtimer canceled: %d\n", __func__, ret);
	}
	if (hrtimer_is_queued(&chip->timer_state_machine) != 0)
	{
		ret = hrtimer_cancel(&chip->timer_state_machine);
        pr_debug("%s - Queued state machine hrtimer canceled: %d\n", __func__, ret);
	}
	mutex_unlock(&chip->lock);
    pr_debug("FUSB  %s - Timer stopped!\n", __func__);
}

static const FSC_U32 MAX_DELAY_10US = (UINT_MAX / 10);
void fusb_Delay10us(FSC_U32 delay10us)
{
    FSC_U32 us = 0;
	if (delay10us > MAX_DELAY_10US)
	{
        pr_err("%s - Error: Delay of '%u' is too long! Must be less than '%u'.\n", __func__, delay10us, MAX_DELAY_10US);
		return;
	}

	us = delay10us * 10;									

	if (us <= 10)											
	{
		udelay(us);											
	}
	else if (us < 20000)									
	{
		
		usleep_range(us, us + (us / 10));					
	}
	else													
	{
		msleep(us / 1000);									
	}
}

#ifdef FSC_DEBUG

void fusb_timestamp_bytes_to_time(FSC_U32* outSec, FSC_U32* outMS10ths, FSC_U8* inBuf)
{
	if (outSec && outMS10ths && inBuf)
	{
		*outMS10ths = inBuf[0];
		*outMS10ths = *outMS10ths << 8;
		*outMS10ths |= inBuf[1];

		*outSec = inBuf[2];
		*outSec = *outSec << 8;
		*outSec |= inBuf[3];
	}
}

/*******************************************************************************
* Function:        fusb_get_pd_message_type
* Input:           header: PD message header. Bits 4..0 are the pd message type, bits 14..12 are num data objs
*                  out: Buffer to which the message type will be written, should be at least 32 bytes long
* Return:          int - Number of chars written to out, negative on error
* Description:     Parses both PD message header bytes for the message type as a null-terminated string.
********************************************************************************/
FSC_S32 fusb_get_pd_message_type(FSC_U16 header, FSC_U8* out)
{
    FSC_S32 numChars = -1;   // Number of chars written, return value
    if ((!out) || !(out + 31))    
    {
        pr_err("%s FUSB - Error: Invalid input buffer! header: 0x%x\n", __func__, header);
        return -1;
    }

    
    
    if ((header & 0x7000) > 0)
    {
        switch (header & 0x0F)
        {
            case DMTSourceCapabilities:    
            {
                numChars = sprintf(out, "Source Capabilities");
                break;
            }
            case DMTRequest:    
            {
                numChars = sprintf(out, "Request");
                break;
            }
            case DMTBIST:    
            {
                numChars = sprintf(out, "BIST");
                break;
            }
            case DMTSinkCapabilities:    
            {
                numChars = sprintf(out, "Sink Capabilities");
                break;
            }
            case 0b00101:    
            {
                numChars = sprintf(out, "Battery Status");
                break;
            }
            case 0b00110:    
            {
                numChars = sprintf(out, "Source Alert");
                break;
            }
            case DMTVenderDefined:    
            {
                numChars = sprintf(out, "Vendor Defined");
                break;
            }
            default:            
            {
                numChars = sprintf(out, "Reserved (Data) (0x%x)", header);
                break;
            }
        }
    }
    else
    {
        switch (header & 0x0F)
        {
            case CMTGoodCRC:    
            {
                numChars = sprintf(out, "Good CRC");
                break;
            }
            case CMTGotoMin:    
            {
                numChars = sprintf(out, "Go to Min");
                break;
            }
            case CMTAccept:    
            {
                numChars = sprintf(out, "Accept");
                break;
            }
            case CMTReject:    
            {
                numChars = sprintf(out, "Reject");
                break;
            }
            case CMTPing:    
            {
                numChars = sprintf(out, "Ping");
                break;
            }
            case CMTPS_RDY:    
            {
                numChars = sprintf(out, "PS_RDY");
                break;
            }
            case CMTGetSourceCap:    
            {
                numChars = sprintf(out, "Get Source Capabilities");
                break;
            }
            case CMTGetSinkCap:    
            {
                numChars = sprintf(out, "Get Sink Capabilities");
                break;
            }
            case CMTDR_Swap:    
            {
                numChars = sprintf(out, "Data Role Swap");
                break;
            }
            case CMTPR_Swap:    
            {
                numChars = sprintf(out, "Power Role Swap");
                break;
            }
            case CMTVCONN_Swap:    
            {
                numChars = sprintf(out, "VConn Swap");
                break;
            }
            case CMTWait:    
            {
                numChars = sprintf(out, "Wait");
                break;
            }
            case CMTSoftReset:    
            {
                numChars = sprintf(out, "Soft Reset");
                break;
            }
            case 0b01110:    
            {
                numChars = sprintf(out, "Not Supported");
                break;
            }
            case 0b01111:    
            {
                numChars = sprintf(out, "Get Source Cap Ext");
                break;
            }
            case 0b10000:    
            {
                numChars = sprintf(out, "Get Source Status");
                break;
            }
            case 0b10001:    
            {
                numChars = sprintf(out, "FR Swap");
                break;
            }
            default:            
            {
                numChars = sprintf(out, "Reserved (CMD) (0x%x)", header);
                break;
            }
        }
    }
    return numChars;
}

/*******************************************************************************
* Function:		   fusb_Sysfs_Handle_Read
* Input:		   output: Buffer to which the output will be written
* Return:		   Number of chars written to output
* Description:	   Reading this file will output the most recently saved hostcomm output buffer
********************************************************************************/
#define FUSB_MAX_BUF_SIZE 256	
static ssize_t _fusb_Sysfs_Hostcomm_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    FSC_S32 i = 0;
    FSC_S32 numLogs = 0;
    FSC_S32 numChars = 0;
    FSC_U32 TimeStampSeconds = 0;   
    FSC_U32 TimeStampMS10ths = 0;   
    FSC_S8 tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL)
	{
        pr_err("%s - Chip structure is null!\n", __func__);
	}
	else if (buf == NULL || chip->HostCommBuf == NULL)
	{
        pr_err("%s - Buffer is null!\n", __func__);
	}
	else if (chip->HostCommBuf[0] == CMD_READ_PD_STATE_LOG)  
	{
		numLogs = chip->HostCommBuf[3];
		
		numChars += sprintf(tempBuf, "PD State Log has %u entries:\n", numLogs); 
		strcat(buf, tempBuf);

		
		for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--) 
		{
			
			fusb_timestamp_bytes_to_time(&TimeStampSeconds, &TimeStampMS10ths, &chip->HostCommBuf[i + 1]);

			
			switch (chip->HostCommBuf[i])
			{
				case peDisabled:			
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDisabled\t\tPolicy engine is disabled\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peErrorRecovery:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeErrorRecovery\t\tError recovery state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceHardReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceHardReset\t\tReceived a hard reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendHardReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendHardReset\t\tSource send a hard reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSoftReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSoftReset\t\tReceived a soft reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendSoftReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendSoftReset\t\tSend a soft reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceStartup:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceStartup\t\tInitial state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendCaps\t\tSend the source capabilities\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceDiscovery:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceDiscovery\t\tWaiting to detect a USB PD sink\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceDisabled:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceDisabled\t\tDisabled state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceTransitionDefault:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceTransitionDefault\t\tTransition to default 5V state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceNegotiateCap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceNegotiateCap\t\tNegotiate capability and PD contract\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceCapabilityResponse:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceCapabilityResponse\t\tRespond to a request message with a reject/wait\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceTransitionSupply:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceTransitionSupply\t\tTransition the power supply to the new setting (accept request)\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceReady:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceReady\t\tContract is in place and output voltage is stable\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceGiveSourceCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGiveSourceCaps\t\tState to resend source capabilities\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceGetSinkCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGetSinkCaps\t\tState to request the sink capabilities\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendPing:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendPing\t\tState to send a ping message\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceGotoMin:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGotoMin\t\tState to send the gotoMin and ready the power supply\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceGiveSinkCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGiveSinkCaps\t\tState to send the sink capabilities if dual-role\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceGetSourceCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGetSourceCaps\t\tState to request the source caps from the UFP\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendDRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendDRSwap\t\tState to send a DR_Swap message\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceEvaluateDRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkHardReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkHardReset\t\tReceived a hard reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkSendHardReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendHardReset\t\tSink send hard reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkSoftReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSoftReset\t\tSink soft reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkSendSoftReset:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendSoftReset\t\tSink send soft reset\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkTransitionDefault:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkTransitionDefault\t\tTransition to the default state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkStartup:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkStartup\t\tInitial sink state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkDiscovery:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkDiscovery\t\tSink discovery state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkWaitCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkWaitCaps\t\tSink wait for capabilities state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkEvaluateCaps:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluateCaps\t\tSink state to evaluate the received source capabilities\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkSelectCapability:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSelectCapability\t\tSink state for selecting a capability\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkTransitionSink:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkTransitionSink\t\tSink state for transitioning the current power\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkReady:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkReady\t\tSink ready state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkGiveSinkCap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGiveSinkCap\t\tSink send capabilities state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkGetSourceCap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGetSourceCap\t\tSink get source capabilities state\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkGetSinkCap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGetSinkCap\t\tSink state to get the sink capabilities of the connected source\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkGiveSourceCap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGiveSourceCap\t\tSink state to send the source capabilities if dual-role\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkSendDRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendDRSwap\t\tState to send a DR_Swap message\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkEvaluateDRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendVCONNSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendVCONNSwap\t\tInitiate a VCONN swap sequence\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkEvaluateVCONNSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluateVCONNSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceSendPRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendPRSwap\t\tInitiate a PR_Swap sequence\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceEvaluatePRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkSendPRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendPRSwap\t\tInitiate a PR_Swap sequence\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSinkEvaluatePRSwap:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peGiveVdm:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeGiveVdm\t\tSend VDM data\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmGetIdentity:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmGetIdentity\t\tRequesting Identity information from DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmSendIdentity:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmSendIdentity\t\tSending Discover Identity ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmGetSvids:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmGetSvids\t\tRequesting SVID info from DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmSendSvids:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmSendSvids\t\tSending Discover SVIDs ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmGetModes:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmGetModes\t\tRequesting Mode info from DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmSendModes:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmSendModes\t\tSending Discover Modes ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmEvaluateModeEntry:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmEvaluateModeEntry\t\tRequesting DPM to evaluate request to enter a mode, and enter if OK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmModeEntryNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeEntryNak\t\tSending Enter Mode NAK response\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmModeEntryAck:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeEntryAck\t\tSending Enter Mode ACK response\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmModeExit:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeExit\t\tRequesting DPM to evalute request to exit mode\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmModeExitNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeExitNak\t\tSending Exit Mode NAK reponse\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmModeExitAck:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeExitAck\t\tSending Exit Mode ACK Response\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peUfpVdmAttentionRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmAttentionRequest\t\tSending Attention Command\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpUfpVdmIdentityRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpUfpVdmIdentityRequest\t\tSending Identity Request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpUfpVdmIdentityAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpUfpVdmIdentityAcked\t\tInform DPM of Identity\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpUfpVdmIdentityNaked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpUfpVdmIdentityNaked\t\tInform DPM of result\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpCblVdmIdentityRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpCblVdmIdentityRequest\t\tSending Identity Request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpCblVdmIdentityAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpCblVdmIdentityAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpCblVdmIdentityNaked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpCblVdmIdentityNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmSvidsRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmSvidsRequest\t\tSending Discover SVIDs request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmSvidsAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmSvidsAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmSvidsNaked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmSvidsNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModesRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModesRequest\t\tSending Discover Modes request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModesAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModesAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModesNaked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModesNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModeEntryRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeEntryRequest\t\tSending Mode Entry request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModeEntryAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeEntryAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModeEntryNaked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeEntryNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmModeExitRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeExitRequest\t\tSending Exit Mode request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmExitModeAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmExitModeAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSrcVdmIdentityRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSrcVdmIdentityRequest\t\tsending Discover Identity request\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSrcVdmIdentityAcked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSrcVdmIdentityAcked\t\tinform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSrcVdmIdentityNaked:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSrcVdmIdentityNaked\t\tinform DPM\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDfpVdmAttentionRequest:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmAttentionRequest\t\tAttention Request received\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblReady:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblReady\t\tCable power up state?\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblGetIdentity:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetIdentity\t\tDiscover Identity request received\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblGetIdentityNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetIdentityNak\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblSendIdentity:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblSendIdentity\t\tRespond with Ack\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblGetSvids:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetSvids\t\tDiscover SVIDs request received\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblGetSvidsNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetSvidsNak\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblSendSvids:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblSendSvids\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblGetModes:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetModes\t\tDiscover Modes request received\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblGetModesNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetModesNak\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblSendModes:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblSendModes\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblEvaluateModeEntry:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblEvaluateModeEntry\t\tEnter Mode request received\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblModeEntryAck:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeEntryAck\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblModeEntryNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeEntryNak\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblModeExit:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeExit\t\tExit Mode request received\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblModeExitAck:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeExitAck\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peCblModeExitNak:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeExitNak\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peDpRequestStatus:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeDpRequestStatus\t\tRequesting PP Status\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case PE_BIST_Receive_Mode:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tPE_BIST_Receive_Mode\t\tBist Receive Mode\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case PE_BIST_Frame_Received:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tPE_BIST_Frame_Received\t\tTest Frame received by Protocol layer\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case PE_BIST_Carrier_Mode_2:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tPE_BIST_Carrier_Mode_2\t\tBIST Carrier Mode 2\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case peSourceWaitNewCapabilities:		
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceWaitNewCapabilities\t\tWait for new Source Capabilities from Policy Manager\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case dbgGetRxPacket:				
				{
					
					numChars += sprintf(tempBuf, "%02u|0.%04u\tdbgGetRxPacket\t\t\tNumber of I2C bytes read | Time elapsed\n", TimeStampSeconds, TimeStampMS10ths);
                
                    strcat(buf, tempBuf);
                    break;
                }

                case dbgSendTxPacket:		        
                {
                    
                    numChars += sprintf(tempBuf, "%02u|0.%04u\tdbgGetTxPacket\t\t\tNumber of I2C bytes sent | Time elapsed\n", TimeStampSeconds, TimeStampMS10ths);
                    
					strcat(buf, tempBuf);
					break;
				}

				default:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tUKNOWN STATE: 0x%02x\n", TimeStampSeconds, TimeStampMS10ths, chip->HostCommBuf[i]);
					strcat(buf, tempBuf);
					break;
				}
			}
		}
		strcat(buf, "\n");	 
		numChars++;			 
	}
	else if (chip->HostCommBuf[0] == CMD_READ_STATE_LOG)  
	{
		numLogs = chip->HostCommBuf[3];
		
		numChars += sprintf(tempBuf, "Type-C State Log has %u entries:\n", numLogs); 
		strcat(buf, tempBuf);

		
		for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--) 
		{
			
			fusb_timestamp_bytes_to_time(&TimeStampSeconds, &TimeStampMS10ths, &chip->HostCommBuf[i + 1]);

			
			switch (chip->HostCommBuf[i])
			{
				case Disabled:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tDisabled\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case ErrorRecovery:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tErrorRecovery\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case Unattached:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tUnattached\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case AttachWaitSink:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tAttachWaitSink\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case AttachedSink:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tAttachedSink\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case AttachWaitSource:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tAttachWaitSource\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case AttachedSource:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tAttachedSource\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case TrySource:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tTrySource\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case TryWaitSink:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tTryWaitSink\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case TrySink:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tTrySink\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case TryWaitSource:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tTryWaitSource\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case AudioAccessory:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tAudioAccessory\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case DebugAccessory:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tDebugAccessory\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case AttachWaitAccessory:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tAttachWaitAccessory\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case PoweredAccessory:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tPoweredAccessory\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case UnsupportedAccessory:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tUnsupportedAccessory\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case DelayUnattached:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tDelayUnattached\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

				case UnattachedSource:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tUnattachedSource\n", TimeStampSeconds, TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}
				default:
				{
					numChars += sprintf(tempBuf, "[%u.%04u]\tUKNOWN STATE: 0x%02x\n", TimeStampSeconds, TimeStampMS10ths, chip->HostCommBuf[i]);
					strcat(buf, tempBuf);
					break;
				}
			}
		}
		strcat(buf, "\n");	 
		numChars++;			 
	}
	else
	{
		for (i = 0; i < FSC_HOSTCOMM_BUFFER_SIZE; i++)
		{
			numChars += scnprintf(tempBuf, 6 * sizeof(char), "0x%02x ", chip->HostCommBuf[i]); 
			strcat(buf, tempBuf);	
		}
		strcat(buf, "\n");	 
		numChars++;			 
	}
	return numChars;
}

/*******************************************************************************
* Function:		   fusb_Sysfs_Handle_Write
* Input:		   input: Buffer passed in from OS (space-separated list of 8-bit hex values)
*				   size: Number of chars in input
*				   output: Buffer to which the output will be written
* Return:		   Number of chars written to output
* Description:	   Performs hostcomm duties, and stores output buffer in chip structure
********************************************************************************/
static ssize_t _fusb_Sysfs_Hostcomm_store(struct device* dev, struct device_attribute* attr, const char* input, size_t size)
{
    FSC_S32 ret = 0;
    FSC_S32 i = 0;
    FSC_S32 j = 0;
    FSC_S8 tempByte = 0;
    FSC_S32 numBytes = 0;
    FSC_S8 temp[6] = { 0 };   
    FSC_S8 temp_input[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
    FSC_S8 output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL)
	{
        pr_err("%s - Chip structure is null!\n", __func__);
	}
	else if (input == NULL)
	{
        pr_err("%s - Error: Input buffer is NULL!\n", __func__);
	}
	else
	{
		
		for (i = 0; i < size; i = i + j)
		{
			
			for (j = 0; (j < 5) && (j + i < size); j++)
			{
				
				if (input[i + j] == ' ')
				{
					break;					
				}

				temp[j] = input[i + j];		
			}

			temp[++j] = 0;					

			
			ret = kstrtou8(temp, 16, &tempByte);
			if (ret != 0)
			{
                pr_err("FUSB  %s - Error: Hostcomm input is not a valid hex value! Return: '%d'\n", __func__, ret);
				return 0;  
			}
			else
			{
				temp_input[numBytes++] = tempByte;
				if (numBytes >= FSC_HOSTCOMM_BUFFER_SIZE)
				{
					break;
				}
			}
		}

		fusb_ProcessMsg(temp_input, output);												
		memcpy(chip->HostCommBuf, output, FSC_HOSTCOMM_BUFFER_SIZE);						
	}

	return size;
}

static ssize_t _fusb_Sysfs_PDStateLog_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    FSC_S32 i = 0;
    FSC_S32 numChars = 0;
    FSC_S32 numLogs = 0;
    FSC_U16 PDMessageHeader = 0;                        
    FSC_U32 TimeStampSeconds = 0;                       
    FSC_U32 TimeStampMS10ths = 0;                       
    FSC_U8 MessageType[32] = { 0 };                     
    FSC_U8 output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
    FSC_U8 tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
    tempBuf[0] = CMD_READ_PD_STATE_LOG;                 

    
    fusb_ProcessMsg(tempBuf, output);

    numLogs = output[3];
    
    numChars += sprintf(tempBuf, "PD State Log has %u entries:\n", numLogs); 
    strcat(buf, tempBuf);

    
    for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--) 
    {
        

        
        if (output[i] != dbgGetRxPacket)
        {
            fusb_timestamp_bytes_to_time(&TimeStampSeconds, &TimeStampMS10ths, &output[i + 1]);
        }

        
        switch (output[i])
        {
            case peDisabled:		    
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDisabled\t\tPolicy engine is disabled\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peErrorRecovery:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeErrorRecovery\t\tError recovery state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceHardReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceHardReset\t\tReceived a hard reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendHardReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendHardReset\t\tSource send a hard reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSoftReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSoftReset\t\tReceived a soft reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendSoftReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendSoftReset\t\tSend a soft reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceStartup:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceStartup\t\tInitial state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendCaps\t\tSend the source capabilities\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceDiscovery:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceDiscovery\t\tWaiting to detect a USB PD sink\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceDisabled:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceDisabled\t\tDisabled state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceTransitionDefault:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceTransitionDefault\t\tTransition to default 5V state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceNegotiateCap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceNegotiateCap\t\tNegotiate capability and PD contract\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceCapabilityResponse:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceCapabilityResponse\t\tRespond to a request message with a reject/wait\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceTransitionSupply:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceTransitionSupply\t\tTransition the power supply to the new setting (accept request)\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceReady:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceReady\t\tContract is in place and output voltage is stable\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceGiveSourceCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGiveSourceCaps\t\tState to resend source capabilities\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceGetSinkCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGetSinkCaps\t\tState to request the sink capabilities\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendPing:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendPing\t\tState to send a ping message\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceGotoMin:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGotoMin\t\tState to send the gotoMin and ready the power supply\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceGiveSinkCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGiveSinkCaps\t\tState to send the sink capabilities if dual-role\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceGetSourceCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceGetSourceCaps\t\tState to request the source caps from the UFP\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendDRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendDRSwap\t\tState to send a DR_Swap message\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceEvaluateDRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkHardReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkHardReset\t\tReceived a hard reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkSendHardReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendHardReset\t\tSink send hard reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkSoftReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSoftReset\t\tSink soft reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkSendSoftReset:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendSoftReset\t\tSink send soft reset\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkTransitionDefault:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkTransitionDefault\t\tTransition to the default state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkStartup:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkStartup\t\tInitial sink state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkDiscovery:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkDiscovery\t\tSink discovery state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkWaitCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkWaitCaps\t\tSink wait for capabilities state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkEvaluateCaps:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluateCaps\t\tSink state to evaluate the received source capabilities\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkSelectCapability:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSelectCapability\t\tSink state for selecting a capability\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkTransitionSink:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkTransitionSink\t\tSink state for transitioning the current power\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkReady:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkReady\t\tSink ready state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkGiveSinkCap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGiveSinkCap\t\tSink send capabilities state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkGetSourceCap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGetSourceCap\t\tSink get source capabilities state\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkGetSinkCap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGetSinkCap\t\tSink state to get the sink capabilities of the connected source\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkGiveSourceCap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkGiveSourceCap\t\tSink state to send the source capabilities if dual-role\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkSendDRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendDRSwap\t\tState to send a DR_Swap message\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkEvaluateDRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendVCONNSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendVCONNSwap\t\tInitiate a VCONN swap sequence\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkEvaluateVCONNSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluateVCONNSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceSendPRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceSendPRSwap\t\tInitiate a PR_Swap sequence\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceEvaluatePRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkSendPRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkSendPRSwap\t\tInitiate a PR_Swap sequence\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSinkEvaluatePRSwap:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSinkEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peGiveVdm:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeGiveVdm\t\tSend VDM data\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmGetIdentity:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmGetIdentity\t\tRequesting Identity information from DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmSendIdentity:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmSendIdentity\t\tSending Discover Identity ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmGetSvids:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmGetSvids\t\tRequesting SVID info from DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmSendSvids:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmSendSvids\t\tSending Discover SVIDs ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmGetModes:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmGetModes\t\tRequesting Mode info from DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmSendModes:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmSendModes\t\tSending Discover Modes ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmEvaluateModeEntry:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmEvaluateModeEntry\t\tRequesting DPM to evaluate request to enter a mode, and enter if OK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmModeEntryNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeEntryNak\t\tSending Enter Mode NAK response\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmModeEntryAck:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeEntryAck\t\tSending Enter Mode ACK response\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmModeExit:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeExit\t\tRequesting DPM to evalute request to exit mode\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmModeExitNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeExitNak\t\tSending Exit Mode NAK reponse\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmModeExitAck:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmModeExitAck\t\tSending Exit Mode ACK Response\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peUfpVdmAttentionRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeUfpVdmAttentionRequest\t\tSending Attention Command\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpUfpVdmIdentityRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpUfpVdmIdentityRequest\t\tSending Identity Request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpUfpVdmIdentityAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpUfpVdmIdentityAcked\t\tInform DPM of Identity\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpUfpVdmIdentityNaked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpUfpVdmIdentityNaked\t\tInform DPM of result\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpCblVdmIdentityRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpCblVdmIdentityRequest\t\tSending Identity Request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpCblVdmIdentityAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpCblVdmIdentityAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpCblVdmIdentityNaked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpCblVdmIdentityNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmSvidsRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmSvidsRequest\t\tSending Discover SVIDs request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmSvidsAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmSvidsAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmSvidsNaked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmSvidsNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModesRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModesRequest\t\tSending Discover Modes request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModesAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModesAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModesNaked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModesNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModeEntryRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeEntryRequest\t\tSending Mode Entry request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModeEntryAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeEntryAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModeEntryNaked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeEntryNaked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmModeExitRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmModeExitRequest\t\tSending Exit Mode request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmExitModeAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmExitModeAcked\t\tInform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSrcVdmIdentityRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSrcVdmIdentityRequest\t\tsending Discover Identity request\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSrcVdmIdentityAcked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSrcVdmIdentityAcked\t\tinform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSrcVdmIdentityNaked:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSrcVdmIdentityNaked\t\tinform DPM\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDfpVdmAttentionRequest:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDfpVdmAttentionRequest\t\tAttention Request received\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblReady:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblReady\t\tCable power up state?\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblGetIdentity:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetIdentity\t\tDiscover Identity request received\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblGetIdentityNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetIdentityNak\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblSendIdentity:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblSendIdentity\t\tRespond with Ack\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblGetSvids:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetSvids\t\tDiscover SVIDs request received\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblGetSvidsNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetSvidsNak\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblSendSvids:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblSendSvids\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblGetModes:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetModes\t\tDiscover Modes request received\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblGetModesNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblGetModesNak\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblSendModes:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblSendModes\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblEvaluateModeEntry:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblEvaluateModeEntry\t\tEnter Mode request received\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblModeEntryAck:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeEntryAck\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblModeEntryNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeEntryNak\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblModeExit:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeExit\t\tExit Mode request received\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblModeExitAck:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeExitAck\t\tRespond with NAK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peCblModeExitNak:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeCblModeExitNak\t\tRespond with ACK\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peDpRequestStatus:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeDpRequestStatus\t\tRequesting PP Status\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case PE_BIST_Receive_Mode:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tPE_BIST_Receive_Mode\t\tBist Receive Mode\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case PE_BIST_Frame_Received:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tPE_BIST_Frame_Received\t\tTest Frame received by Protocol layer\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case PE_BIST_Carrier_Mode_2:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tPE_BIST_Carrier_Mode_2\t\tBIST Carrier Mode 2\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case peSourceWaitNewCapabilities:		
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tpeSourceWaitNewCapabilities\t\tWait for new Source Capabilities from Policy Manager\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case dbgSendTxPacket:                
            case dbgGetRxPacket:		        
            {
                
           

                
                PDMessageHeader = output[i + 4];                
                PDMessageHeader = PDMessageHeader << 8;         
                PDMessageHeader |= output[i + 2];               

                
                if (fusb_get_pd_message_type(PDMessageHeader, MessageType) > -1)
                {
                    numChars += sprintf(tempBuf, "0x%x\t\t%s\t\tMessage Type: %s\n", PDMessageHeader, (output[i] == dbgGetRxPacket) ? "dbgGetRxPacket" : "dbgSendTxPacket", MessageType);
                }
                else
                {
                    numChars += sprintf(tempBuf, "0x%x\t\t%s\t\tMessage Type: UNKNOWN\n", PDMessageHeader, (output[i] == dbgGetRxPacket) ? "dbgGetRxPacket" : "dbgSendTxPacket");
                }
                strcat(buf, tempBuf);
                break;
            }

            default:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tUKNOWN STATE: 0x%02x\n", TimeStampSeconds, TimeStampMS10ths, output[i]);
                strcat(buf, tempBuf);
                break;
            }
        }
    }
    strcat(buf, "\n");   
    return ++numChars;   
}

static ssize_t _fusb_Sysfs_TypeCStateLog_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    FSC_S32 i = 0;
    FSC_S32 numChars = 0;
    FSC_S32 numLogs = 0;
    FSC_U32 TimeStampSeconds = 0;                       
    FSC_U32 TimeStampMS10ths = 0;                       
    FSC_S8 output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
    FSC_S8 tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
    tempBuf[0] = CMD_READ_STATE_LOG;                    

    
    fusb_ProcessMsg(tempBuf, output);

    numLogs = output[3];
    
    numChars += sprintf(tempBuf, "Type-C State Log has %u entries:\n", numLogs); 
    strcat(buf, tempBuf);

    
    for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--) 
    {
        
        fusb_timestamp_bytes_to_time(&TimeStampSeconds, &TimeStampMS10ths, &output[i + 1]);

        
        switch (output[i])
        {
            case Disabled:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tDisabled\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case ErrorRecovery:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tErrorRecovery\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case Unattached:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tUnattached\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case AttachWaitSink:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tAttachWaitSink\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case AttachedSink:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tAttachedSink\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case AttachWaitSource:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tAttachWaitSource\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case AttachedSource:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tAttachedSource\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case TrySource:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tTrySource\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case TryWaitSink:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tTryWaitSink\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case TrySink:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tTrySink\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case TryWaitSource:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tTryWaitSource\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case AudioAccessory:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tAudioAccessory\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case DebugAccessory:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tDebugAccessory\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case AttachWaitAccessory:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tAttachWaitAccessory\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case PoweredAccessory:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tPoweredAccessory\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case UnsupportedAccessory:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tUnsupportedAccessory\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case DelayUnattached:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tDelayUnattached\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }

            case UnattachedSource:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tUnattachedSource\n", TimeStampSeconds, TimeStampMS10ths);
                strcat(buf, tempBuf);
                break;
            }
            default:
            {
                numChars += sprintf(tempBuf, "[%u.%04u]\tUKNOWN STATE: 0x%02x\n", TimeStampSeconds, TimeStampMS10ths, output[i]);
                strcat(buf, tempBuf);
                break;
            }
        }
    }
    strcat(buf, "\n");   
    return ++numChars;   
}

static ssize_t _fusb_Sysfs_Reinitialize_fusb302(struct device* dev, struct device_attribute* attr, char* buf)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL)
    {
        return sprintf(buf, "FUSB302 Error: Internal chip structure pointer is NULL!\n");
    }

    
#ifdef FSC_INTERRUPT_TRIGGERED
    disable_irq(chip->gpio_IntN_irq);   
#else
    fusb_StopThreads();                 
#endif 

    fusb_StopTimers();
    core_initialize();
    pr_debug ("FUSB  %s - Core is initialized!\n", __func__);
    fusb_StartTimers();
    core_enable_typec(TRUE);
    pr_debug ("FUSB  %s - Type-C State Machine is enabled!\n", __func__);

#ifdef FSC_INTERRUPT_TRIGGERED
    enable_irq(chip->gpio_IntN_irq);
#else
    
    schedule_work(&chip->worker);
#endif 

    return sprintf(buf, "FUSB302 Reinitialized!\n");
}

static DEVICE_ATTR(fusb30x_hostcomm, S_IRWXU | S_IRWXG | S_IROTH, _fusb_Sysfs_Hostcomm_show, _fusb_Sysfs_Hostcomm_store);
static DEVICE_ATTR(pd_state_log, S_IRUSR | S_IRGRP | S_IROTH, _fusb_Sysfs_PDStateLog_show, NULL);
static DEVICE_ATTR(typec_state_log, S_IRUSR | S_IRGRP | S_IROTH, _fusb_Sysfs_TypeCStateLog_show, NULL);
static DEVICE_ATTR(reinitialize, S_IRUSR | S_IRGRP | S_IROTH, _fusb_Sysfs_Reinitialize_fusb302, NULL);

static struct attribute *fusb302_sysfs_attrs[] = {
    &dev_attr_fusb30x_hostcomm.attr,
    &dev_attr_pd_state_log.attr,
    &dev_attr_typec_state_log.attr,
    &dev_attr_reinitialize.attr,
    NULL
};

static struct attribute_group fusb302_sysfs_attr_grp = {
    .name = "control",
    .attrs = fusb302_sysfs_attrs,
};

void fusb_Sysfs_Init(void)
{
    FSC_S32 ret = 0;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (chip == NULL)
    {
        pr_err("%s - Chip structure is null!\n", __func__);
        return;
    }

    
    ret = sysfs_create_group(&chip->client->dev.kobj, &fusb302_sysfs_attr_grp);
    if (ret)
    {
        pr_err("FUSB %s - Error creating sysfs attributes!\n", __func__);
    }
}

#endif 
void fusb_InitializeCore(void)
{
	core_initialize();
    pr_debug("FUSB  %s - Core is initialized!\n", __func__);
	core_enable_typec(TRUE);
    pr_debug("FUSB  %s - Type-C State Machine is enabled!\n", __func__);
}

FSC_BOOL fusb_IsDeviceValid(void)
{
    FSC_U8 val = 0;
	struct device_node* node;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}

	
		node = chip->client->dev.of_node;

	
    if (!fusb_I2C_ReadData((FSC_U8)0x01, &val))
	{
        pr_err("FUSB  %s - Error: Could not communicate with device over I2C!\n", __func__);
		return FALSE;
	}
	printk("FUSB [%s] - device id = %x \n", __func__, val);

	return TRUE;
}

void fusb_InitChipData(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL)
	{
        pr_err("%s - Chip structure is null!\n", __func__);
		return;
	}

#ifdef FSC_DEBUG
	chip->dbgTimerTicks = 0;
	chip->dbgTimerRollovers = 0;
	chip->dbgSMTicks = 0;
	chip->dbgSMRollovers = 0;
    chip->dbg_gpio_StateMachine = -1;
    chip->dbg_gpio_StateMachine_value = false;
#endif  

	
	chip->gpio_VBus5V = -1;
	chip->gpio_VBus5V_value = false;
	chip->gpio_VBusOther = -1;
	chip->gpio_VBusOther_value = false;
	chip->gpio_IntN = -1;

#ifdef FSC_INTERRUPT_TRIGGERED
	chip->gpio_IntN_irq = -1;
#endif 

	
	chip->InitDelayMS = INIT_DELAY_MS;												
	chip->numRetriesI2C = RETRIES_I2C;												
	chip->use_i2c_blocks = false;													

	chip->pmode = PMODE_UNKNOWN;
	chip->prole = UNKNOWN_POWER_ROLE;
	chip->drole = UNKNOWN_DATA_ROLE;
	chip->vconn = VCONN_SUPPLY_NO;

}


#ifdef FSC_INTERRUPT_TRIGGERED

FSC_S32 fusb_EnableInterrupts(void)
{
    FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}

	wake_lock_init(&chip->fusb_wlock, WAKE_LOCK_SUSPEND, "fusb_wlock");

	
	ret = gpio_to_irq(chip->gpio_IntN); 
	if (ret < 0)
	{
        dev_err(&chip->client->dev, "%s - Error: Unable to request IRQ for INT_N GPIO! Error code: %d\n", __func__, ret);
		chip->gpio_IntN_irq = -1;	
		fusb_GPIO_Cleanup();
		return ret;
	}
	chip->gpio_IntN_irq = ret;
    pr_debug("%s - Success: Requested INT_N IRQ: '%d'\n", __func__, chip->gpio_IntN_irq);

	
	ret = devm_request_threaded_irq(&chip->client->dev, chip->gpio_IntN_irq, NULL, _fusb_isr_intn, IRQF_ONESHOT | IRQF_TRIGGER_LOW, FUSB_DT_INTERRUPT_INTN, chip);	
	if (ret)
	{
		dev_err(&chip->client->dev, "%s - Error: Unable to request threaded IRQ for INT_N GPIO! Error code: %d\n", __func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}
	if (chip->gpio_IntN_irq)
		enable_irq_wake(chip->gpio_IntN_irq);

	return 0;
}

int handle_case = 0;
bool ReStartIrq_flag = false;
int fusb_ReStartIrq(int val)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&chip->lock);
	if (gpio_is_valid(chip->gpio_IntN) && chip->gpio_IntN_irq != -1)
	{
		printk("FUSB  [%s] -(error handle case:%d) Re-start chip->gpio_IntN_irq after %d seconds\n", __func__, handle_case, val);
		handle_case = 0;
		ReStartIrq_flag = true;
		disable_irq_nosync(chip->gpio_IntN_irq);
		schedule_delayed_work(&chip->debounce_work, val * HZ);
	}
	mutex_unlock(&chip->lock);

	return 0;
}


extern int state_runcount;
static irqreturn_t _fusb_isr_intn(FSC_S32 irq, void *dev_id)
{
	struct fusb30x_chip* chip = dev_id;
	printk("FUSB  [%s]: FUSB-interrupt triggered ++\n", __func__);

	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return IRQ_NONE;
	}

#ifdef FSC_DEBUG
	dbg_fusb_GPIO_Set_SM_Toggle(!chip->dbg_gpio_StateMachine_value);	

	if (chip->dbgSMTicks++ >= U8_MAX)									
	{
		chip->dbgSMRollovers++;											
	}
#endif  

	state_runcount = 0;
	wake_lock_timeout(&chip->fusb_wlock, 2 * HZ);
	core_state_machine();												
	wake_unlock(&chip->fusb_wlock);

	return IRQ_HANDLED;
}

#else

void _fusb_InitWorker(struct work_struct* delayed_work)
{
	struct fusb30x_chip* chip = container_of(delayed_work, struct fusb30x_chip, init_worker.work);
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	
	schedule_work(&chip->worker);
}

void _fusb_MainWorker(struct work_struct* work)
{
	struct fusb30x_chip* chip = container_of(work, struct fusb30x_chip, worker);
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

#ifdef FSC_DEBUG
	dbg_fusb_GPIO_Set_SM_Toggle(!chip->dbg_gpio_StateMachine_value);	

	if (chip->dbgSMTicks++ >= U8_MAX)									
	{
		chip->dbgSMRollovers++;											
	}
#endif  

	core_state_machine();												
	schedule_work(&chip->worker);										
}

void fusb_InitializeWorkers(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
    pr_debug("FUSB  %s - Initializing threads!\n", __func__);
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	
	INIT_DELAYED_WORK(&chip->init_worker, _fusb_InitWorker);
	INIT_WORK(&chip->worker, _fusb_MainWorker);
}

void fusb_StopThreads(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	
	cancel_delayed_work_sync(&chip->init_worker);
	flush_delayed_work(&chip->init_worker);

	
	flush_work(&chip->worker);
	cancel_work_sync(&chip->worker);

	if (chip->gpio_IntN_irq)
		disable_irq_wake(chip->gpio_IntN_irq);
	wake_lock_destroy(&chip->fusb_wlock);
}

void fusb_ScheduleWork(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	schedule_delayed_work(&chip->init_worker, msecs_to_jiffies(chip->InitDelayMS));
}

#endif 

