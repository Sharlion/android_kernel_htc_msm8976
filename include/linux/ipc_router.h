/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef _IPC_ROUTER_H
#define _IPC_ROUTER_H

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/pm.h>
#include <linux/msm_ipc.h>
#include <linux/device.h>
#include <linux/kref.h>

#define MAX_WS_NAME_SZ 32

#define IPC_RTR_ERR(buf, ...) \
	pr_err("IPC_RTR: " buf, __VA_ARGS__)

enum msm_ipc_router_event {
	IPC_ROUTER_CTRL_CMD_DATA = 1,
	IPC_ROUTER_CTRL_CMD_HELLO,
	IPC_ROUTER_CTRL_CMD_BYE,
	IPC_ROUTER_CTRL_CMD_NEW_SERVER,
	IPC_ROUTER_CTRL_CMD_REMOVE_SERVER,
	IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT,
	IPC_ROUTER_CTRL_CMD_RESUME_TX,
};

union rr_control_msg {
	uint32_t cmd;
	struct {
		uint32_t cmd;
		uint32_t magic;
		uint32_t capability;
	} hello;
	struct {
		uint32_t cmd;
		uint32_t service;
		uint32_t instance;
		uint32_t node_id;
		uint32_t port_id;
	} srv;
	struct {
		uint32_t cmd;
		uint32_t node_id;
		uint32_t port_id;
	} cli;
};

struct comm_mode_info {
	int mode;
	void *xprt_info;
};

struct msm_ipc_port {
#ifdef CONFIG_HTC_DEBUG_RIL_PCN0010_HTC_DUMP_IPCROUTER_LOG
	pid_t pid;
	pid_t tgid;
#endif
#ifdef CONFIG_HTC_DEBUG_RIL_PCN0011_HTC_DUMP_IPC_UNREAD_PACKAGE
	struct delayed_work rx_data_check_wq;
	int rx_data_check_wq_delay_time;
#endif
	struct list_head list;
	struct kref ref;

	struct msm_ipc_port_addr this_port;
	struct msm_ipc_port_name port_name;
	uint32_t type;
	unsigned flags;
	struct mutex port_lock_lhc3;
	struct comm_mode_info mode_info;

	struct msm_ipc_port_addr dest_addr;
	int conn_status;

	struct list_head port_rx_q;
	struct mutex port_rx_q_lock_lhc3;
	char rx_ws_name[MAX_WS_NAME_SZ];
	struct wakeup_source *port_rx_ws;
	wait_queue_head_t port_rx_wait_q;
	wait_queue_head_t port_tx_wait_q;

	int restart_state;
	spinlock_t restart_lock;
	wait_queue_head_t restart_wait;

	void *rport_info;
	void *endpoint;
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv);
	int (*check_send_permissions)(void *data);

	uint32_t num_tx;
	uint32_t num_rx;
	unsigned long num_tx_bytes;
	unsigned long num_rx_bytes;
	void *priv;
};

#ifdef CONFIG_IPC_ROUTER
struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv),
	void *priv);

int msm_ipc_router_bind_control_port(struct msm_ipc_port *port_ptr);

int msm_ipc_router_lookup_server_name(struct msm_ipc_port_name *srv_name,
				      struct msm_ipc_server_info *srv_info,
				      int num_entries_in_array,
				      uint32_t lookup_mask);

int msm_ipc_router_send_msg(struct msm_ipc_port *src,
			    struct msm_ipc_addr *dest,
			    void *data, unsigned int data_len);

int msm_ipc_router_get_curr_pkt_size(struct msm_ipc_port *port_ptr);

int msm_ipc_router_read_msg(struct msm_ipc_port *port_ptr,
			    struct msm_ipc_addr *src,
			    unsigned char **data,
			    unsigned int *len);

int msm_ipc_router_close_port(struct msm_ipc_port *port_ptr);

int msm_ipc_router_register_server(struct msm_ipc_port *server_port,
				   struct msm_ipc_addr *name);

int msm_ipc_router_unregister_server(struct msm_ipc_port *server_port);

#else

struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv),
	void *priv)
{
	return NULL;
}

static inline int msm_ipc_router_bind_control_port(
		struct msm_ipc_port *port_ptr)
{
	return -ENODEV;
}

int msm_ipc_router_lookup_server_name(struct msm_ipc_port_name *srv_name,
				      struct msm_ipc_server_info *srv_info,
				      int num_entries_in_array,
				      uint32_t lookup_mask)
{
	return -ENODEV;
}

int msm_ipc_router_send_msg(struct msm_ipc_port *src,
			    struct msm_ipc_addr *dest,
			    void *data, unsigned int data_len)
{
	return -ENODEV;
}

int msm_ipc_router_get_curr_pkt_size(struct msm_ipc_port *port_ptr)
{
	return -ENODEV;
}

int msm_ipc_router_read_msg(struct msm_ipc_port *port_ptr,
			    struct msm_ipc_addr *src,
			    unsigned char **data,
			    unsigned int *len)
{
	return -ENODEV;
}

int msm_ipc_router_close_port(struct msm_ipc_port *port_ptr)
{
	return -ENODEV;
}

static inline int msm_ipc_router_register_server(
			struct msm_ipc_port *server_port,
			struct msm_ipc_addr *name)
{
	return -ENODEV;
}

static inline int msm_ipc_router_unregister_server(
			struct msm_ipc_port *server_port)
{
	return -ENODEV;
}

#endif

#endif
