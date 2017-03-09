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

#ifndef __VDM_BITFIELD_TRANSLATORS_H__
#define __VDM_BITFIELD_TRANSLATORS_H__

#include "../platform.h"

	UnstructuredVdmHeader 	getUnstructuredVdmHeader(FSC_U32 in);	
	StructuredVdmHeader 	getStructuredVdmHeader(FSC_U32 in);		
	IdHeader 				getIdHeader(FSC_U32 in);					
	VdmType 				getVdmTypeOf(FSC_U32 in);				

	FSC_U32 	getBitsForUnstructuredVdmHeader(UnstructuredVdmHeader in);	
	FSC_U32 	getBitsForStructuredVdmHeader(StructuredVdmHeader in);		
	FSC_U32 	getBitsForIdHeader(IdHeader in);							

	CertStatVdo 			getCertStatVdo(FSC_U32 in);
	ProductVdo 				getProductVdo(FSC_U32 in);
	CableVdo 				getCableVdo(FSC_U32 in);
	AmaVdo 					getAmaVdo(FSC_U32 in);

	FSC_U32 getBitsForProductVdo(ProductVdo in);	
	FSC_U32 getBitsForCertStatVdo(CertStatVdo in);	
	FSC_U32	getBitsForCableVdo(CableVdo in);		
	FSC_U32	getBitsForAmaVdo(AmaVdo in);			

#endif 

#endif 
