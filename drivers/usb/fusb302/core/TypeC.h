/*********************************************************************
 * FileName:        TypeC.h
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC32
 * Compiler:        XC32
 * Company:         Fairchild Semiconductor
 *
 * Author           Date          Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * M. Smith         12/04/2014    Initial Version
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the “Company”)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 ********************************************************************/

#ifndef __FSC_TYPEC_H__
#define	__FSC_TYPEC_H__

#include "platform.h"
#include "fusb30X.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "TypeC_Types.h"

#define tAMETimeout     900 * 10
#define tCCDebounce     120 * 10
#define tPDDebounce     15 * 10
#define tDRPTry         125 * 10
#define tDRPTryWait     600 * 10
#define tErrorRecovery  30 * 10

#define tDeviceToggle   3 * 10      
#define tTOG2           30 * 10     

#define T_TIMER_DISABLE (0xFFFF)
#define SLEEP_DELAY     100

typedef enum {
    Source = 0,
    Sink
} SourceOrSink;

extern DeviceReg_t              Registers;                                      
extern FSC_BOOL                 USBPDActive;                                    
extern FSC_BOOL                 USBPDEnabled;                                   
extern FSC_U32                  PRSwapTimer;                                    
extern USBTypeCPort             PortType;                                       
extern FSC_BOOL                 blnCCPinIsCC1;                                  
extern FSC_BOOL                 blnCCPinIsCC2;                                  
extern FSC_BOOL                 blnSMEnabled;                                   
extern ConnectionState          ConnState;                                      
extern FSC_BOOL                 IsHardReset;                                    
extern FSC_BOOL                 IsPRSwap;
extern FSC_BOOL                 PolicyHasContract;                              
extern PolicyState_t            PolicyState;
void TypeCTickAt100us(void);

void InitializeRegisters(void);
void InitializeTypeCVariables(void);
void InitializeTypeC(void);

void DisableTypeCStateMachine(void);
void EnableTypeCStateMachine(void);

void StateMachineTypeC(void);
void StateMachineDisabled(void);
void StateMachineErrorRecovery(void);
void StateMachineDelayUnattached(void);
void StateMachineUnattached(void);

#ifdef FSC_HAVE_SNK
void StateMachineAttachWaitSink(void);
void StateMachineAttachedSink(void);
void StateMachineTryWaitSink(void);
void stateMachineTrySink(void);
#endif 

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(void);
void stateMachineTryWaitSource(void);
void stateMachineUnattachedSource(void);             
void StateMachineAttachedSource(void);
void StateMachineTrySource(void);
#endif 

#ifdef FSC_HAVE_ACCMODE
void StateMachineAttachWaitAccessory(void);
void StateMachineDebugAccessory(void);
void StateMachineAudioAccessory(void);
void StateMachinePoweredAccessory(void);
void StateMachineUnsupportedAccessory(void);
#endif 

void SetStateErrorRecovery(void);
void SetStateDelayUnattached(void);
void SetStateUnattached(void);

#ifdef FSC_HAVE_SNK
void SetStateAttachWaitSink(void);
void SetStateAttachedSink(void);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSink(void);
#endif 
void SetStateTryWaitSink(void);
void SetStateTrySink(void);
#endif 

#ifdef FSC_HAVE_SRC
void SetStateAttachWaitSource(void);
void SetStateAttachedSource(void);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSource(void);
#endif 
void SetStateTrySource(void);
void SetStateTryWaitSource(void);
void SetStateUnattachedSource(void);             
#endif 

#ifdef FSC_HAVE_ACCMODE
void SetStateAttachWaitAccessory(void);
void SetStateDebugAccessory(void);
void SetStateAudioAccessory(void);
void SetStatePoweredAccessory(void);
void SetStateUnsupportedAccessory(void);
#endif 

void updateSourceCurrent(void);
void updateSourceMDACHigh(void);
void updateSourceMDACLow(void);

void ToggleMeasureCC1(void);
void ToggleMeasureCC2(void);

CCTermType DecodeCCTermination(void);
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(void);
#endif 
#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(void);
#endif 

void UpdateSinkCurrent(CCTermType Termination);
FSC_BOOL VbusVSafe0V (void);

#ifdef FSC_HAVE_SNK
FSC_BOOL VbusUnder5V(void);
#endif 

FSC_BOOL isVSafe5V(void);
FSC_BOOL isVBUSOverVoltage(FSC_U8 vbusMDAC);
void resetDebounceVariables(void);
void setDebounceVariablesCC1(CCTermType term);
void setDebounceVariablesCC2(CCTermType term);
void debounceCC(void);

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void DetectCCPinSource(void);
void peekCC1Source(void);
void peekCC2Source(void);
#endif 

#ifdef FSC_HAVE_SNK
void DetectCCPinSink(void);
void peekCC1Sink(void);
void peekCC2Sink(void);
#endif 

#ifdef FSC_HAVE_ACCMODE
void checkForAccessory(void);
#endif 

#ifdef FSC_DEBUG
void LogTickAt100us(void);
void SetStateDisabled(void);

void ConfigurePortType(FSC_U8 Control);
void UpdateCurrentAdvert(FSC_U8 Current);
void GetDeviceTypeCStatus(FSC_U8 abytData[]);
FSC_U8 GetTypeCSMControl(void);
FSC_U8 GetCCTermination(void);

FSC_BOOL GetLocalRegisters(FSC_U8 * data, FSC_S32 size); 
FSC_BOOL GetStateLog(FSC_U8 * data);   

void ProcessTypeCPDStatus(FSC_U8* MsgBuffer, FSC_U8* retBuffer);
void ProcessTypeCPDControl(FSC_U8* MsgBuffer, FSC_U8* retBuffer);
void ProcessLocalRegisterRequest(FSC_U8* MsgBuffer, FSC_U8* retBuffer);
void ProcessSetTypeCState(FSC_U8* MsgBuffer, FSC_U8* retBuffer);
void ProcessReadTypeCStateLog(FSC_U8* MsgBuffer, FSC_U8* retBuffer);
void setAlternateModes(FSC_U8 mode);
FSC_U8 getAlternateModes(void);
#endif 

#endif	

