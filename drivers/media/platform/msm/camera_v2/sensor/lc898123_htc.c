/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"

#include "lc898123_htc.h"
#include "lc898123_Ois.h"
#include "lc898123_FromCode.h"	


static struct msm_sensor_ctrl_t *g_s_ctrl = NULL;

int lc898123_check_ois_component(struct msm_sensor_ctrl_t *s_ctrl, int valid_otp_ois, uint8_t *otp_NVR0_data, uint8_t *otp_NVR1_data)
{
	int rc = 0;
	unsigned int UlReadVal = 0;
	unsigned int UlFwVer = 0;
	int i;

	g_s_ctrl = s_ctrl;
	msleep(100);

	
	RamRead32A(SiVerNum, &UlFwVer);
	pr_info("[OIS] %s : Firmware Version = 0x%08x\n", __func__, UlFwVer);

	
	RamWrite32A(CMD_IO_ADR_ACCESS, CVER_123);
	RamRead32A(CMD_IO_DAT_ACCESS, &UlReadVal);
	pr_info("[OIS] %s : ChipVersion = 0x%x\n", __func__, (unsigned int)UlReadVal);

	
	if (UlReadVal != 0xB4) {
		if (UlReadVal == 0xB6) {
			OscStb();
		}
		pr_info("[OIS] %s : ChipVersion=0x%x , check complete\n", __func__, (unsigned int)UlReadVal);
		return 0;
	}


	
	for (i=0; i<8; i++) {
		if (otp_NVR0_data[i] != i) {
			pr_info("Invalid otp_NVR0_data[%d] = 0x%x\n", i, otp_NVR0_data[0]);
			valid_otp_ois = 0;
		}
	}

	if (otp_NVR1_data[0] != 0x02) {
		pr_info("Invalid otp_NVR1_data[0] = 0x%x\n", otp_NVR1_data[0]);
		valid_otp_ois = 0;
	}
	if (otp_NVR1_data[1] != 0x3F) {
		pr_info("Invalid otp_NVR1_data[1] = 0x%x\n", otp_NVR1_data[1]);
		valid_otp_ois = 0;
	}

	
	if (valid_otp_ois == 0) {
		pr_err("[OIS] %s : The otp NVR0/NVR1 data is invalid, skip the flash NVR0/NVR1 check !\n", __func__);
	} else {
		
		rc = lc898123_read_and_check_NVR0(otp_NVR0_data);
		if (rc != 0) {
			pr_err("[OIS] %s : flash NVR0 data error, reflash the NVR0 data !\n", __func__);
			rc = lc898123_reflash_NVR0(otp_NVR0_data);
		} else {
			pr_info("[OIS] %s : flash NVR0 data is OK, no need to reflash NVR0 data\n", __func__);
		}

		
		rc = lc898123_read_and_check_NVR1(otp_NVR1_data);
		if (rc != 0) {
			pr_err("[OIS] %s : flash NVR1 data error, reflash the NVR1 data !\n", __func__);
			rc = lc898123_reflash_NVR1(otp_NVR1_data);
		} else {
			pr_info("[OIS] %s : flash NVR1 data is OK, no need to reflash NVR1 data\n", __func__);
		}
	}


	
	RamWrite32A(CMD_IO_ADR_ACCESS, SYS_DSP_REMAP);
	RamRead32A(CMD_IO_DAT_ACCESS, &UlReadVal);
	pr_info("[OIS] %s : Remap = 0x%x\n", __func__, (unsigned int)UlReadVal);

	
	if (UlReadVal != 1) {
		pr_info("[OIS] %s : Remap data is NG, need to reflash FW\n", __func__);
		lc898123_reflash_firmware();
	} else {
		pr_info("[OIS] %s : Remap data is OK, no need to reflash FW\n", __func__);
	}

	OscStb();
	return rc;
}





#define FLASH_NVR0_DATA_SIZE 64
#define FLASH_NVR1_DATA_SIZE 162

int lc898123_read_and_check_NVR0(uint8_t *otp_NVR0_data)
{
	unsigned char flash_NVR0_data[FLASH_NVR0_DATA_SIZE] = {0};
	int i;
	int data_error = 0;

	pr_info("[OIS] %s : E\n", __func__);
	FlashNVR_ReadData_Byte(0, flash_NVR0_data, FLASH_NVR0_DATA_SIZE);

	for(i=0; i < FLASH_NVR0_DATA_SIZE; i++) {
		pr_info("[OIS] flash_NVR0_data[%d] = 0x%x\n",  i, flash_NVR0_data[i]);

		if(flash_NVR0_data[i] != *(otp_NVR0_data+i) ) {
			data_error = -1;
		}
	}

	pr_info("[OIS] %s : X\n", __func__);
	return data_error;
}

int lc898123_reflash_NVR0(uint8_t *otp_NVR0_data)
{
	pr_info("[OIS] %s : E\n", __func__);
	FlashNVRSectorErase_Byte(0);

	FlashNVR_WriteData_Byte(0, otp_NVR0_data, FLASH_NVR0_DATA_SIZE);
	pr_info("[OIS] %s : X\n", __func__);
	return 0;
}

int lc898123_read_and_check_NVR1(uint8_t *otp_NVR1_data)
{
	unsigned char flash_NVR1_data[FLASH_NVR1_DATA_SIZE] = {0};
	int i;
	int data_error = 0;

	pr_info("[OIS] %s : E\n", __func__);
	FlashNVR_ReadData_Byte(0x100, flash_NVR1_data, 130);
	FlashNVR_ReadData_Byte(0x1B0, &flash_NVR1_data[130], 32);

	for(i=0; i < FLASH_NVR1_DATA_SIZE; i++) {
		pr_info("[OIS] flash_NVR1_data[%d] = 0x%x\n",  i, flash_NVR1_data[i]);

		if(flash_NVR1_data[i] != *(otp_NVR1_data+i) ) {
			data_error = -1;
		}
	}

	pr_info("[OIS] %s : X\n", __func__);
	return data_error;
}

int lc898123_reflash_NVR1(uint8_t *otp_NVR1_data)
{
	pr_info("[OIS] %s : E\n", __func__);
	FlashNVRSectorErase_Byte(0x100);

	FlashNVR_WriteData_Byte(0x100, otp_NVR1_data, 130);
	FlashNVR_WriteData_Byte(0x1B0, otp_NVR1_data+130, 32);
	pr_info("[OIS] %s : X\n", __func__);
	return 0;
}


int lc898123_reflash_firmware(void)
{
	int rc = -1;
	int i;

	pr_info("[OIS] %s : E\n", __func__);
	for (i=0; i<5; i++) {
		rc = FlashUpdate();
		pr_info("[OIS] %s : FlashUpdate() is called, rc=%d , loop count=%d\n", __func__, rc, i);
		if (rc == 0) {
			pr_info("[OIS] %s : Update normally\n", __func__);
			msleep(100);
			rc = 0;
			break;
		} else if (rc == 1) {
			pr_info("[OIS] %s : Magic code erased\n", __func__);
			msleep(20);
			rc = -1;
		} else {
			pr_info("[OIS] %s : Verify error and retry\n", __func__);
			msleep(20);
			rc = -1;
		}
	}
	pr_info("[OIS] %s : X\n", __func__);

	return rc;
}


void RamWrite32A( unsigned short RamAddr, unsigned int RamData )
{
	int rc = 0;
	uint8_t data[4] = {0,0,0,0};
	struct msm_sensor_ctrl_t *s_ctrl = g_s_ctrl;

	data[0] = (RamData >> 24) & 0xFF;
	data[1] = (RamData >> 16) & 0xFF;
	data[2] = (RamData >> 8)  & 0xFF;
	data[3] = (RamData) & 0xFF;
	
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client, RamAddr, &data[0], 4);
	if (rc < 0)
		pr_err("[OIS] %s : write failed\n", __func__);
}

void RamRead32A( unsigned short RamAddr, unsigned int * ReadData )
{
	int rc = 0;
	uint8_t buf[4] = {0,0,0,0};
	struct msm_sensor_ctrl_t *s_ctrl = g_s_ctrl;

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(
		s_ctrl->sensor_i2c_client, RamAddr, &buf[0], 4);
	if (rc < 0)
		pr_err("[OIS] %s : read failed\n", __func__);
	else
		*ReadData = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}
 
unsigned short MakeNVRSel( unsigned short UsAddress )
{
        
        
        
        return 1 << ((UsAddress >> 8) & 0x03);
}
 
unsigned int MakeNVRDat( unsigned short UsAddress, unsigned char UcData )
{
        
        
        
        return (unsigned int)UcData << (((UsAddress >> 8) & 0x03) * 8);
}
 
unsigned char MakeDatNVR( unsigned short UsAddress, unsigned int UlData )
{
        return (unsigned char)((UlData >> (((UsAddress >> 8) & 0x03) * 8)) & 0xFF);
}
 
void WPBCtrl( unsigned char UcCtrl )
{
        if (UcCtrl == 0)
        {       
                ;
        } else {
                
                ;
        }
}








void FlashInitialSetting( char val )
{
	unsigned int UlReadVal = 0;
	int i;

	
	RamWrite32A( CMD_IO_ADR_ACCESS, CVER_123 ) ;
	RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;
	if( UlReadVal > 0xB4 ) {
		return ;
	}

	
	RamWrite32A( CMD_IO_ADR_ACCESS, SOFTRESET );
	RamRead32A ( CMD_IO_DAT_ACCESS, &UlReadVal );
	UlReadVal |= 0x00000010;									
	
	RamWrite32A( CMD_IO_DAT_ACCESS, UlReadVal );

	
	if( val ) {
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_TPGS ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 118 ) ;					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_TPROG ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 70 ) ;					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_TERASES ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 92 ) ;					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_TERASEC ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 115 ) ;					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 7 ) ;

		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 0x00000000	  ) ;		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;
		WPBCtrl(WPB_OFF) ;										

		for( i = 0; i < 8; i++ )
		{
			RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WDAT ) ;
			RamWrite32A( CMD_IO_DAT_ACCESS, 0xFFFFFFFF ) ; 

			RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
			RamWrite32A( CMD_IO_DAT_ACCESS, 3) ;  				
		}

		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010100	  ) ;		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 7 ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 7 ) ;  					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;
		WPBCtrl(WPB_ON) ;										
	}
}



void FlashReset(void)
{
	unsigned int UlReadVal = 0;

	
	RamWrite32A( CMD_IO_ADR_ACCESS, CVER_123 ) ;
	RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;
	if( UlReadVal > 0xB4 ) {
		return ;
	}

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;

	
	RamWrite32A( CMD_IO_ADR_ACCESS, SOFTRESET ) ;
	RamRead32A ( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
	UlReadVal &= ~0x00000010;									

	RamWrite32A( CMD_IO_DAT_ACCESS, UlReadVal ) ;
}


void FlashNVRSectorErase_Byte( unsigned short SetAddress )
{
	unsigned char UcCnt;
	unsigned int UlReadVal = 0;
	
	FlashInitialSetting(1);										

	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000	  ) ;
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, MakeNVRSel( SetAddress ) ) ;

	WPBCtrl(WPB_OFF) ;											
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;
	
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 4 ) ;						

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_INT ) ;	
	for ( UcCnt = 0; UcCnt < 100; UcCnt++ )						
	{
		RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;
		if( !(UlReadVal ==  0x80) ){
			break;
		}
	}
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS,0 ) ;
	WPBCtrl(WPB_ON) ;											

	FlashReset();												
}


void FlashNVR_ReadData_Byte( unsigned short SetAddress, unsigned char * ReadPtr, unsigned short Num )
{
	unsigned short UsNum;
	unsigned int UlReadVal = 0;

	FlashInitialSetting(1);										

	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;

	for ( UsNum = 0; UsNum < Num; UsNum++ )
	{
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 + ((SetAddress + UsNum) & 0x000000FF) ) ;
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, MakeNVRSel(SetAddress + UsNum) ) ;

		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_RDAT ) ;
		RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;
		
		ReadPtr[ UsNum ] = MakeDatNVR( (SetAddress + UsNum), UlReadVal );
	}
	FlashReset();												
}

void FlashNVR_WriteData_Byte( unsigned short SetAddress, unsigned char * WritePtr, unsigned short Num )
{
	unsigned short UsNum;

	FlashInitialSetting(1);										

	WPBCtrl(WPB_OFF) ;											
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;
	
	for ( UsNum = 0; UsNum < Num; UsNum++ )
	{
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 + ( (SetAddress + UsNum) & 0x000000FF) ) ;
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, MakeNVRSel(SetAddress + UsNum) ) ;

		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WDAT ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, MakeNVRDat((SetAddress + UsNum), WritePtr[ UsNum ]) ) ;

		
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_CMD ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , 2) ;					
	}
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;
	WPBCtrl(WPB_ON) ;											

	FlashReset();												

}


int FlashUpdate(void)
{
	unsigned int UlCnt;	
	unsigned int UlCalData[ 256 ] ;
	unsigned int UlNum;
	unsigned int UlReadVal = 0; 
	pr_info("[OIS] %s : E\n", __func__);
	FlashInitialSetting(1);										

	RamWrite32A( CMD_IO_ADR_ACCESS, SYS_DSP_REMAP) ;			
	RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;

	if( UlReadVal != 0) {	
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;

		WPBCtrl(WPB_OFF) ;										
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010001 ) ;			

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT	 ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;					

		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WDAT ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 2) ;  					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS,0 ) ;
		WPBCtrl(WPB_ON) ;										

		FlashReset();											

		RamWrite32A( 0xF003 , 0 ) ;								
		return ( 1 );

	} else {
		

		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 2 - 1 ) ;
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 + 32 ) ;		
		
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;  					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_RDAT ) ;		
		RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, OSCRSEL ) ;				
		RamWrite32A( CMD_IO_DAT_ACCESS, UlReadVal ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_RDAT ) ;		
		RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, OSCCURSEL ) ;			
		RamWrite32A( CMD_IO_DAT_ACCESS, UlReadVal ) ;

	}

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 7 ) ;						

	WPBCtrl(WPB_OFF) ;											
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 5 ) ;						

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_INT ) ;	
	for ( UlCnt = 0; UlCnt < 1000; UlCnt++ )					
	{
		RamRead32A( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
		if( !(UlReadVal ==  0x80) ){
			break;
		}
	}

	for ( UlCnt = 0; UlCnt < 4096; UlCnt += 128 )
	{
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, UlCnt ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;

		for( UlNum = 0; UlNum < 128; UlNum++ )
		{
			RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WDAT ) ;
			RamWrite32A( CMD_IO_DAT_ACCESS, ClFromCode[ UlNum + UlCnt ] ) ; 

			RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
			RamWrite32A( CMD_IO_DAT_ACCESS, 2) ;  				
		}
	}
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;
	WPBCtrl(WPB_ON) ;											

	for ( UlCnt = 0; UlCnt < 4096; UlCnt += 128 )
	{
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, UlCnt ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT	 ) ;	
		RamWrite32A( CMD_IO_DAT_ACCESS, 128 - 1 ) ;

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS, 1) ;  					

		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_RDAT ) ;		
		for ( UlNum = 0; UlNum < 128; UlNum++ )
		{
			RamRead32A( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
			
			
			if ( UlReadVal != ClFromCode[ UlNum + UlCnt ] ){
				FlashReset();								
				return( 2 );
			}
		}
	}
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_SEL ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;						

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 ) ;
		
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 80 - 1 ) ;

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 1) ;  						
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_RDAT ) ;
	for ( UlNum = 0; UlNum < 80; UlNum++ )						
	{
		RamRead32A( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
		
		if ( (UlNum <= 7) || ((UlNum >= 16 )&&(UlNum <= 31)) ){
				UlCalData[ UlNum ]  = (UlReadVal & 0xFFFFFF00) | CcMagicCode[ UlNum ] ;
		}else{
        
				UlCalData[ UlNum ]  = UlReadVal;			
		}				
	}


	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 ) ;

	WPBCtrl(WPB_OFF) ;											
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 1 ) ;
	
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 4 ) ;						

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_INT ) ;	
	for ( UlCnt = 0; UlCnt < 100; UlCnt++ )						
	{
		RamRead32A( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
		if( !(UlReadVal ==  0x80) ){
			break;
		}
	}

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 ) ;

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT	 ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;						

	for( UlNum = 0; UlNum < 80; UlNum++ )
	{
		RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WDAT ) ;		
		RamWrite32A( CMD_IO_DAT_ACCESS, UlCalData[ UlNum ] ) ;
		RamWrite32A( CMD_IO_ADR_ACCESS , FLASHROM_CMD ) ;
		RamWrite32A( CMD_IO_DAT_ACCESS , 2) ;  					
	}

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_WPB ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0 ) ;
	WPBCtrl(WPB_ON) ;											

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ADR ) ;			
	RamWrite32A( CMD_IO_DAT_ACCESS, 0x00010000 ) ;
		
	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_ACSCNT ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 80 - 1 ) ;					

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_CMD ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS, 1) ;						

	RamWrite32A( CMD_IO_ADR_ACCESS, FLASHROM_RDAT ) ;
	for ( UlNum = 0; UlNum < 80; UlNum++ )
	{
		RamRead32A( CMD_IO_DAT_ACCESS, &UlReadVal ) ;
		
		if( UlReadVal != UlCalData[ UlNum ] ){
			FlashReset();										
			return( 3 );
		}
	}
	RamWrite32A( 0xF000 , 0 ) ;									
	pr_info("[OIS] %s : X\n", __func__);
	return( 0 );
}







void	OscStb( void )
{
	RamWrite32A( CMD_IO_ADR_ACCESS , STBOSCPLL ) ;
	RamWrite32A( CMD_IO_DAT_ACCESS , OSC_STB ) ;
}

