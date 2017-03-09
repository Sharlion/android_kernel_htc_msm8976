#ifndef __FUSB_PLATFORM_HELPERS_H_
#define __FUSB_PLATFORM_HELPERS_H_

#define INIT_DELAY_MS   500     
#define RETRIES_I2C 3           

FSC_S32 fusb_InitializeGPIO(void);

void fusb_GPIO_Set_VBus5v(FSC_BOOL set);
void fusb_GPIO_Set_VBusOther(FSC_BOOL set);

FSC_BOOL fusb_GPIO_Get_VBus5v(void);
FSC_BOOL fusb_GPIO_Get_VBusOther(void);
FSC_BOOL fusb_GPIO_Get_IntN(void);

#ifdef FSC_DEBUG
void dbg_fusb_GPIO_Set_SM_Toggle(FSC_BOOL set);

FSC_BOOL dbg_fusb_GPIO_Get_SM_Toggle(void);
#endif  

void fusb_GPIO_Cleanup(void);


FSC_BOOL fusb_I2C_WriteData(FSC_U8 address, FSC_U8 length, FSC_U8* data);

FSC_BOOL fusb_I2C_ReadData(FSC_U8 address, FSC_U8* data);

FSC_BOOL fusb_I2C_ReadBlockData(FSC_U8 address, FSC_U8 length, FSC_U8* data);


void fusb_InitializeTimer(void);

void fusb_StartTimers(void);

void fusb_StopTimers(void);

void fusb_Delay10us(FSC_U32 delay10us);

#ifdef FSC_DEBUG
void fusb_Sysfs_Init(void);
#endif 
void fusb_InitializeCore(void);

bool fusb_PowerFusb302(void);

bool fusb_PowerVconn(bool set);

FSC_BOOL fusb_IsDeviceValid(void);

void fusb_InitChipData(void);


#ifdef FSC_INTERRUPT_TRIGGERED

FSC_S32 fusb_EnableInterrupts(void);

int fusb_ReStartIrq(int val);
#else

void fusb_InitializeWorkers(void);

void fusb_StopThreads(void);

void fusb_ScheduleWork(void);

#endif  

#endif  
