/*
 * Copyright (C) 2007 HTC Incorporated
 * Author: Jay Tu (jay_tu@htc.com)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _HTC_BATTERY_COMMON_H_
#define _HTC_BATTERY_COMMON_H_



enum charger_type_t {
	CHARGER_UNKNOWN = -1,
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC,
	CHARGER_9V_AC,
	CHARGER_WIRELESS,
	CHARGER_MHL_AC,
	CHARGER_DETECTING,
	CHARGER_UNKNOWN_USB,
	CHARGER_NOTIFY,
	CHARGER_MHL_UNKNOWN,
	CHARGER_MHL_100MA,
	CHARGER_MHL_500MA,
	CHARGER_MHL_900MA,
	CHARGER_MHL_1500MA,
	CHARGER_MHL_2000MA,
};

enum power_supplies_type {
	BATTERY_SUPPLY,
#if 0
	USB_SUPPLY,
#endif
	AC_SUPPLY,
	WIRELESS_SUPPLY
};

enum charger_control_flag {
	STOP_CHARGER = 0,
	ENABLE_CHARGER,
	ENABLE_LIMIT_CHARGER,
	DISABLE_LIMIT_CHARGER,
	DISABLE_PWRSRC,
	ENABLE_PWRSRC,
	DISABLE_PWRSRC_FINGERPRINT,
	ENABLE_PWRSRC_FINGERPRINT,
	END_CHARGER
};

enum ftm_charger_control_flag {
	FTM_ENABLE_CHARGER = 0,
	FTM_STOP_CHARGER,
	FTM_FAST_CHARGE,
	FTM_SLOW_CHARGE,
	FTM_END_CHARGER
};

#define HTC_BATT_CHG_LIMIT_BIT_TALK				(1)
#define HTC_BATT_CHG_LIMIT_BIT_NAVI				(1<<1)
#define HTC_BATT_CHG_LIMIT_BIT_THRML				(1<<2)
#define HTC_BATT_CHG_LIMIT_BIT_KDDI				(1<<3)
#define HTC_BATT_CHG_LIMIT_BIT_NET_TALK			(1<<4)
#define HTC_BATT_CHG_LIMIT_BIT_THRML_IUSB			(1<<5)

enum batt_context_event {
	EVENT_TALK_START = 0,
	EVENT_TALK_STOP,
	EVENT_NETWORK_SEARCH_START,
	EVENT_NETWORK_SEARCH_STOP,
	EVENT_NAVIGATION_START,
	EVENT_NAVIGATION_STOP,
	EVENT_MUSIC_START,
	EVENT_MUSIC_STOP,
	EVENT_NET_TALK_START,
	EVENT_NET_TALK_STOP
};


int htc_battery_charger_disable(void);
int htc_battery_pwrsrc_disable(void);
int htc_battery_charger_switch_internal(int enable);
int htc_battery_get_zcharge_mode(void);
int htc_battery_set_max_input_current(int target_ma);
#endif
