#include <linux/kernel.h>
#include "core.h"
#include "TypeC.h"
#include "PDProtocol.h"
#include "PDPolicy.h"

#ifdef FSC_DEBUG
#include "version.h"
#endif 

void core_initialize(void)
{
    InitializeTypeCVariables();                     
    InitializePDProtocolVariables();                
    InitializePDPolicyVariables();                  
    InitializeTypeC();
}

void core_enable_typec(FSC_BOOL enable)
{
    if (enable == TRUE) EnableTypeCStateMachine();
    else                DisableTypeCStateMachine();
}

void core_state_machine(void)
{
    StateMachineTypeC();
}

void core_tick_at_100us(void)
{
    TypeCTickAt100us();
    ProtocolTickAt100us();
    PolicyTickAt100us();

#ifdef FSC_DEBUG
    LogTickAt100us();
#endif 
}

#ifdef FSC_DEBUG
FSC_U8 core_get_rev_lower(void)
{
    return FSC_TYPEC_CORE_FW_REV_LOWER;
}

FSC_U8 core_get_rev_middle(void)
{
    return FSC_TYPEC_CORE_FW_REV_MIDDLE;
}

FSC_U8 core_get_rev_upper(void)
{
    return FSC_TYPEC_CORE_FW_REV_UPPER;
}
#endif 

void core_set_vbus_transition_time(FSC_U32 time_ms)
{
    SetVbusTransitionTime(time_ms);
}

void core_enable_pd(FSC_BOOL enable)
{
	printk("FUSB  %s : PD detection enable: %d\n", __func__, enable);
    if (enable == TRUE) EnableUSBPD();
    else                DisableUSBPD();
}

#ifdef FSC_DEBUG
void core_configure_port_type(FSC_U8 config)
{
    ConfigurePortType(config);
}

#ifdef FSC_HAVE_SRC
void core_set_source_caps(FSC_U8* buf)
{
    WriteSourceCapabilities(buf);
}

void core_get_source_caps(FSC_U8* buf)
{
    ReadSourceCapabilities(buf);
}
#endif 

#ifdef FSC_HAVE_SNK
void core_set_sink_caps(FSC_U8* buf)
{
    WriteSinkCapabilities(buf);
}

void core_get_sink_caps(FSC_U8* buf)
{
    ReadSinkCapabilities(buf);
}

void core_set_sink_req(FSC_U8* buf)
{
    WriteSinkRequestSettings(buf);
}

void core_get_sink_req(FSC_U8* buf)
{
    ReadSinkRequestSettings(buf);
}
#endif 

void core_send_hard_reset(void)
{
    SendUSBPDHardReset();
}

void core_process_pd_buffer_read(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    ProcessPDBufferRead(InBuffer, OutBuffer);
}

void core_process_typec_pd_status(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    ProcessTypeCPDStatus(InBuffer, OutBuffer);
}

void core_process_typec_pd_control(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    ProcessTypeCPDControl(InBuffer, OutBuffer);
}

void core_process_local_register_request(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    ProcessLocalRegisterRequest(InBuffer, OutBuffer);
}

void core_process_set_typec_state(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    ProcessSetTypeCState(InBuffer, OutBuffer);
}

void core_process_read_typec_state_log(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
   ProcessReadTypeCStateLog(InBuffer, OutBuffer);
}

void core_process_read_pd_state_log(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    ProcessReadPDStateLog(InBuffer, OutBuffer);
}

void core_set_alternate_modes(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    setAlternateModes(InBuffer[3]);
}

void core_set_manual_retries(FSC_U8* InBuffer, FSC_U8* OutBuffer)
{
    setManualRetries(InBuffer[4]);
}

FSC_U8 core_get_alternate_modes(void)
{
    return getAlternateModes();
}

FSC_U8 core_get_manual_retries(void)
{
    return getManualRetries();
}

void core_set_state_unattached(void)
{
    SetStateUnattached();
}

#endif 
