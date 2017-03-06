/* Copyright (c) 2015, HTC Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _HTC_NET_MONITOR_H
#define _HTC_NET_MONITOR_H

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#define HTC_NM_DUMP_SEND			1 << 0
#define HTC_NM_DUMP_RECEIVE		1 << 1
#define HTC_NM_DUMP_ACCEPT		1 << 2
#define HTC_NM_DUMP_TCP_CONNECT	1 << 3
#define HTC_NM_DUMP_UDP_CONNECT	1 << 4
#define HTC_NM_DUMP_CLOSE			1 << 5
#define HTC_NM_DUMP_LOCAL			1 << 6
#define HTC_NM_DUMP_REMOTE		1 << 7
#define HTC_NM_DUMP_EXTEND		1 << 8
#define HTC_NM_DUMP_TASK_INFO	1 << 9

#define HTC_NM_TYPE_SEND			1 << 0
#define HTC_NM_TYPE_RECEIVE		1 << 1
#define HTC_NM_TYPE_ACCEPT		1 << 2
#define HTC_NM_TYPE_TCP_CONNECT	1 << 3
#define HTC_NM_TYPE_UDP_CONNECT	1 << 4
#define HTC_NM_TYPE_CLOSE			1 << 5

#define HTC_NM_TYPE_SEND_DESP				"SEND"
#define HTC_NM_TYPE_RECEIVE_DESP			"RECV"
#define HTC_NM_TYPE_ACCEPT_DESP			"ACCEPT"
#define HTC_NM_TYPE_TCP_CONNECT_DESP	"TCP CONNECT"
#define HTC_NM_TYPE_UDP_CONNECT_DESP	"UDP CONNECT"
#define HTC_NM_TYPE_CLOSE_DESP			"CLOSE"
#define HTC_NM_TYPE_OTHER_DESP			"OTHER"

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0012_HTC_NET_MONITOR
extern void net_monitor_dump_data(struct sock *sk, size_t size, unsigned long long type);
extern void net_monitor_enable_dump(int count);
#else
static inline void net_monitor_dump_data(struct sock *sk, size_t size, unsigned long long type)
{
	return;
}

static inline void net_monitor_enable_dump(int count)
{
	return;
}
#endif

#endif 

