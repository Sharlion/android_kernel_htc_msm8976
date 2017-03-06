#ifndef _FSC_PLATFORM_H_
#define _FSC_PLATFORM_H_

#ifdef PLATFORM_NONE
#include "../Platform_None/FSCTypes.h"
#endif 

#ifdef PLATFORM_PIC32
#include "../Platform_PIC32/GenericTypeDefs.h"
#endif 

#ifdef PLATFORM_ARM
#include "../Platform_ARM/app/FSCTypes.h"
#endif 

#ifdef FSC_PLATFORM_LINUX
#include "../FSCTypes.h"
#endif 

typedef enum {
    VBUS_LVL_5V,
  
    VBUS_LVL_12V,
  
  
    VBUS_LVL_COUNT,
    VBUS_LVL_ALL = 99
} VBUS_LVL;

void platform_set_vbus_lvl_enable(VBUS_LVL level, FSC_BOOL blnEnable, FSC_BOOL blnDisableOthers);
FSC_BOOL platform_get_vbus_lvl_enable(VBUS_LVL level);

void platform_set_vbus_discharge(FSC_BOOL blnEnable);

FSC_BOOL platform_get_device_irq_state( void );

FSC_BOOL platform_i2c_write(FSC_U8 SlaveAddress,
                            FSC_U8 RegAddrLength,
                            FSC_U8 DataLength,
                            FSC_U8 PacketSize,
                            FSC_U8 IncSize,
                            FSC_U32 RegisterAddress,
                            FSC_U8* Data);

FSC_BOOL platform_i2c_read( FSC_U8 SlaveAddress,
                            FSC_U8 RegAddrLength,
                            FSC_U8 DataLength,
                            FSC_U8 PacketSize,
                            FSC_U8 IncSize,
                            FSC_U32 RegisterAddress,
                            FSC_U8* Data);

void platform_enable_timer(FSC_BOOL enable);

void platform_delay_10us( FSC_U32 delayCount );

#endif  
