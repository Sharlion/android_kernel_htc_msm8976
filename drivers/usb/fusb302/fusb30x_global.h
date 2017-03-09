
#ifndef FUSB30X_TYPES_H
#define FUSB30X_TYPES_H

#include <linux/i2c.h>                              
#include <linux/hrtimer.h>                          
#include <linux/wakelock.h>
#include "FSCTypes.h"                               
#include <linux/usb/class-dual-role.h>

#ifdef FSC_DEBUG
#define FSC_HOSTCOMM_BUFFER_SIZE    64              
#endif 

enum port_mode {
	PMODE_UFP = 0,
	PMODE_DFP,
	PMODE_UNKNOWN,
};

enum power_role {
	PR_SOURCE = 0,
	PR_SINK,
	UNKNOWN_POWER_ROLE,
};

enum data_role {
	DR_HOST = 0,
	DR_DEVICE,
	UNKNOWN_DATA_ROLE,
};

enum vconn_supply {
	VCONN_SUPPLY_NO = 0,
	VCONN_SUPPLY_YES,
};

struct fusb30x_chip                                 
{
    struct mutex lock;                              

#ifdef FSC_DEBUG
    FSC_U8 dbgTimerTicks;                           
    FSC_U8 dbgTimerRollovers;                       
    FSC_U8 dbgSMTicks;                              
    FSC_U8 dbgSMRollovers;                          
    FSC_S32 dbg_gpio_StateMachine;                  
    FSC_BOOL dbg_gpio_StateMachine_value;           
    char HostCommBuf[FSC_HOSTCOMM_BUFFER_SIZE];     
#endif 

    
    FSC_S32 InitDelayMS;                            
    FSC_S32 numRetriesI2C;                          

    
    struct i2c_client* client;                      
    FSC_BOOL use_i2c_blocks;                        

    struct pinctrl *fusb_pinctrl;                   
    struct pinctrl_state *fusb_default_state;       

    
    FSC_S32 gpio_VBus5V;                            
    FSC_BOOL gpio_VBus5V_value;                     
    FSC_S32 gpio_VBusOther;                         
    FSC_BOOL gpio_VBusOther_value;                  
    FSC_S32 gpio_IntN;                              

	int gpio_Vconn;                                  

#ifdef FSC_INTERRUPT_TRIGGERED
    FSC_S32 gpio_IntN_irq;                          
#endif  

    
    struct delayed_work init_worker;                
    struct work_struct worker;                      
    struct delayed_work debounce_work;
	struct delayed_work delayHost_work;
	struct dual_role_phy_instance *fusb_instance;
	enum port_mode pmode;
	enum power_role prole;
	enum data_role drole;
	enum vconn_supply vconn;

    
    struct hrtimer timer_state_machine;             
	atomic_t pm_suspended;
	struct wake_lock fusb_wlock;
};

extern struct fusb30x_chip* fg_chip;

struct fusb30x_chip* fusb30x_GetChip(void);         
void fusb30x_SetChip(struct fusb30x_chip* newChip); 

#endif 
