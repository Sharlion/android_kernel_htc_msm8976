/****************************************************************************
 * FileName:		TypeC.c
 * Processor:		PIC32MX250F128B
 * Compiler:		MPLAB XC32
 * Company:			Fairchild Semiconductor
 *
 * Author			Date		  Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * M. Smith			12/04/2014	  Initial Version
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the ?Company?)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN ?AS IS? CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/
#include <linux/kernel.h>
#include "TypeC.h"
#include "fusb30X.h"
#include "AlternateModes.h"

#include "PDPolicy.h"
#ifdef FSC_HAVE_VDM
#include "vdm/vdm_config.h"
#endif 

#ifdef FSC_DEBUG
#include "Log.h"
#endif 

#include <linux/switch.h>
#include "../fusb30x_global.h"
#include <linux/delay.h>
#define MAX_DEB_FAKE_IRQ_COUNT	2
#define MAX_MAYBE_FAKE_IRQ_COUNT	10
#define MAX_C2CwithRd_FAIL_COUNT	400
#define MAX_LOOPS_IN_STATE_COUNT_noPD	2000
#define MAX_LOOPS_IN_STATE_COUNT_PD	10000
#define MAX_I2C_FAIL_COUNT	2


DeviceReg_t             Registers = {{0}};  
FSC_BOOL                USBPDActive;        
FSC_BOOL                USBPDEnabled;       
FSC_U32                 PRSwapTimer;        
SourceOrSink            sourceOrSink;       
FSC_BOOL                g_Idle;             

USBTypeCPort            PortType;           
FSC_BOOL                blnCCPinIsCC1;      
FSC_BOOL                blnCCPinIsCC2;      
FSC_BOOL                blnSMEnabled = FALSE;       
ConnectionState         ConnState;          
FSC_U8                  TypeCSubState = 0;  
#ifdef FSC_DEBUG
StateLog                TypeCStateLog;      
volatile FSC_U16        Timer_S;            
volatile FSC_U16        Timer_tms;          
#endif 

#ifdef FSC_HAVE_DRP
FSC_BOOL         blnSrcPreferred;        
FSC_BOOL         blnSnkPreferred;        
#endif 

#ifdef FSC_HAVE_ACCMODE
FSC_BOOL                blnAccSupport;          
#endif 

FSC_U16                 StateTimer;             
FSC_U16                 PDDebounce;             
FSC_U16                 CCDebounce;             
FSC_U16                 ToggleTimer;            
FSC_U16                 DRPToggleTimer;         
FSC_U16                 OverPDDebounce;         
CCTermType              CC1TermPrevious;        
CCTermType              CC2TermPrevious;        
CCTermType              CC1TermCCDebounce;      
CCTermType              CC2TermCCDebounce;      
CCTermType              CC1TermPDDebounce;
CCTermType              CC2TermPDDebounce;
CCTermType              CC1TermPDDebouncePrevious;
CCTermType              CC2TermPDDebouncePrevious;

USBTypeCCurrent  SinkCurrent;		 
static USBTypeCCurrent  SourceCurrent;          

#ifdef FM150911A
static FSC_U8 alternateModes = 1;               
#else
static FSC_U8 alternateModes = 0;               
#endif

extern int fusb_debug_level;
static int fake_irq_counter = 0;
extern int fusb_ReStartIrq(int val);
extern bool fusb_PowerVconn(bool set);
bool VCONN_enabled = false;

extern bool flag_DualRoleSet;

extern int handle_case;
extern int readI2C_failcount;
extern int writeI2C_failcount;
extern bool ReStartIrq_flag;
int state_runcount = 0;

extern int htc_usb_nonCable_notification(int ebl);
extern bool nonCableNotify_flag;
extern bool connect2pc;
static bool allowCableNotify = false;
static int nonCable_count = 0;
static int maybe_fake_irq_counter = 0;
extern bool needDelayHostMode;

void TypeCTickAt100us(void)
{
	if ((StateTimer<T_TIMER_DISABLE) && (StateTimer>0))
		StateTimer--;
	if ((PDDebounce<T_TIMER_DISABLE) && (PDDebounce>0))
		PDDebounce--;
	if ((CCDebounce<T_TIMER_DISABLE) && (CCDebounce>0))
		CCDebounce--;
	if ((ToggleTimer<T_TIMER_DISABLE) && (ToggleTimer>0))
		ToggleTimer--;
	if (PRSwapTimer)
		PRSwapTimer--;
	if ((OverPDDebounce<T_TIMER_DISABLE) && (OverPDDebounce>0))
		OverPDDebounce--;
	if ((DRPToggleTimer<T_TIMER_DISABLE) && (DRPToggleTimer>0))
		DRPToggleTimer--;
}

#ifdef FSC_DEBUG
void LogTickAt100us(void)
{
	Timer_tms++;
	if(Timer_tms==10000)
	{
		Timer_S++;
		Timer_tms = 0;
	}
}
#endif 

void InitializeRegisters(void)
{
	DeviceRead(regDeviceID, 1, &Registers.DeviceID.byte);
	DeviceRead(regSwitches0, 1, &Registers.Switches.byte[0]);
	DeviceRead(regSwitches1, 1, &Registers.Switches.byte[1]);
	DeviceRead(regMeasure, 1, &Registers.Measure.byte);
	DeviceRead(regSlice, 1, &Registers.Slice.byte);
	DeviceRead(regControl0, 1, &Registers.Control.byte[0]);
	DeviceRead(regControl1, 1, &Registers.Control.byte[1]);
	DeviceRead(regControl2, 1, &Registers.Control.byte[2]);
	DeviceRead(regControl3, 1, &Registers.Control.byte[3]);
	DeviceRead(regMask, 1, &Registers.Mask.byte);
	DeviceRead(regPower, 1, &Registers.Power.byte);
	DeviceRead(regReset, 1, &Registers.Reset.byte);
	DeviceRead(regOCPreg, 1, &Registers.OCPreg.byte);
	DeviceRead(regMaska, 1, &Registers.MaskAdv.byte[0]);
	DeviceRead(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	DeviceRead(regControl4, 1, &Registers.Control4.byte);
	DeviceRead(regStatus0a, 1, &Registers.Status.byte[0]);
	DeviceRead(regStatus1a, 1, &Registers.Status.byte[1]);
	
	
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);
	DeviceRead(regStatus1, 1, &Registers.Status.byte[5]);
	
}

void InitializeTypeCVariables(void)
{
	InitializeRegisters();				

	Registers.Control.INT_MASK = 0;		   
	DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);

    Registers.Control.TOG_RD_ONLY = 1;                                          
    DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);

    SourceCurrent = utcc1p5A;                                                   
    updateSourceCurrent();

    blnSMEnabled = FALSE;                

#ifdef FSC_HAVE_ACCMODE
    blnAccSupport = TRUE;              
#endif 

    blnSMEnabled = FALSE;               

#ifdef FSC_HAVE_DRP
    blnSrcPreferred = FALSE;            
    blnSnkPreferred = TRUE;
#endif 

    g_Idle = FALSE;

    
    PortType = USBTypeC_UNDEFINED;

    switch (Registers.Control.MODE)
    {
        case 0b10:
#ifdef FSC_HAVE_SNK
            PortType = USBTypeC_Sink;
#endif 
            break;
        case 0b11:
#ifdef FSC_HAVE_SRC
            PortType = USBTypeC_Source;
#endif 
            break;
        case 0b01:
#ifdef FSC_HAVE_DRP
            PortType = USBTypeC_DRP;
#endif 
            break;
        default:
#ifdef FSC_HAVE_DRP
            PortType = USBTypeC_DRP;
#endif 
            break;
    }

    
    
    if( PortType == USBTypeC_UNDEFINED )
    {
#ifdef FSC_HAVE_SNK
        PortType = USBTypeC_Sink;
#endif 
#ifdef FSC_HAVE_SRC
        PortType = USBTypeC_Source;
#endif 
#ifdef FSC_HAVE_DRP
        PortType = USBTypeC_DRP;
#endif 
    }

    
    
    ConnState = Disabled;               
    blnCCPinIsCC1 = FALSE;              
    blnCCPinIsCC2 = FALSE;              
    StateTimer = T_TIMER_DISABLE;             
    PDDebounce = T_TIMER_DISABLE;         
    CCDebounce = T_TIMER_DISABLE;         
    ToggleTimer = T_TIMER_DISABLE;            
    resetDebounceVariables();           

#ifdef FSC_HAVE_SNK
    SinkCurrent = utccNone;             
#endif 

#ifdef FSC_HAVE_SRC
    SourceCurrent = utccDefault;        
    updateSourceCurrent();
#endif 

    USBPDActive = FALSE;                
    
    USBPDEnabled = FALSE;                
    PRSwapTimer = 0;                    
    IsHardReset = FALSE;                
    TypeCSubState = 0;                  
}

void InitializeTypeC(void)
{
#ifdef FSC_DEBUG
    Timer_tms = 0;
    Timer_S = 0;
    InitializeStateLog(&TypeCStateLog); 
#endif 

    SetStateDelayUnattached();
}

void DisableTypeCStateMachine(void)
{
    blnSMEnabled = FALSE;
}

void EnableTypeCStateMachine(void)
{
	blnSMEnabled = TRUE;

#ifdef FSC_DEBUG
    Timer_tms = 0;
    Timer_S = 0;
#endif 
}

void core_reset(void)
{
	DisableTypeCStateMachine();              

	Registers.Reset.SW_RES = 1;
	DeviceWrite(regReset, 1, &Registers.Reset.byte); 
	Registers.Reset.SW_RES = 0;

	InitializeRegisters(); 
	InitializeTypeCVariables();
	InitializeTypeC();

	EnableTypeCStateMachine(); 
}

static int detect_done = 0;
int workable_charging_cable(void)
{
	printk("FUSB  [%s]: Read CC Type Value, CC1TermPrevious:%d, CC2TermPrevious:%d, ReStartIrq_flag: %d\n", __func__, CC1TermPrevious, CC2TermPrevious, ReStartIrq_flag);
	if (ReStartIrq_flag || detect_done  == 0)
		return -1;
	else if (CC1TermPrevious == CCTypeRdUSB && CC2TermPrevious == 0)
		return 1;
	else if (CC1TermPrevious == 0 && CC2TermPrevious == CCTypeRdUSB)
		return 1;
	else if (CC1TermPrevious == CCTypeRd1p5 && CC2TermPrevious == 0)
		return 1;
	else if (CC1TermPrevious == 0 && CC2TermPrevious == CCTypeRd1p5)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(workable_charging_cable);

void StateMachineTypeC(void)
{
#ifdef	FSC_INTERRUPT_TRIGGERED
	int ret = 0;
	int preConnState = 0;

	detect_done = 0;

	do{
#endif 
		if (!blnSMEnabled)
			return;

		if (platform_get_device_irq_state())
		{
			DeviceRead(regStatus0a, 7, &Registers.Status.byte[0]);	   
			Registers.Status2 = Registers.Status; 
			if (fusb_debug_level >= 1)
				printk("FUSB  %s: register status: 0x%x:%d, 0x%x:%d, 0x%x:%d, 0x%x:%d, 0x%x:%d, 0x%x:%d, 0x%x:%d\n", __func__,
					regStatus0a, Registers.Status.byte[0],
					regStatus1a, Registers.Status.byte[1],
					regInterrupta, Registers.Status.byte[2],
					regInterruptb, Registers.Status.byte[3],
					regStatus0, Registers.Status.byte[4],
					regStatus1, Registers.Status.byte[5],
					regInterrupt, Registers.Status.byte[6]);
		}
#ifdef FSC_INTERRUPT_TRIGGERED
        else if(PolicyState != PE_BIST_Test_Data)
        {
            DeviceRead(regStatus1, 1, &Registers.Status.byte[5]);                      
        }
#endif 

		if (fusb_debug_level >= 2)
			printk("FUSB  [%s]: Enter StateMachineTypeC(), now ConnState:%d, g_Idle:%d, USBPDActive:%d\n", __func__, ConnState, g_Idle, USBPDActive);

		if (USBPDActive)												
		{
			USBPDProtocol();											
			USBPDPolicyEngine();										
		}

		state_runcount++;
		preConnState = ConnState;
		switch (ConnState)
		{
			case Disabled:
				StateMachineDisabled();
				break;
			case ErrorRecovery:
				StateMachineErrorRecovery();
				break;
			case Unattached:
				StateMachineUnattached();
				break;
#ifdef FSC_HAVE_SNK
			case AttachWaitSink:
				StateMachineAttachWaitSink();
				break;
			case AttachedSink:
				StateMachineAttachedSink();
				break;
#ifdef FSC_HAVE_DRP
            case TryWaitSink:
                StateMachineTryWaitSink();
                break;
            case TrySink:
                stateMachineTrySink();
                break;
#endif 
#endif 
#ifdef FSC_HAVE_SRC
			case AttachWaitSource:
				StateMachineAttachWaitSource();
				break;
			case AttachedSource:
				StateMachineAttachedSource();
				break;
#ifdef FSC_HAVE_DRP
            case TryWaitSource:
                stateMachineTryWaitSource();
                break;
			case TrySource:
				StateMachineTrySource();
				break;
#endif 
            case UnattachedSource:
                stateMachineUnattachedSource();
                break;
#endif 
#ifdef FSC_HAVE_ACCMODE
			case AudioAccessory:
				StateMachineAudioAccessory();
				break;
			case DebugAccessory:
				StateMachineDebugAccessory();
				break;
			case AttachWaitAccessory:
				StateMachineAttachWaitAccessory();
				break;
			case PoweredAccessory:
				StateMachinePoweredAccessory();
				break;
			case UnsupportedAccessory:
				StateMachineUnsupportedAccessory();
				break;
#endif 
			case DelayUnattached:
				StateMachineDelayUnattached();
				break;
			default:
				SetStateDelayUnattached();											
				break;
		}
		Registers.Status.Interrupt1 = 0;			
		Registers.Status.InterruptAdv = 0;			

		
		if ((USBPDEnabled == 0 && state_runcount > MAX_LOOPS_IN_STATE_COUNT_noPD)
			|| (USBPDEnabled == 1 && state_runcount > MAX_LOOPS_IN_STATE_COUNT_PD)) {
			printk("FUSB  [%s] - Skip Disable IRQ for protection, but there are %d loops in state machine, USBPDEnabled:%d\n", __func__, state_runcount, USBPDEnabled);
			state_runcount = 0;
		}
		
		if (readI2C_failcount > MAX_I2C_FAIL_COUNT || writeI2C_failcount > MAX_I2C_FAIL_COUNT) {
			printk("FUSB  [%s] - Disable IRQ for 5 seconds since too many I2C read/write failed (R:%d/W:%d)\n", __func__, readI2C_failcount, writeI2C_failcount);
			readI2C_failcount = 0;
			writeI2C_failcount = 0;
			
			g_Idle = TRUE;
			ConnState = Unattached;
			
			if (!ReStartIrq_flag) {
				handle_case = 4;
				ret = fusb_ReStartIrq(5);
				if (ret)
					printk("FUSB  [%s] - Error: Unable to disable interrupts! Error code: %d\n", __func__, ret);
			}
		}
#ifdef	FSC_INTERRUPT_TRIGGERED
		platform_delay_10us(SLEEP_DELAY);
	}while(g_Idle == FALSE);
#endif

	if (fusb_debug_level >= 0)
		printk("FUSB  [%s]: leave StateMachineTypeC(),(R:%d/W:%d) now ConnState:%d(%d circles), CC1TermPrevious:%d, CC2TermPrevious:%d, flag_DualRoleSet:%d, PortType:%d, blnSrcPreferred:%d, blnSnkPreferred:%d\n",
			__func__, readI2C_failcount, writeI2C_failcount, ConnState, state_runcount, CC1TermPrevious, CC2TermPrevious, flag_DualRoleSet, PortType, blnSrcPreferred, blnSnkPreferred);

	if (fake_irq_counter > MAX_DEB_FAKE_IRQ_COUNT) {
#ifdef FSC_INTERRUPT_TRIGGERED
		fake_irq_counter = 0;
		if (!ReStartIrq_flag) {
			handle_case = 1;
			ret = fusb_ReStartIrq(5);
			if (ret)
				printk("FUSB  [%s] - Error: Unable to disable interrupts! Error code: %d\n", __func__, ret);
		}
#endif
	} else {
		if (preConnState == ConnState)
			maybe_fake_irq_counter++;
		else
			maybe_fake_irq_counter = 0;

		if (maybe_fake_irq_counter > MAX_MAYBE_FAKE_IRQ_COUNT) {
#ifdef FSC_INTERRUPT_TRIGGERED
			maybe_fake_irq_counter = 0;
			if (!ReStartIrq_flag) {
				handle_case = 2;
				ret = fusb_ReStartIrq(5);
				if (ret)
					printk("FUSB  [%s] - Error: Unable to disable interrupts! Error code: %d\n", __func__, ret);
			}
#endif
		}
	}
	nonCable_count = 0;
	detect_done = 1;

}

void StateMachineDisabled(void)
{
	
}

void StateMachineErrorRecovery(void)
{
	if (StateTimer == 0)
	{
		SetStateDelayUnattached();
	}
}

void StateMachineDelayUnattached(void)
{
	if (StateTimer == 0)
	{
		SetStateUnattached();
	}
}

void StateMachineUnattached(void)	
{
	if(alternateModes)
	{
		StateMachineAlternateUnattached();
		return;
	}

	if (Registers.Control.HOST_CUR != 0b01) 
	{
		Registers.Control.HOST_CUR = 0b01;
		DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);
	}

	if (Registers.Status.I_TOGDONE)
	{
		switch (Registers.Status.TOGSS)
		{
#ifdef FSC_HAVE_SNK
			case 0b101: 
				blnCCPinIsCC1 = TRUE;
				blnCCPinIsCC2 = FALSE;
				if (fusb_debug_level >= 0)
					printk("FUSB  [%s] - Registers.Status.TOGSS: 0x%x(%d), Rp detected on CC1, PortType: %d, blnSrcPreferred: %d, blnSnkPreferred: %d\n",
						__func__, Registers.Status.TOGSS, Registers.Status.TOGSS, PortType, blnSrcPreferred, blnSnkPreferred);
				SetStateAttachWaitSink();										 
				break;
			case 0b110: 
				blnCCPinIsCC1 = FALSE;
				blnCCPinIsCC2 = TRUE;
				if (fusb_debug_level >= 0)
					printk("FUSB  [%s] - Registers.Status.TOGSS: 0x%x(%d), Rp detected on CC2, PortType: %d, blnSrcPreferred: %d, blnSnkPreferred: %d\n",
						__func__, Registers.Status.TOGSS, Registers.Status.TOGSS, PortType, blnSrcPreferred, blnSnkPreferred);
				SetStateAttachWaitSink();										 
				break;
#endif 
#ifdef FSC_HAVE_SRC
			case 0b001: 
				blnCCPinIsCC1 = TRUE;
				blnCCPinIsCC2 = FALSE;
				if (fusb_debug_level >= 0)
					printk("FUSB  [%s] - Registers.Status.TOGSS: 0x%x(%d), Rd detected on CC1, PortType: %d, blnSrcPreferred: %d, blnSnkPreferred: %d\n",
						__func__, Registers.Status.TOGSS, Registers.Status.TOGSS, PortType, blnSrcPreferred, blnSnkPreferred);
#ifdef FSC_HAVE_ACCMODE
				if ((PortType == USBTypeC_Sink) && (blnAccSupport))				
					checkForAccessory();										
				else															
#endif 
					SetStateAttachWaitSource();									   
				break;
			case 0b010: 
				blnCCPinIsCC1 = FALSE;
				blnCCPinIsCC2 = TRUE;
				if (fusb_debug_level >= 0)
					printk("FUSB  [%s] - Registers.Status.TOGSS: 0x%x(%d), Rd detected on CC2, PortType: %d, blnSrcPreferred: %d, blnSnkPreferred: %d\n",
						__func__, Registers.Status.TOGSS, Registers.Status.TOGSS, PortType, blnSrcPreferred, blnSnkPreferred);
#ifdef FSC_HAVE_ACCMODE
				if ((PortType == USBTypeC_Sink) && (blnAccSupport))				
					checkForAccessory();									   
				else															
#endif 
					SetStateAttachWaitSource();									   
				break;
			case 0b111: 
				blnCCPinIsCC1 = FALSE;
				blnCCPinIsCC2 = FALSE;
				if (fusb_debug_level >= 0)
					printk("FUSB  [%s] - Registers.Status.TOGSS: 0x%x(%d), Ra detected on both CC1 and CC2, PortType: %d, blnSrcPreferred: %d, blnSnkPreferred: %d\n",
						__func__, Registers.Status.TOGSS, Registers.Status.TOGSS, PortType, blnSrcPreferred, blnSnkPreferred);
#ifdef FSC_HAVE_ACCMODE
				if ((PortType == USBTypeC_Sink) && (blnAccSupport))				
					SetStateAttachWaitAccessory();									  
				else															
#endif 
					SetStateAttachWaitSource();									   
				break;
#endif 
			default:	
				Registers.Control.TOGGLE = 0;									
				DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	   
				platform_delay_10us(1);
				Registers.Control.TOGGLE = 1;									
				DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	   
				break;
		}
	}

#ifdef FPGA_BOARD
	rand();
#endif 
}

#ifdef FSC_HAVE_SNK
void StateMachineAttachWaitSink(void)
{
	debounceCC();

	if ((CC1TermPDDebounce == CCTypeOpen) && (CC2TermPDDebounce == CCTypeOpen)) 
	{
#ifdef FSC_HAVE_DRP
		if (PortType == USBTypeC_DRP)
		{
			
			fake_irq_counter++;
			SetStateUnattachedSource();											
		}
		else
#endif 
		{
			SetStateDelayUnattached();
		}
	}
	else if (Registers.Status.VBUSOK)											
	{
		fake_irq_counter = 0;
		if ((CC1TermCCDebounce > CCTypeOpen) && (CC2TermCCDebounce == CCTypeOpen)) 
		{
			blnCCPinIsCC1 = TRUE;												
			blnCCPinIsCC2 = FALSE;
#ifdef FSC_HAVE_DRP
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)					
				SetStateTrySource();											
			else																
#endif 
			{
				SetStateAttachedSink();											
			}
		}
		else if ((CC1TermCCDebounce == CCTypeOpen) && (CC2TermCCDebounce > CCTypeOpen)) 
		{
			blnCCPinIsCC1 = FALSE;												
			blnCCPinIsCC2 = TRUE;
#ifdef FSC_HAVE_DRP
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)					
				SetStateTrySource();											
			else																
#endif 
			{
				SetStateAttachedSink();											
			}
		}
	}
}
#endif 

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(void)
{
	debounceCC();

	if(blnCCPinIsCC1)															
	{
		if(CC1TermCCDebounce != CCTypeUndefined)
		{
			peekCC2Source();
		}
	}
	else if (blnCCPinIsCC2)
	{
		if(CC2TermCCDebounce != CCTypeUndefined)
		{
			peekCC1Source();
		}
	}

#ifdef FSC_HAVE_ACCMODE
	if(blnAccSupport)															
	{
		if ((CC1TermCCDebounce == CCTypeRa) && (CC2TermCCDebounce == CCTypeRa))				  
			SetStateAudioAccessory();
		else if ((CC1TermCCDebounce >= CCTypeRdUSB) && (CC1TermCCDebounce < CCTypeUndefined) && (CC2TermCCDebounce >= CCTypeRdUSB) && (CC2TermCCDebounce < CCTypeUndefined))		   
			SetStateDebugAccessory();
	}
#endif 
	if (((CC1TermCCDebounce >= CCTypeRdUSB) && (CC1TermCCDebounce < CCTypeUndefined)) && ((CC2TermCCDebounce == CCTypeOpen) || (CC2TermCCDebounce == CCTypeRa)))	  
	{
		if (VbusVSafe0V()) {												
#ifdef FSC_HAVE_DRP
			if(blnSnkPreferred)
			{
				SetStateTrySink();
			}
			else
#endif 
			{
				SetStateAttachedSource();										   
			}										   
		} else {
			nonCable_count++;
		}
	}
	else if (((CC2TermCCDebounce >= CCTypeRdUSB) && (CC2TermCCDebounce < CCTypeUndefined)) && ((CC1TermCCDebounce == CCTypeOpen) || (CC1TermCCDebounce == CCTypeRa))) 
	{
		if (VbusVSafe0V()) {												
#ifdef FSC_HAVE_DRP
			if(blnSnkPreferred)
			{
				SetStateTrySink();
			}
			else
#endif 
			{
				SetStateAttachedSource();										   
			}
		} else {
			nonCable_count++;
		}
	}
	else if ((CC1TermPrevious == CCTypeOpen) && (CC2TermPrevious == CCTypeOpen))	  
	{
		printk("FUSB  [%s]: go to unattached state,  ConnState:%d, CC1TermPrevious:%d, CC2TermPrevious:%d\n", __func__, ConnState, CC1TermPrevious, CC2TermPrevious);
		SetStateDelayUnattached();
	}
	else if ((CC1TermPrevious == CCTypeOpen) && (CC2TermPrevious == CCTypeRa))		  
	{
		printk("FUSB  [%s]: go to unattached state,  ConnState:%d, CC1TermPrevious:%d, CC2TermPrevious:%d\n", __func__, ConnState, CC1TermPrevious, CC2TermPrevious);
		SetStateDelayUnattached();
	}
	else if ((CC1TermPrevious == CCTypeRa) && (CC2TermPrevious == CCTypeOpen))		  
	{
		printk("FUSB  [%s]: go to unattached state,  ConnState:%d, CC1TermPrevious:%d, CC2TermPrevious:%d\n", __func__, ConnState, CC1TermPrevious, CC2TermPrevious);
		SetStateDelayUnattached();
	}

	
	if (nonCable_count >= MAX_C2CwithRd_FAIL_COUNT && !nonCableNotify_flag) {
		if (connect2pc) {
			printk("FUSB  [%s]: We detect a NonStandard Cable but device can connect to host, so we skip notification and go StateAttachedSink\n", __func__);
			SetStateAttachedSink();
		} else {
			if (allowCableNotify) {
				printk("FUSB  [%s]: Send NonStandard Cable Notification(1)\n", __func__);
				htc_usb_nonCable_notification(1);
			} else {
				printk("FUSB  [%s]: Do not Send NonStandard Cable Notification(1) since allowCableNotify is %d\n", __func__, allowCableNotify);
			}
		}
		nonCable_count = 0;
	}

}
#endif 

#ifdef FSC_HAVE_ACCMODE
void StateMachineAttachWaitAccessory(void)
{
	debounceCC();

	if ((CC1TermCCDebounce == CCTypeRa) && (CC2TermCCDebounce == CCTypeRa))				  
	{
		SetStateAudioAccessory();
	}
	else if ((CC1TermCCDebounce >= CCTypeRdUSB) && (CC1TermCCDebounce < CCTypeUndefined) && (CC2TermCCDebounce >= CCTypeRdUSB) && (CC2TermCCDebounce < CCTypeUndefined))			
	{
		SetStateDebugAccessory();
	}
    else if ((CC1TermPrevious == CCTypeOpen) && (CC2TermPrevious == CCTypeOpen))      
	{
		SetStateDelayUnattached();
	}
	else if ((CC1TermCCDebounce >= CCTypeRdUSB) && (CC1TermCCDebounce < CCTypeUndefined) && (CC2TermCCDebounce == CCTypeRa))		   
	{
		SetStatePoweredAccessory();
	}
	else if ((CC1TermCCDebounce == CCTypeRa) && (CC2TermCCDebounce >= CCTypeRdUSB) && (CC2TermCCDebounce < CCTypeUndefined))		   
	{
		SetStatePoweredAccessory();
	}
}
#endif 

#ifdef FSC_HAVE_SNK
void StateMachineAttachedSink(void)
{
	

#ifdef COMPLIANCE
    if ((!IsPRSwap) && (IsHardReset == FALSE) && VbusUnder5V())               
#else
    if ((!IsPRSwap) && (IsHardReset == FALSE) && !Registers.Status.VBUSOK)      
#endif 
	{
		SetStateDelayUnattached();												
	}

	if (blnCCPinIsCC1)
	{
		UpdateSinkCurrent(CC1TermCCDebounce);									
	}
	else if(blnCCPinIsCC2)
	{
		UpdateSinkCurrent(CC2TermCCDebounce);									
	}

}
#endif 

#ifdef FSC_HAVE_SRC
void StateMachineAttachedSource(void)
{
    switch(TypeCSubState)
    {
        default:
        case 0:
            debounceCC();

            if(Registers.Switches.MEAS_CC1)
            {
                if ((CC1TermPrevious == CCTypeOpen) && (!IsPRSwap))                       
                {
#ifdef FSC_HAVE_DRP
                    if ((PortType == USBTypeC_DRP) && blnSrcPreferred)                  
                        SetStateTryWaitSink();
                    else                                                                
#endif 
                    {
                        platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
                        USBPDDisable(TRUE);                                             
                        Registers.Switches.byte[0] = 0x00;                              
                        DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
                        TypeCSubState++;
                        g_Idle = FALSE;                                                 
                    }
                }
            }
            else if (Registers.Switches.MEAS_CC2)
            {
                if ((CC2TermPrevious == CCTypeOpen) && (!IsPRSwap))                       
                {
#ifdef FSC_HAVE_DRP
                    if ((PortType == USBTypeC_DRP) && blnSrcPreferred)                  
                        SetStateTryWaitSink();
                    else                                                                
#endif 
                    {
                        platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
                        USBPDDisable(TRUE);                                             
                        Registers.Switches.byte[0] = 0x00;                              
                        DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
                        TypeCSubState++;
                        g_Idle = FALSE;                                                 
                    }
                }
            }
            break;
        case 1:
            if(VbusVSafe0V())
            {
                SetStateDelayUnattached();
            }
            break;
		case 2:
			platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
			USBPDDisable(TRUE);                                             
			Registers.Switches.byte[0] = 0x00;                              
			DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
			TypeCSubState = 1;
			g_Idle = FALSE;                                                 
			break;
    }
}
#endif 

#ifdef FSC_HAVE_DRP
void StateMachineTryWaitSink(void)
{
	debounceCC();

	if ((StateTimer == 0) && (CC1TermPrevious == CCTypeOpen) && (CC2TermPrevious == CCTypeOpen))  
		SetStateDelayUnattached();												
	else if (Registers.Status.VBUSOK)				   
	{
		if ((CC1TermCCDebounce > CCTypeOpen) && (CC2TermCCDebounce == CCTypeOpen))				  
		{											 
			SetStateAttachedSink();												
		}
		else if ((CC1TermCCDebounce == CCTypeOpen) && (CC2TermCCDebounce > CCTypeOpen))			  
		{
			SetStateAttachedSink();												
		}
	}
}
#endif 

#ifdef FSC_HAVE_DRP
void StateMachineTrySource(void)
{
	debounceCC();

	if ((CC1TermPDDebounce > CCTypeRa) && (CC1TermPDDebounce < CCTypeUndefined) && ((CC2TermPDDebounce == CCTypeOpen) || (CC2TermPDDebounce == CCTypeRa)))	  
	{
		SetStateAttachedSource();												   
	}
	else if ((CC2TermPDDebounce > CCTypeRa) && (CC2TermPDDebounce < CCTypeUndefined) && ((CC1TermPDDebounce == CCTypeOpen) || (CC1TermPDDebounce == CCTypeRa)))   
	{
		SetStateAttachedSource();												   
	}
	else if (StateTimer == 0)													
		SetStateTryWaitSink();													 
}
#endif 

#ifdef FSC_HAVE_ACCMODE
void StateMachineDebugAccessory(void)
{
	debounceCC();

	if ((CC1TermCCDebounce == CCTypeOpen) || (CC2TermCCDebounce == CCTypeOpen)) 
	{
#ifdef FSC_HAVE_SRC
		if(PortType == USBTypeC_Source)
		{
			SetStateUnattachedSource();
		}
		else
#endif 
		{
			SetStateDelayUnattached();
		}
	}

}

void StateMachineAudioAccessory(void)
{
	debounceCC();

	if ((CC1TermCCDebounce == CCTypeOpen) || (CC2TermCCDebounce == CCTypeOpen)) 
	{
#ifdef FSC_HAVE_SRC
		if(PortType == USBTypeC_Source)
		{
			SetStateUnattachedSource();
		}
		else
#endif 
		{
			SetStateDelayUnattached();
		}
	}
#ifdef FSC_INTERRUPT_TRIGGERED
    if(((CC1TermPrevious == CCTypeOpen) || (CC2TermPrevious == CCTypeOpen)) && (g_Idle == TRUE))
    {
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
    }
    if((CC1TermPDDebounce == CCTypeRa) && (CC2TermPDDebounce == CCTypeRa))      
    {
        g_Idle = TRUE;                                                          
        Registers.Mask.byte = 0xFF;
        Registers.Mask.M_COMP_CHNG = 0;
        DeviceWrite(regMask, 1, &Registers.Mask.byte);
        Registers.MaskAdv.byte[0] = 0xFF;
        DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
        Registers.MaskAdv.M_GCRCSENT = 1;
        DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
        platform_enable_timer(FALSE);
    }
#endif  
}

void StateMachinePoweredAccessory(void) 
{
    debounceCC();

    if(blnCCPinIsCC1 && (CC1TermPrevious == CCTypeOpen))    
    {
        SetStateDelayUnattached();
    }
    else if(blnCCPinIsCC2 && (CC2TermPrevious == CCTypeOpen))    
    {
        SetStateDelayUnattached();
    }
    
    else if (StateTimer == 0)                                                   
        SetStateDelayUnattached(); 
        
}

void StateMachineUnsupportedAccessory(void)
{
	debounceCC();

	if((blnCCPinIsCC1) && (CC1TermPrevious == CCTypeOpen))	  
	{
		SetStateDelayUnattached();
	}
	else if((blnCCPinIsCC2) && (CC2TermPrevious == CCTypeOpen))    
	{
		SetStateDelayUnattached();
	}
}
#endif 

#ifdef FSC_HAVE_DRP
void stateMachineTrySink(void)
{
	if (StateTimer == 0)
	{
		debounceCC();
	}

	if(Registers.Status.VBUSOK)
	{
		if ((CC1TermPDDebounce >= CCTypeRdUSB) && (CC1TermPDDebounce < CCTypeUndefined) && (CC2TermPDDebounce == CCTypeOpen))	 
		{
			SetStateAttachedSink();													 
		}
		else if ((CC2TermPDDebounce >= CCTypeRdUSB) && (CC2TermPDDebounce < CCTypeUndefined) && (CC1TermPDDebounce == CCTypeOpen))	 
		{
			SetStateAttachedSink();													 
		}
	}

	if ((CC1TermPDDebounce == CCTypeOpen) && (CC2TermPDDebounce == CCTypeOpen))
	{
		SetStateTryWaitSource();
	}
}
#endif 

#ifdef FSC_HAVE_DRP
void stateMachineTryWaitSource(void)
{
	debounceCC();

	if(VbusVSafe0V())
	{
		if (((CC1TermPDDebounce >= CCTypeRdUSB) && (CC1TermPDDebounce < CCTypeUndefined)) && ((CC2TermPDDebounce == CCTypeRa) || CC2TermPDDebounce == CCTypeOpen))	  
		{
			SetStateAttachedSource();												   
		}
		else if (((CC2TermPDDebounce >= CCTypeRdUSB) && (CC2TermPDDebounce < CCTypeUndefined)) && ((CC1TermPDDebounce == CCTypeRa) || CC1TermPDDebounce == CCTypeOpen))   
		{
			SetStateAttachedSource();												   
		}
	}

	if(StateTimer == 0)
	{
		if ((CC1TermPrevious == CCTypeOpen) && (CC1TermPrevious == CCTypeOpen))
		{
			SetStateDelayUnattached();
		}
	}
}
#endif 

#ifdef FSC_HAVE_SRC
void stateMachineUnattachedSource(void)
{
	if(alternateModes)
	{
		StateMachineAlternateUnattachedSource();
		return;
	}

	debounceCC();

	if ((CC1TermPrevious == CCTypeRa) && (CC2TermPrevious == CCTypeRa))
	{
		SetStateAttachWaitSource();
	}

	if ((CC1TermPrevious >= CCTypeRdUSB) && (CC1TermPrevious < CCTypeUndefined) && ((CC2TermPrevious == CCTypeRa) || CC2TermPrevious == CCTypeOpen))	
	{
		blnCCPinIsCC1 = TRUE;													
		blnCCPinIsCC2 = FALSE;
		SetStateAttachWaitSource();													 
	}
	else if ((CC2TermPrevious >= CCTypeRdUSB) && (CC2TermPrevious < CCTypeUndefined) && ((CC1TermPrevious == CCTypeRa) || CC1TermPrevious == CCTypeOpen))	
	{
		blnCCPinIsCC1 = FALSE;													
		blnCCPinIsCC2 = TRUE;
		SetStateAttachWaitSource();													 
	}

	if (DRPToggleTimer == 0)
	{
		SetStateDelayUnattached();
	}

}
#endif 

#ifdef FSC_DEBUG
void SetStateDisabled(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;																
	Registers.Mask.byte = 0x00;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(TRUE);
#endif 
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
	Registers.Power.PWR = 0x1;									   
	Registers.Control.TOGGLE = 0;									
	Registers.Control.HOST_CUR = 0b00;								
	Registers.Switches.byte[0] = 0x00;								
	DeviceWrite(regPower, 1, &Registers.Power.byte);			   
	DeviceWrite(regControl0, 3, &Registers.Control.byte[0]);	   
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	   
	USBPDDisable(TRUE);											   
	resetDebounceVariables();
	blnCCPinIsCC1 = FALSE;											
	blnCCPinIsCC2 = FALSE;											
	ConnState = Disabled;											
	StateTimer = T_TIMER_DISABLE;										  
	PDDebounce = T_TIMER_DISABLE;									  
	CCDebounce = T_TIMER_DISABLE;									  
	ToggleTimer = T_TIMER_DISABLE;										  
	OverPDDebounce = T_TIMER_DISABLE;  
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
}
#endif 

void SetStateErrorRecovery(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;																
	Registers.Mask.byte = 0x00;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(TRUE);
#endif 
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
	Registers.Power.PWR = 0x1;									   
	Registers.Control.TOGGLE = 0;									
	Registers.Control.HOST_CUR = 0b00;								
	Registers.Switches.byte[0] = 0x00;								 
	DeviceWrite(regPower, 1, &Registers.Power.byte);			   
	DeviceWrite(regControl0, 3, &Registers.Control.byte[0]);	   
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	   
	USBPDDisable(TRUE);											   
	resetDebounceVariables();
	blnCCPinIsCC1 = FALSE;											
	blnCCPinIsCC2 = FALSE;											
	ConnState = ErrorRecovery;										
	StateTimer = tErrorRecovery;									
	PDDebounce = T_TIMER_DISABLE;									  
	CCDebounce = T_TIMER_DISABLE;									  
	ToggleTimer = T_TIMER_DISABLE;										  
	OverPDDebounce = T_TIMER_DISABLE;  
	IsHardReset = FALSE;
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}

void SetStateDelayUnattached(void)
{
#ifndef FPGA_BOARD
	SetStateUnattached();
	return;
#else
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif  
    
    
    
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    Registers.Power.PWR = 0x1;                                      
    Registers.Control.TOGGLE = 0;                                   
    Registers.Switches.word &= 0x6800;                              
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    DeviceWrite(regControl0, 3, &Registers.Control.byte[0]);       
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    USBPDDisable(TRUE);                                            
    resetDebounceVariables();
    blnCCPinIsCC1 = FALSE;                                          
    blnCCPinIsCC2 = FALSE;                                          
    ConnState = DelayUnattached;                                    
	StateTimer = rand() % 64;                                       
	PDDebounce = T_TIMER_DISABLE;                                     
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
#endif 
}

void SetStateUnattached(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	if(alternateModes)
	{
		SetStateAlternateUnattached();
		return;
	}
	if (VCONN_enabled) {
		
		if (fusb_PowerVconn(false))
			printk("FUSB  [%s]: Error: Unable to power off VCONN!\n", __func__);
		else {
			printk("FUSB  [%s]: State Unattached, power off the VCONN\n", __func__);
			VCONN_enabled = false;
			chip->vconn = VCONN_SUPPLY_NO;
		}
	}
	if (nonCableNotify_flag) {
		printk("FUSB  [%s]: trigger Non Standard Cable Notification(0)\n", __func__);
		htc_usb_nonCable_notification(0);
		nonCable_count = 0;
	}

#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;																
	Registers.Mask.byte = 0xFF;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	Registers.MaskAdv.M_TOGDONE = 0;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(FALSE);
#endif 
    
    
    Registers.MaskAdv.M_TOGDONE = 0;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.Control.TOGGLE = 0;
    DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);       
    Registers.Switches.byte[0] = 0x03;                               
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     

    if (PortType == USBTypeC_DRP)                                   
        Registers.Control.MODE = 0b01;                              
#ifdef FSC_HAVE_ACCMODE
    else if((PortType == USBTypeC_Sink) && (blnAccSupport))         
        Registers.Control.MODE = 0b01;                              
#endif 
    else if (PortType == USBTypeC_Source)                           
        Registers.Control.MODE = 0b11;                              
    else                                                            
        Registers.Control.MODE = 0b10;                              

    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    Registers.Control.HOST_CUR = 0b01;                              
    Registers.Control.TOGGLE = 1;                                   
    platform_delay_10us(1);                                                  
    DeviceWrite(regControl0, 3, &Registers.Control.byte[0]);       
    USBPDDisable(TRUE);                                            
    ConnState = Unattached;
    SinkCurrent = utccNone;
    resetDebounceVariables();
    blnCCPinIsCC1 = FALSE;                                          
    blnCCPinIsCC2 = FALSE;                                          
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = T_TIMER_DISABLE;                                     
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
	chip->pmode = PMODE_UNKNOWN;
	chip->prole = UNKNOWN_POWER_ROLE;
	chip->drole = UNKNOWN_DATA_ROLE;
	dual_role_instance_changed(chip->fusb_instance);

}

#ifdef FSC_HAVE_SNK
void SetStateAttachWaitSink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = AttachWaitSink;                                     
    sourceOrSink = Sink;
    Registers.Control.TOGGLE = 0;                                               
    DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);                    
    updateSourceCurrent();
    Registers.Power.PWR = 0x7;                                                  
    DeviceWrite(regPower, 1, &Registers.Power.byte);                            
    Registers.Switches.byte[0] = 0x07;                                          
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);                  
    resetDebounceVariables();
    SinkCurrent = utccNone;                                         
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tPDDebounce;                                
    CCDebounce = tCCDebounce;                                     
    ToggleTimer = tDeviceToggle;                                   
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_SRC
void SetStateAttachWaitSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    Registers.Control.TOGGLE = 0;                                   
    DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);        

    updateSourceCurrent();
    ConnState = AttachWaitSource;                                   
    sourceOrSink = Source;
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                              
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;                           
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else
    {
        peekCC1Source();
        Registers.Switches.byte[0] = 0x88;                           
        setDebounceVariablesCC2(CCTypeUndefined);
    }

    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    OverPDDebounce = T_TIMER_DISABLE;  
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tPDDebounce;                                
    CCDebounce = tCCDebounce;                                     
    ToggleTimer = T_TIMER_DISABLE;                                             
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_ACCMODE
void SetStateAttachWaitAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = AttachWaitAccessory;                                
    sourceOrSink = Source;
    updateSourceCurrent();
    Registers.Control.TOGGLE = 0;                                   
    DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);       
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                              
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;                           
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else
    {
        peekCC1Source();
        Registers.Switches.byte[0] = 0x88;                           
        setDebounceVariablesCC2(CCTypeUndefined);
    }
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    OverPDDebounce = T_TIMER_DISABLE;  
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tCCDebounce;                                
    CCDebounce = tCCDebounce;                                     
    ToggleTimer = T_TIMER_DISABLE;                                   
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_SRC
void SetStateAttachedSource(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	if (needDelayHostMode) {
		printk("FUSB  [%s] - Delay 1 second to enable Host mode\n", __func__);
		mdelay(1000);
	}
	if (nonCableNotify_flag) {
		printk("FUSB  [%s]: trigger Non Standard Cable Notification(0)\n", __func__);
		htc_usb_nonCable_notification(0);
		nonCable_count = 0;
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;																
	Registers.Mask.byte = 0xFF;
	Registers.Mask.M_COMP_CHNG = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(FALSE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);                      
    ConnState = AttachedSource;                                                 
    TypeCSubState = 0;
	sourceOrSink = Source;
	updateSourceCurrent();
	Registers.Power.PWR = 0x7;													
	DeviceWrite(regPower, 1, &Registers.Power.byte);							

	if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))					
	{
		DetectCCPinSource();
	}

	chip->pmode = PMODE_DFP;
	chip->prole = PR_SOURCE;
	chip->drole = DR_HOST;
	if (flag_DualRoleSet) {
		PortType = USBTypeC_DRP;
		blnSrcPreferred = FALSE;
		blnSnkPreferred = TRUE;
		flag_DualRoleSet = FALSE;
	}
	dual_role_instance_changed(chip->fusb_instance);

	if (blnCCPinIsCC1)															
	{
		peekCC2Source();
		Registers.Switches.byte[0] = 0x44;										
		if(CC2TermPrevious == CCTypeRa)										
		{
			Registers.Switches.VCONN_CC2 = 1;
			
			if (!fusb_PowerVconn(true))
				printk("FUSB  [%s]: Error: Unable to power on VCONN!\n", __func__);
			else {
				printk("FUSB  [%s]: Ra detected on CC2, power on the VCONN\n", __func__);
				VCONN_enabled = true;
				chip->vconn = VCONN_SUPPLY_YES;
			}
		}
		setDebounceVariablesCC1(CCTypeUndefined);
	}
	else																		
	{
		peekCC1Source();
		Registers.Switches.byte[0] = 0x88;										
		if(CC1TermPrevious == CCTypeRa)										
		{
			Registers.Switches.VCONN_CC1 = 1;
			
			if (!fusb_PowerVconn(true))
				printk("FUSB  [%s]: Error: Unable to power on VCONN!\n", __func__);
			else {
				printk("FUSB  [%s]: Ra detected on CC1, power on the VCONN\n", __func__);
				VCONN_enabled = true;
				chip->vconn = VCONN_SUPPLY_YES;
			}
		}
		setDebounceVariablesCC2(CCTypeUndefined);
	}
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);					
	USBPDEnable(TRUE, TRUE);													
	SinkCurrent = utccNone;														
	StateTimer = T_TIMER_DISABLE;												
	PDDebounce = tPDDebounce;												
	CCDebounce = tCCDebounce;												
	ToggleTimer = T_TIMER_DISABLE;												
	OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_SNK
void SetStateAttachedSink(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (nonCableNotify_flag) {
		printk("FUSB  [%s]: trigger Non Standard Cable Notification(0)\n", __func__);
		htc_usb_nonCable_notification(0);
		nonCable_count = 0;
	}

#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;																
	Registers.Mask.byte = 0xFF;
	Registers.Mask.M_VBUSOK = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(FALSE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);                   
	ConnState = AttachedSink;										
	sourceOrSink = Sink;
	updateSourceCurrent();
	Registers.Power.PWR = 0x7;													
	DeviceWrite(regPower, 1, &Registers.Power.byte);							
	if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))					
	{
		DetectCCPinSink();
	}
	if (blnCCPinIsCC1)															
	{
		peekCC2Sink();
		peekCC1Sink();
		Registers.Switches.byte[0] = 0x07;
	}
	else																		
	{
		peekCC1Sink();
		peekCC2Sink();
		Registers.Switches.byte[0] = 0x0B;
	}
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);					
	USBPDEnable(TRUE, FALSE);									   
	SinkCurrent = utccDefault;										
	StateTimer = T_TIMER_DISABLE;										  
	PDDebounce = tPDDebounce;								 
	CCDebounce = T_TIMER_DISABLE;									  
	ToggleTimer = T_TIMER_DISABLE;										  
	OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
	chip->pmode = PMODE_UFP;
	chip->prole = PR_SINK;
	chip->drole = DR_DEVICE;
	if (flag_DualRoleSet) {
		PortType = USBTypeC_DRP;
		blnSrcPreferred = FALSE;
		blnSnkPreferred = TRUE;
		flag_DualRoleSet = FALSE;
	}
	dual_role_instance_changed(chip->fusb_instance);
}
#endif 

#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSink(void)
{
	ConnState = AttachedSink;										
	sourceOrSink = Sink;
	updateSourceCurrent();
	if (blnCCPinIsCC1)												
	{
		
		Registers.Switches.PU_EN1 = 0;								
		Registers.Switches.PDWN1 = 1;								
	}
	else
	{
		
		Registers.Switches.PU_EN2 = 0;								
		Registers.Switches.PDWN2 = 1;								
	}
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	   
	SinkCurrent = utccNone;											
	StateTimer = T_TIMER_DISABLE;										  
	PDDebounce = tPDDebounce;								 
	CCDebounce = tCCDebounce;									  
	ToggleTimer = T_TIMER_DISABLE;
	OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSource(void)
{
    platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);          
    ConnState = AttachedSource;                                     
    TypeCSubState = 0;
    sourceOrSink = Source;
    updateSourceCurrent();
    if (blnCCPinIsCC1)                                              
    {
        Registers.Switches.PU_EN1 = 1;                              
        Registers.Switches.PDWN1 = 0;                               
        Registers.Switches.MEAS_CC1 = 1;
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else
    {
        Registers.Switches.PU_EN2 = 1;                              
        Registers.Switches.PDWN2 = 0;                               
        Registers.Switches.MEAS_CC2 = 1;
        setDebounceVariablesCC2(CCTypeUndefined);
    }
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tPDDebounce;                                
    CCDebounce = tCCDebounce;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_DRP
void SetStateTryWaitSink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    USBPDDisable(TRUE);
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = TryWaitSink;                                        
    sourceOrSink = Sink;
    updateSourceCurrent();
    Registers.Switches.byte[0] = 0x07;                               
    Registers.Power.PWR = 0x7;                                     
    resetDebounceVariables();
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    SinkCurrent = utccNone;                                         
    StateTimer = tDRPTryWait;                                       
    PDDebounce = tPDDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = tDeviceToggle;                                   
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_DRP
void SetStateTrySource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = TrySource;                                          
    sourceOrSink = Source;
    updateSourceCurrent();
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                              
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;                              
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else
    {
        peekCC1Source();
        Registers.Switches.byte[0] = 0x88;                              
        setDebounceVariablesCC2(CCTypeUndefined);
    }

    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    OverPDDebounce = T_TIMER_DISABLE;  
    StateTimer = tDRPTry;                                           
    PDDebounce = tPDDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                   
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_DRP
void SetStateTrySink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = TrySink;                                            
    sourceOrSink = Sink;
    updateSourceCurrent();
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    Registers.Switches.byte[0] = 0x07;                               
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    resetDebounceVariables();
    SinkCurrent = utccNone;                                         
    StateTimer = tDRPTry;                                           
    PDDebounce = tPDDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = tDeviceToggle;                                   
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_DRP
void SetStateTryWaitSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    updateSourceCurrent();
    ConnState = TryWaitSource;                                   
    sourceOrSink = Source;
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                              
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;                           
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else
    {
        peekCC1Source();
        Registers.Switches.byte[0] = 0x88;                           
        setDebounceVariablesCC2(CCTypeUndefined);
    }

    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    OverPDDebounce = T_TIMER_DISABLE;  
    StateTimer = tDRPTry;                                         
    PDDebounce = tPDDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                             
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_ACCMODE
void SetStateDebugAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = TRUE;                                                              
    Registers.Mask.byte = 0xFF;
    Registers.Mask.M_COMP_CHNG = 0;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(FALSE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = DebugAccessory;                                     
    sourceOrSink = Source;
    updateSourceCurrent();
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                      
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;                          
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else                                                           
    {
        peekCC1Source();
        setDebounceVariablesCC2(CCTypeUndefined);
        Registers.Switches.byte[0] = 0x88;                         
    }

    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = T_TIMER_DISABLE;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}

void SetStateAudioAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    if(alternateModes)
    {
        SetStateAlternateAudioAccessory();
        return;
    }
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = AudioAccessory;                                     
    sourceOrSink = Source;
    updateSourceCurrent();
    Registers.Power.PWR = 0x7;                                     
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                      
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;                          
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else                                                           
    {
        peekCC1Source();
        setDebounceVariablesCC2(CCTypeUndefined);
        Registers.Switches.byte[0] = 0x88;                         
    }
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    OverPDDebounce = T_TIMER_DISABLE;  
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tCCDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}

void SetStatePoweredAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = PoweredAccessory;                                   
    sourceOrSink = Source;
    Registers.Power.PWR = 0x7;
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if (SourceCurrent == utccDefault)   
    {
        Registers.Control.HOST_CUR = 0b10;
        DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);
    }
    else
    {
        updateSourceCurrent();
    }
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                      
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x64;                          
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else                                                           
    {
        blnCCPinIsCC2 = TRUE;
        peekCC1Source();
        setDebounceVariablesCC2(CCTypeUndefined);
        Registers.Switches.byte[0] = 0x98;                         
    }
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    
    

    SinkCurrent = utccNone;                                         
    StateTimer = tAMETimeout;                                       
    PDDebounce = tPDDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}

void SetStateUnsupportedAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = TRUE;                                                              
    Registers.Mask.byte = 0xFF;
    Registers.Mask.M_COMP_CHNG = 0;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(FALSE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = UnsupportedAccessory;                               
    sourceOrSink = Source;
    Registers.Control.HOST_CUR = 0b01;                              
    DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);
    Registers.Power.PWR = 0x7;
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))           
    {
        DetectCCPinSource();
    }
    if (blnCCPinIsCC1)                                      
    {
        peekCC2Source();
        Registers.Switches.byte[0] = 0x44;
        setDebounceVariablesCC1(CCTypeUndefined);
    }
    else                                                           
    {
        blnCCPinIsCC2 = TRUE;
        peekCC1Source();
        Registers.Switches.byte[0] = 0x88;
        setDebounceVariablesCC2(CCTypeUndefined);
    }
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    SinkCurrent = utccNone;                                         
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tPDDebounce;                                
    CCDebounce = T_TIMER_DISABLE;                                     
    ToggleTimer = T_TIMER_DISABLE;                                        
    OverPDDebounce = T_TIMER_DISABLE;  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

#ifdef FSC_HAVE_SRC
void SetStateUnattachedSource(void) 
{
	if(alternateModes)
	{
		SetStateAlternateUnattachedSource();
		return;
	}
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = FALSE;                                                             
    Registers.Mask.byte = 0xFF;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(TRUE);
#endif
    platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
    ConnState = UnattachedSource;                                   
    sourceOrSink = Source;
    Registers.Switches.byte[0] = 0x44;                              
    Registers.Power.PWR = 0x7;                                      
    updateSourceCurrent();                                          
    DeviceWrite(regPower, 1, &Registers.Power.byte);               
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     
    USBPDDisable(TRUE);                                            
    SinkCurrent = utccNone;
    resetDebounceVariables();
    blnCCPinIsCC1 = FALSE;                                          
    blnCCPinIsCC2 = FALSE;                                          
    StateTimer = T_TIMER_DISABLE;                                         
    PDDebounce = tPDDebounce;                                     
    CCDebounce = tCCDebounce;                                     
    ToggleTimer = tDeviceToggle;                                        
    DRPToggleTimer = tTOG2;                                             
    OverPDDebounce = tPDDebounce;                                  
#ifdef FSC_DEBUG
    WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif 
}
#endif 

void updateSourceCurrent(void)
{
    switch(SourceCurrent)
    {
        case utccDefault:
            Registers.Control.HOST_CUR = 0b01;                      
            break;
        case utcc1p5A:
            Registers.Control.HOST_CUR = 0b10;                      
            break;
        case utcc3p0A:
            Registers.Control.HOST_CUR = 0b11;                      
            break;
        default:                                                    
            Registers.Control.HOST_CUR = 0b00;                      
            break;
    }
    DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);       
}

void updateSourceMDACHigh(void)
{
    switch(SourceCurrent)
    {
        case utccDefault:
            Registers.Measure.MDAC = MDAC_1P6V;                     
            break;
        case utcc1p5A:
            Registers.Measure.MDAC = MDAC_1P6V;                     
            break;
        case utcc3p0A:
            Registers.Measure.MDAC = MDAC_2P6V;                     
            break;
        default:                                                    
            Registers.Measure.MDAC = MDAC_1P6V;                     
            break;
    }
    DeviceWrite(regMeasure, 1, &Registers.Measure.byte);           
}

void updateSourceMDACLow(void)
{
    switch(SourceCurrent)
    {
        case utccDefault:
            Registers.Measure.MDAC = MDAC_0P2V;                     
            break;
        case utcc1p5A:
            Registers.Measure.MDAC = MDAC_0P4V;                     
            break;
        case utcc3p0A:
            Registers.Measure.MDAC = MDAC_0P8V;                     
            break;
        default:                                                    
            Registers.Measure.MDAC = MDAC_1P6V;                     
            break;
    }
    DeviceWrite(regMeasure, 1, &Registers.Measure.byte);           
}


void ToggleMeasureCC1(void)
{
	if(!alternateModes)
	{
		Registers.Switches.PU_EN1 = Registers.Switches.PU_EN2;					
		Registers.Switches.PU_EN2 = 0;											
	}
	Registers.Switches.MEAS_CC1 = 1;										
	Registers.Switches.MEAS_CC2 = 0;										
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);			   

}

void ToggleMeasureCC2(void)
{
	if(!alternateModes)
	{
		Registers.Switches.PU_EN2 = Registers.Switches.PU_EN1;					
		Registers.Switches.PU_EN1 = 0;											
	}
	Registers.Switches.MEAS_CC1 = 0;										
	Registers.Switches.MEAS_CC2 = 1;										
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);			   
 }

CCTermType DecodeCCTermination(void)
{
    switch (sourceOrSink)
    {
#ifdef FSC_HAVE_SRC
        case Source:
            return DecodeCCTerminationSource();
#endif 
#ifdef FSC_HAVE_SNK
        case Sink:
            return DecodeCCTerminationSink();
#endif 
        default:
            return CCTypeUndefined;
    }
}

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(void)
{
 regMeasure_t saved_measure;
    CCTermType Termination = CCTypeUndefined;            
    saved_measure = Registers.Measure;

    updateSourceMDACHigh();
    platform_delay_10us(25);                                                              
    DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

    if (Registers.Status.COMP == 1)
    {
        Termination = CCTypeOpen;
        return Termination;
    }
    
    else if((ConnState == AttachedSource) && ((Registers.Switches.MEAS_CC1 && blnCCPinIsCC1) || (Registers.Switches.MEAS_CC2 && blnCCPinIsCC2)))
    {
        switch(SourceCurrent)
        {
            case utccDefault:
                Termination = CCTypeRdUSB;
                break;
            case utcc1p5A:
                Termination = CCTypeRd1p5;
                break;
            case utcc3p0A:
                Termination = CCTypeRd3p0;
                break;
            case utccNone:
                break;
        }
        return Termination;
    }

    updateSourceMDACLow();
    platform_delay_10us(25);                                                              
    DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

    if(Registers.Status.COMP == 0)
    {
        Termination = CCTypeRa;
    }
    else
    {
        switch(SourceCurrent)
        {
            case utccDefault:
                Termination = CCTypeRdUSB;
                break;
            case utcc1p5A:
                Termination = CCTypeRd1p5;
                break;
            case utcc3p0A:
                Termination = CCTypeRd3p0;
                break;
            case utccNone:
                break;
        }
    }
    Registers.Measure = saved_measure;
    DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
    return Termination;                             
}
#endif 

#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(void) 
{
	CCTermType Termination;
	platform_delay_10us(25);															  
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

	{
		switch (Registers.Status.BC_LVL)			
		{
			case 0b00:								
				Termination = CCTypeOpen;
				break;
			case 0b01:								
				Termination = CCTypeRdUSB;
				break;
			case 0b10:								
				Termination = CCTypeRd1p5;
				break;
			default:								
				Termination = CCTypeRd3p0;
				break;
		}
	}
	return Termination;								
}
#endif 

#ifdef FSC_HAVE_SNK
void UpdateSinkCurrent(CCTermType Termination)
{
	switch (Termination)
	{
		case CCTypeRdUSB:						
			SinkCurrent = utccDefault;
			break;
		case CCTypeRd1p5:						
			SinkCurrent = utcc1p5A;
			break;
		case CCTypeRd3p0:						
			SinkCurrent = utcc3p0A;
			break;
		default:
			SinkCurrent = utccNone;
			break;
	}
}
#endif 

#ifdef FSC_DEBUG

void ConfigurePortType(FSC_U8 Control)
{
    FSC_U8 value;
    FSC_BOOL setUnattached = FALSE;
    DisableTypeCStateMachine();
    value = Control & 0x03;
    if (PortType != value)
    {
        switch (value)
        {
            case 1:
#ifdef FSC_HAVE_SRC
                PortType = USBTypeC_Source;
#endif 
                break;
            case 2:
#ifdef FSC_HAVE_DRP
                PortType = USBTypeC_DRP;
#endif 
                break;
            default:
#ifdef FSC_HAVE_SNK
                PortType = USBTypeC_Sink;
#endif 
                break;
        }

		setUnattached = TRUE;
	}

#ifdef FSC_HAVE_ACCMODE
    if (((Control & 0x04) >> 2) != blnAccSupport)
    {
        if (Control & 0x04)
        {
            blnAccSupport = TRUE;
        }
        else
        {
            blnAccSupport = FALSE;
        }
        setUnattached = TRUE;
    }
#endif 

#ifdef FSC_HAVE_DRP
    if (((Control & 0x08) >> 3) != blnSrcPreferred)
    {
        if (Control & 0x08)
        {
            blnSrcPreferred = TRUE;
        }
        else
        {
            blnSrcPreferred = FALSE;
        }
        setUnattached = TRUE;
    }

    if (((Control & 0x40) >> 5) != blnSnkPreferred)
    {
        if (Control & 0x40)
        {
            blnSnkPreferred = TRUE;
        }
        else
        {
            blnSnkPreferred = FALSE;
        }
        setUnattached = TRUE;
    }
#endif 

    if(setUnattached)
    {
        SetStateDelayUnattached();
    }

#ifdef FSC_HAVE_SRC
    value = (Control & 0x30) >> 4;
    if (SourceCurrent != value){
        switch (value)
        {
            case 1:
                SourceCurrent = utccDefault;
                break;
            case 2:
                SourceCurrent = utcc1p5A;
                break;
            case 3:
                SourceCurrent = utcc3p0A;
                break;
            default:
                SourceCurrent = utccNone;
                break;
        }
        updateSourceCurrent();
    }
#endif 

    if (Control & 0x80)
        EnableTypeCStateMachine();
}

#ifdef FSC_HAVE_SRC
void UpdateCurrentAdvert(FSC_U8 Current)
{
    switch (Current)
    {
        case 1:
            SourceCurrent = utccDefault;
            break;
        case 2:
            SourceCurrent = utcc1p5A;
            break;
        case 3:
            SourceCurrent = utcc3p0A;
            break;
        default:
            SourceCurrent = utccNone;
            break;
    }
        updateSourceCurrent();
}
#endif 

void GetDeviceTypeCStatus(FSC_U8 abytData[])
{
    FSC_S32 intIndex = 0;
    abytData[intIndex++] = GetTypeCSMControl();     
    abytData[intIndex++] = ConnState & 0xFF;        
    abytData[intIndex++] = GetCCTermination();      
    abytData[intIndex++] = SinkCurrent;             
}

FSC_U8 GetTypeCSMControl(void)
{
    FSC_U8 status = 0;
    status |= (PortType & 0x03);            
    switch(PortType)                        
    {
#ifdef FSC_HAVE_SRC
        case USBTypeC_Source:
            status |= 0x01;                 
            break;
#endif 
#ifdef FSC_HAVE_DRP
        case USBTypeC_DRP:
            status |= 0x02;                 
            break;
#endif 
        default:                            
            break;
    }

#ifdef FSC_HAVE_ACCMODE
    if (blnAccSupport)                      
        status |= 0x04;
#endif 

#ifdef FSC_HAVE_DRP
    if (blnSrcPreferred)                    
        status |= 0x08;
    status |= (SourceCurrent << 4);
    if (blnSnkPreferred)
        status |= 0x40;
#endif 

    if (blnSMEnabled)                       
        status |= 0x80;
    return status;
}

FSC_U8 GetCCTermination(void)
{
    FSC_U8 status = 0;

			status |= (CC1TermPrevious & 0x07);			 
		
		
			status |= ((CC2TermPrevious & 0x07) << 4);	 
		
		

	return status;
}
#endif 

FSC_BOOL VbusVSafe0V (void)
{
#ifdef FPGA_BOARD
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);
	if (Registers.Status.VBUSOK) {
		return FALSE;
	} else {
		return TRUE;
	}
#else
	regSwitches_t switches;
	regSwitches_t saved_switches;

	regMeasure_t measure;
	regMeasure_t saved_measure;

    FSC_U8 val;
    FSC_BOOL ret;

	DeviceRead(regSwitches0, 1, &(switches.byte[0]));							
	saved_switches = switches;
	switches.MEAS_CC1 = 0;														
	switches.MEAS_CC2 = 0;
	DeviceWrite(regSwitches0, 1, &(switches.byte[0]));							

	DeviceRead(regMeasure, 1, &measure.byte);									
	saved_measure = measure;
	measure.MEAS_VBUS = 1;														
	measure.MDAC = VBUS_MDAC_0P8V;												
	DeviceWrite(regMeasure, 1, &measure.byte);									

	platform_delay_10us(25);													

	DeviceRead(regStatus0, 1, &val);											
	val &= 0x20;																

	if (val)	ret = FALSE;													
	else		ret = TRUE;

	DeviceWrite(regSwitches0, 1, &(saved_switches.byte[0]));					
	DeviceWrite(regMeasure, 1, &saved_measure.byte);
	platform_delay_10us(25);													
	return ret;
#endif
}

#ifdef FSC_HAVE_SNK
FSC_BOOL VbusUnder5V(void)                                                           
{
	regMeasure_t measure;

    FSC_U8 val;
    FSC_BOOL ret;

    measure = Registers.Measure;
    measure.MEAS_VBUS = 1;                                                      
    measure.MDAC = VBUS_MDAC_3p8;
    DeviceWrite(regMeasure, 1, &measure.byte);                                  

    platform_delay_10us(35);                                                    

	DeviceRead(regStatus0, 1, &val);											
	val &= 0x20;																

	if (val)	ret = FALSE;													
	else		ret = TRUE;

				   
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
	return ret;
}
#endif 

FSC_BOOL isVSafe5V(void)                                                            
{
    regMeasure_t measure;

    FSC_U8 val;
    FSC_BOOL ret;

    measure = Registers.Measure;
    measure.MEAS_VBUS = 1;                                                      
    measure.MDAC = VBUS_MDAC_4p6;
    DeviceWrite(regMeasure, 1, &measure.byte);                                  

    platform_delay_10us(35);                                                    

    DeviceRead(regStatus0, 1, &val);                                            
    val &= 0x20;                                                                

    if (val)    ret = TRUE;                                                    
    else        ret = FALSE;

                   
    DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
    return ret;
}

FSC_BOOL isVBUSOverVoltage(FSC_U8 vbusMDAC)                                                            
{
    regMeasure_t measure;

    FSC_U8 val;
    FSC_BOOL ret;

    measure = Registers.Measure;
    measure.MEAS_VBUS = 1;                                                      
    measure.MDAC = vbusMDAC;
    DeviceWrite(regMeasure, 1, &measure.byte);                                  

    platform_delay_10us(35);                                                    

    DeviceRead(regStatus0, 1, &val);                                            
    val &= 0x20;                                                                

    if (val)    ret = TRUE;                                                    
    else        ret = FALSE;

                   
    DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
    return ret;
}

void DetectCCPinSource(void)  
{
	CCTermType CCTerm;

	Registers.Switches.byte[0] = 0x44;
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm >= CCTypeRdUSB) && (CCTerm < CCTypeUndefined))
	{
		blnCCPinIsCC1 = TRUE;													
		blnCCPinIsCC2 = FALSE;
		return;
	}

	Registers.Switches.byte[0] = 0x88;
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm >= CCTypeRdUSB) && (CCTerm < CCTypeUndefined))
	{

		blnCCPinIsCC1 = FALSE;													
		blnCCPinIsCC2 = TRUE;
		return;
	}
}

void DetectCCPinSink(void)	
{
	CCTermType CCTerm;

	Registers.Switches.byte[0] = 0x07;
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm > CCTypeRa) && (CCTerm < CCTypeUndefined))
	{
		blnCCPinIsCC1 = TRUE;													
		blnCCPinIsCC2 = FALSE;
		return;
	}

	Registers.Switches.byte[0] = 0x0B;
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm > CCTypeRa) && (CCTerm < CCTypeUndefined))
	{

		blnCCPinIsCC1 = FALSE;													
		blnCCPinIsCC2 = TRUE;
		return;
	}
}

void resetDebounceVariables(void)
{
	CC1TermPrevious = CCTypeUndefined;
	CC2TermPrevious = CCTypeUndefined;
	CC1TermCCDebounce = CCTypeUndefined;
	CC2TermCCDebounce = CCTypeUndefined;
	CC1TermPDDebounce = CCTypeUndefined;
	CC2TermPDDebounce = CCTypeUndefined;
	CC1TermPDDebouncePrevious = CCTypeUndefined;
	CC2TermPDDebouncePrevious = CCTypeUndefined;
}

void setDebounceVariablesCC1(CCTermType term)
{
	CC1TermPrevious = term;
	CC1TermCCDebounce = term;
	CC1TermPDDebounce = term;
	CC1TermPDDebouncePrevious = term;

}

void setDebounceVariablesCC2(CCTermType term)
{

	CC2TermPrevious = term;
	CC2TermCCDebounce = term;
	CC2TermPDDebounce = term;
	CC2TermPDDebouncePrevious = term;
}

#ifdef FSC_DEBUG
FSC_BOOL GetLocalRegisters(FSC_U8 * data, FSC_S32 size)	
{
    if (size!=23) return FALSE;

	data[0] = Registers.DeviceID.byte;
	data[1]= Registers.Switches.byte[0];
	data[2]= Registers.Switches.byte[1];
	data[3]= Registers.Measure.byte;
	data[4]= Registers.Slice.byte;
	data[5]= Registers.Control.byte[0];
	data[6]= Registers.Control.byte[1];
	data[7]= Registers.Control.byte[2];
	data[8]= Registers.Control.byte[3];
	data[9]= Registers.Mask.byte;
	data[10]= Registers.Power.byte;
	data[11]= Registers.Reset.byte;
	data[12]= Registers.OCPreg.byte;
	data[13]= Registers.MaskAdv.byte[0];
	data[14]= Registers.MaskAdv.byte[1];
	data[15]= Registers.Control4.byte;
	data[16]=Registers.Status.byte[0];
	data[17]=Registers.Status.byte[1];
	data[18]=Registers.Status.byte[2];
	data[19]=Registers.Status.byte[3];
	data[20]=Registers.Status.byte[4];
	data[21]=Registers.Status.byte[5];
	data[22]=Registers.Status.byte[6];

	return TRUE;
}

FSC_BOOL GetStateLog(FSC_U8 * data){   
    FSC_S32 i;
    FSC_S32 entries = TypeCStateLog.Count;
    FSC_U16 state_temp;
    FSC_U16 time_tms_temp;
    FSC_U16 time_s_temp;

    for(i=0; ((i<entries) && (i<12)); i++)
    {
        ReadStateLog(&TypeCStateLog, &state_temp, &time_tms_temp, &time_s_temp);

        data[i*5+1] = state_temp;
        data[i*5+2] = (time_tms_temp>>8);
        data[i*5+3] = (FSC_U8)time_tms_temp;
        data[i*5+4] = (time_s_temp)>>8;
        data[i*5+5] = (FSC_U8)time_s_temp;
    }

    data[0] = i;    

    return TRUE;
}
#endif 

void debounceCC(void)
{
	
	CCTermType CCTermCurrent = DecodeCCTermination();					  
	if (Registers.Switches.MEAS_CC1)											
	{
		if (CC1TermPrevious != CCTermCurrent)								 
		{
			CC1TermPrevious = CCTermCurrent;								 
			PDDebounce = tPDDebounce;										
			if(OverPDDebounce == T_TIMER_DISABLE)							
			{
				OverPDDebounce = tPDDebounce;
			}
		}
	}
	else if(Registers.Switches.MEAS_CC2)										
	{
		if (CC2TermPrevious != CCTermCurrent)								 
		{
			CC2TermPrevious = CCTermCurrent;								 
			PDDebounce = tPDDebounce;										
			if(OverPDDebounce == T_TIMER_DISABLE)							
			{
				OverPDDebounce = tPDDebounce;
			}
		}
	}
	if (PDDebounce == 0)													   
	{
		CC1TermPDDebounce = CC1TermPrevious;							  
		CC2TermPDDebounce = CC2TermPrevious;
		PDDebounce = T_TIMER_DISABLE;
		OverPDDebounce = T_TIMER_DISABLE;
	}
	if (OverPDDebounce == 0)
	{
		CCDebounce = tCCDebounce;
	}

	
	if ((CC1TermPDDebouncePrevious != CC1TermPDDebounce) || (CC2TermPDDebouncePrevious != CC2TermPDDebounce))	
	{
		CC1TermPDDebouncePrevious = CC1TermPDDebounce;					  
		CC2TermPDDebouncePrevious = CC2TermPDDebounce;
		CCDebounce = tCCDebounce - tPDDebounce;							 
		CC1TermCCDebounce = CCTypeUndefined;							  
		CC2TermCCDebounce = CCTypeUndefined;
	}
	if (CCDebounce == 0)
	{
		CC1TermCCDebounce = CC1TermPDDebouncePrevious;					  
		CC2TermCCDebounce = CC2TermPDDebouncePrevious;
		CCDebounce = T_TIMER_DISABLE;
		OverPDDebounce = T_TIMER_DISABLE;
	}

	if (ToggleTimer == 0)														
	{
		if (Registers.Switches.MEAS_CC1)										
			ToggleMeasureCC2();													
		else																	
			ToggleMeasureCC1();													
		ToggleTimer = tDeviceToggle;										   
	}
}

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void peekCC1Source(void)
{
    FSC_U8 saveRegister = Registers.Switches.byte[0];                            

    Registers.Switches.byte[0] = 0x44;                                          
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
    setDebounceVariablesCC1(DecodeCCTerminationSource());

    Registers.Switches.byte[0] = saveRegister;                                  
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
}

void peekCC2Source(void)
{
    FSC_U8 saveRegister = Registers.Switches.byte[0];                            

    Registers.Switches.byte[0] = 0x88;  
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
    setDebounceVariablesCC2(DecodeCCTerminationSource());

    Registers.Switches.byte[0] = saveRegister;                                  
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
}
#endif 

#ifdef FSC_HAVE_SNK
void peekCC1Sink(void)
{
    FSC_U8 saveRegister = Registers.Switches.byte[0];                            

    Registers.Switches.byte[0] = 0x07;
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
    setDebounceVariablesCC1(DecodeCCTermination());

    Registers.Switches.byte[0] = saveRegister;                                  
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
}

void peekCC2Sink(void)
{
    FSC_U8 saveRegister = Registers.Switches.byte[0];                            

    Registers.Switches.byte[0] = 0x0B;
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
    setDebounceVariablesCC2(DecodeCCTermination());

    Registers.Switches.byte[0] = saveRegister;                                  
    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
}
#endif 

#ifdef FSC_HAVE_ACCMODE
void checkForAccessory(void)
{
	Registers.Control.TOGGLE = 0;
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);
	peekCC1Source();
	peekCC2Source();

	if((CC1TermPrevious > CCTypeOpen) && (CC2TermPrevious > CCTypeOpen))	
	{
		SetStateAttachWaitAccessory();
	}
	else																	
	{
		SetStateDelayUnattached();
	}
}
#endif 

#ifdef FSC_DEBUG
void ProcessTypeCPDStatus(FSC_U8* MsgBuffer, FSC_U8* retBuffer)
{
    if (MsgBuffer[1] != 1)                                  
        retBuffer[1] = 0x01;                         
    else
    {
        GetDeviceTypeCStatus((FSC_U8*)&retBuffer[4]); 
        GetUSBPDStatus((FSC_U8*)&retBuffer[8]);        
    }
}

void ProcessTypeCPDControl(FSC_U8* MsgBuffer, FSC_U8* retBuffer)
{
    if (MsgBuffer[1] != 0)
    {
        retBuffer[1] = 0x01;             
        return;
    }
    switch (MsgBuffer[4])                       
    {
        case 0x01:                              
            DisableTypeCStateMachine();
            EnableTypeCStateMachine();
            break;
        case 0x02:                              
            DisableTypeCStateMachine();
            break;
        case 0x03:                              
            EnableTypeCStateMachine();
            break;
        case 0x04:                              
            ConfigurePortType(MsgBuffer[5]);
            break;
#ifdef FSC_HAVE_SRC
        case 0x05:                              
            UpdateCurrentAdvert(MsgBuffer[5]);
            break;
#endif 
        case 0x06:                              
            EnableUSBPD();
            break;
        case 0x07:                              
            DisableUSBPD();
            break;
        case 0x08:                              
            SendUSBPDMessage((FSC_U8*) &MsgBuffer[5]);
            break;
#ifdef FSC_HAVE_SRC
        case 0x09:                              
            WriteSourceCapabilities((FSC_U8*) &MsgBuffer[5]);
            break;
        case 0x0A:                              
            retBuffer[4] = MsgBuffer[4]; 
            ReadSourceCapabilities((FSC_U8*) &retBuffer[5]);
            break;
#endif 
#ifdef FSC_HAVE_SNK
        case 0x0B:                              
            WriteSinkCapabilities((FSC_U8*) &MsgBuffer[5]);
            break;
        case 0x0C:                              
            retBuffer[4] = MsgBuffer[4]; 
            ReadSinkCapabilities((FSC_U8*) &retBuffer[5]);
            break;
        case 0x0D:                              
            WriteSinkRequestSettings((FSC_U8*) &MsgBuffer[5]);
            break;
        case 0x0E:                              
            retBuffer[4] = MsgBuffer[4]; 
            ReadSinkRequestSettings((FSC_U8*) &retBuffer[5]);
            break;
#endif 
        case 0x0F:                              
            retBuffer[4] = MsgBuffer[4]; 
            SendUSBPDHardReset();               
            break;

#ifdef FSC_HAVE_VDM
        case 0x10:
            retBuffer[4] = MsgBuffer[4]; 
            ConfigureVdmResponses((FSC_U8*) &MsgBuffer[5]);            
            break;
        case 0x11:
            retBuffer[4] = MsgBuffer[4]; 
            ReadVdmConfiguration((FSC_U8*) &retBuffer[5]);            
            break;
#endif 

#ifdef FSC_HAVE_DP
        case 0x12:
            retBuffer[4] = MsgBuffer[4];
            WriteDpControls((FSC_U8*) &MsgBuffer[5]);
            break;
        case 0x13:
            retBuffer[4] = MsgBuffer[4];
            ReadDpControls((FSC_U8*) &retBuffer[5]);
            break;
        case 0x14:
            retBuffer[4] = MsgBuffer[4];
            ReadDpStatus((FSC_U8*) &retBuffer[5]);
            break;
#endif 

        default:
            break;
    }
}

void ProcessLocalRegisterRequest(FSC_U8* MsgBuffer, FSC_U8* retBuffer)  
{
	if (MsgBuffer[1] != 0)
	{
		retBuffer[1] = 0x01;			 
		return;
	}

	GetLocalRegisters(&retBuffer[3], 23);  
}

void ProcessSetTypeCState(FSC_U8* MsgBuffer, FSC_U8* retBuffer)   
{
    ConnectionState state = (ConnectionState)MsgBuffer[3];

    if (MsgBuffer[1] != 0)
    {
        retBuffer[1] = 0x01;             
        return;
    }

    switch(state)
    {
        case(Disabled):
            SetStateDisabled();
            break;
        case(ErrorRecovery):
            SetStateErrorRecovery();
            break;
        case(Unattached):
            SetStateUnattached();
            break;
#ifdef FSC_HAVE_SNK
        case(AttachWaitSink):
            SetStateAttachWaitSink();
            break;
        case(AttachedSink):
            SetStateAttachedSink();
            break;
#ifdef FSC_HAVE_DRP
        case(TryWaitSink):
            SetStateTryWaitSink();
            break;
        case(TrySink):
            SetStateTrySink();
            break;
#endif 
#endif 
#ifdef FSC_HAVE_SRC
        case(AttachWaitSource):
            SetStateAttachWaitSource();
            break;
        case(AttachedSource):
            SetStateAttachedSource();
            break;
#ifdef FSC_HAVE_DRP
        case(TrySource):
            SetStateTrySource();
            break;
        case(TryWaitSource):
            SetStateTryWaitSource();
            break;
#endif 
        case(UnattachedSource):
            SetStateUnattachedSource();
            break;
#endif 
#ifdef FSC_HAVE_ACCMODE
        case(AudioAccessory):
            SetStateAudioAccessory();
            break;
        case(DebugAccessory):
            SetStateDebugAccessory();
            break;
        case(AttachWaitAccessory):
            SetStateAttachWaitAccessory();
            break;
        case(PoweredAccessory):
            SetStatePoweredAccessory();
            break;
        case(UnsupportedAccessory):
            SetStateUnsupportedAccessory();
            break;
#endif 
        case(DelayUnattached):
            SetStateDelayUnattached();
            break;
        default:
            SetStateDelayUnattached();
            break;
    }
}

void ProcessReadTypeCStateLog(FSC_U8* MsgBuffer, FSC_U8* retBuffer)
{
	if (MsgBuffer[1] != 0)
	{
		retBuffer[1] = 0x01;			 
		return;
	}

	GetStateLog(&retBuffer[3]);   
}


void setAlternateModes(FSC_U8 mode)
{
    alternateModes = mode;
}

FSC_U8 getAlternateModes(void)
{
	return alternateModes;
}

#endif 
