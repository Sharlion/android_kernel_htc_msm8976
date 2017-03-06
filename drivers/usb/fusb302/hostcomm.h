#ifdef FSC_DEBUG

#ifndef __FSC_HOSTCOMM_H_
#define __FSC_HOSTCOMM_H_

#define PKTOUT_REQUEST                  0x00
#define PKTOUT_VERSION                  0x01
#define PKTIN_REQUEST                   0x00
#define PKTIN_STATUS                    0x01
#define PKTIN_ERRORCODE                 0x03

#define CMD_GETDEVICEINFO               0x00
#define CMD_USBPD_BUFFER_READ           0xA0
#define CMD_USBPD_STATUS                0xA2
#define CMD_USBPD_CONTROL               0xA3
#define CMD_GET_SRC_CAPS                0xA4
#define CMD_GET_SINK_CAPS               0xA5
#define CMD_GET_SINK_REQ                0xA6
#define CMD_ENABLE_PD                   0xA7
#define CMD_DISABLE_PD                  0xA8
#define CMD_GET_ALT_MODES               0xA9
#define CMD_GET_MANUAL_RETRIES          0xAA
#define CMD_SET_STATE_UNATTACHED        0xAB
#define CMD_ENABLE_TYPEC_SM             0xAC
#define CMD_DISABLE_TYPEC_SM            0xAD
#define CMD_SEND_HARD_RESET             0xAE

#define CMD_DEVICE_LOCAL_REGISTER_READ  0xB0    
#define CMD_SET_STATE                   0xB1
#define CMD_READ_STATE_LOG              0xB2
#define CMD_READ_PD_STATE_LOG           0xB3

#define CMD_READ_I2C                    0xC0
#define CMD_WRITE_I2C                   0xC1
#define CMD_GET_VBUS5V                  0xC4
#define CMD_SET_VBUS5V                  0xC5
#define CMD_GET_INTN                    0xC6

#ifdef FSC_DEBUG
#define CMD_GET_TIMER_TICKS             0xF0
#define CMD_GET_SM_TICKS                0xF1
#define CMD_GET_GPIO_SM_TOGGLE          0xF2
#define CMD_SET_GPIO_SM_TOGGLE          0xF3
#endif  

#define TEST_FIRMWARE                   0X01    


typedef enum
{
    mcuUnknown = 0,
    mcuPIC18F14K50 = 1,
    mcuPIC32MX795F512L = 2,
    mcuPIC32MX250F128B = 3,
    mcuGENERIC_LINUX = 4,             
} mcu_t;

typedef enum
{
    dtUnknown = -1,
    dtUSBI2CStandard = 0,
    dtUSBI2CPDTypeC = 1
} dt_t;

typedef enum
{
    bcUnknown = -1,
    bcStandardI2CConfig = 0,
    bcFUSB300Eval = 0x100,
    bcFUSB302FPGA = 0x200,
    bcFM14014 = 0x300
} bc_t;

#define MY_MCU          mcuGENERIC_LINUX
#define MY_DEV_TYPE     dtUSBI2CPDTypeC
#define MY_BC           bcStandardI2CConfig



void fusb_ProcessMsg(FSC_U8* inMsgBuffer, FSC_U8* outMsgBuffer);

void fusb_hc_Handle_I2CRead(FSC_U8* inBuf, FSC_U8* outBuf);

void fusb_hc_Handle_I2CWrite(FSC_U8* inBuf, FSC_U8* outBuf);

void fusb_hc_GetVBus5V(FSC_U8* outMsgBuffer);

void fusb_hc_SetVBus5V(FSC_U8* inMsgBuffer, FSC_U8* outMsgBuffer);

void fusb_hc_GetIntN(FSC_U8* outMsgBuffer);

void fusb_hc_GetTimerTicks(FSC_U8* outMsgBuffer);

void fusb_hc_GetSMTicks(FSC_U8* outMsgBuffer);

void fusb_hc_GetGPIO_SM_Toggle(FSC_U8* outMsgBuffer);

void fusb_hc_SetGPIO_SM_Toggle(FSC_U8* inMsgBuffer, FSC_U8* outMsgBuffer);

#endif  

#endif  
