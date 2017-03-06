/****************************************************************************
 * FileName:        PDPolicy.c
 * Processor:       PIC32MX250F128B
 * Compiler:        MPLAB XC32
 * Company:         Fairchild Semiconductor
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

#ifdef FSC_DEBUG
#include "Log.h"
#endif 

#include "PD_Types.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "TypeC.h"
#include "fusb30X.h"

#ifdef FSC_HAVE_VDM
#include "vdm/vdm_callbacks.h"
#include "vdm/vdm_callbacks_defs.h"
#include "vdm/vdm.h"
#include "vdm/vdm_types.h"
#include "vdm/bitfield_translators.h"
#include "vdm/DisplayPort/dp_types.h"
#include "vdm/DisplayPort/dp.h"
#include "vdm/DisplayPort/interface_dp.h"
#endif 


extern int fusb_debug_level;

#ifdef FSC_DEBUG
StateLog                         PDStateLog;                                     
extern volatile FSC_U16          Timer_S;                                        
extern volatile FSC_U16          Timer_tms;                                      

extern FSC_U8                    manualRetries;                                  
extern FSC_U8                    nTries;                                         
#endif 

extern FSC_BOOL                  g_Idle;                                         

FSC_BOOL                        USBPDTxFlag;                                    
FSC_BOOL                        IsHardReset;                                    
FSC_BOOL                        IsPRSwap;                                       
FSC_BOOL                        IsVCONNSource;                                  
sopMainHeader_t                 PDTransmitHeader;                               

#ifdef FSC_HAVE_SNK
sopMainHeader_t                 CapsHeaderSink;                                 
doDataObject_t                  CapsSink[7];                                    
doDataObject_t                  SinkRequest;                                    
sopMainHeader_t                 PDTransmitHeader = {0};                               
sopMainHeader_t                 CapsHeaderSink = {0};                                 
sopMainHeader_t                 CapsHeaderSource = {0};                               
sopMainHeader_t                 CapsHeaderReceived = {0};                             
doDataObject_t                  PDTransmitObjects[7] = {{0}};                           
doDataObject_t                  CapsSink[7] = {{0}};                                    
doDataObject_t                  CapsSource[7] = {{0}};                                  
doDataObject_t                  CapsReceived[7] = {{0}};                                
doDataObject_t                  USBPDContract = {0};                                  
doDataObject_t                  SinkRequest = {0};                                    
FSC_U32                         SinkRequestMaxVoltage;                          
FSC_U32                         SinkRequestMaxPower;                            
FSC_U32                         SinkRequestOpPower;                             
FSC_BOOL                        SinkGotoMinCompatible;                          
FSC_BOOL                        SinkUSBSuspendOperation;                        
FSC_BOOL                        SinkUSBCommCapable;                             
#endif 

doDataObject_t                  PartnerCaps = {0};                              

#ifdef FSC_HAVE_SRC
sopMainHeader_t                 CapsHeaderSource;                               
doDataObject_t                  CapsSource[7];                                  
#endif 

#ifdef FSC_DEBUG
FSC_BOOL                        SourceCapsUpdated;                              
#endif 

sopMainHeader_t                 CapsHeaderReceived;                             
doDataObject_t                  PDTransmitObjects[7];                           
doDataObject_t                  CapsReceived[7];                                
doDataObject_t                  USBPDContract;                                  

       PolicyState_t            PolicyState;                                    
       PolicyState_t            LastPolicyState;                                
       FSC_U8                   PolicySubIndex;                                 
       FSC_BOOL                 PolicyIsSource;                                 
       FSC_BOOL                 PolicyIsDFP;                                    
       FSC_BOOL                 PolicyHasContract;                              
       FSC_U32                  VbusTransitionTime;                             
static FSC_U8                   CollisionCounter;                               
static FSC_U8                   HardResetCounter;                               
static FSC_U8                   CapsCounter;                                    
       FSC_U32                  PolicyStateTimer;                               
       FSC_U32                  NoResponseTimer;                                
static FSC_U32                  SwapSourceStartTimer;                           
       sopMainHeader_t          PolicyRxHeader = {0};                           
       sopMainHeader_t          PolicyTxHeader = {0};                           
       doDataObject_t           PolicyRxDataObj[7] = {{0}};                     
       doDataObject_t           PolicyTxDataObj[7] = {{0}};                     
static FSC_BOOL                 isContractValid;                                

#ifdef FSC_HAVE_VDM
extern VdmManager               vdmm;
VdmDiscoveryState_t             AutoVdmState;

FSC_U32                         vdm_msg_length;
doDataObject_t                  vdm_msg_obj[7] = {{0}};
PolicyState_t                   vdm_next_ps;
FSC_BOOL                        sendingVdmData;

FSC_U32                         VdmTimer;
FSC_BOOL                        VdmTimerStarted;

FSC_U16                         auto_mode_disc_tracker;

extern FSC_BOOL                 mode_entered;
extern SvidInfo                 core_svid_info;
#endif 

#ifdef FSC_HAVE_DP
extern FSC_U32                  DpModeEntered;
extern FSC_S32                  AutoDpModeEntryObjPos;
#endif 

extern FSC_BOOL                 ProtocolCheckRxBeforeTx;

void PolicyTickAt100us( void )
{
    if( !USBPDActive )
        return;

    if (PolicyStateTimer)                                                       
        PolicyStateTimer--;                                                     
    if ((NoResponseTimer < T_TIMER_DISABLE) && (NoResponseTimer > 0))           
        NoResponseTimer--;                                                      
    if (SwapSourceStartTimer)
        SwapSourceStartTimer--;

#ifdef FSC_HAVE_VDM
    if (VdmTimer)
        VdmTimer--;
#endif 
}

void InitializePDPolicyVariables(void)
{
    SwapSourceStartTimer = 0;

#ifdef FSC_HAVE_SNK
    SinkRequestMaxVoltage = 240;                                                
    SinkRequestMaxPower = 1000;                                                 
    SinkRequestOpPower = 1000;                                                  
    SinkGotoMinCompatible = FALSE;                                              
    SinkUSBSuspendOperation = FALSE;                                            
    SinkUSBCommCapable = FALSE;                                                 

    CapsHeaderSink.NumDataObjects = 2;                                          
    CapsHeaderSink.PortDataRole = 0;                                            
    CapsHeaderSink.PortPowerRole = 0;                                           
    CapsHeaderSink.SpecRevision = 1;                                            
    CapsHeaderSink.Reserved0 = 0;
    CapsHeaderSink.Reserved1 = 0;
    CapsSink[0].FPDOSink.Voltage = 100;                                         
    CapsSink[0].FPDOSink.OperationalCurrent = 10;                               
    CapsSink[0].FPDOSink.DataRoleSwap = 1;                                      
    CapsSink[0].FPDOSink.USBCommCapable = 0;                                    
    CapsSink[0].FPDOSink.ExternallyPowered = 1;                                 
    CapsSink[0].FPDOSink.HigherCapability = FALSE;                              
    CapsSink[0].FPDOSink.DualRolePower = 1;                                     
    CapsSink[0].FPDOSink.Reserved = 0;

    CapsSink[1].FPDOSink.Voltage = 240;                                         
    CapsSink[1].FPDOSink.OperationalCurrent = 10;                               
    CapsSink[1].FPDOSink.DataRoleSwap = 0;                                      
    CapsSink[1].FPDOSink.USBCommCapable = 0;                                    
    CapsSink[1].FPDOSink.ExternallyPowered = 0;                                 
    CapsSink[1].FPDOSink.HigherCapability = 0;                                  
    CapsSink[1].FPDOSink.DualRolePower = 0;                                     
#endif 

#ifdef FSC_HAVE_SRC

#ifdef FM150911A
    CapsHeaderSource.NumDataObjects = 2;                                        
#else
    CapsHeaderSource.NumDataObjects = 1;                                        
#endif 
    CapsHeaderSource.PortDataRole = 0;                                          
    CapsHeaderSource.PortPowerRole = 1;                                         
    CapsHeaderSource.SpecRevision = 1;                                          
    CapsHeaderSource.Reserved0 = 0;
    CapsHeaderSource.Reserved1 = 0;

    CapsSource[0].FPDOSupply.Voltage = 100;                                     
    CapsSource[0].FPDOSupply.MaxCurrent = 150;                                  
    CapsSource[0].FPDOSupply.PeakCurrent = 0;                                   
    CapsSource[0].FPDOSupply.DataRoleSwap = TRUE;                               
    CapsSource[0].FPDOSupply.USBCommCapable = FALSE;                            
    CapsSource[0].FPDOSupply.ExternallyPowered = TRUE;                          
    CapsSource[0].FPDOSupply.USBSuspendSupport = FALSE;                         
    CapsSource[0].FPDOSupply.DualRolePower = TRUE;                              
    CapsSource[0].FPDOSupply.SupplyType = 0;                                    
    CapsSource[0].FPDOSupply.Reserved = 0;                                      

#ifdef FM150911A
    CapsSource[1].FPDOSupply.Voltage = 240;                                     
    CapsSource[1].FPDOSupply.MaxCurrent = 150;                                  
    CapsSource[1].FPDOSupply.PeakCurrent = 0;                                   
    CapsSource[1].FPDOSupply.DataRoleSwap = 0;                                  
    CapsSource[1].FPDOSupply.USBCommCapable = 0;                                
    CapsSource[1].FPDOSupply.ExternallyPowered = 0;                             
    CapsSource[1].FPDOSupply.USBSuspendSupport = 0;                             
    CapsSource[1].FPDOSupply.DualRolePower = 0;                                 
    CapsSource[1].FPDOSupply.SupplyType = 0;                                    
#endif 

    CapsSource[0].FPDOSupply.Reserved = 0;                                      
#endif 

#ifdef FSC_DEBUG
    SourceCapsUpdated = FALSE;                                                  
#endif 

    VbusTransitionTime = 20 * TICK_SCALE_TO_MS;                                 

#ifdef FSC_HAVE_VDM
    InitializeVdmManager();                                                
    vdmInitDpm();
    AutoVdmState = AUTO_VDM_INIT;
    auto_mode_disc_tracker = 0;
#endif 

#ifdef FSC_HAVE_DP
    AutoDpModeEntryObjPos = -1;
#endif 

    ProtocolCheckRxBeforeTx = FALSE;
    isContractValid = FALSE;

    Registers.Slice.SDAC = SDAC_DEFAULT;                                        
    Registers.Slice.SDAC_HYS = 0b01;                                            
    DeviceWrite(regSlice, 1, &Registers.Slice.byte);

#ifdef FSC_DEBUG
    InitializeStateLog(&PDStateLog);
#endif 
}


void USBPDEnable(FSC_BOOL DeviceUpdate, FSC_BOOL TypeCDFP)
{
    FSC_U8 data[5];
    IsHardReset = FALSE;
    IsPRSwap = FALSE;
    HardResetCounter = 0;

	if (fusb_debug_level >= 0)
		printk("FUSB  [%s] - USBPDEnable(%d,%d), PDflag -> USBPDEnabled:%d\n",
			__func__, DeviceUpdate, TypeCDFP, USBPDEnabled);

    if (USBPDEnabled == TRUE)
    {
        if (blnCCPinIsCC1) {                                                    
            Registers.Switches.TXCC1 = 1;                                    
            Registers.Switches.MEAS_CC1 = 1;

            Registers.Switches.TXCC2 = 0;                                   
            Registers.Switches.MEAS_CC2 = 0;
        }
        else if (blnCCPinIsCC2) {                                               
            Registers.Switches.TXCC2 = 1;                                    
            Registers.Switches.MEAS_CC2 = 1;

            Registers.Switches.TXCC1 = 0;                                   
            Registers.Switches.MEAS_CC1 = 0;
        }
        if (blnCCPinIsCC1 || blnCCPinIsCC2)                                     
        {
            USBPDActive = TRUE;                                                 
            ResetProtocolLayer(FALSE);                                          
            NoResponseTimer = T_TIMER_DISABLE;                                  
            PolicyIsSource = TypeCDFP;                                          
            PolicyIsDFP = TypeCDFP;
            IsVCONNSource = TypeCDFP;
            
            if (PolicyIsSource)                                                 
            {
                PolicyState = peSourceStartup;                                  
                PolicySubIndex = 0;
                LastPolicyState = peDisabled;
                Registers.Switches.POWERROLE = 1;                               
                Registers.Switches.DATAROLE = 1;                                

            }
            else                                                                
            {
                PolicyState = peSinkStartup;                                    
                PolicySubIndex = 0;
                PolicyStateTimer =0;
                LastPolicyState = peDisabled;
                Registers.Switches.POWERROLE = 0;                               
                Registers.Switches.DATAROLE = 0;                                

                Registers.Control.ENSOP1 = 0;
                Registers.Control.ENSOP1DP = 0;
                Registers.Control.ENSOP2 = 0;
                Registers.Control.ENSOP2DB = 0;
            }
            Registers.Switches.AUTO_CRC = 0;                                    
            Registers.Power.PWR |= 0x8;                                         
            Registers.Control.AUTO_PRE = 0;                                     
            Registers.Control.N_RETRIES = 3;                                    
#ifdef FSC_DEBUG
            if(manualRetries)
            {
                Registers.Control.N_RETRIES = 0;                                
            }
#endif 

            Registers.Control.AUTO_RETRY = 1;                                   

            data[0] = Registers.Slice.byte;                                     
            data[1] = Registers.Control.byte[0] | 0x40;                         
            data[2] = Registers.Control.byte[1] | 0x04;                         
            data[3] = Registers.Control.byte[2];
            data[4] = Registers.Control.byte[3];
            
            DeviceWrite(regControl0, 4, &data[1]);
            if (DeviceUpdate)
            {
                DeviceWrite(regPower, 1, &Registers.Power.byte);                
                DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);      
            }

#ifdef FSC_DEBUG
            StoreUSBPDToken(TRUE, pdtAttach);                                   
#endif 
        }

#ifdef FSC_INTERRUPT_TRIGGERED
        g_Idle = FALSE;                                                         
        Registers.Mask.byte = 0xFF;
        Registers.Mask.M_COLLISION = 0;
        DeviceWrite(regMask, 1, &Registers.Mask.byte);
        Registers.MaskAdv.byte[0] = 0xFF;
        Registers.MaskAdv.M_RETRYFAIL = 0;
        Registers.MaskAdv.M_TXCRCSENT = 0;
        Registers.MaskAdv.M_HARDRST = 0;
        DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
        Registers.MaskAdv.M_GCRCSENT = 0;
        DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
        platform_enable_timer(TRUE);
#endif  
    }
}

void USBPDDisable(FSC_BOOL DeviceUpdate)
{
    IsHardReset = FALSE;
#ifdef FSC_DEBUG
    if (USBPDActive == TRUE)                                                    
        StoreUSBPDToken(TRUE, pdtDetach);                                       
    SourceCapsUpdated = TRUE;                                                   
#endif 
    USBPDActive = FALSE;                                                        
    ProtocolState = PRLDisabled;                                                
    PolicyState = peDisabled;                                                   
    PDTxStatus = txIdle;                                                        
    PolicyIsSource = FALSE;                                                     
    PolicyHasContract = FALSE;                                                  

    if (DeviceUpdate)
    {
        Registers.Switches.TXCC1 = 0;                                           
        Registers.Switches.TXCC2 = 0;
        Registers.Switches.AUTO_CRC = 0;                                        
        Registers.Power.PWR &= 0x7;                                             

        DeviceWrite(regPower, 1, &Registers.Power.byte);                       
        DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);             
    }
    ProtocolFlushRxFIFO();
    ProtocolFlushTxFIFO();
}




void USBPDPolicyEngine(void)
{
#ifdef FSC_DEBUG
    if (LastPolicyState != PolicyState)                                    
    {
        WriteStateLog(&PDStateLog, PolicyState, Timer_tms, Timer_S);
    }
#endif 

    LastPolicyState = PolicyState;

    switch (PolicyState)
    {
        case peDisabled:
            break;
        case peErrorRecovery:
            PolicyErrorRecovery();
            break;
        
#ifdef FSC_HAVE_SRC
        case peSourceSendHardReset:
            PolicySourceSendHardReset();
            break;
        case peSourceSendSoftReset:
            PolicySourceSendSoftReset();
            break;
        case peSourceSoftReset:
            PolicySourceSoftReset();
            break;
        case peSourceStartup:
            PolicySourceStartup();
            break;
        case peSourceDiscovery:
            PolicySourceDiscovery();
            break;
        case peSourceSendCaps:
            PolicySourceSendCaps();
            break;
        case peSourceDisabled:
            PolicySourceDisabled();
            break;
        case peSourceTransitionDefault:
            PolicySourceTransitionDefault();
            break;
        case peSourceNegotiateCap:
            PolicySourceNegotiateCap();
            break;
        case peSourceCapabilityResponse:
            PolicySourceCapabilityResponse();
            break;
        case peSourceTransitionSupply:
            PolicySourceTransitionSupply();
            break;
        case peSourceReady:
            PolicySourceReady();
            break;
        case peSourceGiveSourceCaps:
            PolicySourceGiveSourceCap();
            break;
        case peSourceGetSinkCaps:
            PolicySourceGetSinkCap();
            break;
        case peSourceSendPing:
            PolicySourceSendPing();
            break;
        case peSourceGotoMin:
            PolicySourceGotoMin();
            break;
        case peSourceGiveSinkCaps:
            PolicySourceGiveSinkCap();
            break;
        case peSourceGetSourceCaps:
            PolicySourceGetSourceCap();
            break;
        case peSourceSendDRSwap:
            PolicySourceSendDRSwap();
            break;
        case peSourceEvaluateDRSwap:
            PolicySourceEvaluateDRSwap();
            break;
        case peSourceSendVCONNSwap:
            PolicySourceSendVCONNSwap();
            break;
        case peSourceSendPRSwap:
            PolicySourceSendPRSwap();
            break;
        case peSourceEvaluatePRSwap:
            PolicySourceEvaluatePRSwap();
            break;
        case peSourceWaitNewCapabilities:
            PolicySourceWaitNewCapabilities();
            break;
        case peSourceEvaluateVCONNSwap:
            PolicySourceEvaluateVCONNSwap();
            break;
#endif 
        
#ifdef FSC_HAVE_SNK
        case peSinkStartup:
            PolicySinkStartup();
            break;
        case peSinkSendHardReset:
            PolicySinkSendHardReset();
            break;
        case peSinkSoftReset:
            PolicySinkSoftReset();
            break;
        case peSinkSendSoftReset:
            PolicySinkSendSoftReset();
            break;
        case peSinkTransitionDefault:
            PolicySinkTransitionDefault();
            break;
        case peSinkDiscovery:
            PolicySinkDiscovery();
            break;
        case peSinkWaitCaps:
            PolicySinkWaitCaps();
            break;
        case peSinkEvaluateCaps:
            PolicySinkEvaluateCaps();
            break;
        case peSinkSelectCapability:
            PolicySinkSelectCapability();
            break;
        case peSinkTransitionSink:
            PolicySinkTransitionSink();
            break;
        case peSinkReady:
            PolicySinkReady();
            break;
        case peSinkGiveSinkCap:
            PolicySinkGiveSinkCap();
            break;
        case peSinkGetSourceCap:
            PolicySinkGetSourceCap();
            break;
        case peSinkGetSinkCap:
            PolicySinkGetSinkCap();
            break;
        case peSinkGiveSourceCap:
            PolicySinkGiveSourceCap();
            break;
        case peSinkSendDRSwap:
            PolicySinkSendDRSwap();
            break;
        case peSinkEvaluateDRSwap:
            PolicySinkEvaluateDRSwap();
            break;
        case peSinkEvaluateVCONNSwap:
            PolicySinkEvaluateVCONNSwap();
            break;
        case peSinkSendPRSwap:
            PolicySinkSendPRSwap();
            break;
        case peSinkEvaluatePRSwap:
            PolicySinkEvaluatePRSwap();
            break;
#endif 

#ifdef FSC_HAVE_VDM
        case peGiveVdm:
            PolicyGiveVdm();
            break;
#endif 

        
        case PE_BIST_Receive_Mode:      
            policyBISTReceiveMode();
            break;
        case PE_BIST_Frame_Received:    
            policyBISTFrameReceived();
            break;

        
        case PE_BIST_Carrier_Mode_2:     
            policyBISTCarrierMode2();
            break;

        case PE_BIST_Test_Data:
            policyBISTTestData();
            break;

        default:
#ifdef FSC_HAVE_VDM
            if ((PolicyState >= FIRST_VDM_STATE) && (PolicyState <= LAST_VDM_STATE) ) {
                
                PolicyVdm();
            } else
#endif 
            {
                
                PolicyInvalidState();
            }
            break;
    }

    USBPDTxFlag = FALSE;                                                        
}


void PolicyErrorRecovery(void)
{
    SetStateErrorRecovery();
}

#ifdef FSC_HAVE_SRC
void PolicySourceSendHardReset(void)
{
    PolicySendHardReset(peSourceTransitionDefault, 0);
}

void PolicySourceSoftReset(void)
{
    PolicySendCommand(CMTAccept, peSourceSendCaps, 0);
}

void PolicySourceSendSoftReset(void)
{
#ifdef FSC_DEBUG
    if (manualRetries && nTries != 4)
    {
        nTries = 4;                                                       
    }
    else
#endif 
    if (Registers.Control.N_RETRIES == 0)
    {
        Registers.Control.N_RETRIES = 3;                                    
        DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
    }

    switch (PolicySubIndex)
    {
        case 0:
            if (PolicySendCommand(CMTSoftReset, peSourceSendSoftReset, 1) == STAT_SUCCESS) 
                PolicyStateTimer = tSenderResponse;                             
            break;
        default:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTAccept))  
                {
                    PolicyState = peSourceSendCaps;                             
                }
                else                                                            
                    PolicyState = peSourceSendHardReset;                        
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceSendHardReset;                            
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
}

void PolicySourceStartup(void)
{
#ifdef FSC_HAVE_VDM
    FSC_S32 i;
#endif 

           
    Registers.Mask.byte = 0x00;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0x00;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 0;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    if(Registers.Control.RX_FLUSH == 1)                                         
    {
        Registers.Control.RX_FLUSH = 0;
        DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
    }

#ifdef FSC_DEBUG
    if (manualRetries && nTries != 1)
    {
        nTries = 1;                                                             
    }
    else
#endif 
    if (Registers.Control.N_RETRIES != 0)
    {
        Registers.Control.N_RETRIES = 0;                                        
        DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
    }

    switch (PolicySubIndex)
    {
        case 0:
            USBPDContract.object = 0;                                           
            PartnerCaps.object = 0;                                             
            IsPRSwap = FALSE;
            PolicyIsSource = TRUE;                                              
            Registers.Switches.POWERROLE = PolicyIsSource;
            DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
            ResetProtocolLayer(TRUE);                                           
            PRSwapTimer = 0;                                                    
            CapsCounter = 0;                                                    
            CollisionCounter = 0;                                               
            PolicyStateTimer = 1500;                                            
            PolicySubIndex++;
            break;
        case 1:
            if ((isVBUSOverVoltage(VBUS_MDAC_4p6) && (SwapSourceStartTimer == 0)) || (PolicyStateTimer == 0))                     
            {
                PolicySubIndex++;
            }
            break;
        case 2:
            PolicyStateTimer = 0;                                                       
            PolicyState = peSourceSendCaps;                                             
            PolicySubIndex = 0;                                                         

#ifdef FSC_HAVE_VDM
            AutoVdmState = AUTO_VDM_INIT;

            mode_entered = FALSE;

            auto_mode_disc_tracker = 0;

            core_svid_info.num_svids = 0;
            for (i = 0; i < MAX_NUM_SVIDS; i++) {
                core_svid_info.svids[i] = 0;
            }
#endif 

#ifdef FSC_HAVE_DP
            AutoDpModeEntryObjPos = -1;

            resetDp();
#endif 

            break;
        default:
            PolicySubIndex = 0;
            break;
    }
}

void PolicySourceDiscovery(void)
{
    switch (PolicySubIndex)
    {
        case 0:
            PolicyStateTimer = tTypeCSendSourceCap;                             
            PolicySubIndex++;                                                   
            break;
        default:
            if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0) && (PolicyHasContract == TRUE))
            {                                                                   
                PolicyState = peErrorRecovery;                                  
                PolicySubIndex = 0;
            }
            else if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0) && (PolicyHasContract == FALSE))
            {                                                                   
                    PolicyState = peSourceDisabled;                             
                    PolicySubIndex = 0;                                             
            }
            if (PolicyStateTimer == 0)                                          
            {
                if (CapsCounter > nCapsCount)                                   
                    PolicyState = peSourceDisabled;                             
                else                                                            
                    PolicyState = peSourceSendCaps;                             
                PolicySubIndex = 0;                                             
            }
            break;
    }
}

void PolicySourceSendCaps(void)
{
#ifdef FSC_DEBUG
    if (manualRetries && nTries != 1)                                           
    {
        nTries = 1;                                                       
    }
    else
#endif 
    if (Registers.Control.N_RETRIES != 0)
    {
        Registers.Control.N_RETRIES = 0;                                    
        DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
    }

    if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0))         
    {
        if (PolicyHasContract)                                                  
            PolicyState = peErrorRecovery;                                      
        else                                                                    
            PolicyState = peSourceDisabled;                                     
    }
    else                                                                        
    {
        switch (PolicySubIndex)
        {
            case 0:
                if (PolicySendData(DMTSourceCapabilities, CapsHeaderSource.NumDataObjects, &CapsSource[0], peSourceSendCaps, 1, SOP_TYPE_SOP) == STAT_SUCCESS)
                {
                    HardResetCounter = 0;                                       
                    CapsCounter = 0;                                            
                    NoResponseTimer = T_TIMER_DISABLE;                          
                    
                    PolicyStateTimer = tSenderResponse-25;                     
                }
                break;
            default:
#ifdef FSC_DEBUG
                if (manualRetries && nTries != 4)
                {
                    nTries = 4;                                                       
                }
                else
#endif 
                if (Registers.Control.N_RETRIES == 0)
                {
                    Registers.Control.N_RETRIES = 3;                                    
                    DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
                }

                if (ProtocolMsgRx)                                              
                {
                    ProtocolMsgRx = FALSE;                                      
                    if ((PolicyRxHeader.NumDataObjects == 1) && (PolicyRxHeader.MessageType == DMTRequest)) 
                        PolicyState = peSourceNegotiateCap;                     
                    else                                                        
                        PolicyState = peSourceSendSoftReset;                    
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                }
                else if (!PolicyStateTimer)                                     
                {
                    ProtocolMsgRx = FALSE;                                      
                    PolicyState = peSourceSendHardReset;                        
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                }
                break;
        }
    }
}

void PolicySourceDisabled(void)
{
    USBPDContract.object = 0;                                                   
    
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = TRUE;                                                              
    Registers.Mask.byte = 0xFF;
    Registers.Mask.M_COMP_CHNG = 0;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    Registers.MaskAdv.M_HARDRST = 0;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 0;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(FALSE);
#endif  
}

void PolicySourceTransitionDefault(void)
{
    switch (PolicySubIndex)
    {
        case 0:
            if(PolicyStateTimer == 0)
            {
                PolicySubIndex++;
            }
            break;
        case 1:
            platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);           
            platform_set_vbus_discharge(TRUE);                                  
            if(!PolicyIsDFP)                                                    
            {
                PolicyIsDFP = TRUE;;                                            
                Registers.Switches.DATAROLE = PolicyIsDFP;                      
                DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);      
            }
            if(IsVCONNSource)                                                   
            {
                Registers.Switches.VCONN_CC1 = 0;
                Registers.Switches.VCONN_CC2 = 0;
                DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
            }
            PolicySubIndex++;
            
            break;
        case 2:
            if(VbusVSafe0V())                                                   
            {
                platform_set_vbus_discharge(FALSE);                             
                PolicyStateTimer = tSrcRecover;
                PolicySubIndex++;
            }
            break;
        case 3:
            if(PolicyStateTimer == 0)                                           
            {
                PolicySubIndex++;
            }
            break;
        default:
                platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, FALSE);         
                if(blnCCPinIsCC1)
                {
                    Registers.Switches.VCONN_CC2 = 1;
                }
                else
                {
                    Registers.Switches.VCONN_CC1 = 1;
                }
                DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
                IsVCONNSource = TRUE;
                NoResponseTimer = tNoResponse;                                  
                PolicyState = peSourceStartup;                                  
                PolicySubIndex = 0;
            break;
    }
}

void PolicySourceNegotiateCap(void)
{
    
    FSC_BOOL reqAccept = FALSE;                                                     
    FSC_U8 objPosition;                                                          
    objPosition = PolicyRxDataObj[0].FVRDO.ObjectPosition;                      
    if ((objPosition > 0) && (objPosition <= CapsHeaderSource.NumDataObjects))  
    {
        if ((PolicyRxDataObj[0].FVRDO.OpCurrent <= CapsSource[objPosition-1].FPDOSupply.MaxCurrent)) 
            reqAccept = TRUE;                                                   
    }
    if (reqAccept)                                                              
    {
        PolicyState = peSourceTransitionSupply;                                 

    }
    else                                                                        
        PolicyState = peSourceCapabilityResponse;                               
}

void PolicySourceTransitionSupply(void)
{
    FSC_U8 sourceVoltage = 0;
    switch (PolicySubIndex)
    {
        case 0:
            PolicySendCommand(CMTAccept, peSourceTransitionSupply, 1);          
            break;
        case 1:
            PolicyStateTimer = tSrcTransition;                                  
            PolicySubIndex++;                                                   
            break;
        case 2:
            if (!PolicyStateTimer)                                              
                PolicySubIndex++;                                               
            break;
        case 3:
            PolicyHasContract = TRUE;                                           
            USBPDContract.object = PolicyRxDataObj[0].object;                   

            
            sourceVoltage = CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage;
            if (sourceVoltage == 100)   
            {
                if(platform_get_vbus_lvl_enable(VBUS_LVL_5V))                   
                {
                    PolicySubIndex = 5;
                }
                else
                {
                    platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, FALSE);
                    PolicyStateTimer = t5To12VTransition;                       
                    PolicySubIndex++;
                }
            }
#ifdef FM150911A
            else if (sourceVoltage == 240)  
            {
                if(platform_get_vbus_lvl_enable(VBUS_LVL_12V))                  
                {
                    PolicySubIndex = 5;
                }
                else
                {
                    platform_set_vbus_lvl_enable(VBUS_LVL_12V, TRUE, FALSE);
                    PolicyStateTimer = t5To12VTransition;                       
                    PolicySubIndex++;
                }
            }
#endif 
            else                                                                
            {
                if(platform_get_vbus_lvl_enable(VBUS_LVL_5V))                   
                {
                    PolicySubIndex = 5;
                }
                else
                {
                    platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, FALSE);
                    PolicyStateTimer = t5To12VTransition;                       
                    PolicySubIndex++;
                }
            }
            break;
        case 4:
            
            if (PolicyStateTimer == 0)
            {
                if(CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage == 100)
                {
                    platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);      
                }
#ifdef FM150911A
                else if(CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage == 240)
                {
                    platform_set_vbus_lvl_enable(VBUS_LVL_12V, TRUE, TRUE);     
                }
#endif 
                else
                {
                    platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);      
                }

                PolicyStateTimer = 3500;                                        
                PolicySubIndex++;                                               
            }
            break;
        default:
            if(CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage == 100)
            {
                if((!isVBUSOverVoltage(VBUS_MDAC_5p04) && isVBUSOverVoltage(VBUS_MDAC_4p6)) || (PolicyStateTimer == 0))   
                {
                    PolicySendCommand(CMTPS_RDY, peSourceReady, 0);                     
                }
            }
#ifdef FM150911A
            else if(CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage == 240)
            {
                if((isVBUSOverVoltage(VBUS_MDAC_11p8))  || (PolicyStateTimer == 0))                           
                {
                    PolicySendCommand(CMTPS_RDY, peSourceReady, 0);             
                }
            }
#endif 
            else if(PolicyStateTimer == 0)
            {
                PolicySendCommand(CMTPS_RDY, peSourceReady, 0);
            }
            break;
    }
}

void PolicySourceCapabilityResponse(void)
{
    if (PolicyHasContract)                                                      
    {
        if(isContractValid)
        {
            PolicySendCommand(CMTReject, peSourceReady, 0);                     
        }
        else
        {
            PolicySendCommand(CMTReject, peSourceSendHardReset, 0);                     
        }
    }
    else                                                                        
    {
        PolicySendCommand(CMTReject, peSourceWaitNewCapabilities, 0);           
    }
}

void PolicySourceReady(void)
{
    if (ProtocolMsgRx)                                                          
    {
        ProtocolMsgRx = FALSE;                                                  
        if (PolicyRxHeader.NumDataObjects == 0)                                 
        {
            switch (PolicyRxHeader.MessageType)                                 
            {
                case CMTGetSourceCap:
                    PolicyState = peSourceGiveSourceCaps;                       
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTGetSinkCap:                                             
                    PolicyState = peSourceGiveSinkCaps;                         
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTDR_Swap:                                                
                    PolicyState = peSourceEvaluateDRSwap;                       
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTPR_Swap:
                    PolicyState = peSourceEvaluatePRSwap;                       
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTVCONN_Swap:                                             
                    PolicyState = peSourceEvaluateVCONNSwap;                    
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTSoftReset:
                    PolicyState = peSourceSoftReset;                            
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                default:                                                        
                    break;
            }
        }
        else                                                                    
        {
            switch (PolicyRxHeader.MessageType)
            {
                case DMTRequest:
                    PolicyState = peSourceNegotiateCap;                         
                    break;
#ifdef FSC_HAVE_VDM
                case DMTVenderDefined:
                    convertAndProcessVdmMessage(ProtocolMsgRxSop);
                    break;
#endif 
                case DMTBIST:
                    processDMTBIST();
                    break;
                default:                                                        
                    break;
            }
            PolicySubIndex = 0;                                                 
            PDTxStatus = txIdle;                                                
        }
    }
    else if (USBPDTxFlag)                                                       
    {
        if (PDTransmitHeader.NumDataObjects == 0)
        {
            switch (PDTransmitHeader.MessageType)                               
            {
                case CMTGetSinkCap:
                    PolicyState = peSourceGetSinkCaps;                          
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTGetSourceCap:
                    PolicyState = peSourceGetSourceCaps;                        
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTPing:
                    PolicyState = peSourceSendPing;                             
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTGotoMin:
                    PolicyState = peSourceGotoMin;                              
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
#ifdef FSC_HAVE_DRP
                case CMTPR_Swap:
                    PolicyState = peSourceSendPRSwap;                       
                    PolicySubIndex = 0;                                     
                    PDTxStatus = txIdle;                                    
                    break;
#endif 
                case CMTDR_Swap:
                    PolicyState = peSourceSendDRSwap;                           
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTVCONN_Swap:
                    PolicyState = peSourceSendVCONNSwap;                        
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTSoftReset:
                    PolicyState = peSourceSendSoftReset;                        
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                default:                                                        
                    break;
            }
        }
        else
        {
            switch (PDTransmitHeader.MessageType)
            {
                case DMTSourceCapabilities:
                    PolicyState = peSourceSendCaps;
                    PolicySubIndex = 0;
                    PDTxStatus = txIdle;
                    break;
                case DMTVenderDefined:
                    PolicySubIndex = 0;
#ifdef FSC_HAVE_VDM
                    doVdmCommand();
#endif 
                    break;
                default:
                    break;
            }
        }
    }
    else if(PartnerCaps.object == 0)
    {
        PolicyState = peSourceGetSinkCaps;                                      
        PolicySubIndex = 0;                                                     
        PDTxStatus = txIdle;                                                    
    }
#ifdef FSC_HAVE_VDM
    else if(PolicyIsDFP
            && (AutoVdmState != AUTO_VDM_DONE)
#ifdef FSC_DEBUG
            && (GetUSBPDBufferNumBytes() == 0)
#endif 
            )
    {
        autoVdmDiscovery();
    }
#endif 
    else
    {
#ifdef FSC_INTERRUPT_TRIGGERED
        g_Idle = TRUE;                                                          
        Registers.Mask.byte = 0xFF;
        Registers.Mask.M_COMP_CHNG = 0;
        DeviceWrite(regMask, 1, &Registers.Mask.byte);
        Registers.MaskAdv.byte[0] = 0xFF;
        Registers.MaskAdv.M_HARDRST = 0;
        DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
        Registers.MaskAdv.M_GCRCSENT = 0;
        DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
        platform_enable_timer(FALSE);
#endif  
    }

}

void PolicySourceGiveSourceCap(void)
{
    PolicySendData(DMTSourceCapabilities, CapsHeaderSource.NumDataObjects, &CapsSource[0], peSourceReady, 0, SOP_TYPE_SOP);
}

void PolicySourceGetSourceCap(void)
{
    PolicySendCommand(CMTGetSourceCap, peSourceReady, 0);
}

void PolicySourceGetSinkCap(void)
{
    switch (PolicySubIndex)
    {
        case 0:
            if (PolicySendCommand(CMTGetSinkCap, peSourceGetSinkCaps, 1) == STAT_SUCCESS) 
                PolicyStateTimer = tSenderResponse;                             
            break;
        default:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if ((PolicyRxHeader.NumDataObjects > 0) && (PolicyRxHeader.MessageType == DMTSinkCapabilities))
                {
                    UpdateCapabilitiesRx(FALSE);
                    PolicyState = peSourceReady;                                
                }
                else                                                            
                {
                    PolicyState = peSourceSendHardReset;                        
                }
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceSendHardReset;                            
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
}

void PolicySourceGiveSinkCap(void)
{
#ifdef FSC_HAVE_DRP
    if (PortType == USBTypeC_DRP)
        PolicySendData(DMTSinkCapabilities, CapsHeaderSink.NumDataObjects, &CapsSink[0], peSourceReady, 0, SOP_TYPE_SOP);
    else
#endif 
        PolicySendCommand(CMTReject, peSourceReady, 0);                         
}

void PolicySourceSendPing(void)
{
    PolicySendCommand(CMTPing, peSourceReady, 0);
}

void PolicySourceGotoMin(void)
{
    if (ProtocolMsgRx)
    {
        ProtocolMsgRx = FALSE;                                                  
        if (PolicyRxHeader.NumDataObjects == 0)                                 
        {
            switch(PolicyRxHeader.MessageType)                                  
            {
                case CMTSoftReset:
                    PolicyState = peSourceSoftReset;                            
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                default:                                                        
                    break;
            }
        }
    }
    else
    {
        switch (PolicySubIndex)
        {
            case 0:
                PolicySendCommand(CMTGotoMin, peSourceGotoMin, 1);                  
                break;
            case 1:
                PolicyStateTimer = tSrcTransition;                                  
                PolicySubIndex++;                                                   
                break;
            case 2:
                if (!PolicyStateTimer)                                              
                    PolicySubIndex++;                                               
                break;
            case 3:
                
                PolicySubIndex++;                                                   
                break;
            case 4:
                
                PolicySubIndex++;                                                   
                break;
            default:
                PolicySendCommand(CMTPS_RDY, peSourceReady, 0);                     
                break;
        }
    }
}

void PolicySourceSendDRSwap(void)
{
    FSC_U8 Status;
    switch (PolicySubIndex)
    {
        case 0:
            Status = PolicySendCommandNoReset(CMTDR_Swap, peSourceSendDRSwap, 1);   
            if (Status == STAT_SUCCESS)                                         
                PolicyStateTimer = tSenderResponse;                             
            else if (Status == STAT_ERROR)                                      
                PolicyState = peErrorRecovery;                                  
            break;
        default:
            if (ProtocolMsgRx)
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch(PolicyRxHeader.MessageType)                          
                    {
                        case CMTAccept:
                            PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE; 
                            Registers.Switches.DATAROLE = PolicyIsDFP;          
                            DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]); 
                            PolicyState = peSourceReady;                        
                            break;
                        case CMTSoftReset:
                            PolicyState = peSourceSoftReset;                    
                            break;
                        default:                                                
                            PolicyState = peSourceReady;                        
                            break;
                    }
                }
                else                                                            
                {
                    PolicyState = peSourceReady;                                
                }
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            else if (PolicyStateTimer == 0)                                     
            {
                PolicyState = peSourceReady;                                    
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
}

void PolicySourceEvaluateDRSwap(void)
{
    FSC_U8 Status;

#ifdef FSC_HAVE_VDM
    if (mode_entered == TRUE)                                               
    {
        PolicyState = peSourceSendHardReset;
        PolicySubIndex = 0;
    }
#endif 

    Status = PolicySendCommandNoReset(CMTAccept, peSourceReady, 0);         
    if (Status == STAT_SUCCESS)                                             
    {
        PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE;                 
        Registers.Switches.DATAROLE = PolicyIsDFP;                          
        DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);          
    }
    else if (Status == STAT_ERROR)                                          
    {
        PolicyState = peErrorRecovery;                                      
        PolicySubIndex = 0;                                                 
        PDTxStatus = txIdle;                                                
    }
}

void PolicySourceSendVCONNSwap(void)
{
    switch(PolicySubIndex)
    {
        case 0:
            if (PolicySendCommand(CMTVCONN_Swap, peSourceSendVCONNSwap, 1) == STAT_SUCCESS) 
                PolicyStateTimer = tSenderResponse;                             
            break;
        case 1:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTAccept:                                         
                            PolicySubIndex++;                                   
                            break;
                        case CMTWait:                                           
                        case CMTReject:
                            PolicyState = peSourceReady;                        
                            PolicySubIndex = 0;                                 
                            PDTxStatus = txIdle;                                
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceReady;                                    
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        case 2:
            if (IsVCONNSource)                                                  
            {
                PolicyStateTimer = tVCONNSourceOn;                              
                PolicySubIndex++;                                               
            }
            else                                                                
            {
                if (blnCCPinIsCC1)                                              
                    Registers.Switches.VCONN_CC2 = 1;                           
                else                                                            
                    Registers.Switches.VCONN_CC1 = 1;                           
                DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);      
                IsVCONNSource = TRUE;
                PolicyStateTimer = VbusTransitionTime;                          
                PolicySubIndex = 4;                                             
            }
            break;
        case 3:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            Registers.Switches.VCONN_CC1 = 0;                   
                            Registers.Switches.VCONN_CC2 = 0;                   
                            DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]); 
                            IsVCONNSource = FALSE;
                            PolicyState = peSourceReady;                        
                            PolicySubIndex = 0;                                 
                            PDTxStatus = txIdle;                                
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceSendHardReset;                            
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default:
            if (!PolicyStateTimer)
            {
                PolicySendCommand(CMTPS_RDY, peSourceReady, 0);                 
            }
            break;
    }
}

void PolicySourceSendPRSwap(void)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch(PolicySubIndex)
    {
        case 0: 
            if (PolicySendCommand(CMTPR_Swap, peSourceSendPRSwap, 1) == STAT_SUCCESS) 
                PolicyStateTimer = tSenderResponse;                             
            break;
        case 1:  
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTAccept:                                         
                            IsPRSwap = TRUE;
                            PRSwapTimer = tPRSwapBailout;                       
                            PolicyStateTimer = tSrcTransition;                  
                            PolicySubIndex++;                                   
                            break;
                        case CMTWait:                                           
                        case CMTReject:
                            PolicyState = peSourceReady;                        
                            PolicySubIndex = 0;                                 
                            IsPRSwap = FALSE;
                            PDTxStatus = txIdle;                                
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceReady;                                    
                PolicySubIndex = 0;                                             
                IsPRSwap = FALSE;
                PDTxStatus = txIdle;                                            
            }
            break;
        case 2: 
            if (!PolicyStateTimer)
            {
                platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
                platform_set_vbus_discharge(TRUE);                              
                PolicySubIndex++;                                               
            }
            break;
        case 3:
            if (VbusVSafe0V())
            {
                RoleSwapToAttachedSink();
                platform_set_vbus_discharge(FALSE);                             
                PolicyIsSource = FALSE;
                Registers.Switches.POWERROLE = PolicyIsSource;
                DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
                PolicySubIndex++;
            }
            break;
        case 4: 
                Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceSendPRSwap, 5);
                if (Status == STAT_SUCCESS)                                     
                    PolicyStateTimer = tPSSourceOn;                          
                else if (Status == STAT_ERROR)
                    PolicyState = peErrorRecovery;                              
            break;
        case 5: 
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            PolicySubIndex++;                                 
                            PolicyStateTimer = tGoodCRCDelay;                   
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peErrorRecovery;                                  
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default:
            if (PolicyStateTimer == 0)
            {
                PolicyState = peSinkStartup;                        
                PolicySubIndex = 0;                                 
                PolicyStateTimer = 0;                   
            }
            break;
    }
#endif 
}

void PolicySourceEvaluatePRSwap(void)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch(PolicySubIndex)
    {
        case 0: 
            if ((CapsSource[0].FPDOSupply.DualRolePower == FALSE) || (PartnerCaps.FPDOSink.DualRolePower == FALSE) || 
                ((CapsSource[0].FPDOSupply.ExternallyPowered == TRUE) && 
                    (PartnerCaps.FPDOSink.SupplyType == pdoTypeFixed) && (PartnerCaps.FPDOSink.ExternallyPowered == FALSE)))
            {
                PolicySendCommand(CMTReject, peSourceReady, 0);                 
            }
            else
            {
                if (PolicySendCommand(CMTAccept, peSourceEvaluatePRSwap, 1) == STAT_SUCCESS) 
                {
                    IsPRSwap = TRUE;
                    RoleSwapToAttachedSink();
                    PolicyStateTimer = tSrcTransition;
                }
            }
            break;
        case 1:
            if(PolicyStateTimer == 0)
            {
                platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);       
                platform_set_vbus_discharge(TRUE);                              
                PolicySubIndex++;
            }
            break;
        case 2:
            if (VbusVSafe0V())  
            {
                
                PolicyStateTimer = tSrcTransition;                              
                PolicyIsSource = FALSE;
                Registers.Switches.POWERROLE = PolicyIsSource;
                DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
                PolicySubIndex++;
            }
            break;
        case 3:
            if(PolicyStateTimer == 0)
            {
                platform_set_vbus_discharge(FALSE);                             
                
                PolicySubIndex++;
            }
            break;
        case 4:
            Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceEvaluatePRSwap, 5);    
            if (Status == STAT_SUCCESS)                                     
                PolicyStateTimer = tPSSourceOn;                          
            else if (Status == STAT_ERROR)
                PolicyState = peErrorRecovery;                              
            break;
        case 5: 
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            PolicySubIndex++;                                 
                            PolicyStateTimer = tGoodCRCDelay;                   
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceSendHardReset;                                  
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default:
                if (PolicyStateTimer == 0)
                {
                    PolicyState = peSinkStartup;                        
                    PolicySubIndex = 0;                                 
                    PolicyStateTimer = 0;
                }
            break;
    }
#else
    PolicySendCommand(CMTReject, peSourceReady, 0);                             
#endif 
}

void PolicySourceWaitNewCapabilities(void)                                      
{
#ifdef FSC_INTERRUPT_TRIGGERED
    g_Idle = TRUE;                                                              
    Registers.Mask.byte = 0xFF;
    Registers.Mask.M_COMP_CHNG = 0;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0xFF;
    Registers.MaskAdv.M_HARDRST = 0;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 0;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    platform_enable_timer(FALSE);
#endif 
    switch(PolicySubIndex)
    {
        case 0:
            
            break;
        default:
            
            PolicyState = peSourceSendCaps;
            PolicySubIndex = 0;
            break;
    }
}
#endif 

void PolicySourceEvaluateVCONNSwap(void)
{
    switch(PolicySubIndex)
    {
        case 0:
            PolicySendCommand(CMTAccept, peSourceEvaluateVCONNSwap, 1);         
            break;
        case 1:
            if (IsVCONNSource)                                                  
            {
                PolicyStateTimer = tVCONNSourceOn;                              
                PolicySubIndex++;                                               
            }
            else                                                                
            {
                if (blnCCPinIsCC1)                                              
                {
                    Registers.Switches.VCONN_CC2 = 1;                           
                    Registers.Switches.PDWN2 = 0;                               
                }
                else                                                            
                {
                    Registers.Switches.VCONN_CC1 = 1;                           
                    Registers.Switches.PDWN1 = 0;                               
                }
                DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);      
                IsVCONNSource = TRUE;
                PolicyStateTimer = VbusTransitionTime;                          
                PolicySubIndex = 3;                                             
            }
            break;
        case 2:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            Registers.Switches.VCONN_CC1 = 0;                   
                            Registers.Switches.VCONN_CC2 = 0;                   
                            Registers.Switches.PDWN1 = 1;                       
                            Registers.Switches.PDWN2 = 1;                       
                            DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]); 
                            IsVCONNSource = FALSE;
                            PolicyState = peSourceReady;                          
                            PolicySubIndex = 0;                                 
                            PDTxStatus = txIdle;                                
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceSendHardReset;                            
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default:
            if (!PolicyStateTimer)
            {
                PolicySendCommand(CMTPS_RDY, peSourceReady, 0);                       
            }
            break;
    }
}


#ifdef FSC_HAVE_SNK
void PolicySinkSendHardReset(void)
{
    IsHardReset = TRUE;
    PolicySendHardReset(peSinkTransitionDefault, 0);
}

void PolicySinkSoftReset(void)
{
    if (PolicySendCommand(CMTAccept, peSinkWaitCaps, 0) == STAT_SUCCESS)
        PolicyStateTimer = tSinkWaitCap;
}

void PolicySinkSendSoftReset(void)
{
    switch (PolicySubIndex)
    {
        case 0:
            if (PolicySendCommand(CMTSoftReset, peSinkSendSoftReset, 1) == STAT_SUCCESS)    
                PolicyStateTimer = tSenderResponse;                             
            break;
        default:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTAccept))  
                {
                    PolicyState = peSinkWaitCaps;                               
                    PolicyStateTimer = tSinkWaitCap;                            
                }
                else                                                            
                    PolicyState = peSinkSendHardReset;                          
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSinkSendHardReset;                              
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
}

void PolicySinkTransitionDefault(void)
{
    switch (PolicySubIndex)
    {
        case 0:
            IsHardReset = TRUE;
            Registers.Switches.AUTO_CRC = 0;                                        
            DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
            NoResponseTimer = tNoResponse;                                      
            if(PolicyIsDFP)                                                     
            {
                PolicyIsDFP = FALSE;                                            
                Registers.Switches.DATAROLE = PolicyIsDFP;                      
                DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);      
            }
            if(IsVCONNSource)                                                   
            {
                Registers.Switches.VCONN_CC1 = 0;
                Registers.Switches.VCONN_CC2 = 0;
                DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
                IsVCONNSource = FALSE;
            }
            PolicySubIndex++;
            break;
        case 1:
            if(VbusVSafe0V())
            {
                PolicySubIndex++;
                PolicyStateTimer = 15000;                                       
            }
            else if (NoResponseTimer == 0)                                      
            {
                if(PolicyHasContract)
                {
                    PolicyState = peErrorRecovery;
                    PolicySubIndex = 0;
                }
                else
                {
                    PolicyState = peSinkStartup;
                    PolicySubIndex = 0;
                    PolicyStateTimer = 0;
                }
            }
            break;
        case 2:
            if(isVBUSOverVoltage(VBUS_MDAC_4p2) || (PolicyStateTimer == 0))
            {
                PolicySubIndex++;
            }
            else if (NoResponseTimer == 0)                                      
            {
                if(PolicyHasContract)
                {
                    PolicyState = peErrorRecovery;
                    PolicySubIndex = 0;
                }
                else
                {
                    PolicyState = peSinkStartup;
                    PolicySubIndex = 0;
                    PolicyStateTimer = 0;
                }
            }
            break;
        default:
            PolicyState = peSinkStartup;                                                
            PolicySubIndex = 0;                                                         
            PolicyStateTimer = 0;
            PDTxStatus = txIdle;                                                        
            break;
    }



}

void PolicySinkStartup(void)
{
#ifdef FSC_HAVE_VDM
    FSC_S32 i;
#endif 

      
    Registers.Mask.byte = 0x00;
    DeviceWrite(regMask, 1, &Registers.Mask.byte);
    Registers.MaskAdv.byte[0] = 0x00;
    DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
    Registers.MaskAdv.M_GCRCSENT = 0;
    DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);

    if(Registers.Control.RX_FLUSH == 1)                                         
    {
        Registers.Control.RX_FLUSH = 0;
        DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
    }

#ifdef FSC_DEBUG
    if (manualRetries && nTries != 4)
    {
        nTries = 4;                                                         
    }
    else
#endif 
    if (Registers.Control.N_RETRIES == 0)
    {
        Registers.Control.N_RETRIES = 3;                                    
        DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
    }

    USBPDContract.object = 0;                                           
    PartnerCaps.object = 0;                                         
    IsPRSwap = FALSE;
    PolicyIsSource = FALSE;                                                     
    Registers.Switches.POWERROLE = PolicyIsSource;
    DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
    Registers.Switches.AUTO_CRC = 1;                                            
    DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
    ResetProtocolLayer(TRUE);                                                   

    CapsCounter = 0;                                                            
    CollisionCounter = 0;                                                       
    PolicyStateTimer = 0;                                                       
    PolicyState = peSinkDiscovery;                                              
    PolicySubIndex = 0;                                                         

#ifdef FSC_HAVE_VDM
    AutoVdmState = AUTO_VDM_INIT;

    auto_mode_disc_tracker = 0;

    mode_entered = FALSE;

    core_svid_info.num_svids = 0;
    for (i = 0; i < MAX_NUM_SVIDS; i++) {
        core_svid_info.svids[i] = 0;
    }
#endif 

#ifdef FSC_HAVE_DP
    AutoDpModeEntryObjPos = -1;

    resetDp();
#endif 
}

void PolicySinkDiscovery(void)
{

        IsHardReset = FALSE;
        PRSwapTimer = 0;                                                        
        PolicyState = peSinkWaitCaps;
        PolicySubIndex = 0;
        PolicyStateTimer = tTypeCSinkWaitCap;
}

void PolicySinkWaitCaps(void)
{
    if (ProtocolMsgRx)                                                          
    {
        ProtocolMsgRx = FALSE;                                                  
        if ((PolicyRxHeader.NumDataObjects > 0) && (PolicyRxHeader.MessageType == DMTSourceCapabilities)) 
        {
            UpdateCapabilitiesRx(TRUE);                                         
            PolicyState = peSinkEvaluateCaps;                                   
        }
        else if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTSoftReset))
        {
            PolicyState = peSinkSoftReset;                                      
        }
        PolicySubIndex = 0;                                                     
    }
    else if ((PolicyHasContract == TRUE) && (NoResponseTimer == 0) && (HardResetCounter > nHardResetCount))
    {
        PolicyState = peErrorRecovery;
        PolicySubIndex = 0;
    }
    else if ((PolicyStateTimer == 0) && (HardResetCounter <= nHardResetCount))
    {
        PolicyState = peSinkSendHardReset;
        PolicySubIndex = 0;
    }
    else if ((PolicyHasContract == FALSE) && (NoResponseTimer == 0) && (HardResetCounter > nHardResetCount))
    {
#ifdef FSC_INTERRUPT_TRIGGERED
        g_Idle = TRUE;                                                          
        Registers.Mask.byte = 0xFF;
        Registers.Mask.M_VBUSOK = 0;
        DeviceWrite(regMask, 1, &Registers.Mask.byte);
        Registers.MaskAdv.byte[0] = 0xFF;
        Registers.MaskAdv.M_HARDRST = 0;
        DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
        Registers.MaskAdv.M_GCRCSENT = 0;
        DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
        platform_enable_timer(FALSE);
#endif 
    }
}

void PolicySinkEvaluateCaps(void)
{
    
    
    
    
    FSC_S32 i, reqPos;
    FSC_U32 objVoltage = 0;
    FSC_U32 objCurrent, objPower, MaxPower, SelVoltage, ReqCurrent;
    objCurrent = 0;
    NoResponseTimer = T_TIMER_DISABLE;                                          
    HardResetCounter = 0;                                                       
    SelVoltage = 0;
    MaxPower = 0;
    reqPos = 0;                                                                 
    for (i=0; i<CapsHeaderReceived.NumDataObjects; i++)                         
    {
        switch (CapsReceived[i].PDO.SupplyType)
        {
            case pdoTypeFixed:
                objVoltage = CapsReceived[i].FPDOSupply.Voltage;                
                if (objVoltage > SinkRequestMaxVoltage)                         
                    objPower = 0;                                               
                else                                                            
                {
                    objCurrent = CapsReceived[i].FPDOSupply.MaxCurrent;
                    objPower = objVoltage * objCurrent;                         
                }
                break;
            case pdoTypeVariable:
                objVoltage = CapsReceived[i].VPDO.MaxVoltage;                   
                if (objVoltage > SinkRequestMaxVoltage)                         
                    objPower = 0;                                               
                else                                                            
                {
                    objVoltage = CapsReceived[i].VPDO.MinVoltage;               
                    objCurrent = CapsReceived[i].VPDO.MaxCurrent;               
                    objPower = objVoltage * objCurrent;                         
                }
                break;
            case pdoTypeBattery:                                                
            default:                                                            
                objPower = 0;                                                   
                break;
        }
        if (objPower >= MaxPower)                                               
        {
            MaxPower = objPower;                                                
            SelVoltage = objVoltage;                                            
            reqPos = i + 1;                                                     
        }
    }
    if ((reqPos > 0) && (SelVoltage > 0))
    {
        PartnerCaps.object = CapsReceived[0].object;
        SinkRequest.FVRDO.ObjectPosition = reqPos & 0x07;                       
        SinkRequest.FVRDO.GiveBack = SinkGotoMinCompatible;                     
        SinkRequest.FVRDO.NoUSBSuspend = SinkUSBSuspendOperation;               
        SinkRequest.FVRDO.USBCommCapable = SinkUSBCommCapable;                  
        ReqCurrent = SinkRequestOpPower / SelVoltage;                           
        SinkRequest.FVRDO.OpCurrent = (ReqCurrent & 0x3FF);                     
        ReqCurrent = SinkRequestMaxPower / SelVoltage;                          
        SinkRequest.FVRDO.MinMaxCurrent = (ReqCurrent & 0x3FF);                 
        if (SinkGotoMinCompatible)                                              
            SinkRequest.FVRDO.CapabilityMismatch = FALSE;                       
        else                                                                    
        {
            if (objCurrent < ReqCurrent)                                 
            {
                SinkRequest.FVRDO.CapabilityMismatch = TRUE;                    
                SinkRequest.FVRDO.MinMaxCurrent = objCurrent;                     
                SinkRequest.FVRDO.OpCurrent =  objCurrent;
            }
            else                                                                
            {
                SinkRequest.FVRDO.CapabilityMismatch = FALSE;                   
            }
        }
        PolicyState = peSinkSelectCapability;                                   
        PolicySubIndex = 0;                                                     
        PolicyStateTimer = tSenderResponse;                                     
    }
    else
    {
        
        PolicyState = peSinkWaitCaps;                                           
        PolicyStateTimer = tTypeCSinkWaitCap;                                        
    }
}

void PolicySinkSelectCapability(void)
{
    switch (PolicySubIndex)
    {
        case 0:
            if (PolicySendData(DMTRequest, 1, &SinkRequest, peSinkSelectCapability, 1, SOP_TYPE_SOP) == STAT_SUCCESS)
            {
                NoResponseTimer = tSenderResponse+25;                                   
            }
            break;
       case 1:
            if (ProtocolMsgRx)
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch(PolicyRxHeader.MessageType)                          
                    {
                        case CMTAccept:
                            PolicyHasContract = TRUE;                           
                            USBPDContract.object = SinkRequest.object;          
                            PolicyStateTimer = tPSTransition;                   
                            PolicyState = peSinkTransitionSink;                 
                            break;
                        case CMTWait:
                        case CMTReject:
                            if(PolicyHasContract)
                            {
                                PolicyState = peSinkReady;                      
                            }
                            else
                            {
                                PolicyState = peSinkWaitCaps;                   
                                HardResetCounter = nHardResetCount + 1;         
                            }
                            break;
                        case CMTSoftReset:
                            PolicyState = peSinkSoftReset;                      
                            break;
                        default:
                            PolicyState = peSinkSendSoftReset;                  
                            break;
                    }
                }
                else                                                            
                {
                    switch (PolicyRxHeader.MessageType)
                    {
                        case DMTSourceCapabilities:                             
                            UpdateCapabilitiesRx(TRUE);                         
                            PolicyState = peSinkEvaluateCaps;                   
                            break;
                        default:
                            PolicyState = peSinkSendSoftReset;                  
                            break;
                    }
                }
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            else if (PolicyStateTimer == 0)                                     
            {
                PolicyState = peSinkSendHardReset;                              
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
}

void PolicySinkTransitionSink(void)
{
    if (ProtocolMsgRx)
    {
        ProtocolMsgRx = FALSE;                                                  
        if (PolicyRxHeader.NumDataObjects == 0)                                 
        {
            switch(PolicyRxHeader.MessageType)                                  
            {
                case CMTPS_RDY:
                    PolicyState = peSinkReady;                                  
                    break;
                case CMTSoftReset:
                    PolicyState = peSinkSoftReset;                              
                    break;
                default:
                    PolicyState = peSinkSendSoftReset;                          
                    break;
            }
        }
        else                                                                    
        {
            switch (PolicyRxHeader.MessageType)                                 
            {
                case DMTSourceCapabilities:                                     
                    UpdateCapabilitiesRx(TRUE);                                 
                    PolicyState = peSinkEvaluateCaps;                           
                    break;
                default:                                                        
                    PolicyState = peSinkSendSoftReset;                          
                    break;
            }
        }
        PolicySubIndex = 0;                                                     
        PDTxStatus = txIdle;                                                    
    }
    else if (PolicyStateTimer == 0)                                             
    {
        PolicyState = peSinkSendHardReset;                                      
        PolicySubIndex = 0;                                                     
        PDTxStatus = txIdle;                                                    
    }
}

void PolicySinkReady(void)
{
    if (ProtocolMsgRx)                                                          
    {
        ProtocolMsgRx = FALSE;                                                  
        if (PolicyRxHeader.NumDataObjects == 0)                                 
        {
            switch (PolicyRxHeader.MessageType)                                 
            {
                case CMTGotoMin:
                    PolicyState = peSinkTransitionSink;                         
                    PolicyStateTimer = tPSTransition;                           
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTGetSinkCap:
                    PolicyState = peSinkGiveSinkCap;                            
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTGetSourceCap:
                    PolicyState = peSinkGiveSourceCap;                          
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTDR_Swap:                                                
                    PolicyState = peSinkEvaluateDRSwap;                         
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTPR_Swap:
                    PolicyState = peSinkEvaluatePRSwap;                         
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTVCONN_Swap:                                             
                    PolicyState = peSinkEvaluateVCONNSwap;                      
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTSoftReset:
                    PolicyState = peSinkSoftReset;                              
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                default:                                                        
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
            }
        }
        else
        {
            switch (PolicyRxHeader.MessageType)
            {
                case DMTSourceCapabilities:
                    UpdateCapabilitiesRx(TRUE);                                 
                    PolicyState = peSinkEvaluateCaps;                           
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
#ifdef FSC_HAVE_VDM
                case DMTVenderDefined:
                    convertAndProcessVdmMessage(ProtocolMsgRxSop);
                    break;
#endif 
                case DMTBIST:
                    processDMTBIST();
                    break;
                default:                                                        
                    PolicySubIndex = 0;                                                     
                    PDTxStatus = txIdle;                                                    
                    break;
            }
        }
    }
    else if (USBPDTxFlag)                                                       
    {
        if (PDTransmitHeader.NumDataObjects == 0)
        {
            switch (PDTransmitHeader.MessageType)
            {
                case CMTGetSourceCap:
                    PolicyState = peSinkGetSourceCap;                           
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                   break;
                case CMTGetSinkCap:
                    PolicyState = peSinkGetSinkCap;                             
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                case CMTDR_Swap:
                    PolicyState = peSinkSendDRSwap;                         
                    PolicySubIndex = 0;                                     
                    PDTxStatus = txIdle;                                    
                    break;
#ifdef FSC_HAVE_DRP
                case CMTPR_Swap:
                    PolicyState = peSinkSendPRSwap;                         
                    PolicySubIndex = 0;                                     
                    PDTxStatus = txIdle;                                    
                    break;
#endif 
                case CMTSoftReset:
                    PolicyState = peSinkSendSoftReset;                          
                    PolicySubIndex = 0;                                         
                    PDTxStatus = txIdle;                                        
                    break;
                default:
                    break;
            }
        }
        else
        {
            switch (PDTransmitHeader.MessageType)
            {
                case DMTRequest:
                    SinkRequest.object = PDTransmitObjects[0].object;           
                    PolicyState = peSinkSelectCapability;                       
                    PolicySubIndex = 0;                                         
                    PolicyStateTimer = tSenderResponse;                         
                    break;
                case DMTVenderDefined:
#ifdef FSC_HAVE_VDM
                    doVdmCommand();
#endif 
                    break;
                default:
                    break;
            }
        }
    }
#ifdef FSC_HAVE_VDM
    else if (PolicyIsDFP
             && (AutoVdmState != AUTO_VDM_DONE)
#ifdef FSC_DEBUG
             && (GetUSBPDBufferNumBytes() == 0)
#endif 
             )
    {
        autoVdmDiscovery();
    }
#endif 
    else
    {
#ifdef FSC_INTERRUPT_TRIGGERED
        g_Idle = TRUE;                                                          
        Registers.Mask.byte = 0xFF;
        Registers.Mask.M_VBUSOK = 0;
        DeviceWrite(regMask, 1, &Registers.Mask.byte);
        Registers.MaskAdv.byte[0] = 0xFF;
        Registers.MaskAdv.M_HARDRST = 0;
        DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
        Registers.MaskAdv.M_GCRCSENT = 0;
        DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
        platform_enable_timer(FALSE);
#endif  
    }
}

void PolicySinkGiveSinkCap(void)
{
    PolicySendData(DMTSinkCapabilities, CapsHeaderSink.NumDataObjects, &CapsSink[0], peSinkReady, 0, SOP_TYPE_SOP);
}

void PolicySinkGetSinkCap(void)
{
    PolicySendCommand(CMTGetSinkCap, peSinkReady, 0);
}

void PolicySinkGiveSourceCap(void)
{
#ifdef FSC_HAVE_DRP
    if (PortType == USBTypeC_DRP)
        PolicySendData(DMTSourceCapabilities, CapsHeaderSource.NumDataObjects, &CapsSource[0], peSinkReady, 0, SOP_TYPE_SOP);
    else
#endif 
        PolicySendCommand(CMTReject, peSinkReady, 0);                           
}

void PolicySinkGetSourceCap(void)
{
    PolicySendCommand(CMTGetSourceCap, peSinkReady, 0);
}

void PolicySinkSendDRSwap(void)
{
    FSC_U8 Status;
    switch (PolicySubIndex)
    {
        case 0:
            Status = PolicySendCommandNoReset(CMTDR_Swap, peSinkSendDRSwap, 1); 
            if (Status == STAT_SUCCESS)                                         
                PolicyStateTimer = tSenderResponse;                             
            else if (Status == STAT_ERROR)                                      
                PolicyState = peErrorRecovery;                                  
            break;
        default:
            if (ProtocolMsgRx)
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch(PolicyRxHeader.MessageType)                          
                    {
                        case CMTAccept:
                            PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE; 
                            Registers.Switches.DATAROLE = PolicyIsDFP;          
                            DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]); 
                            PolicyState = peSinkReady;                          
                            break;
                        case CMTSoftReset:
                            PolicyState = peSinkSoftReset;                      
                            break;
                        default:                                                
                            PolicyState = peSinkReady;                          
                            break;
                    }
                }
                else                                                            
                {
                    PolicyState = peSinkReady;                                  
                }
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            else if (PolicyStateTimer == 0)                                     
            {
                PolicyState = peSinkReady;                                      
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
}

void PolicySinkEvaluateDRSwap(void)
{
    FSC_U8 Status;

#ifdef FSC_HAVE_VDM
    if (mode_entered == TRUE)                                              
    {
        PolicyState = peSinkSendHardReset;
        PolicySubIndex = 0;
    }
#endif 

    Status = PolicySendCommandNoReset(CMTAccept, peSinkReady, 0);           
    if (Status == STAT_SUCCESS)                                             
    {
        PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE;                 
        Registers.Switches.DATAROLE = PolicyIsDFP;                          
        DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);         
    }
    else if (Status == STAT_ERROR)                                          
    {
        PolicyState = peErrorRecovery;                                      
        PolicySubIndex = 0;                                                 
        PDTxStatus = txIdle;                                                
    }
}

void PolicySinkEvaluateVCONNSwap(void)
{
    switch(PolicySubIndex)
    {
        case 0:
            PolicySendCommand(CMTAccept, peSinkEvaluateVCONNSwap, 1);           
            break;
        case 1:
            if (IsVCONNSource)                                                  
            {
                PolicyStateTimer = tVCONNSourceOn;                              
                PolicySubIndex++;                                               
            }
            else                                                                
            {
                if (blnCCPinIsCC1)                                              
                {
                    Registers.Switches.VCONN_CC2 = 1;                           
                    Registers.Switches.PDWN2 = 0;                               
                }
                else                                                            
                {
                    Registers.Switches.VCONN_CC1 = 1;                           
                    Registers.Switches.PDWN1 = 0;                               
                }
                DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);      
                IsVCONNSource = TRUE;
                PolicyStateTimer = VbusTransitionTime;                          
                PolicySubIndex = 3;                                             
            }
            break;
        case 2:
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            Registers.Switches.VCONN_CC1 = 0;                   
                            Registers.Switches.VCONN_CC2 = 0;                   
                            Registers.Switches.PDWN1 = 1;                       
                            Registers.Switches.PDWN2 = 1;                       
                            DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]); 
                            IsVCONNSource = FALSE;
                            PolicyState = peSinkReady;                          
                            PolicySubIndex = 0;                                 
                            PDTxStatus = txIdle;                                
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSourceSendHardReset;                            
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default:
            if (!PolicyStateTimer)
            {
                PolicySendCommand(CMTPS_RDY, peSinkReady, 0);                       
            }
            break;
    }
}

void PolicySinkSendPRSwap(void)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch(PolicySubIndex)
    {
        case 0: 
            if (PolicySendCommand(CMTPR_Swap, peSinkSendPRSwap, 1) == STAT_SUCCESS) 
                PolicyStateTimer = tSenderResponse;                             
            break;
        case 1:  
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTAccept:                                         
                            IsPRSwap = TRUE;
                            PRSwapTimer = tPRSwapBailout;                       
                            PolicyStateTimer = tPSSourceOff;                    
                            PolicySubIndex++;                                   
                            break;
                        case CMTWait:                                           
                        case CMTReject:
                            PolicyState = peSinkReady;                          
                            PolicySubIndex = 0;                                 
                            IsPRSwap = FALSE;
                            PDTxStatus = txIdle;                                
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSinkReady;                                      
                PolicySubIndex = 0;                                             
                IsPRSwap = FALSE;
                PDTxStatus = txIdle;                                            
            }
            break;
        case 2:     
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            RoleSwapToAttachedSource();                         
                            PolicyIsSource = TRUE;
                            Registers.Switches.POWERROLE = PolicyIsSource;
                            DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
                            PolicyStateTimer = tSourceOnDelay;                  
                            PolicySubIndex++;                                   
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peErrorRecovery;                                  
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default: 
            if (!PolicyStateTimer)
            {
                Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceStartup, 0);   
                if (Status == STAT_ERROR)
                    PolicyState = peErrorRecovery;                              
                SwapSourceStartTimer = tSwapSourceStart;
            }
            break;
    }
#endif 
}

void PolicySinkEvaluatePRSwap(void)
{
#ifdef FSC_HAVE_DRP
    FSC_U8 Status;
    switch(PolicySubIndex)
    {
        case 0: 
            if ((PartnerCaps.FPDOSupply.SupplyType == pdoTypeFixed) && (PartnerCaps.FPDOSupply.DualRolePower == FALSE)) 
                    
                    
            {
                PolicySendCommand(CMTReject, peSinkReady, 0);                   
            }
            else
            {
                if (PolicySendCommand(CMTAccept, peSinkEvaluatePRSwap, 1) == STAT_SUCCESS) 
                {
                    IsPRSwap = TRUE;
                    PRSwapTimer = tPRSwapBailout;                               
                    PolicyStateTimer = tPSSourceOff;                         
                }
            }
            break;
        case 1: 
            if (ProtocolMsgRx)                                                  
            {
                ProtocolMsgRx = FALSE;                                          
                if (PolicyRxHeader.NumDataObjects == 0)                         
                {
                    switch (PolicyRxHeader.MessageType)                         
                    {
                        case CMTPS_RDY:                                         
                            RoleSwapToAttachedSource();                         
                            PolicyIsSource = TRUE;
                            Registers.Switches.POWERROLE = PolicyIsSource;
                            DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
                            PolicyStateTimer = tSourceOnDelay;                  
                            PolicySubIndex++;                                   
                            break;
                        default:                                                
                            break;
                    }
                }
            }
            else if (!PolicyStateTimer)                                         
            {
                PolicyState = peSinkSendHardReset;                                  
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
        default:    
            if (!PolicyStateTimer)
            {
                Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceStartup, 0);   
                if (Status == STAT_ERROR)
                    PolicyState = peErrorRecovery;                              
                SwapSourceStartTimer = tSwapSourceStart;
            }
            break;
    }
#else
    PolicySendCommand(CMTReject, peSinkReady, 0);                               
#endif 
}
#endif 

#ifdef FSC_HAVE_VDM

void PolicyGiveVdm(void) {

    if (ProtocolMsgRx && PolicyRxHeader.MessageType == DMTVenderDefined)        
    {
        sendVdmMessageFailed();                                                 
        PolicySubIndex = 0;                                                     
        PDTxStatus = txIdle;                                                    
    }
    else if (sendingVdmData)
    {

        FSC_U8 result = PolicySendData(DMTVenderDefined, vdm_msg_length, vdm_msg_obj, vdm_next_ps, 0, SOP_TYPE_SOP);
        if (result == STAT_SUCCESS)
        {
            if (expectingVdmResponse())
            {
                startVdmTimer(PolicyState);
            }
            else
            {
                resetPolicyState();
            }
            sendingVdmData = FALSE;
        }
        else if (result == STAT_ERROR)
        {
            sendVdmMessageFailed();
            sendingVdmData = FALSE;
        }
    }
    else
    {
        sendVdmMessageFailed();
    }

    if (VdmTimerStarted && (VdmTimer == 0))
    {
        vdmMessageTimeout();
    }
}

void PolicyVdm (void) {

    if (ProtocolMsgRx)                                                          
    {
        ProtocolMsgRx = FALSE;                                                  
        if (PolicyRxHeader.NumDataObjects != 0)                                 
        {
            switch (PolicyRxHeader.MessageType)
            {
                case DMTVenderDefined:
                    convertAndProcessVdmMessage(ProtocolMsgRxSop);
                    break;
                default:                                                        
                    resetPolicyState();                                    
                    ProtocolMsgRx = TRUE;                                       
                    break;
            }
        }
        else
        {
            resetPolicyState();                                            
            ProtocolMsgRx = TRUE;                                               
        }
        PolicySubIndex = 0;                                                     
        PDTxStatus = txIdle;                                                    
    }
    else
    {
        if (sendingVdmData)
        {
            FSC_U8 result = PolicySendData(DMTVenderDefined, vdm_msg_length, vdm_msg_obj, vdm_next_ps, 0, SOP_TYPE_SOP);
            if (result == STAT_SUCCESS || result == STAT_ERROR)
            {
                sendingVdmData = FALSE;
            }
        }
    }

    if (VdmTimerStarted && (VdmTimer == 0))
    {
        if(PolicyState == peDfpUfpVdmIdentityRequest)
        {
            AutoVdmState = AUTO_VDM_DONE;
        }
        vdmMessageTimeout();
    }
}

#endif 

void PolicyInvalidState (void) {
    
    if (PolicyIsSource)
    {
        PolicyState = peSourceSendHardReset;
    }
    else
    {
        PolicyState = peSinkSendHardReset;
    }
}


FSC_BOOL PolicySendHardReset(PolicyState_t nextState, FSC_U32 delay)
{
    FSC_BOOL Success = FALSE;
    switch (PolicySubIndex)
    {
        case 0:
            switch (PDTxStatus)
            {
                case txReset:
                case txWait:
                    
                    
                    break;
                case txSuccess:
                    PolicyStateTimer = delay;                                   
                    PolicySubIndex++;                                           
                    Success = TRUE;
                    break;
                default:                                                        
                    PDTxStatus = txReset;                                       
                    break;
            }
            break;
        default:
            if (PolicyStateTimer == 0)                                          
            {
                PolicyStateTimer = tPSHardReset;
                HardResetCounter++;                                             
                PolicyState = nextState;                                        
                PolicySubIndex = 0;                                             
                PDTxStatus = txIdle;                                            
            }
            break;
    }
    return Success;
}

FSC_U8 PolicySendCommand(FSC_U8 Command, PolicyState_t nextState, FSC_U8 subIndex)
{
    FSC_U8 Status = STAT_BUSY;
    switch (PDTxStatus)
    {
        case txIdle:
            PolicyTxHeader.word = 0;                                            
            PolicyTxHeader.NumDataObjects = 0;                                  
            PolicyTxHeader.MessageType = Command & 0x0F;                        
            PolicyTxHeader.PortDataRole = PolicyIsDFP;                          
            PolicyTxHeader.PortPowerRole = PolicyIsSource;                      
            PolicyTxHeader.SpecRevision = USBPDSPECREV;                         
            PDTxStatus = txSend;                                                
            break;
        case txSend:
        case txBusy:
        case txWait:
            
            
            break;
        case txSuccess:
            PolicyState = nextState;                                            
            PolicySubIndex = subIndex;
            PDTxStatus = txIdle;                                                
            Status = STAT_SUCCESS;
            break;
        case txError:                                                           
            if (PolicyState == peSourceSendSoftReset)                           
                PolicyState = peSourceSendHardReset;                            
            else if (PolicyState == peSinkSendSoftReset)                        
                PolicyState = peSinkSendHardReset;                              
            else if (PolicyIsSource)                                            
                PolicyState = peSourceSendSoftReset;                            
            else                                                                
                PolicyState = peSinkSendSoftReset;                              
            PolicySubIndex = 0;                                                 
            PDTxStatus = txIdle;                                                
            Status = STAT_ERROR;
            break;
        case txCollision:
            CollisionCounter++;                                                 
            if (CollisionCounter > nRetryCount)                                 
            {
                if (PolicyIsSource)
                    PolicyState = peSourceSendHardReset;                        
                else
                    PolicyState = peSinkSendHardReset;                          
                PolicySubIndex = 0;                                             
                PDTxStatus = txReset;                                           
                Status = STAT_ERROR;
            }
            else                                                                
                PDTxStatus = txIdle;                                            
            break;
        default:                                                                
            if (PolicyIsSource)
                PolicyState = peSourceSendHardReset;                            
            else
                PolicyState = peSinkSendHardReset;                              
            PolicySubIndex = 0;                                                 
            PDTxStatus = txReset;                                               
            Status = STAT_ERROR;
            break;
    }
    return Status;
}

FSC_U8 PolicySendCommandNoReset(FSC_U8 Command, PolicyState_t nextState, FSC_U8 subIndex)
{
    FSC_U8 Status = STAT_BUSY;
    switch (PDTxStatus)
    {
        case txIdle:
            PolicyTxHeader.word = 0;                                            
            PolicyTxHeader.NumDataObjects = 0;                                  
            PolicyTxHeader.MessageType = Command & 0x0F;                        
            PolicyTxHeader.PortDataRole = PolicyIsDFP;                          
            PolicyTxHeader.PortPowerRole = PolicyIsSource;                      
            PolicyTxHeader.SpecRevision = USBPDSPECREV;                         
            PDTxStatus = txSend;                                                
            break;
        case txSend:
        case txBusy:
        case txWait:
            
            
            break;
        case txSuccess:
            PolicyState = nextState;                                            
            PolicySubIndex = subIndex;
            PDTxStatus = txIdle;                                                
            Status = STAT_SUCCESS;
            break;
        default:                                                                
            PolicyState = peErrorRecovery;                                      
            PolicySubIndex = 0;                                                 
            PDTxStatus = txReset;                                               
            Status = STAT_ERROR;
            break;
    }
    return Status;
}

FSC_U8 PolicySendData(FSC_U8 MessageType, FSC_U8 NumDataObjects, doDataObject_t* DataObjects, PolicyState_t nextState, FSC_U8 subIndex, SopType sop)
{
    FSC_U8 Status = STAT_BUSY;
    FSC_U32 i;
    switch (PDTxStatus)
    {
        case txIdle:
            if (NumDataObjects > 7)
                NumDataObjects = 7;
            PolicyTxHeader.word = 0x0000;                                       

            PolicyTxHeader.NumDataObjects = NumDataObjects;                     
            PolicyTxHeader.MessageType = MessageType & 0x0F;                    
            PolicyTxHeader.PortDataRole = PolicyIsDFP;                          
            PolicyTxHeader.PortPowerRole = PolicyIsSource;                      
            PolicyTxHeader.SpecRevision = USBPDSPECREV;                         
            for (i=0; i<NumDataObjects; i++)                                    
                PolicyTxDataObj[i].object = DataObjects[i].object;              
            if (PolicyState == peSourceSendCaps)                                
                CapsCounter++;                                                  
            PDTxStatus = txSend;                                                
            break;
        case txSend:
        case txBusy:
        case txWait:
        case txCollision:
            
            
            break;
        case txSuccess:
            PolicyState = nextState;                                            
            PolicySubIndex = subIndex;
            PDTxStatus = txIdle;                                                
            Status = STAT_SUCCESS;
            break;
        case txError:                                                           
            if (PolicyState == peSourceSendCaps)                                
                PolicyState = peSourceDiscovery;                                
            else if (PolicyIsSource)                                            
                PolicyState = peSourceSendSoftReset;                            
            else                                                                
                PolicyState = peSinkSendSoftReset;                              
            PolicySubIndex = 0;                                                 
            PDTxStatus = txIdle;                                                
            Status = STAT_ERROR;
            break;
        default:                                                                
            if (PolicyIsSource)
                PolicyState = peSourceSendHardReset;                            
            else
                PolicyState = peSinkSendHardReset;                              
            PolicySubIndex = 0;                                                 
            PDTxStatus = txReset;                                               
            Status = STAT_ERROR;
            break;
    }
    return Status;
}

FSC_U8 PolicySendDataNoReset(FSC_U8 MessageType, FSC_U8 NumDataObjects, doDataObject_t* DataObjects, PolicyState_t nextState, FSC_U8 subIndex)
{
    FSC_U8 Status = STAT_BUSY;
    FSC_U32 i;
    switch (PDTxStatus)
    {
        case txIdle:
            if (NumDataObjects > 7)
                NumDataObjects = 7;
            PolicyTxHeader.word = 0x0000;                                       
            PolicyTxHeader.NumDataObjects = NumDataObjects;                     
            PolicyTxHeader.MessageType = MessageType & 0x0F;                    
            PolicyTxHeader.PortDataRole = PolicyIsDFP;                          
            PolicyTxHeader.PortPowerRole = PolicyIsSource;                      
            PolicyTxHeader.SpecRevision = USBPDSPECREV;                         
            for (i=0; i<NumDataObjects; i++)                                    
                PolicyTxDataObj[i].object = DataObjects[i].object;              
            if (PolicyState == peSourceSendCaps)                                
                CapsCounter++;                                                  
            PDTxStatus = txSend;                                                
            break;
        case txSend:
        case txBusy:
        case txWait:
            
            
            break;
        case txSuccess:
            PolicyState = nextState;                                            
            PolicySubIndex = subIndex;
            PDTxStatus = txIdle;                                                
            Status = STAT_SUCCESS;
            break;
        default:                                                                
            PolicyState = peErrorRecovery;                                      
            PolicySubIndex = 0;                                                 
            PDTxStatus = txReset;                                               
            Status = STAT_ERROR;
            break;
    }
    return Status;
}

void UpdateCapabilitiesRx(FSC_BOOL IsSourceCaps)
{
    FSC_U32 i;
#ifdef FSC_DEBUG
    SourceCapsUpdated = IsSourceCaps;                                           
#endif 
    CapsHeaderReceived.word = PolicyRxHeader.word;                              
    for (i=0; i<CapsHeaderReceived.NumDataObjects; i++)                         
        CapsReceived[i].object = PolicyRxDataObj[i].object;                     
    for (i=CapsHeaderReceived.NumDataObjects; i<7; i++)                         
        CapsReceived[i].object = 0;                                             
    PartnerCaps.object = CapsReceived[0].object;
}


void policyBISTReceiveMode(void)    
{
    
    
    
}

void policyBISTFrameReceived(void)  
{
    
    
    
}


void policyBISTCarrierMode2(void)
{
    switch (PolicySubIndex)
    {
        default:
        case 0:
            Registers.Control.BIST_MODE2 = 1;                                   
            DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
            Registers.Control.TX_START = 1;                                             
            DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);                    
            Registers.Control.TX_START = 0;                                             
            PolicyStateTimer = tBISTContMode;                                        
            PolicySubIndex = 1;
            break;
        case 1:
            if(PolicyStateTimer == 0)                                                   
            {
                Registers.Control.BIST_MODE2 = 0;                                           
                DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
                PolicyStateTimer = tGoodCRCDelay;                               
                PolicySubIndex++;
            }
        case 2:
            if(PolicyStateTimer == 0)                                                   
            {
                if (PolicyIsSource)                                                     
                {
#ifdef FSC_HAVE_SRC
                    PolicyState = peSourceSendHardReset;                                
                    PolicySubIndex = 0;
#endif 
                }
                else                                                                    
                {
#ifdef FSC_HAVE_SNK
                    PolicyState = peSinkSendHardReset;                                
                    PolicySubIndex = 0;
#endif 
                }
            }
            break;
    }

}

void policyBISTTestData(void)
{
    DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
}

#ifdef FSC_HAVE_VDM

void InitializeVdmManager(void)
{
	initializeVdm();

	
	vdmm.req_id_info 		= &vdmRequestIdentityInfo;
	vdmm.req_svid_info 		= &vdmRequestSvidInfo;
	vdmm.req_modes_info 	= &vdmRequestModesInfo;
	vdmm.enter_mode_result  = &vdmEnterModeResult;
	vdmm.exit_mode_result   = &vdmExitModeResult;
	vdmm.inform_id 			= &vdmInformIdentity;
	vdmm.inform_svids 		= &vdmInformSvids;
	vdmm.inform_modes 		= &vdmInformModes;
	vdmm.inform_attention   = &vdmInformAttention;
	vdmm.req_mode_entry		= &vdmModeEntryRequest;
	vdmm.req_mode_exit		= &vdmModeExitRequest;
}

void convertAndProcessVdmMessage(SopType sop)
{
    FSC_U32 i;
    
    
    FSC_U32 vdm_arr[7] = {0};
    for (i = 0; i < PolicyRxHeader.NumDataObjects; i++) {
        vdm_arr[i] = 0;
        vdm_arr[i] = PolicyRxDataObj[i].object;
    }
    processVdmMessage(sop, vdm_arr, PolicyRxHeader.NumDataObjects);
}

void sendVdmMessage(SopType sop, FSC_U32* arr, FSC_U32 length, PolicyState_t next_ps) {
    FSC_U32 i;
    
    
    vdm_msg_length = length;
    vdm_next_ps = next_ps;
    for (i = 0; i < vdm_msg_length; i++) {
        vdm_msg_obj[i].object = arr[i];
    }
    sendingVdmData = TRUE;
    ProtocolCheckRxBeforeTx = TRUE;
    VdmTimerStarted = FALSE;
    PolicyState = peGiveVdm;
}

void doVdmCommand(void)
{
    FSC_U32 command;
    FSC_U32 svid;
    FSC_U32 mode_index;
    SopType sop;


    command = PDTransmitObjects[0].byte[0] & 0x1F;
    svid = 0;
    svid |= (PDTransmitObjects[0].byte[3] << 8);
    svid |= (PDTransmitObjects[0].byte[2] << 0);

    mode_index = 0;
    mode_index = PDTransmitObjects[0].byte[1] & 0x7;

    
    sop = SOP_TYPE_SOP;

#ifdef FSC_HAVE_DP
    if (svid == DP_SID) {
        if (command == DP_COMMAND_STATUS) {
            requestDpStatus();
        } else if (command == DP_COMMAND_CONFIG) {
            DisplayPortConfig_t temp;
            temp.word = PDTransmitObjects[1].object;
            requestDpConfig(temp);
        }
    }
#endif 

    if (command == DISCOVER_IDENTITY) {
        requestDiscoverIdentity(sop);
    } else if (command == DISCOVER_SVIDS) {
        requestDiscoverSvids(sop);
    } else if (command == DISCOVER_MODES) {
        requestDiscoverModes(sop, svid);
    } else if (command == ENTER_MODE) {
        requestEnterMode(sop, svid, mode_index);
    } else if (command == EXIT_MODE) {
        requestExitMode(sop, svid, mode_index);
    }

}

void autoVdmDiscovery (void)
{
#ifdef FSC_DEBUG
    
    if (GetUSBPDBufferNumBytes() != 0) return;
#endif 

    if (!PolicyIsDFP) return; 

    if (PDTxStatus == txIdle) { 
        switch (AutoVdmState) {
            case AUTO_VDM_INIT:
            case AUTO_VDM_DISCOVER_ID_PP:
                requestDiscoverIdentity(SOP_TYPE_SOP);
                AutoVdmState = AUTO_VDM_DISCOVER_SVIDS_PP;
                break;
            case AUTO_VDM_DISCOVER_SVIDS_PP:
                requestDiscoverSvids(SOP_TYPE_SOP);
                AutoVdmState = AUTO_VDM_DISCOVER_MODES_PP;
                break;
            case AUTO_VDM_DISCOVER_MODES_PP:
                if (auto_mode_disc_tracker == core_svid_info.num_svids) {
#ifdef FSC_HAVE_DP
                    AutoVdmState = AUTO_VDM_ENTER_DP_MODE_PP;
#else
                    AutoVdmState = AUTO_VDM_DONE;
#endif 
                    auto_mode_disc_tracker = 0;
                } else {
                    requestDiscoverModes(SOP_TYPE_SOP, core_svid_info.svids[auto_mode_disc_tracker]);
                    auto_mode_disc_tracker++;
                }
                break;
#ifdef FSC_HAVE_DP
            case AUTO_VDM_ENTER_DP_MODE_PP:
                if (AutoDpModeEntryObjPos > 0) {
                    requestEnterMode(SOP_TYPE_SOP, DP_SID, AutoDpModeEntryObjPos);
                    AutoVdmState = AUTO_VDM_DP_GET_STATUS;
                } else {
                    AutoVdmState = AUTO_VDM_DONE;
                }
                break;
            case AUTO_VDM_DP_GET_STATUS:
                if (DpModeEntered) {
                    requestDpStatus();
                }
                AutoVdmState = AUTO_VDM_DONE;
                break;
#endif 
            default:
                AutoVdmState = AUTO_VDM_DONE;
                break;
        }
    }
}

#endif 

SopType TokenToSopType(FSC_U8 data)
{
    SopType ret;
    
    if ((data & 0b11100000) == 0b11100000) {
        ret = SOP_TYPE_SOP;
    } else if ((data & 0b11100000) == 0b11000000) {
        ret = SOP_TYPE_SOP1;
    } else if ((data & 0b11100000) == 0b10100000) {
        ret = SOP_TYPE_SOP2;
    } else if ((data & 0b11100000) == 0b10000000) {
        ret = SOP_TYPE_SOP1_DEBUG;
    } else if ((data & 0b11100000) == 0b01100000) {
        ret = SOP_TYPE_SOP2_DEBUG;
    } else {
        ret = SOP_TYPE_ERROR;
    }
    return ret;
}

void resetLocalHardware(void)
{
    FSC_U8 data = 0x20;
    DeviceWrite(regReset, 1, &data);   

    DeviceRead(regSwitches1, 1, &Registers.Switches.byte[1]);  
    DeviceRead(regSlice, 1, &Registers.Slice.byte);
    DeviceRead(regControl0, 1, &Registers.Control.byte[0]);
    DeviceRead(regControl1, 1, &Registers.Control.byte[1]);
    DeviceRead(regControl3, 1, &Registers.Control.byte[3]);
    DeviceRead(regMask, 1, &Registers.Mask.byte);
    DeviceRead(regMaska, 1, &Registers.MaskAdv.byte[0]);
    DeviceRead(regMaskb, 1, &Registers.MaskAdv.byte[1]);
    DeviceRead(regStatus0a, 2, &Registers.Status.byte[0]);
    DeviceRead(regStatus0, 2, &Registers.Status.byte[4]);
}

void processDMTBIST(void)
{
    FSC_U8 bdo = PolicyRxDataObj[0].byte[3]>>4;
    switch (bdo)
    {
        case BDO_BIST_Carrier_Mode_2:
            if(CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage == 100)
            {
                PolicyState = PE_BIST_Carrier_Mode_2;
                PolicySubIndex = 0;
                ProtocolState = PRLIdle;
            }
            break;
        case BDO_BIST_Test_Data:                                                
            if(CapsSource[USBPDContract.FVRDO.ObjectPosition-1].FPDOSupply.Voltage == 100)
            {
                Registers.Mask.byte = 0xFF;
                DeviceWrite(regMask, 1, &Registers.Mask.byte);
                Registers.MaskAdv.byte[0] = 0xFF;
                Registers.MaskAdv.M_HARDRST = 0;
                DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
                Registers.MaskAdv.M_GCRCSENT = 1;
                DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
                Registers.Control.RX_FLUSH = 1;                                     
                PolicyState = PE_BIST_Test_Data;
                ProtocolState = PRLDisabled;                                        
            }
            break;
        default:
            break;
    }
}

#ifdef FSC_DEBUG
void SendUSBPDHardReset(void)
{
    if (PolicyIsSource)                                                         
        PolicyState = peSourceSendHardReset;                                    
    else                                                                        
        PolicyState = peSinkSendHardReset;                                      
    PolicySubIndex = 0;
    PDTxStatus = txIdle;                                                        
}

#ifdef FSC_HAVE_SRC
void WriteSourceCapabilities(FSC_U8* abytData)
{
    FSC_U32 i, j;
    sopMainHeader_t Header = {0};
    Header.byte[0] = *abytData++;                                               
    Header.byte[1] = *abytData++;                                               
    if ((Header.NumDataObjects > 0) && (Header.MessageType == DMTSourceCapabilities))   
    {
        CapsHeaderSource.word = Header.word;                                    
        for (i=0; i<CapsHeaderSource.NumDataObjects; i++)                       
        {
            for (j=0; j<4; j++)                                                 
                CapsSource[i].byte[j] = *abytData++;                            
        }
        if (PolicyIsSource)                                                     
        {
            PDTransmitHeader.word = CapsHeaderSource.word;                      
            USBPDTxFlag = TRUE;                                                 
            SourceCapsUpdated = TRUE;                                           
        }
    }
}

void ReadSourceCapabilities(FSC_U8* abytData)
{
    FSC_U32 i, j;
    *abytData++ = CapsHeaderSource.byte[0];
    *abytData++ = CapsHeaderSource.byte[1];
    for (i=0; i<CapsHeaderSource.NumDataObjects; i++)
    {
        for (j=0; j<4; j++)
            *abytData++ = CapsSource[i].byte[j];
    }
}
#endif 

#ifdef FSC_HAVE_SNK
void WriteSinkCapabilities(FSC_U8* abytData)
{
    FSC_U32 i, j;
    sopMainHeader_t Header = {0};
    Header.byte[0] = *abytData++;                                               
    Header.byte[1] = *abytData++;                                               
    if ((Header.NumDataObjects > 0) && (Header.MessageType == DMTSinkCapabilities))   
    {
        CapsHeaderSink.word = Header.word;                                      
        for (i=0; i<CapsHeaderSink.NumDataObjects; i++)                         
        {
            for (j=0; j<4; j++)                                                 
                CapsSink[i].byte[j] = *abytData++;                              
        }
        
    }
}

void ReadSinkCapabilities(FSC_U8* abytData)
{
    FSC_U32 i, j;
    *abytData++ = CapsHeaderSink.byte[0];
    *abytData++ = CapsHeaderSink.byte[1];
    for (i=0; i<CapsHeaderSink.NumDataObjects; i++)
    {
        for (j=0; j<4; j++)
            *abytData++ = CapsSink[i].byte[j];
    }
}

void WriteSinkRequestSettings(FSC_U8* abytData)
{
    FSC_U32 uintPower;
    SinkGotoMinCompatible = *abytData & 0x01 ? TRUE : FALSE;
    SinkUSBSuspendOperation = *abytData & 0x02 ? TRUE : FALSE;
    SinkUSBCommCapable = *abytData++ & 0x04 ? TRUE : FALSE;
    SinkRequestMaxVoltage = (FSC_U32) *abytData++;
    SinkRequestMaxVoltage |= ((FSC_U32) (*abytData++) << 8);                     
    uintPower = (FSC_U32) *abytData++;
    uintPower |= ((FSC_U32) (*abytData++) << 8);
    uintPower |= ((FSC_U32) (*abytData++) << 16);
    uintPower |= ((FSC_U32) (*abytData++) << 24);
    SinkRequestOpPower = uintPower;                                             
    uintPower = (FSC_U32) *abytData++;
    uintPower |= ((FSC_U32) (*abytData++) << 8);
    uintPower |= ((FSC_U32) (*abytData++) << 16);
    uintPower |= ((FSC_U32) (*abytData++) << 24);
    SinkRequestMaxPower = uintPower;                                            
    
}

void ReadSinkRequestSettings(FSC_U8* abytData)
{
    *abytData = SinkGotoMinCompatible ? 0x01 : 0;
    *abytData |= SinkUSBSuspendOperation ? 0x02 : 0;
    *abytData++ |= SinkUSBCommCapable ? 0x04 : 0;
    *abytData++ = (FSC_U8) (SinkRequestMaxVoltage & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestMaxVoltage & 0xFF) >> 8);
    *abytData++ = (FSC_U8) (SinkRequestOpPower & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestOpPower >> 8) & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestOpPower >> 16) & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestOpPower >> 24) & 0xFF);
    *abytData++ = (FSC_U8) (SinkRequestMaxPower & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestMaxPower >> 8) & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestMaxPower >> 16) & 0xFF);
    *abytData++ = (FSC_U8) ((SinkRequestMaxPower >> 24) & 0xFF);
}
#endif 

FSC_BOOL GetPDStateLog(FSC_U8 * data){   
    FSC_U32 i;
    FSC_U32 entries = PDStateLog.Count;
    FSC_U16 state_temp;
    FSC_U16 time_tms_temp;
    FSC_U16 time_s_temp;


    for(i=0; ((i<entries) && (i<12)); i++)
    {
        ReadStateLog(&PDStateLog, &state_temp, &time_tms_temp, &time_s_temp);

        data[i*5+1] = state_temp;
        data[i*5+2] = (time_tms_temp>>8);
        data[i*5+3] = (FSC_U8)time_tms_temp;
        data[i*5+4] = (time_s_temp)>>8;
        data[i*5+5] = (FSC_U8)time_s_temp;
    }

    data[0] = i;    

    return TRUE;
}

void ProcessReadPDStateLog(FSC_U8* MsgBuffer, FSC_U8* retBuffer)
{
    if (MsgBuffer[1] != 0)
    {
        retBuffer[1] = 0x01;             
        return;
    }

    GetPDStateLog(&retBuffer[3]);   
}

void ProcessPDBufferRead(FSC_U8* MsgBuffer, FSC_U8* retBuffer)
{
    if (MsgBuffer[1] != 0)
        retBuffer[1] = 0x01;                                             
    else
    {
        retBuffer[4] = GetUSBPDBufferNumBytes();                         
        retBuffer[5] = ReadUSBPDBuffer((FSC_U8*)&retBuffer[6], 58); 
    }
}

#endif 

void EnableUSBPD(void)
{
    FSC_BOOL enabled = blnSMEnabled;                                            
    if (USBPDEnabled)                                                           
        return;                                                                 
    else
    {
        DisableTypeCStateMachine();                                             
        USBPDEnabled = TRUE;                                                    
        if (enabled)                                                            
            EnableTypeCStateMachine();                                          
    }
}

void DisableUSBPD(void)
{
    FSC_BOOL enabled = blnSMEnabled;                                            
    if (!USBPDEnabled)                                                          
        return;                                                                 
    else
    {
        DisableTypeCStateMachine();                                             
        USBPDEnabled = FALSE;                                                   
        if (enabled)                                                            
            EnableTypeCStateMachine();                                          
    }
}

void SetVbusTransitionTime(FSC_U32 time_ms) {
    VbusTransitionTime = time_ms * TICK_SCALE_TO_MS;
}
