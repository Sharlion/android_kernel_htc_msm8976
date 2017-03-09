/*********************************************************************
* FileName:        PDPolicy.h
* Dependencies:    See INCLUDES section below
* Processor:       PIC32
* Compiler:        XC32
* Company:         Fairchild Semiconductor
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

#ifndef _PDPOLICY_H_
#define	_PDPOLICY_H_

#include "platform.h"
#include "PD_Types.h"

extern FSC_BOOL                 USBPDTxFlag;                                    
extern sopMainHeader_t          PDTransmitHeader;                               

#ifdef FSC_HAVE_SNK
extern sopMainHeader_t          CapsHeaderSink;                                 
extern doDataObject_t           CapsSink[7];                                    

extern doDataObject_t           SinkRequest;                                    
extern FSC_U32                  SinkRequestMaxVoltage;                          
extern FSC_U32                  SinkRequestMaxPower;                            
extern FSC_U32                  SinkRequestOpPower;                             
extern FSC_BOOL                 SinkGotoMinCompatible;                          
extern FSC_BOOL                 SinkUSBSuspendOperation;                        
extern FSC_BOOL                 SinkUSBCommCapable;                             
#endif 

#ifdef FSC_HAVE_SRC
extern sopMainHeader_t          CapsHeaderSource;                               
extern doDataObject_t           CapsSource[7];                                  
#endif 

extern sopMainHeader_t          CapsHeaderReceived;                             
extern doDataObject_t           PDTransmitObjects[7];                           
extern doDataObject_t           CapsReceived[7];                                
extern doDataObject_t           USBPDContract;                                  

#ifdef FSC_DEBUG
extern FSC_BOOL                 SourceCapsUpdated;                              
#endif 

extern PolicyState_t            PolicyState;                                    
extern FSC_U8                   PolicySubIndex;                                 
extern FSC_BOOL                 PolicyIsSource;                                 
extern FSC_BOOL                 PolicyIsDFP;                                    
extern FSC_BOOL                 PolicyHasContract;                              
extern FSC_U32                  VbusTransitionTime;                             

extern sopMainHeader_t          PolicyRxHeader;                                 
extern sopMainHeader_t          PolicyTxHeader;                                 
extern doDataObject_t           PolicyRxDataObj[7];                             
extern doDataObject_t           PolicyTxDataObj[7];                             

extern FSC_U32                  NoResponseTimer;                                

#ifdef FSC_HAVE_VDM
extern FSC_U32                  VdmTimer;
extern FSC_BOOL                 VdmTimerStarted;
#endif 

void PolicyTickAt100us(void);
void InitializePDPolicyVariables(void);
void USBPDEnable(FSC_BOOL DeviceUpdate, FSC_BOOL TypeCDFP);
void USBPDDisable(FSC_BOOL DeviceUpdate);
void USBPDPolicyEngine(void);
void PolicyErrorRecovery(void);

#ifdef FSC_HAVE_SRC
void PolicySourceSendHardReset(void);
void PolicySourceSoftReset(void);
void PolicySourceSendSoftReset(void);
void PolicySourceStartup(void);
void PolicySourceDiscovery(void);
void PolicySourceSendCaps(void);
void PolicySourceDisabled(void);
void PolicySourceTransitionDefault(void);
void PolicySourceNegotiateCap(void);
void PolicySourceTransitionSupply(void);
void PolicySourceCapabilityResponse(void);
void PolicySourceReady(void);
void PolicySourceGiveSourceCap(void);
void PolicySourceGetSourceCap(void);
void PolicySourceGetSinkCap(void);
void PolicySourceSendPing(void);
void PolicySourceGotoMin(void);
void PolicySourceGiveSinkCap(void);
void PolicySourceSendDRSwap(void);
void PolicySourceEvaluateDRSwap(void);
void PolicySourceSendVCONNSwap(void);
void PolicySourceSendPRSwap(void);
void PolicySourceEvaluatePRSwap(void);
void PolicySourceWaitNewCapabilities(void);
void PolicySourceEvaluateVCONNSwap(void);
#endif 

#ifdef FSC_HAVE_SNK
void PolicySinkSendHardReset(void);
void PolicySinkSoftReset(void);
void PolicySinkSendSoftReset(void);
void PolicySinkTransitionDefault(void);
void PolicySinkStartup(void);
void PolicySinkDiscovery(void);
void PolicySinkWaitCaps(void);
void PolicySinkEvaluateCaps(void);
void PolicySinkSelectCapability(void);
void PolicySinkTransitionSink(void);
void PolicySinkReady(void);
void PolicySinkGiveSinkCap(void);
void PolicySinkGetSinkCap(void);
void PolicySinkGiveSourceCap(void);
void PolicySinkGetSourceCap(void);
void PolicySinkSendDRSwap(void);
void PolicySinkEvaluateDRSwap(void);
void PolicySinkEvaluateVCONNSwap(void);
void PolicySinkSendPRSwap(void);
void PolicySinkEvaluatePRSwap(void);
#endif

void PolicyInvalidState(void);
void policyBISTReceiveMode(void);
void policyBISTFrameReceived(void);
void policyBISTCarrierMode2(void);
void policyBISTTestData(void);

FSC_BOOL PolicySendHardReset(PolicyState_t nextState, FSC_U32 delay);
FSC_U8 PolicySendCommand(FSC_U8 Command, PolicyState_t nextState, FSC_U8 subIndex);
FSC_U8 PolicySendCommandNoReset(FSC_U8 Command, PolicyState_t nextState, FSC_U8 subIndex);
FSC_U8 PolicySendData(FSC_U8 MessageType, FSC_U8 NumDataObjects, doDataObject_t* DataObjects, PolicyState_t nextState, FSC_U8 subIndex, SopType sop);
FSC_U8 PolicySendDataNoReset(FSC_U8 MessageType, FSC_U8 NumDataObjects, doDataObject_t* DataObjects, PolicyState_t nextState, FSC_U8 subIndex);
void UpdateCapabilitiesRx(FSC_BOOL IsSourceCaps);

void processDMTBIST(void);

#ifdef FSC_HAVE_VDM
void InitializeVdmManager(void);
void convertAndProcessVdmMessage(SopType sop);
void sendVdmMessage(SopType sop, FSC_U32 * arr, FSC_U32 length, PolicyState_t next_ps);
void doVdmCommand(void);
void doDiscoverIdentity(void);
void doDiscoverSvids(void);
void PolicyGiveVdm(void);
void PolicyVdm(void);
void autoVdmDiscovery(void);
#endif 

SopType TokenToSopType(FSC_U8 data);

#ifdef FSC_DEBUG
#ifdef FSC_HAVE_SRC
void WriteSourceCapabilities(FSC_U8* abytData);
void ReadSourceCapabilities(FSC_U8* abytData);
#endif 

#ifdef FSC_HAVE_SNK
void WriteSinkCapabilities(FSC_U8* abytData);
void ReadSinkCapabilities(FSC_U8* abytData);
void WriteSinkRequestSettings(FSC_U8* abytData);
void ReadSinkRequestSettings(FSC_U8* abytData);

#endif 

FSC_BOOL GetPDStateLog(FSC_U8 * data);
void ProcessReadPDStateLog(FSC_U8* MsgBuffer, FSC_U8* retBuffer);
void ProcessPDBufferRead(FSC_U8* MsgBuffer, FSC_U8* retBuffer);

#endif 

void EnableUSBPD(void);
void DisableUSBPD(void);

void SetVbusTransitionTime(FSC_U32 time_ms);

#endif	
