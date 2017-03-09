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

int lc898123_check_ois_component(struct msm_sensor_ctrl_t *s_ctrl, int valid_otp_ois, uint8_t *otp_NVR0_data, uint8_t *otp_NVR1_data);


int lc898123_read_and_check_NVR0(uint8_t *otp_NVR0_data);
int lc898123_reflash_NVR0(uint8_t *otp_NVR0_data);
int lc898123_read_and_check_NVR1(uint8_t *otp_NVR1_data);
int lc898123_reflash_NVR1(uint8_t *otp_NVR1_data);
int lc898123_reflash_firmware(void);


void RamWrite32A( unsigned short RamAddr, unsigned int RamData );
void RamRead32A( unsigned short RamAddr, unsigned int * ReadData );
unsigned short MakeNVRSel( unsigned short UsAddress );
unsigned int MakeNVRDat( unsigned short UsAddress, unsigned char UcData );
unsigned char MakeDatNVR( unsigned short UsAddress, unsigned int UlData );
void WPBCtrl( unsigned char UcCtrl );


