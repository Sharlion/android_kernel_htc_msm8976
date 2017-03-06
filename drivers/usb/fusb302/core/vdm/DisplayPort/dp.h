#ifdef FSC_HAVE_DP

#ifndef __DISPLAYPORT_DP_H__
#define __DISPLAYPORT_DP_H__

#include "../../platform.h"
#include "../../PD_Types.h"
#include "../vdm_types.h"
#include "dp_types.h"

extern FSC_BOOL DpEnabled;
extern FSC_BOOL DpAutoModeEntryEnabled;
extern DisplayPortCaps_t DpModeEntryMask;
extern DisplayPortCaps_t DpModeEntryValue;
extern DisplayPortCaps_t DpCaps;
extern DisplayPortStatus_t DpStatus;
extern DisplayPortConfig_t DpConfig;
extern DisplayPortStatus_t DpPpStatus;
extern DisplayPortConfig_t DpPpRequestedConfig;
extern DisplayPortConfig_t DpPpConfig;
extern FSC_U32 DpModeEntered;

void initializeDp(void);

void resetDp(void);

FSC_BOOL processDpCommand(FSC_U32* arr_in);

void sendStatusData(doDataObject_t svdmh_in);

void replyToConfig(doDataObject_t svdmh_in, FSC_BOOL success);

FSC_BOOL dpEvaluateModeEntry(FSC_U32 mode_in);


#endif 

#endif 
