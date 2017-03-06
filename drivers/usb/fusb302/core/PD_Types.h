#ifndef __USBPD_TYPES_H__
#define __USBPD_TYPES_H__

#include "platform.h"

#ifdef FSC_DEBUG
#define PDBUFSIZE               128
#endif 

#define USBPDSPECREV            1           

#define STAT_BUSY               0
#define STAT_SUCCESS            1
#define STAT_ERROR              2

#define TXON                    0xA1
#define SOP1                    0x12  
#define SOP2                    0x13  
#define SOP3                    0x1B
#define SYNC1_TOKEN             0x12
#define SYNC2_TOKEN             0x13
#define SYNC3_TOKEN             0x1B
#define RESET1                  0x15
#define RESET2                  0x16
#define PACKSYM                 0x80
#define JAM_CRC                 0xFF
#define EOP                     0x14
#define TXOFF                   0xFE

#define PDPortRoleSink          0
#define PDPortRoleSource        1
#define PDSpecRev1p0            0
#define PDSpecRev2p0            1
#define PDDataRoleUFP           0
#define PDDataRoleDFP           1
#define PDCablePlugSource       0
#define PDCablePlugPlug         1

#define CMTGoodCRC              0b0001
#define CMTGotoMin              0b0010
#define CMTAccept               0b0011
#define CMTReject               0b0100
#define CMTPing                 0b0101
#define CMTPS_RDY               0b0110
#define CMTGetSourceCap         0b0111
#define CMTGetSinkCap           0b1000
#define CMTDR_Swap              0b1001
#define CMTPR_Swap              0b1010
#define CMTVCONN_Swap           0b1011
#define CMTWait                 0b1100
#define CMTSoftReset            0b1101

#define DMTSourceCapabilities   0b0001
#define DMTRequest              0b0010
#define DMTBIST                 0b0011
#define DMTSinkCapabilities     0b0100
#define DMTVenderDefined        0b1111

#define BDO_BIST_Receiver_Mode  0b0000      
#define BDO_BIST_Transmit_Mode  0b0001      
#define BDO_Returned_BIST_Counters  0b0010  
#define BDO_BIST_Carrier_Mode_0 0b0011      
#define BDO_BIST_Carrier_Mode_1 0b0100      
#define BDO_BIST_Carrier_Mode_2 0b0101      
#define BDO_BIST_Carrier_Mode_3 0b0110      
#define BDO_BIST_Eye_Pattern    0b0111      
#define BDO_BIST_Test_Data      0b1000      

#define TICK_SCALE_TO_MS        10

#define tNoResponse             5000    * TICK_SCALE_TO_MS
#define tSenderResponse         270
#define tTypeCSendSourceCap     150     * TICK_SCALE_TO_MS
#define tSinkWaitCap            500     * TICK_SCALE_TO_MS                      
#define tTypeCSinkWaitCap       500     * TICK_SCALE_TO_MS
#define tSrcTransition          30      * TICK_SCALE_TO_MS
#define tPSHardReset            30      * TICK_SCALE_TO_MS
#define tPSTransition           500     * TICK_SCALE_TO_MS
#define tPSSourceOff            835     * TICK_SCALE_TO_MS
#define tPSSourceOn             435     * TICK_SCALE_TO_MS
#define tSourceOnDelay          30      * TICK_SCALE_TO_MS                      
#define tVCONNSourceOn          90      * TICK_SCALE_TO_MS
#define tBMCTimeout             5       * TICK_SCALE_TO_MS                      
#define tPRSwapBailout          5000    * TICK_SCALE_TO_MS                      
#define tBISTContMode           50      * TICK_SCALE_TO_MS
#define tSwapSourceStart        25      * TICK_SCALE_TO_MS
#define tSrcRecover             830     * TICK_SCALE_TO_MS
#define tGoodCRCDelay           1       * TICK_SCALE_TO_MS
#define t5To12VTransition       8       * TICK_SCALE_TO_MS


#define nHardResetCount         2
#define nRetryCount             3
#define nCapsCount              50


typedef union {
    FSC_U16 word;
    FSC_U8 byte[2] __PACKED;

    struct {
        
        unsigned MessageType:4;         
        unsigned Reserved0:1;                    
        unsigned PortDataRole:1;        
        unsigned SpecRevision:2;        
        unsigned PortPowerRole:1;       
        unsigned MessageID:3;           
        unsigned NumDataObjects:3;      
        unsigned Reserved1:1;                    
    };
} sopMainHeader_t;

typedef union {
    FSC_U16 word;
    FSC_U8 byte[2] __PACKED;
    struct {
        
        unsigned MessageType:4;         
        unsigned Reserved0:1;                    
        unsigned Reserved1:1;                    
        unsigned SpecRevision:2;        
        unsigned CablePlug:1;           
        unsigned MessageID:3;           
        unsigned NumDataObjects:3;      
        unsigned Reserved2:1;                    
    };
} sopPlugHeader_t;

typedef union {
    FSC_U32 object;
    FSC_U16 word[2] __PACKED;
    FSC_U8 byte[4] __PACKED;
    struct {
        unsigned :30;
        unsigned SupplyType:2;
    } PDO;          
    struct {
        unsigned MaxCurrent:10;         
        unsigned Voltage:10;            
        unsigned PeakCurrent:2;         
        unsigned Reserved:3;                    
        unsigned DataRoleSwap:1;        
        unsigned USBCommCapable:1;      
        unsigned ExternallyPowered:1;   
        unsigned USBSuspendSupport:1;   
        unsigned DualRolePower:1;       
        unsigned SupplyType:2;          
    } FPDOSupply;   
    struct {
        unsigned OperationalCurrent:10; 
        unsigned Voltage:10;            
        unsigned Reserved:5;                    
        unsigned DataRoleSwap:1;        
        unsigned USBCommCapable:1;      
        unsigned ExternallyPowered:1;   
        unsigned HigherCapability:1;    
        unsigned DualRolePower:1;       
        unsigned SupplyType:2;          
    } FPDOSink;     
    struct {
        unsigned MaxCurrent:10;         
        unsigned MinVoltage:10;         
        unsigned MaxVoltage:10;         
        unsigned SupplyType:2;          
    } VPDO;         
    struct {
        unsigned MaxPower:10;           
        unsigned MinVoltage:10;         
        unsigned MaxVoltage:10;         
        unsigned SupplyType:2;          
    } BPDO;         
    struct {
        unsigned MinMaxCurrent:10;      
        unsigned OpCurrent:10;          
        unsigned Reserved0:4;                    
        unsigned NoUSBSuspend:1;        
        unsigned USBCommCapable:1;      
        unsigned CapabilityMismatch:1;  
        unsigned GiveBack:1;            
        unsigned ObjectPosition:3;      
        unsigned Reserved1:1;                    
    } FVRDO;        
    struct {
        unsigned MinMaxPower:10;        
        unsigned OpPower:10;            
        unsigned Reserved0:4;                    
        unsigned NoUSBSuspend:1;        
        unsigned USBCommCapable:1;      
        unsigned CapabilityMismatch:1;  
        unsigned GiveBack:1;            
        unsigned ObjectPosition:3;      
        unsigned Reserved1:1;                    
    } BRDO;         
    struct {
        unsigned VendorDefined:15;      
        unsigned VDMType:1;             
        unsigned VendorID:16;           
    } UVDM;
    struct {
        unsigned Command:5;             
        unsigned Reserved0:1;                    
        unsigned CommandType:2;         
        unsigned ObjPos:3;              
        unsigned Reserved1:2;                    
        unsigned Version:2;             
        unsigned VDMType:1;             
        unsigned SVID:16;               
    } SVDM;
} doDataObject_t;

typedef enum {
    pdtNone = 0,                
    pdtAttach,                  
    pdtDetach,                  
    pdtHardReset,               
    pdtBadMessageID,            
} USBPD_BufferTokens_t;

typedef enum {
    peDisabled = 0,             
    FIRST_PE_ST = peDisabled,   
    peErrorRecovery,            
    peSourceHardReset,          
    peSourceSendHardReset,      
    peSourceSoftReset,          
    peSourceSendSoftReset,      
    peSourceStartup,            
    peSourceSendCaps,           
    peSourceDiscovery,          
    peSourceDisabled,           

    peSourceTransitionDefault,  
    peSourceNegotiateCap,       
    peSourceCapabilityResponse, 
    peSourceTransitionSupply,   
    peSourceReady,              
    peSourceGiveSourceCaps,     
    peSourceGetSinkCaps,        
    peSourceSendPing,           
    peSourceGotoMin,            
    peSourceGiveSinkCaps,       

    peSourceGetSourceCaps,      
    peSourceSendDRSwap,         
    peSourceEvaluateDRSwap,     

    peSinkHardReset,            
    peSinkSendHardReset,        
    peSinkSoftReset,            
    peSinkSendSoftReset,        
    peSinkTransitionDefault,    
    peSinkStartup,              

    peSinkDiscovery,            
    peSinkWaitCaps,             
    peSinkEvaluateCaps,         
    peSinkSelectCapability,     
    peSinkTransitionSink,       
    peSinkReady,                
    peSinkGiveSinkCap,          
    peSinkGetSourceCap,         
    peSinkGetSinkCap,           
    peSinkGiveSourceCap,        

    peSinkSendDRSwap,           
    peSinkEvaluateDRSwap,       
    
    peSourceSendVCONNSwap,      
    peSinkEvaluateVCONNSwap,    
    peSourceSendPRSwap,         
    peSourceEvaluatePRSwap,     
    peSinkSendPRSwap,           
    peSinkEvaluatePRSwap,       
    
    peGiveVdm,
    
    peUfpVdmGetIdentity,		
    FIRST_VDM_STATE = peUfpVdmGetIdentity,

    peUfpVdmSendIdentity,		
    peUfpVdmGetSvids,			
    peUfpVdmSendSvids,			
    peUfpVdmGetModes,			
    peUfpVdmSendModes,			
    peUfpVdmEvaluateModeEntry,	
    peUfpVdmModeEntryNak,		
    peUfpVdmModeEntryAck,		
    peUfpVdmModeExit,			
    peUfpVdmModeExitNak,		

    peUfpVdmModeExitAck,		
    
    peUfpVdmAttentionRequest,	
    
    peDfpUfpVdmIdentityRequest,	
    peDfpUfpVdmIdentityAcked,	
    peDfpUfpVdmIdentityNaked,	
    
    peDfpCblVdmIdentityRequest,	
    peDfpCblVdmIdentityAcked,	
    peDfpCblVdmIdentityNaked,	
    
    peDfpVdmSvidsRequest,		
    peDfpVdmSvidsAcked,			

    peDfpVdmSvidsNaked,			
    
    peDfpVdmModesRequest,		
    peDfpVdmModesAcked,			
    peDfpVdmModesNaked,			
    
    peDfpVdmModeEntryRequest,	
    peDfpVdmModeEntryAcked,		
    peDfpVdmModeEntryNaked,		
    
    peDfpVdmModeExitRequest,	
    peDfpVdmExitModeAcked,		
    
    
    peSrcVdmIdentityRequest,	

    peSrcVdmIdentityAcked,		
    peSrcVdmIdentityNaked,		
    
    peDfpVdmAttentionRequest,	
    
    peCblReady,					
    
    peCblGetIdentity,			
    peCblGetIdentityNak,		
    peCblSendIdentity,		
    
    peCblGetSvids,				
    peCblGetSvidsNak,			
    peCblSendSvids,			

            
    peCblGetModes,				
    peCblGetModesNak,			
    peCblSendModes,			
    
    peCblEvaluateModeEntry,		
    peCblModeEntryAck,			
    peCblModeEntryNak,			
    
    peCblModeExit,				
    peCblModeExitAck,			
    peCblModeExitNak,			
    
    peDpRequestStatus,          

            
    PE_BIST_Receive_Mode,       
    PE_BIST_Frame_Received,     
    
    PE_BIST_Carrier_Mode_2,     
    LAST_VDM_STATE = PE_BIST_Carrier_Mode_2,
    peSourceWaitNewCapabilities,    
    LAST_PE_ST = peSourceWaitNewCapabilities,          
    PE_BIST_Test_Data,              
    peSourceEvaluateVCONNSwap,
#ifdef FSC_DEBUG
    dbgGetRxPacket,                 
    dbgSendTxPacket,                 
#endif 
} PolicyState_t;

typedef enum {
    PRLDisabled = 0,            
    PRLIdle,                    
    PRLReset,                   
    PRLResetWait,               
    PRLRxWait,                  
    PRLTxSendingMessage,        
    PRLTxWaitForPHYResponse,    
    PRLTxVerifyGoodCRC,         

    
    PRL_BIST_Rx_Reset_Counter,      
    PRL_BIST_Rx_Test_Frame,         
    PRL_BIST_Rx_Error_Count,        
    PRL_BIST_Rx_Inform_Policy,      
} ProtocolState_t;

typedef enum {
    txIdle = 0,
    txReset,
    txSend,
    txBusy,
    txWait,
    txSuccess,
    txError,
    txCollision
} PDTxStatus_t;

typedef enum {
    pdoTypeFixed = 0,
    pdoTypeBattery,
    pdoTypeVariable,
    pdoTypeReserved
} pdoSupplyType;

typedef enum {
    AUTO_VDM_INIT,
    AUTO_VDM_DISCOVER_ID_PP,
    AUTO_VDM_DISCOVER_SVIDS_PP,
    AUTO_VDM_DISCOVER_MODES_PP,
    AUTO_VDM_ENTER_DP_MODE_PP,
    AUTO_VDM_DP_GET_STATUS,
    AUTO_VDM_DONE
} VdmDiscoveryState_t;

typedef enum {
	SOP_TYPE_SOP,
	SOP_TYPE_SOP1,
	SOP_TYPE_SOP2,
	SOP_TYPE_SOP1_DEBUG,
	SOP_TYPE_SOP2_DEBUG,
    SOP_TYPE_ERROR = 0xFF
} SopType;

#endif 

