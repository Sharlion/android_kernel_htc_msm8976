#ifdef FSC_HAVE_DP

#ifndef __DISPLAYPORT_TYPES_H__
#define __DISPLAYPORT_TYPES_H__

#include "../../platform.h"

typedef enum {
	DP_COMMAND_STATUS	=	0x10,	
	DP_COMMAND_CONFIG	=	0x11	
} DpCommand;

typedef union {
	FSC_U32 word;
	FSC_U8 byte[4] __PACKED;
	struct {
		FSC_U32     UfpDCapable:1;          
        FSC_U32     DfpDCapable:1;          
		FSC_U32	    SupportsDPv1p3:1;	    
		FSC_U32 	SupportsUSBGen2:1;	    
		FSC_U32	    Rsvd0:2;
		FSC_U32	    ReceptacleIndication:1;	
		FSC_U32	    USB2p0NotUsed:1;		
		FSC_U32 	DFP_DPinAssignA:1;		
		FSC_U32 	DFP_DPinAssignB:1;
		FSC_U32 	DFP_DPinAssignC:1;
		FSC_U32 	DFP_DPinAssignD:1;
		FSC_U32 	DFP_DPinAssignE:1;
		FSC_U32 	DFP_DPinAssignF:1;
		FSC_U32 	DFP_DPinAssignRsvd:2;
		FSC_U32 	UFP_DPinAssignA:1;		
		FSC_U32 	UFP_DPinAssignB:1;
		FSC_U32 	UFP_DPinAssignC:1;
		FSC_U32 	UFP_DPinAssignD:1;
		FSC_U32 	UFP_DPinAssignE:1;
		FSC_U32 	UFP_DPinAssignRsvd:3;
		FSC_U32	    Rsvd1:8;
	};
    struct {
        FSC_U32    field0:2;
        FSC_U32    field1:2;
        FSC_U32    fieldrsvd0:2;
        FSC_U32    field2:1;
        FSC_U32    field3:1;
        FSC_U32    field4:6;
        FSC_U32    fieldrsvd1:2;
        FSC_U32    field5:5;
        FSC_U32    fieldrsvd2:11;
    };
} DisplayPortCaps_t;

typedef enum {
	DP_CONN_NEITHER	= 0,	
	DP_CONN_DFP_D	= 1,	
	DP_CONN_UFP_D	= 2,	
	DP_CONN_BOTH	= 3 	
} DpConn_t;

typedef union {
	FSC_U32 word;
	FSC_U8 byte[4] __PACKED;
	struct {
		DpConn_t	Connection:2;				
		FSC_U32	    PowerLow:1;					
		FSC_U32	    Enabled:1;					
		FSC_U32	    MultiFunctionPreferred:1; 	
		FSC_U32 	UsbConfigRequest:1;			
		FSC_U32 	ExitDpModeRequest:1;		
		FSC_U32 	HpdState:1;					
		FSC_U32	    IrqHpd:1;					
		FSC_U32	    Rsvd:23;
	};
} DisplayPortStatus_t;

typedef enum {
	DP_CONF_USB		= 0,	
	DP_CONF_DFP_D	= 1,	
	DP_CONF_UFP_D	= 2,	
	DP_CONF_RSVD	= 3
} DpConf_t;

typedef enum {
	DP_CONF_SIG_UNSPECIFIED	= 0,	
	DP_CONF_SIG_DP_V1P3		= 1,	
	DP_CONF_SIG_GEN2		= 2		
} DpConfSig_t;

typedef enum {
	DP_DFPPA_DESELECT	= 0,		
	DP_DFPPA_A			= 1,		
	DP_DFPPA_B			= 2,
	DP_DFPPA_C			= 4,
	DP_DFPPA_D			= 8,
	DP_DFPPA_E			= 16,
	DP_DFPPA_F			= 32
} DpDfpPa_t;

typedef enum {
	DP_UFPPA_DESELECT	= 0,		
	DP_UFPPA_A			= 1,		
	DP_UFPPA_B			= 2,
	DP_UFPPA_C			= 4,
	DP_UFPPA_D			= 8,
	DP_UFPPA_E			= 16
} DpUfpPa_t;

typedef union {
	FSC_U32 word;
	FSC_U8 byte[4] __PACKED;
	struct {
		DpConf_t		Conf:2;		
		DpConfSig_t		SigConf:4;	
		FSC_U32	        Rsvd:2;
		DpDfpPa_t		DfpPa:8;	
		DpUfpPa_t		UfpPa:8;	
	};
} DisplayPortConfig_t;

#endif  

#endif 
