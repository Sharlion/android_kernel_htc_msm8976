
#ifndef ALTERNATEMODES_H
#define	ALTERNATEMODES_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "TypeC.h"
#include "platform.h"

#ifdef FSC_DEBUG
#include "Log.h"
#endif 

#define tAlternateDRPSwap 40 * 10                   

extern DeviceReg_t              Registers;          
extern FSC_BOOL                 USBPDActive;        
extern FSC_BOOL                 USBPDEnabled;       
extern FSC_U32                  PRSwapTimer;        
extern FSC_BOOL                 IsHardReset;        
extern SourceOrSink             sourceOrSink;       

extern USBTypeCPort             PortType;           
extern FSC_BOOL                 blnCCPinIsCC1;      
extern FSC_BOOL                 blnCCPinIsCC2;      
extern FSC_BOOL                 blnSMEnabled;       
extern ConnectionState          ConnState;          

#ifdef FSC_DEBUG
extern StateLog                 TypeCStateLog;      
extern volatile FSC_U16         Timer_S;            
extern volatile FSC_U16         Timer_tms;          
#endif 

#ifdef FSC_HAVE_ACCMODE
extern FSC_BOOL         blnAccSupport;              
#endif 

extern FSC_U16          StateTimer;                 
extern FSC_U16          PDDebounce;                 
extern FSC_U16          CCDebounce;                 
extern FSC_U16          ToggleTimer;                
extern FSC_U16          DRPToggleTimer;             
extern FSC_U16          OverPDDebounce;             
extern CCTermType       CC1TermPrevious;            
extern CCTermType       CC2TermPrevious;            
extern CCTermType       CC1TermCCDebounce;          
extern CCTermType       CC2TermCCDebounce;          
extern CCTermType       CC1TermPDDebounce;
extern CCTermType       CC2TermPDDebounce;
extern CCTermType       CC1TermPDDebouncePrevious;
extern CCTermType       CC2TermPDDebouncePrevious;
extern USBTypeCCurrent  SinkCurrent;        

void SetStateAlternateUnattached(void);
void StateMachineAlternateUnattached(void);

#ifdef FSC_HAVE_DRP
void SetStateAlternateDRP(void);
void StateMachineAlternateDRP(void);
void AlternateDRPSwap(void);
void AlternateDRPSourceSinkSwap(void);
#endif 

#ifdef FSC_HAVE_SRC
void SetStateAlternateUnattachedSource(void);
void StateMachineAlternateUnattachedSource(void);
#endif 

#ifdef FSC_HAVE_SNK
void SetStateAlternateUnattachedSink(void);
void StateMachineAlternateUnattachedSink(void);
#endif 

#ifdef FSC_HAVE_ACCMODE
void SetStateAlternateAudioAccessory(void);
#endif 

CCTermType AlternateDecodeCCTerminationSource(void);


#ifdef	__cplusplus
}
#endif

#endif	

