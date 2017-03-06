#ifndef _FSC_INTERFACE_DP_H
#define _FSC_INTERFACE_DP_H

#include "../../platform.h"
#include "dp_types.h"

void requestDpStatus(void);
void requestDpConfig(DisplayPortConfig_t in);
void configDp(FSC_BOOL enabled, FSC_U32 status);
void configAutoDpModeEntry(FSC_BOOL enabled, FSC_U32 mask, FSC_U32 value);
void WriteDpControls(FSC_U8* data);
void ReadDpControls(FSC_U8* data);
void ReadDpStatus(FSC_U8* data);

void informStatus(DisplayPortStatus_t stat);
void updateStatusData(void);
void informConfigResult (FSC_BOOL success);
FSC_BOOL DpReconfigure(DisplayPortConfig_t config);

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

#endif 
