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
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/tcp.h>

#include <htc_ril/htc_net_monitor.h>

#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

#define NIP6QUAD(addr) \
	ntohs((addr).s6_addr16[0]), \
	ntohs((addr).s6_addr16[1]), \
	ntohs((addr).s6_addr16[2]), \
	ntohs((addr).s6_addr16[3]), \
	ntohs((addr).s6_addr16[4]), \
	ntohs((addr).s6_addr16[5]), \
	ntohs((addr).s6_addr16[6]), \
	ntohs((addr).s6_addr16[7])

#define NIPQUAD_FMT "%u.%u.%u.%u"
#define NIP6QUAD_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"

#define DBG_MSG_LEN   512UL
#define DBG_MAX_MSG   256UL
#define TIME_BUF_LEN  20

static int htc_nm_debug_enable = 0;
static int htc_nm_debug_print = 0;
module_param_named(debug_enable, htc_nm_debug_enable,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(debug_print, htc_nm_debug_print,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

static struct {
	char     (buf[DBG_MAX_MSG])[DBG_MSG_LEN];   
	unsigned idx;   
	rwlock_t lck;   
} dbg_htc_nm = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

int net_monitor_dump_count = 0;
int net_monitor_dump_enable = 0;
unsigned long long net_monitor_dump_flag = HTC_NM_DUMP_RECEIVE | HTC_NM_DUMP_REMOTE;

char* net_monitor_type_to_string(unsigned long long type);
int net_monitor_can_dump_data(unsigned long long dump_flag, unsigned long long type);
void net_monitor_dump_data(struct sock *sk, size_t size, unsigned long long type);
void _net_monitor_record_data(struct sock *sk, size_t size, unsigned long long type);
void net_monitor_enable_dump(int count);
void _net_monitor_dump_data(struct sock *sk, size_t size,  unsigned long long type);
void _net_monitor_enable_dump(int count);
void net_monitor_dbg_log_event(const char * event, ...);

#define HTC_NM_INFO(x...) do {				\
		if (htc_nm_debug_enable) \
			net_monitor_dbg_log_event(x); \
	} while (0)

void net_monitor_dump_data(struct sock *sk, size_t size, unsigned long long type)
{
	_net_monitor_record_data(sk, size, type);
	_net_monitor_dump_data(sk, size, type);
}
EXPORT_SYMBOL(net_monitor_dump_data);

void net_monitor_enable_dump(int count)
{
		_net_monitor_enable_dump( count );
}
EXPORT_SYMBOL(net_monitor_enable_dump);

char* net_monitor_type_to_string(unsigned long long type)
{
	char* type_desp;
	switch ( type )
	{
		case HTC_NM_TYPE_SEND:
			type_desp = HTC_NM_TYPE_SEND_DESP;
			break;
		case HTC_NM_TYPE_RECEIVE:
			type_desp = HTC_NM_TYPE_RECEIVE_DESP;
			break;
		case HTC_NM_TYPE_ACCEPT:
			type_desp = HTC_NM_TYPE_ACCEPT_DESP;
			break;
		case HTC_NM_TYPE_TCP_CONNECT:
			type_desp = HTC_NM_TYPE_TCP_CONNECT_DESP;
			break;
		case HTC_NM_TYPE_UDP_CONNECT:
			type_desp = HTC_NM_TYPE_UDP_CONNECT_DESP;
			break;
		case HTC_NM_TYPE_CLOSE:
			type_desp = HTC_NM_TYPE_CLOSE_DESP;
			break;
		default:
			type_desp = HTC_NM_TYPE_OTHER_DESP;
			break;
	};
	return type_desp;
}

void _net_monitor_record_data(struct sock *sk, size_t size, unsigned long long type)
{
	struct inet_sock *inet = NULL;
	int ipv6_check = 0;

	if ( htc_nm_debug_enable == 0 ) {
		return;
	}

	if ( sk == NULL ) {
		pr_info("%s: sk == NULL\n", __func__);
		return;
	}

	inet = inet_sk(sk);

	if ( inet == NULL ) {
		pr_info("%s: inet == NULL\n", __func__);
		return;
	}

	if ( sk->sk_family == AF_INET6) {
		if ( inet->pinet6 != NULL ) {
			ipv6_check = 1;
		}
	}


	if ( ipv6_check == 0 ) {
		HTC_NM_INFO("UID:%d PID:%d %s " NIPQUAD_FMT ":%d " NIPQUAD_FMT ":%d\n",
			current->cred->uid, current->pid,
			net_monitor_type_to_string(type),
			NIPQUAD(inet->inet_rcv_saddr), ntohs(inet->inet_sport),
			NIPQUAD(inet->inet_daddr), ntohs(inet->inet_dport));
	} else {
		HTC_NM_INFO("UID:%d PID:%d %s " NIPQUAD_FMT " " NIP6QUAD_FMT ":%d " NIPQUAD_FMT " " NIP6QUAD_FMT ":%d\n",
			current->cred->uid, current->pid,
			net_monitor_type_to_string(type),
			NIPQUAD(inet->inet_rcv_saddr),
			NIP6QUAD(inet->pinet6->saddr),
			ntohs(inet->inet_sport),
			NIPQUAD(inet->inet_daddr),
			NIP6QUAD(inet->pinet6->daddr),
			ntohs(inet->inet_dport));
	}

}

int net_monitor_can_dump_data(unsigned long long dump_flag, unsigned long long type)
{
	int r = 0;
	
	switch ( type )
	{
		case HTC_NM_TYPE_SEND:
			if ( net_monitor_dump_flag & HTC_NM_DUMP_SEND ) {
				r = 1;
			}
			break;
		case HTC_NM_TYPE_RECEIVE:
			if ( net_monitor_dump_flag & HTC_NM_DUMP_RECEIVE ) {
				r = 1;
			}
			break;
		case HTC_NM_TYPE_ACCEPT:
			if ( net_monitor_dump_flag & HTC_NM_DUMP_ACCEPT ) {
				r = 1;
			}
			break;
		case HTC_NM_TYPE_TCP_CONNECT:
			if ( net_monitor_dump_flag & HTC_NM_DUMP_TCP_CONNECT ) {
				r = 1;
			}
			break;
		case HTC_NM_TYPE_UDP_CONNECT:
			if ( net_monitor_dump_flag & HTC_NM_DUMP_UDP_CONNECT ) {
				r = 1;
			}
			break;
		case HTC_NM_TYPE_CLOSE:
			if ( net_monitor_dump_flag & HTC_NM_DUMP_CLOSE ) {
				r = 1;
			}
			break;
		default:
			pr_info("%s: type = [%lld]\n", __func__, type);
			break;
	};
	return r;
}

void _net_monitor_dump_data(struct sock *sk, size_t size, unsigned long long type)
{
	struct task_struct *task = current;
	struct inet_sock *inet = NULL;
	
	int can_dump_data = 0;

	can_dump_data = net_monitor_can_dump_data( net_monitor_dump_flag, type);

	if ( can_dump_data == 0 ) {
		return;
	}

	if ( net_monitor_dump_enable == 0 ) {
		net_monitor_dump_count = 0;
		return;
	}

	net_monitor_dump_count--;

	if ( net_monitor_dump_count < 0 ) {
		net_monitor_dump_enable = 0;
		net_monitor_dump_count = 0;
		return;
	}

	if ( sk == NULL ) {
		pr_info("%s: sk == NULL\n", __func__);
		return;
	}

	inet = inet_sk(sk);


	if ( inet ) {
		if ( net_monitor_dump_flag & HTC_NM_DUMP_LOCAL) {
			pr_info("%s: Local: %03d.%03d.%03d.%03d:%05d(0x%x)\n", __func__, NIPQUAD(inet->inet_rcv_saddr), ntohs(inet->inet_sport), inet->inet_rcv_saddr);
		}
		if ( net_monitor_dump_flag & HTC_NM_DUMP_REMOTE) {
			pr_info("%s: Remote: %03d.%03d.%03d.%03d:%05d(0x%x)\n", __func__, NIPQUAD(inet->inet_daddr), ntohs(inet->inet_dport), inet->inet_daddr);
		}
		if ( sk->sk_family == AF_INET6) {
			if ( inet->pinet6 != NULL ) {
				if ( net_monitor_dump_flag & HTC_NM_DUMP_LOCAL) {
					
					pr_info("%s: Local: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x %d\n", __func__, NIP6QUAD(inet->pinet6->saddr), ntohs(inet->inet_sport));
				}
				if ( net_monitor_dump_flag & HTC_NM_DUMP_REMOTE) {
					
					pr_info("%s: Remote: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x %d\n", __func__, NIP6QUAD(inet->pinet6->daddr), ntohs(inet->inet_dport));
				}
			}
		}
	} else {
		pr_info("%s: inet = null\n", __func__);
	}

	if ( net_monitor_dump_flag & HTC_NM_DUMP_EXTEND ) {
		pr_info("%s: sk->sk_shutdown = [%d], sock_flag(sk, SOCK_DONE)=[%d]\n", __func__, sk->sk_shutdown, sock_flag(sk, SOCK_DONE));
		pr_info("%s: sk->sk_socket->state=[%d]\n", __func__, sk->sk_socket->state);
		pr_info("%s: sk->sk_type=[%d]\n", __func__, sk->sk_type);
		pr_info("%s: sk->sk_family=[%d]\n", __func__, sk->sk_family);
		pr_info("%s: sk->sk_state=[%d]\n", __func__, sk->sk_state);
		pr_info("%s: sk->sk_reuse=[%d]\n", __func__, sk->sk_reuse);
		pr_info("%s: sk->sk_reuseport=[%d]\n", __func__, sk->sk_reuseport);
		pr_info("%s: sk->sk_flags=[%lu]\n", __func__, sk->sk_flags);
		if (sock_flag(sk, SOCK_DEAD)) pr_info("%s: sk->sk_flags[SOCK_DEAD]\n", __func__);
		if (sock_flag(sk, SOCK_DONE)) pr_info("%s: sk->sk_flags[SOCK_DONE]\n", __func__);
		if (sock_flag(sk, SOCK_URGINLINE)) pr_info("%s: sk->sk_flags[SOCK_URGINLINE]\n", __func__);
		if (sock_flag(sk, SOCK_KEEPOPEN)) pr_info("%s: sk->sk_flags[SOCK_KEEPOPEN]\n", __func__);
		if (sock_flag(sk, SOCK_LINGER)) pr_info("%s: sk->sk_flags[SOCK_LINGER]\n", __func__);
		if (sock_flag(sk, SOCK_DESTROY)) pr_info("%s: sk->sk_flags[SOCK_DESTROY]\n", __func__);
		if (sock_flag(sk, SOCK_BROADCAST)) pr_info("%s: sk->sk_flags[SOCK_BROADCAST]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMP)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMP]\n", __func__);
		if (sock_flag(sk, SOCK_ZAPPED)) pr_info("%s: sk->sk_flags[SOCK_ZAPPED]\n", __func__);
		if (sock_flag(sk, SOCK_USE_WRITE_QUEUE)) pr_info("%s: sk->sk_flags[SOCK_USE_WRITE_QUEUE]\n", __func__);
		if (sock_flag(sk, SOCK_DBG)) pr_info("%s: sk->sk_flags[SOCK_DBG]\n", __func__);
		if (sock_flag(sk, SOCK_RCVTSTAMP)) pr_info("%s: sk->sk_flags[SOCK_RCVTSTAMP]\n", __func__);
		if (sock_flag(sk, SOCK_RCVTSTAMPNS)) pr_info("%s: sk->sk_flags[SOCK_RCVTSTAMPNS]\n", __func__);
		if (sock_flag(sk, SOCK_LOCALROUTE)) pr_info("%s: sk->sk_flags[SOCK_LOCALROUTE]\n", __func__);
		if (sock_flag(sk, SOCK_QUEUE_SHRUNK)) pr_info("%s: sk->sk_flags[SOCK_QUEUE_SHRUNK]\n", __func__);
		if (sock_flag(sk, SOCK_LOCALROUTE)) pr_info("%s: sk->sk_flags[SOCK_LOCALROUTE]\n", __func__);
		if (sock_flag(sk, SOCK_MEMALLOC)) pr_info("%s: sk->sk_flags[SOCK_MEMALLOC]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_TX_HARDWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_TX_HARDWARE]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_TX_SOFTWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_TX_SOFTWARE]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_RX_HARDWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_RX_HARDWARE]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_RX_SOFTWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_RX_SOFTWARE]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_SOFTWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_SOFTWARE]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_RAW_HARDWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_RAW_HARDWARE]\n", __func__);
		if (sock_flag(sk, SOCK_TIMESTAMPING_SYS_HARDWARE)) pr_info("%s: sk->sk_flags[SOCK_TIMESTAMPING_SYS_HARDWARE]\n", __func__);
		if (sock_flag(sk, SOCK_FASYNC)) pr_info("%s: sk->sk_flags[SOCK_FASYNC]\n", __func__);
		if (sock_flag(sk, SOCK_RXQ_OVFL)) pr_info("%s: sk->sk_flags[SOCK_RXQ_OVFL]\n", __func__);
		if (sock_flag(sk, SOCK_ZEROCOPY)) pr_info("%s: sk->sk_flags[SOCK_ZEROCOPY]\n", __func__);
		if (sock_flag(sk, SOCK_WIFI_STATUS)) pr_info("%s: sk->sk_flags[SOCK_WIFI_STATUS]\n", __func__);
		if (sock_flag(sk, SOCK_NOFCS)) pr_info("%s: sk->sk_flags[SOCK_NOFCS]\n", __func__);
		if (sock_flag(sk, SOCK_FILTER_LOCKED)) pr_info("%s: sk->sk_flags[SOCK_FILTER_LOCKED]\n", __func__);
		if (sock_flag(sk, SOCK_SELECT_ERR_QUEUE)) pr_info("%s: sk->sk_flags[SOCK_SELECT_ERR_QUEUE]\n", __func__);
	}

	if ( net_monitor_dump_flag & HTC_NM_DUMP_TASK_INFO ) {
		pr_info("%s: task->state=[%d][%d]\n", __func__, task->flags, task->flags & PF_EXITING);
		pr_info("%s: === Show stack start===\n", __func__);
		show_stack(0, 0);
		pr_info("%s: === Show stack end===\n", __func__);
	}
}

void _net_monitor_enable_dump(int count)
{
	net_monitor_dump_enable = 1;
	if ( count <= 0 ) {
		net_monitor_dump_count = 1;
	} else if ( count > 10 ) {
		net_monitor_dump_count = 10;
	} else {
		net_monitor_dump_count = count;
	}
}

static void net_monitor_dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) & (DBG_MAX_MSG-1);
}

static char *net_monitor_get_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000)/1000;
	scnprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu] ", (unsigned long)t,
		nanosec_rem);
	return tbuf;
}

void net_monitor_dbg_log_event(const char * event, ...)
{
	unsigned long flags;
	char tbuf[TIME_BUF_LEN];
	char dbg_buff[DBG_MSG_LEN];
	va_list arg_list;
	int data_size;

	if ( !htc_nm_debug_enable ) {
		return;
	}

	va_start(arg_list, event);
	data_size = vsnprintf(dbg_buff,
			      DBG_MSG_LEN, event, arg_list);
	va_end(arg_list);

	write_lock_irqsave(&dbg_htc_nm.lck, flags);

	scnprintf(dbg_htc_nm.buf[dbg_htc_nm.idx], DBG_MSG_LEN,
		"%s %s", net_monitor_get_timestamp(tbuf), dbg_buff);

	net_monitor_dbg_inc(&dbg_htc_nm.idx);

	if ( htc_nm_debug_print )
		pr_info("%s", dbg_buff);
	write_unlock_irqrestore(&dbg_htc_nm.lck, flags);

	return;

}
EXPORT_SYMBOL(net_monitor_dbg_log_event);

static int net_monitor_events_show(struct seq_file *s, void *unused)
{
	unsigned long	flags;
	unsigned	i;

	read_lock_irqsave(&dbg_htc_nm.lck, flags);

	i = dbg_htc_nm.idx;
	for (net_monitor_dbg_inc(&i); i != dbg_htc_nm.idx; net_monitor_dbg_inc(&i)) {
		if (!strnlen(dbg_htc_nm.buf[i], DBG_MSG_LEN))
			continue;
		seq_printf(s, "%s", dbg_htc_nm.buf[i]);
	}

	read_unlock_irqrestore(&dbg_htc_nm.lck, flags);

	return 0;
}

static int net_monitor_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, net_monitor_events_show, inode->i_private);
}

const struct file_operations net_monitor_dbg_fops = {
	.open = net_monitor_events_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init net_monitor_debugfs_init(void)
{
	int r = 0;
	struct dentry *dent_dir;
	struct dentry *dent_file;

	do {
		dent_dir = debugfs_create_dir("htc_nm", 0);
		if (IS_ERR(dent_dir)) {
			r = PTR_ERR(dent_dir);
			break;
		}

		dent_file = debugfs_create_file("dumplog", S_IRUGO, dent_dir, NULL, &net_monitor_dbg_fops);
		r = PTR_ERR(dent_file);

	} while ( 0 );

	return r;
}

static int __init htc_net_monitor_init(void)
{
#ifdef CONFIG_DEBUG_FS
	net_monitor_debugfs_init();
#endif
	return 0;
}

static void __exit htc_net_monitor_exit(void)
{
	return;
}


module_init(htc_net_monitor_init);
module_exit(htc_net_monitor_exit);

MODULE_AUTHOR("Mars Lin");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("htc net monitor driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("htc_net_monitor");
