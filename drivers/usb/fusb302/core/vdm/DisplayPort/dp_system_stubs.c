#ifdef FSC_HAVE_DP

#include "../../platform.h"
#include "interface_dp.h"

void informStatus(DisplayPortStatus_t stat)
{
	
	
    DpPpStatus.word = stat.word;
}

void informConfigResult (FSC_BOOL success)
{
	
	
    if (success == TRUE) DpPpConfig.word = DpPpRequestedConfig.word;
}

void updateStatusData(void)
{
	
	
}

FSC_BOOL DpReconfigure(DisplayPortConfig_t config)
{
    
    
    
    DpConfig.word = config.word;
    
    return TRUE;
}

#endif 

