#ifdef FSC_HAVE_DP

#include "dp.h"
#include "interface_dp.h"
#include "../vdm.h"
#include "../bitfield_translators.h"
#include "../../PD_Types.h"


FSC_BOOL DpEnabled;
FSC_BOOL DpAutoModeEntryEnabled;
DisplayPortCaps_t DpModeEntryMask;
DisplayPortCaps_t DpModeEntryValue;

DisplayPortCaps_t DpCaps;
DisplayPortStatus_t DpStatus;
DisplayPortConfig_t DpConfig;

DisplayPortStatus_t DpPpStatus;
DisplayPortConfig_t DpPpRequestedConfig;
DisplayPortConfig_t DpPpConfig;
FSC_U32 DpModeEntered;

extern VdmManager* vdmm;
extern PolicyState_t PolicyState;

void initializeDp(void)
{
	DpCaps.word = 0x0;
	DpStatus.word = 0x0;
	DpConfig.word = 0x0;

    DpPpStatus.word = 0x0;
    DpPpRequestedConfig.word = 0x0;
    DpPpConfig.word = 0x0;

    DpModeEntryMask.word = 0x0;
    DpModeEntryValue.word = 0x0;

    DpEnabled = FALSE;
    DpAutoModeEntryEnabled = FALSE;
    DpModeEntered = 0x0;
}

void resetDp(void)
{
    DpStatus.word = 0x0;
	DpConfig.word = 0x0;

    DpPpStatus.word = 0x0;
    DpPpRequestedConfig.word = 0x0;
    DpPpConfig.word = 0x0;

    DpModeEntered = 0x0;
}

FSC_BOOL processDpCommand(FSC_U32* arr_in)
{
	doDataObject_t      	__svdmh_in = {0};
    DisplayPortStatus_t     __stat;
    DisplayPortConfig_t     __config;

    if (DpEnabled == FALSE) return TRUE;
	__svdmh_in.object = arr_in[0];

	switch (__svdmh_in.SVDM.Command) {
	case DP_COMMAND_STATUS:
		if (__svdmh_in.SVDM.CommandType == INITIATOR) {
            if (DpModeEntered == FALSE) return TRUE;
            __stat.word = arr_in[1];
            informStatus(__stat);
			updateStatusData();                 
			sendStatusData(__svdmh_in);	
		} else {
            __stat.word = arr_in[1];
			informStatus(__stat);
		}
        break;
	case DP_COMMAND_CONFIG:
		if (__svdmh_in.SVDM.CommandType == INITIATOR) {
            if (DpModeEntered == FALSE) return TRUE;
            __config.word = arr_in[1];
			if (DpReconfigure(__config) == TRUE) {
				
				replyToConfig(__svdmh_in, TRUE);
			} else {
				
				replyToConfig(__svdmh_in, FALSE);
			}
		} else {
			if (__svdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				informConfigResult(TRUE);
			} else {
				informConfigResult(FALSE);
			}
		}
        break;
	default:
		
		return TRUE;
	}
	return FALSE;
}

void sendStatusData(doDataObject_t svdmh_in)
{
	doDataObject_t  __svdmh_out = {0};
	FSC_U32         __length_out;
	FSC_U32			__arr_out[2] = {0};

	__length_out = 0;

	
	__svdmh_out.object = svdmh_in.object;

	__svdmh_out.SVDM.Version        = STRUCTURED_VDM_VERSION;
	__svdmh_out.SVDM.CommandType	= RESPONDER_ACK;

	__arr_out[0] = __svdmh_out.object;
	__length_out++;

	__arr_out[1] = DpStatus.word;
	__length_out++;

	sendVdmMessage(SOP_TYPE_SOP, __arr_out, __length_out, PolicyState);
}

void replyToConfig(doDataObject_t svdmh_in, FSC_BOOL success)
{
	doDataObject_t  __svdmh_out = {0};
	FSC_U32          __length_out;
	FSC_U32			__arr_out[2] = {0};

	__length_out = 0;

	
	__svdmh_out.object = svdmh_in.object;

	__svdmh_out.SVDM.Version 	= STRUCTURED_VDM_VERSION;
	if (success == TRUE) {
		__svdmh_out.SVDM.CommandType = RESPONDER_ACK;
	} else {
		__svdmh_out.SVDM.CommandType	= RESPONDER_NAK;
	}

	__arr_out[0] = __svdmh_out.object;
	__length_out++;

	sendVdmMessage(SOP_TYPE_SOP, __arr_out, __length_out, PolicyState);
}

FSC_BOOL dpEvaluateModeEntry (FSC_U32 mode_in)
{
    DisplayPortCaps_t field_mask;
    DisplayPortCaps_t temp;

    if (DpEnabled == FALSE) return FALSE;
    if (DpAutoModeEntryEnabled == FALSE) return FALSE;

    
    
    field_mask.word = DpModeEntryMask.word;
    if (field_mask.field0) field_mask.field0 = 0x3;
    if (field_mask.field1) field_mask.field1 = 0x3;
    if (field_mask.field2) field_mask.field2 = 0x1;
    if (field_mask.field3) field_mask.field3 = 0x1;
    if (field_mask.field4) field_mask.field4 = 0x3F;
    if (field_mask.field5) field_mask.field5 = 0x1F;
    field_mask.fieldrsvd0  = 0x3;
    field_mask.fieldrsvd1  = 0x3;
    field_mask.fieldrsvd2  = 0x7FF;

    
    temp.word = mode_in & DpModeEntryValue.word;

    
    temp.word = temp.word | field_mask.word;

    
    if ((temp.field0 != 0) && (temp.field1 != 0) &&
        (temp.field2 != 0) && (temp.field3 != 0) &&
        (temp.field4 != 0) && (temp.field5 != 0)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void requestDpStatus(void)
{
    doDataObject_t  __svdmh = {0};
	FSC_U32          __length = 0;
	FSC_U32          __arr[2] = {0};

    __svdmh.SVDM.SVID           = DP_SID; 					
	__svdmh.SVDM.VDMType        = STRUCTURED_VDM;			
	__svdmh.SVDM.Version        = STRUCTURED_VDM_VERSION;	
	__svdmh.SVDM.ObjPos 		= DpModeEntered & 0x7;		
	__svdmh.SVDM.CommandType    = INITIATOR;				
	__svdmh.SVDM.Command        = DP_COMMAND_STATUS;		

    __arr[0] = __svdmh.object;
    __length++;

    __arr[1] = DpStatus.word;
    __length++;

    sendVdmMessageWithTimeout(SOP_TYPE_SOP, __arr, __length, peDpRequestStatus);
}

void requestDpConfig(DisplayPortConfig_t in)
{
    doDataObject_t  __svdmh = {0};
	FSC_U32          __length = 0;
	FSC_U32          __arr[2] = {0};

    DpPpRequestedConfig.word = in.word;

    __svdmh.SVDM.SVID 			= DP_SID; 					
	__svdmh.SVDM.VDMType 		= STRUCTURED_VDM;			
	__svdmh.SVDM.Version        = STRUCTURED_VDM_VERSION;	
	__svdmh.SVDM.ObjPos 		= DpModeEntered & 0x7;		
	__svdmh.SVDM.CommandType 	= INITIATOR;				
	__svdmh.SVDM.Command 		= DP_COMMAND_CONFIG;		

    __arr[0] = __svdmh.object;
    __length++;

    __arr[1] = in.word;
    __length++;

    sendVdmMessage(SOP_TYPE_SOP, __arr, __length, PolicyState);
}

#endif 
