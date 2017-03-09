#ifdef FSC_DEBUG

#include "fusb30x_global.h"
#include "FSCTypes.h"
#include "platform_helpers.h"
#include "core/core.h"
#include "hostcomm.h"

void fusb_ProcessMsg(FSC_U8* inMsgBuffer, FSC_U8* outMsgBuffer)
{
    FSC_U8 i;

    outMsgBuffer[0] = inMsgBuffer[0];           

    switch (inMsgBuffer[0])
    {
        case CMD_GETDEVICEINFO:
            if (inMsgBuffer[1] != 0)
                outMsgBuffer[1] = 0x01;                                     
            else
            {
                outMsgBuffer[1] = 0x00;                                     
                outMsgBuffer[4] = MY_MCU;                                   
                outMsgBuffer[5] = MY_DEV_TYPE;                              

                outMsgBuffer[6] = core_get_rev_lower();
                outMsgBuffer[7] = core_get_rev_upper();

                outMsgBuffer[8] = 0xFF & MY_BC;                             
                outMsgBuffer[9] = 0xFF & (MY_BC >> 8);

                for (i = 0; i < 16; i++)                                    
                {
                    outMsgBuffer[i + 10] = 0x00; 
                }

                outMsgBuffer[26] = TEST_FIRMWARE;                           
            }
            break;
        case CMD_USBPD_BUFFER_READ:
            core_process_pd_buffer_read(inMsgBuffer, outMsgBuffer);         
            break;
        case CMD_USBPD_STATUS:
            core_process_typec_pd_status(inMsgBuffer, outMsgBuffer);        
            break;
        case CMD_USBPD_CONTROL:
            core_process_typec_pd_control(inMsgBuffer, outMsgBuffer);       
            break;
#ifdef FSC_HAVE_SRC
        case CMD_GET_SRC_CAPS:
            core_get_source_caps(outMsgBuffer);
            break;
#endif 
#ifdef FSC_HAVE_SNK
        case CMD_GET_SINK_CAPS:
            core_get_sink_caps(outMsgBuffer);
            break;
        case CMD_GET_SINK_REQ:
            core_get_sink_req(outMsgBuffer);
            break;
#endif 
        case CMD_ENABLE_PD:
            core_enable_pd(TRUE);
            outMsgBuffer[0] = CMD_ENABLE_PD;
            outMsgBuffer[1] = 1;
            break;
        case CMD_DISABLE_PD:
            core_enable_pd(FALSE);
            outMsgBuffer[0] = CMD_DISABLE_PD;
            outMsgBuffer[1] = 0;
            break;
        case CMD_GET_ALT_MODES:
            outMsgBuffer[0] = CMD_GET_ALT_MODES;                            
            outMsgBuffer[1] = core_get_alternate_modes();                   
            break;
        case CMD_GET_MANUAL_RETRIES:
            outMsgBuffer[0] = CMD_GET_MANUAL_RETRIES;
            outMsgBuffer[1] = core_get_manual_retries();
            break;
        case CMD_SET_STATE_UNATTACHED:
            core_set_state_unattached();
            outMsgBuffer[0] = CMD_SET_STATE_UNATTACHED;
            outMsgBuffer[1] = 0;
            break;
        case CMD_ENABLE_TYPEC_SM:
            core_enable_typec(TRUE);
            outMsgBuffer[0] = CMD_ENABLE_TYPEC_SM;
            outMsgBuffer[1] = 0;
            break;
        case CMD_DISABLE_TYPEC_SM:
            core_enable_typec(FALSE);
            outMsgBuffer[0] = CMD_DISABLE_TYPEC_SM;
            outMsgBuffer[1] = 0;
            break;
        case CMD_DEVICE_LOCAL_REGISTER_READ:
            core_process_local_register_request(inMsgBuffer, outMsgBuffer); 
            break;
        case CMD_SET_STATE:                                                 
            core_process_set_typec_state(inMsgBuffer, outMsgBuffer);
            break;
        case CMD_READ_STATE_LOG:                                            
            core_process_read_typec_state_log(inMsgBuffer, outMsgBuffer);
            break;
        case CMD_READ_PD_STATE_LOG:                                         
            core_process_read_pd_state_log(inMsgBuffer, outMsgBuffer);
            break;
        case CMD_READ_I2C:
            fusb_hc_Handle_I2CRead(inMsgBuffer, outMsgBuffer);
            break;
        case CMD_WRITE_I2C:
            fusb_hc_Handle_I2CWrite(inMsgBuffer, outMsgBuffer);
            break;
        case CMD_GET_VBUS5V:
            fusb_hc_GetVBus5V(outMsgBuffer);
            break;
        case CMD_SET_VBUS5V:
            fusb_hc_SetVBus5V(inMsgBuffer, outMsgBuffer);
            break;
        case CMD_GET_INTN:
            fusb_hc_GetIntN(outMsgBuffer);
            break;
        case CMD_SEND_HARD_RESET:
            core_send_hard_reset();
            outMsgBuffer[0] = CMD_SEND_HARD_RESET;
            outMsgBuffer[1] = 0;
            break;
#ifdef FSC_DEBUG
        case CMD_GET_TIMER_TICKS:
            fusb_hc_GetTimerTicks(outMsgBuffer);
            break;
        case CMD_GET_SM_TICKS:
            fusb_hc_GetSMTicks(outMsgBuffer);
            break;
        case CMD_GET_GPIO_SM_TOGGLE:
            fusb_hc_GetGPIO_SM_Toggle(outMsgBuffer);
            break;
        case CMD_SET_GPIO_SM_TOGGLE:
            fusb_hc_SetGPIO_SM_Toggle(inMsgBuffer, outMsgBuffer);
            break;
#endif  
        default:
            outMsgBuffer[1] = 0x01;                                         
            break;
    }
}

void fusb_hc_Handle_I2CRead(FSC_U8* inBuf, FSC_U8* outBuf)
{
    if (!fusb_I2C_ReadData(inBuf[1], outBuf))
    {
        pr_err("FUSB  %s - Error: Could not read I2C Data!\n", __func__);
    }
}

void fusb_hc_Handle_I2CWrite(FSC_U8* inBuf, FSC_U8* outBuf)
{
    if (!fusb_I2C_WriteData(inBuf[1], inBuf[2], &inBuf[3]))
    {
        pr_err("FUSB  %s - Error: Could not write I2C Data!\n", __func__);
        outBuf[0] = 0;  
    }
    else
    {
        outBuf[0] = 1;  
    }
}

void fusb_hc_GetVBus5V(FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    outMsgBuffer[0] = 1;
    outMsgBuffer[1] = fusb_GPIO_Get_VBus5v() ? 1 : 0;
}

void fusb_hc_SetVBus5V(FSC_U8* inMsgBuffer, FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    fusb_GPIO_Set_VBus5v(inMsgBuffer[1]);
    outMsgBuffer[0] = 1;
    outMsgBuffer[1] = fusb_GPIO_Get_VBus5v() ? 1 : 0;
}

void fusb_hc_GetIntN(FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    outMsgBuffer[0] = 1;
    outMsgBuffer[1] = fusb_GPIO_Get_IntN() ? 1 : 0;
}

void fusb_hc_GetTimerTicks(FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    outMsgBuffer[0] = 1;    
    outMsgBuffer[1] = chip->dbgTimerTicks;
    outMsgBuffer[2] = chip->dbgTimerRollovers;
}

void fusb_hc_GetSMTicks(FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    outMsgBuffer[0] = 1;    
    outMsgBuffer[1] = chip->dbgSMTicks;
    outMsgBuffer[2] = chip->dbgSMRollovers;
}

void fusb_hc_GetGPIO_SM_Toggle(FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    outMsgBuffer[0] = 1;
    outMsgBuffer[1] = dbg_fusb_GPIO_Get_SM_Toggle() ? 1 : 0;
}

void fusb_hc_SetGPIO_SM_Toggle(FSC_U8* inMsgBuffer, FSC_U8* outMsgBuffer)
{
    struct fusb30x_chip* chip = fusb30x_GetChip();
    if (!chip)
    {
        pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
        outMsgBuffer[0] = 0;
        return;
    }

    dbg_fusb_GPIO_Set_SM_Toggle(inMsgBuffer[1]);
    outMsgBuffer[0] = 1;
    outMsgBuffer[1] = dbg_fusb_GPIO_Get_SM_Toggle() ? 1 : 0;
}

#endif  
