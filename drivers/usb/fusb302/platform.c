#include <linux/printk.h>                                                       
#include "fusb30x_global.h"                                                     
#include "platform_helpers.h"                                                   
#include "core/platform.h"

int host_mode_status = 0;
extern bool flag_DualRoleSet;
extern void msm_otg_set_id_state(int);
int readI2C_failcount = 0;
int writeI2C_failcount = 0;

void platform_set_vbus_lvl_enable(VBUS_LVL level, FSC_BOOL blnEnable, FSC_BOOL blnDisableOthers)
{
#ifdef VBUS_lvl_SUPPORTED
    FSC_U32 i;

    
    switch (level)
    {
    case VBUS_LVL_5V:
        
        fusb_GPIO_Set_VBus5v(blnEnable == TRUE ? true : false);
        break;
    case VBUS_LVL_12V:
        
        fusb_GPIO_Set_VBusOther(blnEnable == TRUE ? true : false);
        break;
    default:
        
        break;
    }

    
    if (blnDisableOthers || ((level == VBUS_LVL_ALL) && (blnEnable == FALSE)))
    {
        i = 0;

        do {
            
            if( i == level ) continue;

            
            platform_set_vbus_lvl_enable( i, FALSE, FALSE );
        } while (++i < VBUS_LVL_COUNT);
    }
#endif

	if (blnEnable) {
		if (host_mode_status == 0) {
			host_mode_status = 1;
			if (flag_DualRoleSet)
				printk("FUSB  [%s]: Enter Host mode by fusb_dual_role_set_property()\n", __func__);
			else
				printk("FUSB  [%s]: Enter Host mode\n", __func__);
			msm_otg_set_id_state(0);
		} else if (host_mode_status == 1) {
			
		}
	} else {
		if (host_mode_status == 1) {
			if (flag_DualRoleSet)
				printk("FUSB  [%s]: Leave Host mode by fusb_dual_role_set_property()\n", __func__);
			else
				printk("FUSB  [%s]: Leave Host mode\n", __func__);

			host_mode_status = 0;
			msm_otg_set_id_state(1);
		} else {
			
		}
	}

    return;
}

FSC_BOOL platform_get_vbus_lvl_enable(VBUS_LVL level)
{
    
    switch (level)
    {
    case VBUS_LVL_5V:
        
        return fusb_GPIO_Get_VBus5v() ? TRUE : FALSE;

    case VBUS_LVL_12V:
        
        return fusb_GPIO_Get_VBusOther() ? TRUE : FALSE;

    default:
        
        return FALSE;
    }
}

void platform_set_vbus_discharge(FSC_BOOL blnEnable)
{
    
}

FSC_BOOL platform_get_device_irq_state(void)
{
    return fusb_GPIO_Get_IntN() ? TRUE : FALSE;
}

FSC_BOOL platform_i2c_write(FSC_U8 SlaveAddress,
                        FSC_U8 RegAddrLength,
                        FSC_U8 DataLength,
                        FSC_U8 PacketSize,
                        FSC_U8 IncSize,
                        FSC_U32 RegisterAddress,
                        FSC_U8* Data)
{
    FSC_BOOL ret = FALSE;

    if (Data == NULL)
    {
        pr_err("%s - Error: Write data buffer is NULL!\n", __func__);
        ret = FALSE;
    }
    else if (fusb_I2C_WriteData((FSC_U8)RegisterAddress, DataLength, Data))
    {
		writeI2C_failcount = 0;
        ret = TRUE;
    }
    else  
    {
		writeI2C_failcount++;
        ret = FALSE;       
        printk(KERN_ERR "%s - I2C write failed! RegisterAddress: 0x%02x, failcount: %d\n", __func__, (unsigned int)RegisterAddress, writeI2C_failcount);
    }
    return ret;
}

FSC_BOOL platform_i2c_read(FSC_U8 SlaveAddress,
                       FSC_U8 RegAddrLength,
                       FSC_U8 DataLength,
                       FSC_U8 PacketSize,
                       FSC_U8 IncSize,
                       FSC_U32 RegisterAddress,
                       FSC_U8* Data)
{
    FSC_BOOL ret = FALSE;
    FSC_S32 i = 0;
    FSC_U8 temp = 0;
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        return FALSE;
    }

    if (Data == NULL)
    {
        pr_err("%s - Error: Read data buffer is NULL!\n", __func__);
        ret = FALSE;
    }
    else if (DataLength > 1 && chip->use_i2c_blocks)    
    {
        if (!fusb_I2C_ReadBlockData(RegisterAddress, DataLength, Data))
        {
            printk(KERN_ERR "%s - I2C block read failed! RegisterAddress: 0x%02x, Length: %u\n", __func__, (unsigned int)RegisterAddress, (FSC_U8)DataLength);
            ret = FALSE;
        }
        else
        {
            ret = TRUE;
        }
    }
    else
    {
        for (i = 0; i < DataLength; i++)
        {
            if (fusb_I2C_ReadData((FSC_U8)RegisterAddress + i, &temp))
            {
				readI2C_failcount = 0;
                Data[i] = temp;
                ret = TRUE;
            }
            else
            {
				readI2C_failcount++;
                printk(KERN_ERR "%s - I2C read failed! RegisterAddress: 0x%02x, failcount: %d\n", __func__, (unsigned int)RegisterAddress, readI2C_failcount);
                ret = FALSE;
                break;
            }
        }
    }

    return ret;
}

void platform_enable_timer(FSC_BOOL enable)
{
    if (enable == TRUE)
    {
        fusb_StartTimers();
    }
    else
    {
        fusb_StopTimers();
    }
}

void platform_delay_10us(FSC_U32 delayCount)
{
    fusb_Delay10us(delayCount);
}
