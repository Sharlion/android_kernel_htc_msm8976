/****************************************************************************
 * Company:         Fairchild Semiconductor
 *
 * Author           Date          Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * G. Noblesmith
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
#ifdef FSC_HAVE_VDM

#include "vdm.h"
#include "../platform.h"
#include "../PDPolicy.h"
#include "../PD_Types.h"
#include "vdm_types.h"
#include "bitfield_translators.h"
#include "fsc_vdm_defs.h"

#ifdef FSC_HAVE_DP
#include "DisplayPort/dp.h"
#endif 

extern 	PolicyState_t 	PolicyState;
extern  FSC_U32         VdmTimer;
extern  FSC_BOOL        VdmTimerStarted;
extern  PolicyState_t   vdm_next_ps;

VdmManager              vdmm;
PolicyState_t           originalPolicyState;
FSC_BOOL				vdm_timeout;
FSC_BOOL                ExpectingVdmResponse;

FSC_S32 initializeVdm() {
	vdm_timeout = FALSE;
    ExpectingVdmResponse = FALSE;

#ifdef FSC_HAVE_DP
    initializeDp();
#endif 

	return 0;
}

FSC_S32 requestDiscoverIdentity(SopType sop) {
	doDataObject_t __vdmh = {0};
	FSC_U32 __length = 1;
	FSC_U32 __arr[__length];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for(i=0;i<__length;i++)
    {
        __arr[i] = 0;
    }

	if ((PolicyState == peSinkReady) || (PolicyState == peSourceReady)) {
		
		originalPolicyState = PolicyState;
		if (sop == SOP_TYPE_SOP) {
			__n_pe = peDfpUfpVdmIdentityRequest;
		} else if (sop == SOP_TYPE_SOP1) {
			__n_pe = peDfpCblVdmIdentityRequest;
			
		} else {
			return 1;
		}

		__vdmh.SVDM.SVID 		= PD_SID; 					
		__vdmh.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh.SVDM.Version     = STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos      = 0;						
		__vdmh.SVDM.CommandType	= INITIATOR;				
		__vdmh.SVDM.Command 	= DISCOVER_IDENTITY;		

		__arr[0] = __vdmh.object;
		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	} else if (	(sop == SOP_TYPE_SOP1) && 		
				((PolicyState == peSourceStartup) ||
				 (PolicyState == peSourceDiscovery))) {
		originalPolicyState = PolicyState;
		__n_pe = peSrcVdmIdentityRequest;
		__vdmh.SVDM.SVID 		= PD_SID; 					
		__vdmh.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh.SVDM.Version 	= STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos 		= 0;						
		__vdmh.SVDM.CommandType = INITIATOR;				
		__vdmh.SVDM.Command 	= DISCOVER_IDENTITY;		

		__arr[0] = __vdmh.object;

		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	} else {
		return 1;
	}

	return 0;
}

FSC_S32 requestDiscoverSvids(SopType sop) {
	doDataObject_t __vdmh = {0};
	FSC_U32 __length = 1;
	FSC_U32 arr[__length];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for(i=0;i<__length;i++)
    {
        arr[i] = 0;
    }


	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmSvidsRequest;

		__vdmh.SVDM.SVID 		= PD_SID; 					
		__vdmh.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh.SVDM.Version 	= STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos 		= 0;						
		__vdmh.SVDM.CommandType	= INITIATOR;				
		__vdmh.SVDM.Command 	= DISCOVER_SVIDS;			

		arr[0] = __vdmh.object;

		sendVdmMessageWithTimeout(sop, arr, __length, __n_pe);
	}
	return 0;
}

FSC_S32 requestDiscoverModes(SopType sop, FSC_U16 svid) {
	doDataObject_t __vdmh = {0};
	FSC_U32 __length = 1;
	FSC_U32 __arr[__length];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for(i=0;i<__length;i++)
    {
        __arr[i] = 0;
    }


	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmModesRequest;

		__vdmh.SVDM.SVID 			= svid; 					
		__vdmh.SVDM.VDMType 		= STRUCTURED_VDM;			
		__vdmh.SVDM.Version         = STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos          = 0;						
		__vdmh.SVDM.CommandType 	= INITIATOR;				
		__vdmh.SVDM.Command 		= DISCOVER_MODES;			

		__arr[0] = __vdmh.object;

		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	}
	return 0;
}

FSC_S32 requestSendAttention(SopType sop, FSC_U16 svid, FSC_U8 mode) {
	doDataObject_t __vdmh = {0};
	FSC_U32 __length = 1;
	FSC_U32 __arr[__length];
	FSC_U32 i;
    for(i=0;i<__length;i++)
    {
        __arr[i] = 0;
    }

	originalPolicyState = PolicyState;

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		__vdmh.SVDM.SVID 			= svid; 					
		__vdmh.SVDM.VDMType 		= STRUCTURED_VDM;			
		__vdmh.SVDM.Version         = STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos          = mode;						
		__vdmh.SVDM.CommandType 	= INITIATOR;				
		__vdmh.SVDM.Command 		= ATTENTION;				

		__arr[0] = __vdmh.object;
		__length = 1;
		sendVdmMessage(sop, __arr, __length, PolicyState);
	}
	return 0;
}

FSC_S32 processDiscoverIdentity(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t  __vdmh_in = {0};
	doDataObject_t 	__vdmh_out = {0};

	IdHeader		__idh;
	CertStatVdo		__csvdo;
    Identity        __id;
    ProductVdo      __pvdo;

	FSC_U32 			__arr[7] = {0};
	FSC_U32          __length;

    FSC_BOOL            __result;

    __vdmh_in.object = arr_in[0];

    
    if (__vdmh_in.SVDM.SVID != PD_SID) return -1;

    if (__vdmh_in.SVDM.CommandType == INITIATOR) {


        if ((sop == SOP_TYPE_SOP) && ((PolicyState == peSourceReady) || (PolicyState == peSinkReady))) {
        	originalPolicyState = PolicyState;
        	PolicyState = peUfpVdmGetIdentity;
        	__id = vdmm.req_id_info();
        	PolicyState = peUfpVdmSendIdentity;
        } else if ((sop == SOP_TYPE_SOP1) && (PolicyState == peCblReady)) {
        	originalPolicyState = PolicyState;
        	PolicyState = peCblGetIdentity;
        	__id = vdmm.req_id_info();

        	if (__id.nack) {
        		PolicyState = peCblGetIdentityNak;
        	} else {
        		PolicyState = peCblSendIdentity;
        	}
        } else {
        	return 1;
        }

        __vdmh_out.SVDM.SVID 		= PD_SID;					
        __vdmh_out.SVDM.VDMType 	= STRUCTURED_VDM;			
        __vdmh_out.SVDM.Version     = STRUCTURED_VDM_VERSION;	
        __vdmh_out.SVDM.ObjPos		= 0;						
        if (__id.nack) {
            __vdmh_out.SVDM.CommandType 	= RESPONDER_NAK;
        } else {
            __vdmh_out.SVDM.CommandType 	= RESPONDER_ACK;
        }
        __vdmh_out.SVDM.Command		= DISCOVER_IDENTITY;		
        __arr[0] = __vdmh_out.object;
        __length = 1;

        if (PolicyState == peCblGetIdentityNak) {
        	__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        } else {
            
            __idh = __id.id_header;

            
            __csvdo = __id.cert_stat_vdo;

            __arr[1] = getBitsForIdHeader(__idh);
            __length++;
            __arr[2] = getBitsForCertStatVdo(__csvdo);
            __length++;

            
            __pvdo = __id.product_vdo;
            __arr[__length] = getBitsForProductVdo(__pvdo);
            __length++;

            
            if ((__idh.product_type == PASSIVE_CABLE)	|| (__idh.product_type == ACTIVE_CABLE)) {
                CableVdo cvdo_out;
                cvdo_out = __id.cable_vdo;
                __arr[__length] = getBitsForCableVdo(cvdo_out);
                __length++;
            }

            
            if (__idh.product_type == AMA) {
        		AmaVdo amavdo_out;
        		amavdo_out = __id.ama_vdo;

        		__arr[__length] = getBitsForAmaVdo(amavdo_out);
        		__length++;
        	}
        }

        sendVdmMessage(sop, __arr, __length, originalPolicyState);
        return 0;
    } else { 
        if (vdm_timeout) return 0;
		if ((PolicyState != peDfpUfpVdmIdentityRequest) &&
			(PolicyState != peDfpCblVdmIdentityRequest) &&
			(PolicyState != peSrcVdmIdentityRequest)) return 0;

		
		if (length_in < MIN_DISC_ID_RESP_SIZE) {
			PolicyState = originalPolicyState;
			return 1;
		}
		if ((PolicyState == peDfpUfpVdmIdentityRequest) && (sop == SOP_TYPE_SOP)) {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				PolicyState = peDfpUfpVdmIdentityAcked;
			} else {
				PolicyState = peDfpUfpVdmIdentityNaked;
			}
		} else if ((PolicyState == peDfpCblVdmIdentityRequest) && (sop == SOP_TYPE_SOP1)) {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				PolicyState = peDfpCblVdmIdentityAcked;
			} else {
				PolicyState = peDfpCblVdmIdentityNaked;
			}
		} else if ((PolicyState == peSrcVdmIdentityRequest) && (sop == SOP_TYPE_SOP1)) {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				PolicyState = peSrcVdmIdentityAcked;
			} else {
				PolicyState = peSrcVdmIdentityNaked;
            }
		} else {
			
		}

		if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
			__id.id_header = 		getIdHeader(arr_in[1]);
			__id.cert_stat_vdo =	getCertStatVdo(arr_in[2]);

			if ((__id.id_header.product_type == HUB) 		||
				(__id.id_header.product_type == PERIPHERAL)	||
				(__id.id_header.product_type == AMA)) {
				__id.has_product_vdo = TRUE;
				__id.product_vdo = getProductVdo(arr_in[3]); 
            }

			if ((__id.id_header.product_type == PASSIVE_CABLE)	||
				(__id.id_header.product_type == ACTIVE_CABLE)) {
				__id.has_cable_vdo = TRUE;
				__id.cable_vdo = getCableVdo(arr_in[3]);
			}

			if ((__id.id_header.product_type == AMA)) {
				__id.has_ama_vdo = TRUE;
				__id.ama_vdo = getAmaVdo(arr_in[4]); 
			}
		}

		__result =      (PolicyState == peDfpUfpVdmIdentityAcked) ||
						(PolicyState == peDfpCblVdmIdentityAcked) ||
						(PolicyState == peSrcVdmIdentityAcked);
		vdmm.inform_id(__result, sop, __id);
        ExpectingVdmResponse = FALSE;
		PolicyState = originalPolicyState;
        VdmTimer = 0;
        VdmTimerStarted = FALSE;
		return 0;
    }
}

FSC_S32 processDiscoverSvids(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t  __vdmh_in = {0};
	doDataObject_t 	__vdmh_out = {0};

	SvidInfo        __svid_info;

    FSC_U32          __i;
    FSC_U16          __top16;
    FSC_U16          __bottom16;

	FSC_U32 			__arr[7] = {0};
	FSC_U32          __length;


    __vdmh_in.object = arr_in[0];

    
    if (__vdmh_in.SVDM.SVID != PD_SID) return -1;

    if (__vdmh_in.SVDM.CommandType == INITIATOR) {
        if ((sop == SOP_TYPE_SOP) && ((PolicyState == peSourceReady) || (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			
            __svid_info = vdmm.req_svid_info();
			PolicyState = peUfpVdmGetSvids;
			PolicyState = peUfpVdmSendSvids;
		} else if ((sop == SOP_TYPE_SOP1) && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblGetSvids;
			__svid_info = vdmm.req_svid_info();
			if (__svid_info.nack) {
				PolicyState = peCblGetSvidsNak;
			} else {
				PolicyState = peCblSendSvids;
			}
		} else {
			return 1;
		}

		__vdmh_out.SVDM.SVID 		= PD_SID;					
		__vdmh_out.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh_out.SVDM.Version     = STRUCTURED_VDM_VERSION;	
		__vdmh_out.SVDM.ObjPos		= 0;						
        if (__svid_info.nack) {
            __vdmh_out.SVDM.CommandType	= RESPONDER_NAK;
        } else {
            __vdmh_out.SVDM.CommandType	= RESPONDER_ACK;
        }
		__vdmh_out.SVDM.Command		= DISCOVER_SVIDS;			

        __length = 0;
		__arr[__length] = __vdmh_out.object;
		__length++;

		if (PolicyState == peCblGetSvidsNak) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			FSC_U32 i;
			
			if (__svid_info.num_svids > MAX_NUM_SVIDS) {
				PolicyState = originalPolicyState;
				return 1;
			}

			for (i = 0; i < __svid_info.num_svids; i++) {
				
				if (!(i & 0x1)) {
                    __length++;

                    
                    __arr[__length-1] = 0;

                    
                    __arr[__length-1] |= __svid_info.svids[i];
                    __arr[__length-1] <<= 16;
                } else {
                    
                    __arr[__length-1] |= __svid_info.svids[i];
				}
			}
		}

		sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
    } else { 
        __svid_info.num_svids = 0;

        if (PolicyState != peDfpVdmSvidsRequest) {
			return 1;
		} else if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
			for (__i = 1; __i < length_in; __i++) {
				__top16 = 	(arr_in[__i] >> 16) & 0x0000FFFF;
				__bottom16 =	(arr_in[__i] >>  0) & 0x0000FFFF;

				
				if (__top16 == 0x0000) {
					break;
				} else {
					__svid_info.svids[2*(__i-1)] = __top16;
					__svid_info.num_svids += 1;
				}
				
				if (__bottom16 == 0x0000) {
					break;
				} else {
					__svid_info.svids[2*(__i-1)+1] = __bottom16;
					__svid_info.num_svids += 1;
				}
			}
			PolicyState = peDfpVdmSvidsAcked;
		} else {
			PolicyState = peDfpVdmSvidsNaked;
		}

		vdmm.inform_svids(	PolicyState == peDfpVdmSvidsAcked,
								sop, __svid_info);

        ExpectingVdmResponse = FALSE;
        VdmTimer = 0;
        VdmTimerStarted = FALSE;
		PolicyState = originalPolicyState;
		return 0;
    }
}

FSC_S32 processDiscoverModes(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t  __vdmh_in = {0};
	doDataObject_t 	__vdmh_out = {0};

    ModesInfo       __modes_info = {0};

    FSC_U32          __i;
	FSC_U32 			__arr[7] = {0};
	FSC_U32          __length;

    __vdmh_in.object = arr_in[0];
    if (__vdmh_in.SVDM.CommandType == INITIATOR) {
        if ((sop == SOP_TYPE_SOP) && ((PolicyState == peSourceReady) || (PolicyState == peSinkReady))) {
            originalPolicyState = PolicyState;
            __modes_info = vdmm.req_modes_info(__vdmh_in.SVDM.SVID);
            PolicyState = peUfpVdmGetModes;
            PolicyState = peUfpVdmSendModes;
        } else if ((sop == SOP_TYPE_SOP1) && (PolicyState == peCblReady)) {
            originalPolicyState = PolicyState;
            PolicyState = peCblGetModes;
            __modes_info = vdmm.req_modes_info(__vdmh_in.SVDM.SVID);

			if (__modes_info.nack) {
				PolicyState = peCblGetModesNak;
			} else {
				PolicyState = peCblSendModes;
			}
		} else {
			return 1;
		}

		__vdmh_out.SVDM.SVID 		= __vdmh_in.SVDM.SVID;			
		__vdmh_out.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh_out.SVDM.Version     = STRUCTURED_VDM_VERSION;	
		__vdmh_out.SVDM.ObjPos		= 0;						
        if (__modes_info.nack) {
            __vdmh_out.SVDM.CommandType	= RESPONDER_NAK;
        } else {
			__vdmh_out.SVDM.CommandType	= RESPONDER_ACK;
        }
		__vdmh_out.SVDM.Command		= DISCOVER_MODES;			

        __length = 0;
		__arr[__length] = __vdmh_out.object;
		__length++;

		if (PolicyState == peCblGetModesNak) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			FSC_U32 j;

			for (j = 0; j < __modes_info.num_modes; j++) {
				__arr[j+1] = __modes_info.modes[j];
				__length++;
			}
		}

		sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
    } else { 
        if (PolicyState != peDfpVdmModesRequest) {
			return 1;
		} else {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				__modes_info.svid 		= __vdmh_in.SVDM.SVID;
				__modes_info.num_modes = length_in - 1;

                for (__i = 1; __i < length_in; __i++) {
					__modes_info.modes[__i - 1] = arr_in[__i];
				}
				PolicyState = peDfpVdmModesAcked;
			} else {
				PolicyState = peDfpVdmModesNaked;
			}

			vdmm.inform_modes(	PolicyState == peDfpVdmModesAcked,
									sop, __modes_info);
            ExpectingVdmResponse = FALSE;
            VdmTimer = 0;
            VdmTimerStarted = FALSE;
			PolicyState = originalPolicyState;
		}

		return 0;
    }
}

FSC_S32 processEnterMode(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t  __svdmh_in = {0};
	doDataObject_t 	__svdmh_out = {0};

    FSC_BOOL            __mode_entered;
	FSC_U32 			__arr_out[7] = {0};
	FSC_U32          __length_out;

    __svdmh_in.object = arr_in[0];
    if (__svdmh_in.SVDM.CommandType == INITIATOR) {
        if ((sop == SOP_TYPE_SOP) && ((PolicyState == peSourceReady) || (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			PolicyState = peUfpVdmEvaluateModeEntry;
			__mode_entered = vdmm.req_mode_entry(__svdmh_in.SVDM.SVID, __svdmh_in.SVDM.ObjPos);

			
			if (__mode_entered) {
				PolicyState                 = peUfpVdmModeEntryAck;
				__svdmh_out.SVDM.CommandType	= RESPONDER_ACK;		
			} else {
				PolicyState                 = peUfpVdmModeEntryNak;
				__svdmh_out.SVDM.CommandType 	= RESPONDER_NAK;		
			}
		} else if ((sop == SOP_TYPE_SOP1) && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblEvaluateModeEntry;
			__mode_entered = vdmm.req_mode_entry(__svdmh_in.SVDM.SVID, __svdmh_in.SVDM.ObjPos);
			
			if (__mode_entered) {
				PolicyState                 = peCblModeEntryAck;
				__svdmh_out.SVDM.CommandType	= RESPONDER_ACK;		
			} else {
				PolicyState                 = peCblModeEntryNak;
				__svdmh_out.SVDM.CommandType 	= RESPONDER_NAK;		
			}
		} else {
			return 1;
		}

		
		__svdmh_out.SVDM.SVID 		= __svdmh_in.SVDM.SVID;		
		__svdmh_out.SVDM.VDMType 		= STRUCTURED_VDM;			
		__svdmh_out.SVDM.Version      = STRUCTURED_VDM_VERSION;	
		__svdmh_out.SVDM.ObjPos		= __svdmh_in.SVDM.ObjPos;		
		__svdmh_out.SVDM.Command		= ENTER_MODE;				

		__arr_out[0] = __svdmh_out.object;
		__length_out = 1;

		sendVdmMessage(sop, __arr_out, __length_out, originalPolicyState);
		return 0;
    } else { 
		if (__svdmh_in.SVDM.CommandType != RESPONDER_ACK) {
			PolicyState = peDfpVdmModeEntryNaked;
			vdmm.enter_mode_result(FALSE, __svdmh_in.SVDM.SVID, __svdmh_in.SVDM.ObjPos);
		} else {
			PolicyState = peDfpVdmModeEntryAcked;
			vdmm.enter_mode_result(TRUE, __svdmh_in.SVDM.SVID, __svdmh_in.SVDM.ObjPos);
		}
		PolicyState = originalPolicyState;
        ExpectingVdmResponse = FALSE;
		return 0;
    }
}

FSC_S32 processExitMode(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t  __vdmh_in = {0};
	doDataObject_t 	__vdmh_out = {0};

	FSC_BOOL            __mode_exited;
	FSC_U32 			__arr[7] = {0};
	FSC_U32          __length;

    __vdmh_in.object = arr_in[0];
    if (__vdmh_in.SVDM.CommandType == INITIATOR) {
        if ((sop == SOP_TYPE_SOP) && ((PolicyState == peSourceReady) || (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			PolicyState = peUfpVdmModeExit;
			__mode_exited = vdmm.req_mode_exit(__vdmh_in.SVDM.SVID, __vdmh_in.SVDM.ObjPos);

			
			if (__mode_exited) {
				PolicyState 			= peUfpVdmModeExitAck;
				__vdmh_out.SVDM.CommandType		= RESPONDER_ACK;		
			} else {
				PolicyState 			= peUfpVdmModeExitNak;
				__vdmh_out.SVDM.CommandType 		= RESPONDER_NAK;		
			}
		} else if ((sop == SOP_TYPE_SOP1) && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblModeExit;
			__mode_exited = vdmm.req_mode_exit(__vdmh_in.SVDM.SVID, __vdmh_in.SVDM.ObjPos);

			
			if (__mode_exited) {
				PolicyState 			= peCblModeExitAck;
				__vdmh_out.SVDM.CommandType		= RESPONDER_ACK;		
			} else {
				PolicyState 			= peCblModeExitNak;
				__vdmh_out.SVDM.CommandType 		= RESPONDER_NAK;		
			}
		} else {
			return 1;
		}

		
		__vdmh_out.SVDM.SVID 		= __vdmh_in.SVDM.SVID;			
		__vdmh_out.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh_out.SVDM.Version     = STRUCTURED_VDM_VERSION;	
		__vdmh_out.SVDM.ObjPos		= __vdmh_in.SVDM.ObjPos;			
		__vdmh_out.SVDM.Command		= EXIT_MODE;				

		__arr[0] = __vdmh_out.object;
		__length = 1;

        sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
    } else {
		if (__vdmh_in.SVDM.CommandType != RESPONDER_ACK) {
			vdmm.exit_mode_result(FALSE, __vdmh_in.SVDM.SVID, __vdmh_in.SVDM.ObjPos);
			
			if (originalPolicyState == peSourceReady) {
				PolicyState = peSourceHardReset;
			} else if (originalPolicyState == peSinkReady) {
				PolicyState = peSinkHardReset;
			} else {
				
			}
		} else {
			PolicyState = peDfpVdmExitModeAcked;
			vdmm.exit_mode_result(TRUE, __vdmh_in.SVDM.SVID, __vdmh_in.SVDM.ObjPos);
			PolicyState = originalPolicyState;

            VdmTimer = 0;
            VdmTimerStarted = FALSE;
		}
        ExpectingVdmResponse = FALSE;
		return 0;
    }
}
FSC_S32 processAttention(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t     __vdmh_in = {0};
    __vdmh_in.object = arr_in[0];

    originalPolicyState = PolicyState;
	PolicyState = peDfpVdmAttentionRequest;
	vdmm.inform_attention(__vdmh_in.SVDM.SVID, __vdmh_in.SVDM.ObjPos);
	PolicyState = originalPolicyState;

	return 0;
}
FSC_S32 processSvidSpecific(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
    doDataObject_t 	__vdmh_out = {0};
    doDataObject_t 	__vdmh_in = {0};
    FSC_U32 			__arr[7] = {0};
	FSC_U32          __length;

    __vdmh_in.object = arr_in[0];

#ifdef FSC_HAVE_DP
    if (__vdmh_in.SVDM.SVID == DP_SID) {
        if (!processDpCommand(arr_in)) {
            return 0; 
        }
    }
#endif 

    
	__vdmh_out.SVDM.SVID 			= __vdmh_in.SVDM.SVID;			
	__vdmh_out.SVDM.VDMType 		= STRUCTURED_VDM;			
	__vdmh_out.SVDM.Version         = STRUCTURED_VDM_VERSION;	
	__vdmh_out.SVDM.ObjPos          = 0;						
	__vdmh_out.SVDM.CommandType		= RESPONDER_NAK;			
	__vdmh_out.SVDM.Command         = __vdmh_in.SVDM.Command;		

	__arr[0] = __vdmh_out.object;
	__length = 1;

	sendVdmMessage(sop, __arr, __length, originalPolicyState);
	return 0;
}

FSC_S32 processVdmMessage(SopType sop, FSC_U32* arr_in, FSC_U32 length_in) {
	doDataObject_t 	__vdmh_in = {0};

    __vdmh_in.object = arr_in[0];
	if (__vdmh_in.SVDM.VDMType == STRUCTURED_VDM) {

		
		switch (__vdmh_in.SVDM.Command) {
		case DISCOVER_IDENTITY	:
			return processDiscoverIdentity(sop, arr_in, length_in);
		case DISCOVER_SVIDS		:
            return processDiscoverSvids(sop, arr_in, length_in);
		case DISCOVER_MODES		:
            return processDiscoverModes(sop, arr_in, length_in);
		case ENTER_MODE			:
			return processEnterMode(sop, arr_in, length_in);
		case EXIT_MODE			:
			return processExitMode(sop, arr_in, length_in);
		case ATTENTION			:
            return processAttention(sop, arr_in, length_in);
		default					:
			
			return processSvidSpecific(sop, arr_in, length_in);
		}
	} else {
		
		return 1;
	}
}

FSC_S32 requestEnterMode(SopType sop, FSC_U16 svid, FSC_U32 mode_index) {
	doDataObject_t __vdmh = {0};
	FSC_U32 __length = 1;
	FSC_U32 __arr[__length];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for(i=0;i<__length;i++)
    {
        __arr[i] = 0;
    }


	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmModeEntryRequest;

		__vdmh.SVDM.SVID 		= svid; 					
		__vdmh.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh.SVDM.Version     = STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos      = mode_index;				
		__vdmh.SVDM.CommandType = INITIATOR;				
		__vdmh.SVDM.Command 	= ENTER_MODE;				

		__arr[0] = __vdmh.object;
		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	}
	return 0;
}

FSC_S32 requestExitMode(SopType sop, FSC_U16 svid, FSC_U32 mode_index) {
    doDataObject_t __vdmh = {0};
    FSC_U32 __length = 1;
	FSC_U32 __arr[__length];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for(i=0;i<__length;i++)
    {
        __arr[i] = 0;
    }

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmModeExitRequest;

		__vdmh.SVDM.SVID 		= svid; 					
		__vdmh.SVDM.VDMType 	= STRUCTURED_VDM;			
		__vdmh.SVDM.Version 	= STRUCTURED_VDM_VERSION;	
		__vdmh.SVDM.ObjPos 		= mode_index;				
		__vdmh.SVDM.CommandType = INITIATOR;				
		__vdmh.SVDM.Command 	= EXIT_MODE;				

		__arr[0] = __vdmh.object;
		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	}
	return 0;
}

FSC_S32 sendAttention(SopType sop, FSC_U32 obj_pos) {
	
	return 1;
}

void sendVdmMessageWithTimeout (SopType sop, FSC_U32* arr, FSC_U32 length, FSC_S32 n_pe) {
	sendVdmMessage(sop, arr, length, n_pe);
    ExpectingVdmResponse = TRUE;
}

FSC_BOOL expectingVdmResponse(void) {
    return ExpectingVdmResponse;
}

void startVdmTimer(FSC_S32 n_pe) {
    
    switch (n_pe) {
    case peDfpUfpVdmIdentityRequest:
    case peDfpCblVdmIdentityRequest:
    case peSrcVdmIdentityRequest:
    case peDfpVdmSvidsRequest:
    case peDfpVdmModesRequest:
        VdmTimer = tVDMSenderResponse;
        VdmTimerStarted = TRUE;
        break;
    case peDfpVdmModeEntryRequest:
        VdmTimer = tVDMWaitModeEntry;
        VdmTimerStarted = TRUE;
        break;
    case peDfpVdmModeExitRequest:
        VdmTimer = tVDMWaitModeExit;
        VdmTimerStarted = TRUE;
        break;
    case peDpRequestStatus:
        VdmTimer = tVDMSenderResponse;
        VdmTimerStarted = TRUE;
        break;
    default:
        VdmTimer = 0;
        VdmTimerStarted = TRUE; 
        return;
    }
}

void sendVdmMessageFailed() {
    resetPolicyState();
}

void vdmMessageTimeout(void) {
    resetPolicyState();
}

void resetPolicyState (void) {
    Identity __id = {0}; 
	SvidInfo __svid_info = {0}; 
	ModesInfo __modes_info = {0}; 

	ExpectingVdmResponse = FALSE;
    VdmTimerStarted = FALSE;

    if (PolicyState == peGiveVdm) {
        PolicyState = vdm_next_ps;
    }

	switch (PolicyState) {
	case peDfpUfpVdmIdentityRequest:
		PolicyState = peDfpUfpVdmIdentityNaked;
		vdmm.inform_id(FALSE, SOP_TYPE_SOP, __id); 
		PolicyState = originalPolicyState;
        break;
	case peDfpCblVdmIdentityRequest:
		PolicyState = peDfpCblVdmIdentityNaked;
		vdmm.inform_id(FALSE, SOP_TYPE_SOP1, __id); 
		PolicyState = originalPolicyState;
        break;
	case peDfpVdmSvidsRequest:
		PolicyState = peDfpVdmSvidsNaked;
		vdmm.inform_svids(FALSE, SOP_TYPE_SOP, __svid_info);
		PolicyState = originalPolicyState;
        break;
	case peDfpVdmModesRequest:
		PolicyState = peDfpVdmModesNaked;
		vdmm.inform_modes(FALSE, SOP_TYPE_SOP, __modes_info);
		PolicyState = originalPolicyState;
		break;
	case peDfpVdmModeEntryRequest:
		PolicyState = peDfpVdmModeEntryNaked;
		vdmm.enter_mode_result(FALSE, 0, 0);
                PolicyState = originalPolicyState;
		break;
	case peDfpVdmModeExitRequest:
		vdmm.exit_mode_result(FALSE, 0, 0);

		
		if (originalPolicyState == peSinkReady) {
			PolicyState = peSinkHardReset;
		} else if (originalPolicyState == peSourceReady) {
			PolicyState = peSourceHardReset;
		} else {
			
		}
        PolicyState = originalPolicyState;
		return;
	case peSrcVdmIdentityRequest:
		PolicyState = peSrcVdmIdentityNaked;
		vdmm.inform_id(FALSE, SOP_TYPE_SOP1, __id); 
        PolicyState = originalPolicyState;
		break;
	default:
        PolicyState = originalPolicyState;
		break;
	}
}

#endif 
