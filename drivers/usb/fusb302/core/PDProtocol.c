/****************************************************************************
 * FileName:        PDProtocol.c
 * Processor:       PIC32MX250F128B
 * Compiler:        MPLAB XC32
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
 *****************************************************************************/
#include "PDProtocol.h"
#include "PDPolicy.h"
#include "TypeC.h"
#include "fusb30X.h"

#include "platform.h"
#include "PD_Types.h"

#ifdef FSC_HAVE_VDM
#include "vdm/vdm_callbacks.h"
#include "vdm/vdm_callbacks_defs.h"
#include "vdm/vdm.h"
#include "vdm/vdm_types.h"
#include "vdm/bitfield_translators.h"
#endif 

#define FSC_PROTOCOL_BUFFER_SIZE 64                                             

extern FSC_BOOL                 g_Idle;                                         
extern FSC_U32                  PolicyStateTimer;                               

#ifdef FSC_DEBUG
extern volatile FSC_U16         Timer_S;                                        
extern volatile FSC_U16         Timer_tms;                                      
extern StateLog                 PDStateLog;                                     

static FSC_U8                   USBPDBuf[PDBUFSIZE];                            
static FSC_U8                   USBPDBufStart;                                  
static FSC_U8                   USBPDBufEnd;                                    
static FSC_BOOL                 USBPDBufOverflow;                               

#ifdef FM150911A
FSC_U8                          manualRetries = 1;                              
#else
FSC_U8                          manualRetries = 0;                              
#endif  
FSC_U8                          nTries = 4;                                   
#endif 

       ProtocolState_t          ProtocolState;                                  
       PDTxStatus_t             PDTxStatus;                                     
static FSC_U8                   MessageIDCounter;                               
static FSC_U8                   MessageID;                                      
       FSC_BOOL                 ProtocolMsgRx;                                  
       SopType                  ProtocolMsgRxSop;                               
static FSC_U8                   ProtocolTxBytes;                                
static FSC_U8                   ProtocolTxBuffer[FSC_PROTOCOL_BUFFER_SIZE];     
static FSC_U8                   ProtocolRxBuffer[FSC_PROTOCOL_BUFFER_SIZE];     
static FSC_U16                  ProtocolTimer;                                  
static FSC_U8                   ProtocolCRC[4];
       FSC_BOOL                 ProtocolCheckRxBeforeTx;

void ProtocolTickAt100us( void )
{
    if( !USBPDActive )
        return;

    if (ProtocolTimer)                                                          
        ProtocolTimer--;                                                        
}

void InitializePDProtocolVariables(void)
{
}


void USBPDProtocol(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
    if(g_Idle == TRUE)
    {
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
    }
#endif
    if (Registers.Status.I_HARDRST)
    {
        ResetProtocolLayer(TRUE);                                               
        if (PolicyIsSource)                                                     
        {
            PolicyStateTimer = tPSHardReset;
            PolicyState = peSourceTransitionDefault;                            
        }
        else                                                                    
            PolicyState = peSinkTransitionDefault;                              
        PolicySubIndex = 0;
#ifdef FSC_DEBUG
        StoreUSBPDToken(FALSE, pdtHardReset);                                   
#endif 
    }
    else
    {
        switch (ProtocolState)
        {
            case PRLReset:
                ProtocolSendHardReset();                                        
                PDTxStatus = txWait;                                            
                ProtocolState = PRLResetWait;                                   
                ProtocolTimer = tBMCTimeout;                                    
                break;
            case PRLResetWait:                                                  
                ProtocolResetWait();
                break;
            case PRLIdle:                                                       
                ProtocolIdle();
                break;
            case PRLTxSendingMessage:                                           
                ProtocolSendingMessage();                                       
                break;
            case PRLTxVerifyGoodCRC:                                            
                ProtocolVerifyGoodCRC();
                break;
            case PRL_BIST_Rx_Reset_Counter:                                     
                protocolBISTRxResetCounter();
                break;
            case PRL_BIST_Rx_Test_Frame:                                        
                protocolBISTRxTestFrame();
                break;
            case PRL_BIST_Rx_Error_Count:                                       
                protocolBISTRxErrorCount();
                break;
            case PRL_BIST_Rx_Inform_Policy:                                     
                protocolBISTRxInformPolicy();
                break;
            case PRLDisabled:                                                   
                break;
            default:
                break;
        }
    }
}

void ProtocolIdle(void)
{
    if (PDTxStatus == txReset)                                                  
        ProtocolState = PRLReset;                                               
#ifndef FSC_INTERRUPT_TRIGGERED
    else if (Registers.Status.I_GCRCSENT)                                       
#else
    else if (!Registers.Status.RX_EMPTY)                                       
#endif
    {

        ProtocolGetRxPacket();                                                  
        PDTxStatus = txIdle;                                                    
        Registers.Status.I_GCRCSENT = 0;
    }
    else if (PDTxStatus == txSend)                                              
    {
        ProtocolTransmitMessage();                                              
    }
}

void ProtocolResetWait(void)
{
    if (Registers.Status.I_HARDSENT)                                            
    {
        ProtocolState = PRLIdle;                                                
        PDTxStatus = txSuccess;                                                 
    }
    else if (ProtocolTimer == 0)                                                
    {
        ProtocolState = PRLIdle;                                                
        PDTxStatus = txSuccess;                                                  
    }
}

void ProtocolGetRxPacket(void)
{
    FSC_U32 i, j;
    FSC_U8 data[3];
    SopType rx_sop;
#ifdef FSC_DEBUG
    FSC_U8 sop_token = 0;
#endif 

    DeviceRead(regFIFO, 3, &data[0]);                                          
    PolicyRxHeader.byte[0] = data[1];
    PolicyRxHeader.byte[1] = data[2];
    
    PolicyTxHeader.word = 0;                                                    
    PolicyTxHeader.NumDataObjects = 0;                                          
    PolicyTxHeader.MessageType = CMTGoodCRC;                                    
    PolicyTxHeader.PortDataRole = PolicyIsDFP;                                  
    PolicyTxHeader.PortPowerRole = PolicyIsSource;                              
    PolicyTxHeader.SpecRevision = USBPDSPECREV;                                 
    PolicyTxHeader.MessageID = PolicyRxHeader.MessageID;                        

    
    rx_sop = TokenToSopType(data[0]);

    if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTSoftReset))
    {
        MessageIDCounter = 0;                                               
        MessageID = 0xFF;                                                   
        ProtocolMsgRxSop = rx_sop;
        ProtocolMsgRx = TRUE;                                               
#ifdef FSC_DEBUG
        SourceCapsUpdated = TRUE;                                           
#endif 
    }
    else if (PolicyRxHeader.MessageID != MessageID)                         
    {
        MessageID = PolicyRxHeader.MessageID;                               
        ProtocolMsgRxSop = rx_sop;
        ProtocolMsgRx = TRUE;                                               
    }

    if (PolicyRxHeader.NumDataObjects > 0)                                      
    {
        DeviceRead(regFIFO, ((PolicyRxHeader.NumDataObjects<<2)), &ProtocolRxBuffer[0]); 
        for (i=0; i<PolicyRxHeader.NumDataObjects; i++)                         
        {
            for (j=0; j<4; j++)                                                 
                PolicyRxDataObj[i].byte[j] = ProtocolRxBuffer[j + (i<<2)];      
        }
    }

    DeviceRead(regFIFO, 4, &ProtocolCRC[0]);                                   

#ifdef FSC_DEBUG
    StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], FALSE, data[0]);     

    if (rx_sop == SOP_TYPE_SOP) sop_token = 0xE0;
    else if (rx_sop == SOP_TYPE_SOP1) sop_token = 0xC0;
    else if (rx_sop == SOP_TYPE_SOP2) sop_token = 0xA0;
    else if (rx_sop == SOP_TYPE_SOP1_DEBUG) sop_token = 0x80;
    else if (rx_sop == SOP_TYPE_SOP2_DEBUG) sop_token = 0x60;

	StoreUSBPDMessage(PolicyTxHeader, &PolicyTxDataObj[0], TRUE, sop_token);    

    
    WriteStateLog(&PDStateLog, dbgGetRxPacket, PolicyRxHeader.byte[0], PolicyRxHeader.byte[1]);                     
#endif 
}

void ProtocolTransmitMessage(void)
{
    FSC_U32 i, j;
    sopMainHeader_t temp_PolicyTxHeader = {0};

#ifdef FSC_DEBUG
    FSC_U8 sop_token = 0xE0;
#endif 

    ProtocolFlushTxFIFO();                                                      

    ProtocolLoadSOP();

    temp_PolicyTxHeader.word = PolicyTxHeader.word;
    temp_PolicyTxHeader.word &= 0x7FFF;
    temp_PolicyTxHeader.word &= 0xFFEF;

    if ((temp_PolicyTxHeader.NumDataObjects == 0) && (temp_PolicyTxHeader.MessageType == CMTSoftReset))
    {
        MessageIDCounter = 0;                                
        MessageID = 0xFF;                                                       
#ifdef FSC_DEBUG
        SourceCapsUpdated = TRUE;                                               
#endif 
    }
    temp_PolicyTxHeader.MessageID = MessageIDCounter;        

    ProtocolTxBuffer[ProtocolTxBytes++] = PACKSYM | (2+(temp_PolicyTxHeader.NumDataObjects<<2));   
    ProtocolTxBuffer[ProtocolTxBytes++] = temp_PolicyTxHeader.byte[0];               
    ProtocolTxBuffer[ProtocolTxBytes++] = temp_PolicyTxHeader.byte[1];               
    if (temp_PolicyTxHeader.NumDataObjects > 0)                                      
    {
        for (i=0; i<temp_PolicyTxHeader.NumDataObjects; i++)                         
        {
            for (j=0; j<4; j++)                                                 
                ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxDataObj[i].byte[j];  
        }
    }
    ProtocolLoadEOP();                                                          
#ifdef FSC_DEBUG
    if(manualRetries)
    {
        manualRetriesTakeTwo();
    }
    else
    {
#endif 
        DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);                

        
        if (ProtocolCheckRxBeforeTx)
        {
            ProtocolCheckRxBeforeTx = FALSE; 
            DeviceRead(regInterruptb, 1, &Registers.Status.byte[3]);
            if (Registers.Status.I_GCRCSENT)
            {
                
                Registers.Status.I_GCRCSENT = 0;
                ProtocolFlushTxFIFO();
                PDTxStatus = txError;
                return;
            }
        }

        Registers.Control.TX_START = 1;                                             
        DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);                    
        Registers.Control.TX_START = 0;                                             

        PDTxStatus = txBusy;                                                        
        ProtocolState = PRLTxSendingMessage;                                        
        ProtocolTimer = tBMCTimeout;                                                
#ifdef FSC_DEBUG
    }
    StoreUSBPDMessage(temp_PolicyTxHeader, &PolicyTxDataObj[0], TRUE, sop_token);    
    WriteStateLog(&PDStateLog, dbgSendTxPacket, temp_PolicyTxHeader.byte[0], temp_PolicyTxHeader.byte[1]);   
#endif 
}

void ProtocolSendingMessage(void)
{
    if (Registers.Status.I_TXSENT)
    {
        ProtocolFlushTxFIFO();                                                  
        ProtocolVerifyGoodCRC();
    }
#ifdef FSC_DEBUG
    else if (Registers.Status.I_COLLISION && manualRetries)                                      
    {
        
        ProtocolFlushTxFIFO();                                                  
        PDTxStatus = txCollision;                                               
        ProtocolTimer = tBMCTimeout;                                            
        ProtocolState = PRLRxWait;                                              
    }
#endif 
    else if (!Registers.Status.I_COLLISION && ProtocolTimer == 0)               
    {
        ProtocolFlushTxFIFO();                                                  
        ProtocolFlushRxFIFO();                                                  
        PDTxStatus = txError;                                                   
        ProtocolState = PRLIdle;                                                
    }
}

void ProtocolVerifyGoodCRC(void)
{
    FSC_U32 i, j;
    FSC_U8 data[3];
    SopType s;

    DeviceRead(regFIFO, 3, &data[0]);                                          
    PolicyRxHeader.byte[0] = data[1];
    PolicyRxHeader.byte[1] = data[2];
    if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTGoodCRC))
    {
        FSC_U8 MIDcompare;
        switch (TokenToSopType(data[0])) {
            case SOP_TYPE_SOP:
                MIDcompare = MessageIDCounter;
                break;
            default:
                MIDcompare = 0xFF; 
                break;
        }

        if (PolicyRxHeader.MessageID != MIDcompare)                             
        {
            DeviceRead(regFIFO, 4, &ProtocolCRC[0]);                           
#ifdef FSC_DEBUG
            StoreUSBPDToken(FALSE, pdtBadMessageID);                            
#endif 
            PDTxStatus = txError;                                               
            ProtocolState = PRLIdle;                                            
        }
        else                                                                    
        {
            switch (TokenToSopType(data[0])) {
                case SOP_TYPE_SOP:
                    MessageIDCounter++;                                         
                    MessageIDCounter &= 0x07;                                   
                    break;
                default:
                    
                    break;
            }

            ProtocolState = PRLIdle;                                            
            PDTxStatus = txSuccess;                                             
            DeviceRead(regFIFO, 4, &ProtocolCRC[0]);                           
#ifdef FSC_DEBUG
            StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], FALSE, data[0]);   
#endif 
        }
    }
    else
    {
        ProtocolState = PRLIdle;                                                
        PDTxStatus = txError;                                                   

        s = TokenToSopType(data[0]);
        if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTSoftReset))
        {
            DeviceRead(regFIFO, 4, &ProtocolCRC[0]);                           

            MessageIDCounter = 0;                                 
            MessageID = 0xFF;                                                   
            ProtocolMsgRx = TRUE;                                               
            ProtocolMsgRxSop = s;
#ifdef FSC_DEBUG
            SourceCapsUpdated = TRUE;                                           
#endif 
        }
        else if (PolicyRxHeader.MessageID != MessageID)                         
        {
            DeviceRead(regFIFO, 4, &ProtocolCRC[0]);                           
            MessageID = PolicyRxHeader.MessageID;                               
            ProtocolMsgRx = TRUE;                                               
            ProtocolMsgRxSop = s;
        }
        if (PolicyRxHeader.NumDataObjects > 0)                                  
        {
           DeviceRead(regFIFO, PolicyRxHeader.NumDataObjects<<2, &ProtocolRxBuffer[0]);    
            for (i=0; i<PolicyRxHeader.NumDataObjects; i++)                     
            {
                for (j=0; j<4; j++)                                             
                    PolicyRxDataObj[i].byte[j] = ProtocolRxBuffer[j + (i<<2)];  
            }
        }
#ifdef FSC_DEBUG
        StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], FALSE, data[0]); 
#endif 
    }
}

void ProtocolSendGoodCRC(SopType sop)
{
    if (sop == SOP_TYPE_SOP) {
        ProtocolLoadSOP();                                                      
    } else {
        return; 
    }

    ProtocolTxBuffer[ProtocolTxBytes++] = PACKSYM | 0x02;                       
    ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxHeader.byte[0];               
    ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxHeader.byte[1];               
    ProtocolLoadEOP();                                                          
    DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);                
    DeviceRead(regStatus0, 2, &Registers.Status.byte[4]);                      
}

void ProtocolLoadSOP(void)
{
    ProtocolTxBytes = 0;                                                        
    ProtocolTxBuffer[ProtocolTxBytes++] = SYNC1_TOKEN;                          
    ProtocolTxBuffer[ProtocolTxBytes++] = SYNC1_TOKEN;                          
    ProtocolTxBuffer[ProtocolTxBytes++] = SYNC1_TOKEN;                          
    ProtocolTxBuffer[ProtocolTxBytes++] = SYNC2_TOKEN;                          
}

void ProtocolLoadEOP(void)
{
    ProtocolTxBuffer[ProtocolTxBytes++] = JAM_CRC;                              
    ProtocolTxBuffer[ProtocolTxBytes++] = EOP;                                  
    ProtocolTxBuffer[ProtocolTxBytes++] = TXOFF;                                
}

void ProtocolSendHardReset(void)
{
    FSC_U8 data;
    data = Registers.Control.byte[3] | 0x40;                                    
    DeviceWrite(regControl3, 1, &data);                                         
#ifdef FSC_DEBUG
    StoreUSBPDToken(TRUE, pdtHardReset);                                        
#endif 
}

void ProtocolFlushRxFIFO(void)
{
    FSC_U8 data;
    data = Registers.Control.byte[1];                                           
    data |= 0x04;                                                               
    DeviceWrite(regControl1, 1, &data);                                         
}

void ProtocolFlushTxFIFO(void)
{
    FSC_U8 data;
    data = Registers.Control.byte[0];                                           
    data |= 0x40;                                                               
    DeviceWrite(regControl0, 1, &data);                                         
}

void ResetProtocolLayer(FSC_BOOL ResetPDLogic)
{
    FSC_U32 i;
    FSC_U8 data = 0x02;
    if (ResetPDLogic)
        DeviceWrite(regReset, 1, &data);                                       
    ProtocolFlushRxFIFO();                                                      
    ProtocolFlushTxFIFO();                                                      
    ProtocolState = PRLIdle;                                                    
    PDTxStatus = txIdle;                                                        
    ProtocolTimer = 0;                                                          

#ifdef FSC_HAVE_VDM
    VdmTimer = 0;
    VdmTimerStarted = FALSE;
#endif 

    ProtocolTxBytes = 0;                                                        
    MessageIDCounter = 0;                                                       
    MessageID = 0xFF;                                                           
    ProtocolMsgRx = FALSE;                                                      
    ProtocolMsgRxSop = SOP_TYPE_SOP;
    USBPDTxFlag = FALSE;                                                        
    PolicyHasContract = FALSE;                                                  
    USBPDContract.object = 0;                                                   
#ifdef FSC_DEBUG
    SourceCapsUpdated = TRUE;                                                   
#endif 
    CapsHeaderReceived.word = 0;                                                
    for (i=0; i<7; i++)                                                         
        CapsReceived[i].object = 0;                                             
    Registers.Switches.AUTO_CRC = 1;
    DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
}


void protocolBISTRxResetCounter(void)   
{
    
    ProtocolState = PRL_BIST_Rx_Test_Frame; 
}

void protocolBISTRxTestFrame(void)      
{
    
    
}

void protocolBISTRxErrorCount(void)     
{
    
    
    
}

void protocolBISTRxInformPolicy(void)   
{
    
    
}

#ifdef FSC_DEBUG

FSC_BOOL StoreUSBPDToken(FSC_BOOL transmitter, USBPD_BufferTokens_t token)
{
    FSC_U8 header1 = 1;                                                          
    if (ClaimBufferSpace(2) == FALSE)                                           
        return FALSE;                                                           
    if (transmitter)                                                            
        header1 |= 0x40;                                                        
    USBPDBuf[USBPDBufEnd++] = header1;                                          
    USBPDBufEnd %= PDBUFSIZE;                                                   
    token &= 0x0F;                                                              
    USBPDBuf[USBPDBufEnd++] = token;                                            
    USBPDBufEnd %= PDBUFSIZE;                                                   
    return TRUE;
}

FSC_BOOL StoreUSBPDMessage(sopMainHeader_t Header, doDataObject_t* DataObject, FSC_BOOL transmitter, FSC_U8 SOPToken)
{
    FSC_U32 i, j, required;
    FSC_U8 header1;
    required = Header.NumDataObjects * 4 + 2 + 2;                               
    if (ClaimBufferSpace(required) == FALSE)                                    
        return FALSE;                                                           
    header1 = (0x1F & (required-1)) | 0x80;
    if (transmitter)                                                            
        header1 |= 0x40;                                                        
    USBPDBuf[USBPDBufEnd++] = header1;                                          
    USBPDBufEnd %= PDBUFSIZE;                                                   
    SOPToken &= 0xE0;                                                           
    SOPToken >>= 5;                                                             
    USBPDBuf[USBPDBufEnd++] = SOPToken;                                         
    USBPDBufEnd %= PDBUFSIZE;                                                   
    USBPDBuf[USBPDBufEnd++] = Header.byte[0];                                   
    USBPDBufEnd %= PDBUFSIZE;                                                   
    USBPDBuf[USBPDBufEnd++] = Header.byte[1];                                   
    USBPDBufEnd %= PDBUFSIZE;                                                   
    for (i=0; i<Header.NumDataObjects; i++)                                     
    {
        for (j=0; j<4; j++)
        {
            USBPDBuf[USBPDBufEnd++] = DataObject[i].byte[j];                    
            USBPDBufEnd %= PDBUFSIZE;                                           
        }
    }
    return TRUE;
}

FSC_U8 GetNextUSBPDMessageSize(void)
{
    FSC_U8 numBytes;
    if (USBPDBufStart == USBPDBufEnd)                                           
        numBytes = 0;                                                           
    else                                                                        
        numBytes = (USBPDBuf[USBPDBufStart] & 0x1F) + 1;                        
    return numBytes;
}

FSC_U8 GetUSBPDBufferNumBytes(void)
{
    FSC_U8 bytes;
    if (USBPDBufStart == USBPDBufEnd)                                           
        bytes = 0;                                                              
    else if (USBPDBufEnd > USBPDBufStart)                                       
        bytes = USBPDBufEnd - USBPDBufStart;                                    
    else                                                                        
        bytes = USBPDBufEnd + (PDBUFSIZE - USBPDBufStart);                      
    return bytes;
}

FSC_BOOL ClaimBufferSpace(FSC_S32 intReqSize)
{
    FSC_S32 available;
    FSC_U8 numBytes;
    if (intReqSize >= PDBUFSIZE)                                                
        return FALSE;                                                           
    if (USBPDBufStart == USBPDBufEnd)                                           
        available = PDBUFSIZE;                                                  
    else if (USBPDBufStart > USBPDBufEnd)                                       
        available = USBPDBufStart - USBPDBufEnd;                                
    else                                                                        
        available = PDBUFSIZE - (USBPDBufEnd - USBPDBufStart);                  
    do
    {
        if (intReqSize >= available)                                            
        {
            USBPDBufOverflow = TRUE;                                            
            numBytes = GetNextUSBPDMessageSize();                               
            if (numBytes == 0)                                                  
                return FALSE;                                                   // Return FALSE since the data cannot fit in the available buffer size (nothing written)
            available += numBytes;                                              
            USBPDBufStart += numBytes;                                          
            USBPDBufStart %= PDBUFSIZE;                                         
        }
        else
            break;
    } while (1);                                                                
    return TRUE;
}


void GetUSBPDStatus(FSC_U8 abytData[])
{
    FSC_U32 i, j;
    FSC_U32 intIndex = 0;
    abytData[intIndex++] = GetUSBPDStatusOverview();                            
    abytData[intIndex++] = GetUSBPDBufferNumBytes();                            
    abytData[intIndex++] = PolicyState;                                         
    abytData[intIndex++] = PolicySubIndex;                                      
    abytData[intIndex++] = (ProtocolState << 4) | PDTxStatus;                   
    for (i=0;i<4;i++)
            abytData[intIndex++] = USBPDContract.byte[i];                       
    if (PolicyIsSource)
    {
#ifdef FSC_HAVE_SRC
        abytData[intIndex++] = CapsHeaderSource.byte[0];                        
        abytData[intIndex++] = CapsHeaderSource.byte[1];                        
        for (i=0;i<7;i++)                                                       
        {
            for (j=0;j<4;j++)                                                   
                abytData[intIndex++] = CapsSource[i].byte[j];                   
        }
#endif 
    }
    else
    {
#ifdef FSC_HAVE_SNK
        abytData[intIndex++] = CapsHeaderReceived.byte[0];                      
        abytData[intIndex++] = CapsHeaderReceived.byte[1];                      
        for (i=0;i<7;i++)                                                       
        {
            for (j=0;j<4;j++)                                                   
                abytData[intIndex++] = CapsReceived[i].byte[j];                 
        }
#endif 
    }

    
    
    
    intIndex = 44;
    abytData[intIndex++] = Registers.DeviceID.byte;     
    abytData[intIndex++] = Registers.Switches.byte[0];  
    abytData[intIndex++] = Registers.Switches.byte[1];
    abytData[intIndex++] = Registers.Measure.byte;
    abytData[intIndex++] = Registers.Slice.byte;
    abytData[intIndex++] = Registers.Control.byte[0];   
    abytData[intIndex++] = Registers.Control.byte[1];
    abytData[intIndex++] = Registers.Mask.byte;
    abytData[intIndex++] = Registers.Power.byte;
    abytData[intIndex++] = Registers.Status.byte[4];    
    abytData[intIndex++] = Registers.Status.byte[5];    
    abytData[intIndex++] = Registers.Status.byte[6];    
}

FSC_U8 GetUSBPDStatusOverview(void)
{
    FSC_U8 status = 0;
    if (USBPDEnabled)
        status |= 0x01;
    if (USBPDActive)
        status |= 0x02;
    if (PolicyIsSource)
        status |= 0x04;
    if (PolicyIsDFP)
        status |= 0x08;
    if (PolicyHasContract)
        status |= 0x10;
    if (SourceCapsUpdated)
        status |= 0x20;
    SourceCapsUpdated = FALSE;
    if (USBPDBufOverflow)
        status |= 0x80;
    return status;
}

FSC_U8 ReadUSBPDBuffer(FSC_U8* pData, FSC_U8 bytesAvail)
{
    FSC_U8 i, msgSize, bytesRead;
    bytesRead = 0;
    do
    {
        msgSize = GetNextUSBPDMessageSize();                                    
        if ((msgSize != 0) && (msgSize <= bytesAvail))                          
        {
            for (i=0; i<msgSize; i++)                                           
            {
                *pData++ = USBPDBuf[USBPDBufStart++];                           
                USBPDBufStart %= PDBUFSIZE;                                     
            }
            bytesAvail -= msgSize;                                              
            bytesRead += msgSize;                                               
        }
        else                                                                    
            break;                                                              
    } while (1);
    return bytesRead;
}



void SendUSBPDMessage(FSC_U8* abytData)
{
    FSC_U32 i, j;
    PDTransmitHeader.byte[0] = *abytData++;                                     
    PDTransmitHeader.byte[1] = *abytData++;                                     
    for (i=0; i<PDTransmitHeader.NumDataObjects; i++)                           
    {
        for (j=0; j<4; j++)                                                     
        {
            PDTransmitObjects[i].byte[j] = *abytData++;                         
        }
    }
    USBPDTxFlag = TRUE;                                                         
}

void manualRetriesTakeTwo(void)
{
    FSC_U8 tries = nTries;
    regMask_t maskTemp;
    regMaskAdv_t maskAdvTemp;
    
    maskTemp.byte = ~0x02;
    DeviceWrite(regMask, 1, &maskTemp.byte);
    maskAdvTemp.byte[0] = ~0x14;
    DeviceWrite(regMaska, 1, &maskAdvTemp.byte[0]);
    maskAdvTemp.M_GCRCSENT = 1;
    DeviceWrite(regMaskb, 1, &maskAdvTemp.byte[1]);

    
    DeviceRead(regInterrupt, 1, &Registers.Status.byte[6]);
    DeviceRead(regInterrupta, 1, &Registers.Status.byte[2]);
    DeviceRead(regInterruptb, 1, &Registers.Status.byte[3]);
    Registers.Status.I_TXSENT = 0;                                              
    Registers.Status.I_RETRYFAIL = 0;                                           
    Registers.Status.I_COLLISION = 0;                                           

    
    DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);            

    while(tries)
    {
        
        Registers.Control.TX_START = 1;                                         
        DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);                
        Registers.Control.TX_START = 0;                                         
        
        while(!platform_get_device_irq_state());               
        DeviceRead(regInterrupt, 1, &Registers.Status.byte[6]);             
        DeviceRead(regInterrupta, 1, &Registers.Status.byte[2]);

        if(Registers.Status.I_TXSENT)
        {
            
            Registers.Status.I_TXSENT = 0;                                      
            ProtocolVerifyGoodCRC();
            ProtocolState = PRLIdle;                                            
            PDTxStatus = txSuccess;                                             
            tries = 0;
        }
        else if(Registers.Status.I_RETRYFAIL)
        {
            Registers.Status.I_RETRYFAIL = 0;                                   
            tries--;                                                            
            if(!tries)                                                          
            {
                
                ProtocolState = PRLIdle;                                        
                PDTxStatus = txError;                                           
            }
            else
            {
                
                DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);            
            }
        }
        else if(Registers.Status.I_COLLISION)    
        {
            Registers.Status.I_COLLISION = 0;                                   
        }
    }

    
        DeviceWrite(regMask, 1, &Registers.Mask.byte);
        DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
        DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
}

void setManualRetries(FSC_U8 mode)
{
    manualRetries = mode;
}

FSC_U8 getManualRetries(void)
{
    return manualRetries;
}

#endif 
