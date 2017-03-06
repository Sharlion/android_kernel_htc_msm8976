/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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

#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/ipc_router_xprt.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include <soc/qcom/smd.h>
#include <soc/qcom/smsm.h>
#include <soc/qcom/subsystem_restart.h>

static int msm_ipc_router_smd_xprt_debug_mask;
module_param_named(debug_mask, msm_ipc_router_smd_xprt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG
#include <linux/fs.h>
#include <linux/debugfs.h>
#define DBG_SMD_XPRT_MAX_MSG   512UL
#define DBG_SMD_XPRT_MSG_LEN   100UL

#define TIME_BUF_LEN  20

void smd_xprt_dbg_log_event(const char * event, ...);

static int smd_xprt_htc_debug_enable = 0;
static int smd_xprt_htc_debug_dump = 1;
static int smd_xprt_htc_debug_dump_lines = DBG_SMD_XPRT_MAX_MSG;
static int smd_xprt_htc_debug_print = 0;
module_param_named(smd_xprt_htc_debug_enable, smd_xprt_htc_debug_enable,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(smd_xprt_htc_debug_dump, smd_xprt_htc_debug_dump,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(smd_xprt_htc_debug_dump_lines, smd_xprt_htc_debug_dump_lines,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(smd_xprt_htc_debug_print, smd_xprt_htc_debug_print,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

static struct {
	char     (buf[DBG_SMD_XPRT_MAX_MSG])[DBG_SMD_XPRT_MSG_LEN];   
	unsigned idx;   
	rwlock_t lck;   
} dbg_smd_xprt = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};
#endif

#if defined(DEBUG)
#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG
#define D(x...) do { \
	if (msm_ipc_router_smd_xprt_debug_mask) \
		pr_info(x); \
	if (smd_xprt_htc_debug_enable) \
		smd_xprt_dbg_log_event(x); \
} while (0)
#else
#define D(x...) do { \
if (msm_ipc_router_smd_xprt_debug_mask) \
	pr_info(x); \
} while (0)
#endif
#else
#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG
#define D(x...) do { \
	if (smd_htc_debug_enable) \
		smd_xprt_dbg_log_event(x); \
} while (0)
#else
#define D(x...) do { } while (0)
#endif
#endif

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG

static void smd_xprt_dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) & (DBG_SMD_XPRT_MAX_MSG-1);
}

static char *smd_xprt_get_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000)/1000;
	scnprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu] ", (unsigned long)t,
		nanosec_rem);
	return tbuf;
}

void smd_xprt_events_print(void)
{
	unsigned long	flags;
	unsigned	i;
	unsigned lines = 0;

	pr_info("### Show SMD XPRT Log Start ###\n");

	read_lock_irqsave(&dbg_smd_xprt.lck, flags);

	i = dbg_smd_xprt.idx;

	for (smd_xprt_dbg_inc(&i); i != dbg_smd_xprt.idx; smd_xprt_dbg_inc(&i)) {
		if (!strnlen(dbg_smd_xprt.buf[i], DBG_SMD_XPRT_MSG_LEN))
			continue;
		pr_info("%s", dbg_smd_xprt.buf[i]);
		lines++;
		if ( lines > smd_xprt_htc_debug_dump_lines )
			break;
	}

	read_unlock_irqrestore(&dbg_smd_xprt.lck, flags);

	pr_info("### Show SMD XPRT Log End ###\n");
}

void msm_smd_xprt_dumplog(void)
{

	if ( !smd_xprt_htc_debug_enable ) {
		pr_info("%s: smd_xprt_htc_debug_enable=[%d]\n", __func__, smd_xprt_htc_debug_enable);
		return;
	}

	if ( !smd_xprt_htc_debug_dump ) {
		pr_info("%s: smd_xprt_htc_debug_dump=[%d]\n", __func__, smd_xprt_htc_debug_dump);
		return;
	}

	smd_xprt_events_print();
	return;
}
EXPORT_SYMBOL(msm_smd_xprt_dumplog);

void smd_xprt_dbg_log_event(const char * event, ...)
{
	unsigned long flags;
	char tbuf[TIME_BUF_LEN];
	char dbg_buff[DBG_SMD_XPRT_MSG_LEN];
	va_list arg_list;
	int data_size;

	if ( !smd_xprt_htc_debug_enable ) {
		return;
	}

	va_start(arg_list, event);
	data_size = vsnprintf(dbg_buff,
			      DBG_SMD_XPRT_MSG_LEN, event, arg_list);
	va_end(arg_list);

	write_lock_irqsave(&dbg_smd_xprt.lck, flags);

	scnprintf(dbg_smd_xprt.buf[dbg_smd_xprt.idx], DBG_SMD_XPRT_MSG_LEN,
		"%s %s", smd_xprt_get_timestamp(tbuf), dbg_buff);

	smd_xprt_dbg_inc(&dbg_smd_xprt.idx);

	if ( smd_xprt_htc_debug_print )
		pr_info("%s", dbg_buff);
	write_unlock_irqrestore(&dbg_smd_xprt.lck, flags);

	return;

}
EXPORT_SYMBOL(smd_xprt_dbg_log_event);

static int smd_xprt_events_show(struct seq_file *s, void *unused)
{
	unsigned long	flags;
	unsigned	i;

	read_lock_irqsave(&dbg_smd_xprt.lck, flags);

	i = dbg_smd_xprt.idx;
	for (smd_xprt_dbg_inc(&i); i != dbg_smd_xprt.idx; smd_xprt_dbg_inc(&i)) {
		if (!strnlen(dbg_smd_xprt.buf[i], DBG_SMD_XPRT_MSG_LEN))
			continue;
		seq_printf(s, "%s", dbg_smd_xprt.buf[i]);
	}

	read_unlock_irqrestore(&dbg_smd_xprt.lck, flags);

	return 0;
}

static int smd_xprt_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, smd_xprt_events_show, inode->i_private);
}

const struct file_operations smd_xprt_dbg_fops = {
	.open = smd_xprt_events_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#define MIN_FRAG_SZ (IPC_ROUTER_HDR_SIZE + sizeof(union rr_control_msg))

#define NUM_SMD_XPRTS 4
#define XPRT_NAME_LEN (SMD_MAX_CH_NAME_LEN + 12)

struct msm_ipc_router_smd_xprt {
	struct list_head list;
	char ch_name[SMD_MAX_CH_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	uint32_t edge;
	struct platform_driver driver;
	struct msm_ipc_router_xprt xprt;
	smd_channel_t *channel;
	struct workqueue_struct *smd_xprt_wq;
	wait_queue_head_t write_avail_wait_q;
	struct rr_packet *in_pkt;
	int is_partial_in_pkt;
	struct delayed_work read_work;
	spinlock_t ss_reset_lock;	
	int ss_reset;
	void *pil;
	struct completion sft_close_complete;
	unsigned xprt_version;
	unsigned xprt_option;
	bool disable_pil_loading;
};

struct msm_ipc_router_smd_xprt_work {
	struct msm_ipc_router_xprt *xprt;
	struct work_struct work;
};

static void smd_xprt_read_data(struct work_struct *work);
static void smd_xprt_open_event(struct work_struct *work);
static void smd_xprt_close_event(struct work_struct *work);

struct msm_ipc_router_smd_xprt_config {
	char ch_name[SMD_MAX_CH_NAME_LEN];
	char xprt_name[XPRT_NAME_LEN];
	uint32_t edge;
	uint32_t link_id;
	unsigned xprt_version;
	unsigned xprt_option;
	bool disable_pil_loading;
};

struct msm_ipc_router_smd_xprt_config smd_xprt_cfg[] = {
	{"RPCRPY_CNTL", "ipc_rtr_smd_rpcrpy_cntl", SMD_APPS_MODEM, 1, 1},
	{"IPCRTR", "ipc_rtr_smd_ipcrtr", SMD_APPS_MODEM, 1, 1},
	{"IPCRTR", "ipc_rtr_q6_ipcrtr", SMD_APPS_QDSP, 1, 1},
	{"IPCRTR", "ipc_rtr_wcnss_ipcrtr", SMD_APPS_WCNSS, 1, 1},
};

#define MODULE_NAME "ipc_router_smd_xprt"
#define IPC_ROUTER_SMD_XPRT_WAIT_TIMEOUT 3000
static int ipc_router_smd_xprt_probe_done;
static struct delayed_work ipc_router_smd_xprt_probe_work;
static DEFINE_MUTEX(smd_remote_xprt_list_lock_lha1);
static LIST_HEAD(smd_remote_xprt_list);

static void pil_vote_load_worker(struct work_struct *work);
static void pil_vote_unload_worker(struct work_struct *work);
static struct workqueue_struct *pil_vote_wq;

static bool is_pil_loading_disabled(uint32_t edge);

static int msm_ipc_router_smd_get_xprt_version(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;
	if (!xprt)
		return -EINVAL;
	smd_xprtp = container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	return (int)smd_xprtp->xprt_version;
}

static int msm_ipc_router_smd_get_xprt_option(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;
	if (!xprt)
		return -EINVAL;
	smd_xprtp = container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	return (int)smd_xprtp->xprt_option;
}

static int msm_ipc_router_smd_remote_write_avail(
	struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	return smd_write_avail(smd_xprtp->channel);
}

static int msm_ipc_router_smd_remote_write(void *data,
					   uint32_t len,
					   struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *ipc_rtr_pkt;
	int offset, sz_written = 0;
	int ret, num_retries = 0;
	unsigned long flags;
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	if (!pkt)
		return -EINVAL;

	if (!len || pkt->length != len)
		return -EINVAL;

	do {
		spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
		if (smd_xprtp->ss_reset) {
			spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock,
						flags);
			IPC_RTR_ERR("%s: %s chnl reset\n",
					__func__, xprt->name);
			return -ENETRESET;
		}
		spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
		ret = smd_write_start(smd_xprtp->channel, len);
		if (ret < 0 && num_retries >= 5) {
			IPC_RTR_ERR("%s: Error %d @smd_write_start for %s\n",
				__func__, ret, xprt->name);
			return ret;
		} else if (ret < 0) {
			msleep(50);
			num_retries++;
		}
	} while (ret < 0);

	D("%s: Ready to write %d bytes\n", __func__, len);
	skb_queue_walk(pkt->pkt_fragment_q, ipc_rtr_pkt) {
		offset = 0;
		while (offset < ipc_rtr_pkt->len) {
			if (!smd_write_segment_avail(smd_xprtp->channel))
				smd_enable_read_intr(smd_xprtp->channel);

			wait_event(smd_xprtp->write_avail_wait_q,
				(smd_write_segment_avail(smd_xprtp->channel) ||
				smd_xprtp->ss_reset));
			smd_disable_read_intr(smd_xprtp->channel);
			spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
			if (smd_xprtp->ss_reset) {
				spin_unlock_irqrestore(
					&smd_xprtp->ss_reset_lock, flags);
				IPC_RTR_ERR("%s: %s chnl reset\n",
					__func__, xprt->name);
				return -ENETRESET;
			}
			spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock,
						flags);

			sz_written = smd_write_segment(smd_xprtp->channel,
					ipc_rtr_pkt->data + offset,
					(ipc_rtr_pkt->len - offset));
			offset += sz_written;
			sz_written = 0;
		}
		D("%s: Wrote %d bytes over %s\n",
		  __func__, offset, xprt->name);
	}

	if (!smd_write_end(smd_xprtp->channel))
		D("%s: Finished writing\n", __func__);
	return len;
}

static int msm_ipc_router_smd_remote_close(struct msm_ipc_router_xprt *xprt)
{
	int rc;
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	rc = smd_close(smd_xprtp->channel);
	if (smd_xprtp->pil) {
		subsystem_put(smd_xprtp->pil);
		smd_xprtp->pil = NULL;
	}
	return rc;
}

static void smd_xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt, struct msm_ipc_router_smd_xprt, xprt);

	complete_all(&smd_xprtp->sft_close_complete);
}

static void smd_xprt_read_data(struct work_struct *work)
{
	int pkt_size, sz_read, sz;
	struct sk_buff *ipc_rtr_pkt;
	void *data;
	unsigned long flags;
	struct delayed_work *rwork = to_delayed_work(work);
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(rwork, struct msm_ipc_router_smd_xprt, read_work);

	spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
	if (smd_xprtp->ss_reset) {
		spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
		if (smd_xprtp->in_pkt)
			release_pkt(smd_xprtp->in_pkt);
		smd_xprtp->is_partial_in_pkt = 0;
		IPC_RTR_ERR("%s: %s channel reset\n",
			__func__, smd_xprtp->xprt.name);
		return;
	}
	spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);

	D("%s pkt_size: %d, read_avail: %d\n", __func__,
		smd_cur_packet_size(smd_xprtp->channel),
		smd_read_avail(smd_xprtp->channel));
	while ((pkt_size = smd_cur_packet_size(smd_xprtp->channel)) &&
		smd_read_avail(smd_xprtp->channel)) {
		if (!smd_xprtp->is_partial_in_pkt) {
			smd_xprtp->in_pkt = create_pkt(NULL);
			if (!smd_xprtp->in_pkt) {
				IPC_RTR_ERR("%s: Couldn't alloc rr_packet\n",
					__func__);
				return;
			}
			smd_xprtp->is_partial_in_pkt = 1;
			D("%s: Allocated rr_packet\n", __func__);
		}

		if (((pkt_size >= MIN_FRAG_SZ) &&
		     (smd_read_avail(smd_xprtp->channel) < MIN_FRAG_SZ)) ||
		    ((pkt_size < MIN_FRAG_SZ) &&
		     (smd_read_avail(smd_xprtp->channel) < pkt_size)))
			return;

		sz = smd_read_avail(smd_xprtp->channel);
		do {
			ipc_rtr_pkt = alloc_skb(sz, GFP_KERNEL);
			if (!ipc_rtr_pkt) {
				if (sz <= (PAGE_SIZE/2)) {
					queue_delayed_work(
						smd_xprtp->smd_xprt_wq,
						&smd_xprtp->read_work,
						msecs_to_jiffies(100));
					return;
				}
				sz = sz / 2;
			}
		} while (!ipc_rtr_pkt);

		D("%s: Allocated the sk_buff of size %d\n", __func__, sz);
		data = skb_put(ipc_rtr_pkt, sz);
		sz_read = smd_read(smd_xprtp->channel, data, sz);
		if (sz_read != sz) {
			IPC_RTR_ERR("%s: Couldn't read %s completely\n",
				__func__, smd_xprtp->xprt.name);
			kfree_skb(ipc_rtr_pkt);
			release_pkt(smd_xprtp->in_pkt);
			smd_xprtp->is_partial_in_pkt = 0;
			return;
		}
		skb_queue_tail(smd_xprtp->in_pkt->pkt_fragment_q, ipc_rtr_pkt);
		smd_xprtp->in_pkt->length += sz_read;
		if (sz_read != pkt_size)
			smd_xprtp->is_partial_in_pkt = 1;
		else
			smd_xprtp->is_partial_in_pkt = 0;

		if (!smd_xprtp->is_partial_in_pkt) {
			D("%s: Packet size read %d\n",
			  __func__, smd_xprtp->in_pkt->length);
			msm_ipc_router_xprt_notify(&smd_xprtp->xprt,
						IPC_ROUTER_XPRT_EVENT_DATA,
						(void *)smd_xprtp->in_pkt);
			release_pkt(smd_xprtp->in_pkt);
			smd_xprtp->in_pkt = NULL;
		}
	}
}

static void smd_xprt_open_event(struct work_struct *work)
{
	struct msm_ipc_router_smd_xprt_work *xprt_work =
		container_of(work, struct msm_ipc_router_smd_xprt_work, work);
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt_work->xprt,
			     struct msm_ipc_router_smd_xprt, xprt);
	unsigned long flags;

	spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
	smd_xprtp->ss_reset = 0;
	spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
	msm_ipc_router_xprt_notify(xprt_work->xprt,
				IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
	   __func__, xprt_work->xprt->name);
	kfree(xprt_work);
}

static void smd_xprt_close_event(struct work_struct *work)
{
	struct msm_ipc_router_smd_xprt_work *xprt_work =
		container_of(work, struct msm_ipc_router_smd_xprt_work, work);
	struct msm_ipc_router_smd_xprt *smd_xprtp =
		container_of(xprt_work->xprt,
			     struct msm_ipc_router_smd_xprt, xprt);

	if (smd_xprtp->in_pkt) {
		release_pkt(smd_xprtp->in_pkt);
		smd_xprtp->in_pkt = NULL;
	}
	smd_xprtp->is_partial_in_pkt = 0;
	init_completion(&smd_xprtp->sft_close_complete);
	msm_ipc_router_xprt_notify(xprt_work->xprt,
				IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
	   __func__, xprt_work->xprt->name);
	wait_for_completion(&smd_xprtp->sft_close_complete);
	kfree(xprt_work);
}

static void msm_ipc_router_smd_remote_notify(void *_dev, unsigned event)
{
	unsigned long flags;
	struct msm_ipc_router_smd_xprt *smd_xprtp;
	struct msm_ipc_router_smd_xprt_work *xprt_work;

	smd_xprtp = (struct msm_ipc_router_smd_xprt *)_dev;
	if (!smd_xprtp)
		return;

	switch (event) {
	case SMD_EVENT_DATA:
		if (smd_read_avail(smd_xprtp->channel))
			queue_delayed_work(smd_xprtp->smd_xprt_wq,
					   &smd_xprtp->read_work, 0);
		if (smd_write_segment_avail(smd_xprtp->channel))
			wake_up(&smd_xprtp->write_avail_wait_q);
		break;

	case SMD_EVENT_OPEN:
		D("%s: get smd remote notify SMD_EVENT_OPEN\n", __func__);
		pr_info("%s: get smd remote notify SMD_EVENT_OPEN\n", __func__);
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_smd_xprt_work),
				    GFP_ATOMIC);
		if (!xprt_work) {
			IPC_RTR_ERR(
			"%s: Couldn't notify %d event to IPC Router\n",
				__func__, event);
			return;
		}
		xprt_work->xprt = &smd_xprtp->xprt;
		INIT_WORK(&xprt_work->work, smd_xprt_open_event);
		queue_work(smd_xprtp->smd_xprt_wq, &xprt_work->work);
		break;

	case SMD_EVENT_CLOSE:
		D("%s: get smd remote notify SMD_EVENT_CLOSE\n", __func__);
		pr_info("%s: get smd remote notify SMD_EVENT_CLOSE\n", __func__);
		spin_lock_irqsave(&smd_xprtp->ss_reset_lock, flags);
		smd_xprtp->ss_reset = 1;
		spin_unlock_irqrestore(&smd_xprtp->ss_reset_lock, flags);
		wake_up(&smd_xprtp->write_avail_wait_q);
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_smd_xprt_work),
				    GFP_ATOMIC);
		if (!xprt_work) {
			IPC_RTR_ERR(
			"%s: Couldn't notify %d event to IPC Router\n",
				__func__, event);
			return;
		}
		xprt_work->xprt = &smd_xprtp->xprt;
		INIT_WORK(&xprt_work->work, smd_xprt_close_event);
		queue_work(smd_xprtp->smd_xprt_wq, &xprt_work->work);
		break;
	}
}

static void *msm_ipc_load_subsystem(uint32_t edge)
{
	void *pil = NULL;
	const char *peripheral;
	bool loading_disabled;

	loading_disabled = is_pil_loading_disabled(edge);
	peripheral = smd_edge_to_pil_str(edge);
	if (!IS_ERR_OR_NULL(peripheral) && !loading_disabled) {
		pil = subsystem_get(peripheral);
		if (IS_ERR(pil)) {
			IPC_RTR_ERR("%s: Failed to load %s\n",
				__func__, peripheral);
			pil = NULL;
		}
	}
	return pil;
}

static struct msm_ipc_router_smd_xprt *
		find_smd_xprt_list(struct platform_device *pdev)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;

	mutex_lock(&smd_remote_xprt_list_lock_lha1);
	list_for_each_entry(smd_xprtp, &smd_remote_xprt_list, list) {
		if (!strcmp(pdev->name, smd_xprtp->ch_name)
				&& (pdev->id == smd_xprtp->edge)) {
			mutex_unlock(&smd_remote_xprt_list_lock_lha1);
			return smd_xprtp;
		}
	}
	mutex_unlock(&smd_remote_xprt_list_lock_lha1);
	return NULL;
}

static bool is_pil_loading_disabled(uint32_t edge)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;

	mutex_lock(&smd_remote_xprt_list_lock_lha1);
	list_for_each_entry(smd_xprtp, &smd_remote_xprt_list, list) {
		if (smd_xprtp->edge == edge) {
			mutex_unlock(&smd_remote_xprt_list_lock_lha1);
			return smd_xprtp->disable_pil_loading;
		}
	}
	mutex_unlock(&smd_remote_xprt_list_lock_lha1);
	return true;
}

static int msm_ipc_router_smd_remote_probe(struct platform_device *pdev)
{
	int rc;
	struct msm_ipc_router_smd_xprt *smd_xprtp;

	smd_xprtp = find_smd_xprt_list(pdev);
	if (!smd_xprtp) {
		IPC_RTR_ERR("%s No device with name %s\n",
					__func__, pdev->name);
		return -EPROBE_DEFER;
	}
	if (strcmp(pdev->name, smd_xprtp->ch_name)
			|| (pdev->id != smd_xprtp->edge)) {
		IPC_RTR_ERR("%s wrong item name:%s edge:%d\n",
				__func__, smd_xprtp->ch_name, smd_xprtp->edge);
		return -ENODEV;
	}
	smd_xprtp->smd_xprt_wq =
		create_singlethread_workqueue(pdev->name);
	if (!smd_xprtp->smd_xprt_wq) {
		IPC_RTR_ERR("%s: WQ creation failed for %s\n",
			__func__, pdev->name);
		return -EFAULT;
	}

	smd_xprtp->pil = msm_ipc_load_subsystem(
					smd_xprtp->edge);
	rc = smd_named_open_on_edge(smd_xprtp->ch_name,
				    smd_xprtp->edge,
				    &smd_xprtp->channel,
				    smd_xprtp,
				    msm_ipc_router_smd_remote_notify);
	if (rc < 0) {
		IPC_RTR_ERR("%s: Channel open failed for %s\n",
			__func__, smd_xprtp->ch_name);
		if (smd_xprtp->pil) {
			subsystem_put(smd_xprtp->pil);
			smd_xprtp->pil = NULL;
		}
		destroy_workqueue(smd_xprtp->smd_xprt_wq);
		return rc;
	}

	smd_disable_read_intr(smd_xprtp->channel);

	smsm_change_state(SMSM_APPS_STATE, 0, SMSM_RPCINIT);

	return 0;
}

struct pil_vote_info {
	void *pil_handle;
	struct work_struct load_work;
	struct work_struct unload_work;
};

static void pil_vote_load_worker(struct work_struct *work)
{
	const char *peripheral;
	struct pil_vote_info *vote_info;
	bool loading_disabled;

	vote_info = container_of(work, struct pil_vote_info, load_work);
	peripheral = smd_edge_to_pil_str(SMD_APPS_MODEM);
	loading_disabled = is_pil_loading_disabled(SMD_APPS_MODEM);

	if (!IS_ERR_OR_NULL(peripheral) && !strcmp(peripheral, "modem") &&
	    !loading_disabled) {
		vote_info->pil_handle = subsystem_get(peripheral);
		if (IS_ERR(vote_info->pil_handle)) {
			IPC_RTR_ERR("%s: Failed to load %s\n",
				__func__, peripheral);
			vote_info->pil_handle = NULL;
		}
	} else {
		vote_info->pil_handle = NULL;
	}
}

static void pil_vote_unload_worker(struct work_struct *work)
{
	struct pil_vote_info *vote_info;

	vote_info = container_of(work, struct pil_vote_info, unload_work);

	if (vote_info->pil_handle) {
		subsystem_put(vote_info->pil_handle);
		vote_info->pil_handle = NULL;
	}
	kfree(vote_info);
}

void *msm_ipc_load_default_node(void)
{
	struct pil_vote_info *vote_info;

	vote_info = kmalloc(sizeof(struct pil_vote_info), GFP_KERNEL);
	if (vote_info == NULL) {
		pr_err("%s: mem alloc for pil_vote_info failed\n", __func__);
		return NULL;
	}

	INIT_WORK(&vote_info->load_work, pil_vote_load_worker);
	queue_work(pil_vote_wq, &vote_info->load_work);

	return vote_info;
}
EXPORT_SYMBOL(msm_ipc_load_default_node);

void msm_ipc_unload_default_node(void *pil_vote)
{
	struct pil_vote_info *vote_info;

	if (pil_vote) {
		vote_info = (struct pil_vote_info *) pil_vote;
		INIT_WORK(&vote_info->unload_work, pil_vote_unload_worker);
		queue_work(pil_vote_wq, &vote_info->unload_work);
	}
}
EXPORT_SYMBOL(msm_ipc_unload_default_node);

static int msm_ipc_router_smd_driver_register(
			struct msm_ipc_router_smd_xprt *smd_xprtp)
{
	int ret;
	struct msm_ipc_router_smd_xprt *item;
	unsigned already_registered = 0;

	mutex_lock(&smd_remote_xprt_list_lock_lha1);
	list_for_each_entry(item, &smd_remote_xprt_list, list) {
		if (!strcmp(smd_xprtp->ch_name, item->ch_name))
			already_registered = 1;
	}
	list_add(&smd_xprtp->list, &smd_remote_xprt_list);
	mutex_unlock(&smd_remote_xprt_list_lock_lha1);

	if (!already_registered) {
		smd_xprtp->driver.driver.name = smd_xprtp->ch_name;
		smd_xprtp->driver.driver.owner = THIS_MODULE;
		smd_xprtp->driver.probe = msm_ipc_router_smd_remote_probe;

		ret = platform_driver_register(&smd_xprtp->driver);
		if (ret) {
			IPC_RTR_ERR(
			"%s: Failed to register platform driver [%s]\n",
						__func__, smd_xprtp->ch_name);
			return ret;
		}
	} else {
		IPC_RTR_ERR("%s Already driver registered %s\n",
					__func__, smd_xprtp->ch_name);
	}
	return 0;
}

static int msm_ipc_router_smd_config_init(
		struct msm_ipc_router_smd_xprt_config *smd_xprt_config)
{
	struct msm_ipc_router_smd_xprt *smd_xprtp;

	smd_xprtp = kzalloc(sizeof(struct msm_ipc_router_smd_xprt), GFP_KERNEL);
	if (IS_ERR_OR_NULL(smd_xprtp)) {
		IPC_RTR_ERR("%s: kzalloc() failed for smd_xprtp id:%s\n",
				__func__, smd_xprt_config->ch_name);
		return -ENOMEM;
	}

	smd_xprtp->xprt.link_id = smd_xprt_config->link_id;
	smd_xprtp->xprt_version = smd_xprt_config->xprt_version;
	smd_xprtp->edge = smd_xprt_config->edge;
	smd_xprtp->xprt_option = smd_xprt_config->xprt_option;
	smd_xprtp->disable_pil_loading = smd_xprt_config->disable_pil_loading;

	strlcpy(smd_xprtp->ch_name, smd_xprt_config->ch_name,
						SMD_MAX_CH_NAME_LEN);

	strlcpy(smd_xprtp->xprt_name, smd_xprt_config->xprt_name,
						XPRT_NAME_LEN);
	smd_xprtp->xprt.name = smd_xprtp->xprt_name;

	smd_xprtp->xprt.get_version =
		msm_ipc_router_smd_get_xprt_version;
	smd_xprtp->xprt.get_option =
		msm_ipc_router_smd_get_xprt_option;
	smd_xprtp->xprt.read_avail = NULL;
	smd_xprtp->xprt.read = NULL;
	smd_xprtp->xprt.write_avail =
		msm_ipc_router_smd_remote_write_avail;
	smd_xprtp->xprt.write = msm_ipc_router_smd_remote_write;
	smd_xprtp->xprt.close = msm_ipc_router_smd_remote_close;
	smd_xprtp->xprt.sft_close_done = smd_xprt_sft_close_done;
	smd_xprtp->xprt.priv = NULL;

	init_waitqueue_head(&smd_xprtp->write_avail_wait_q);
	smd_xprtp->in_pkt = NULL;
	smd_xprtp->is_partial_in_pkt = 0;
	INIT_DELAYED_WORK(&smd_xprtp->read_work, smd_xprt_read_data);
	spin_lock_init(&smd_xprtp->ss_reset_lock);
	smd_xprtp->ss_reset = 0;

	msm_ipc_router_smd_driver_register(smd_xprtp);

	return 0;
}

static int parse_devicetree(struct device_node *node,
		struct msm_ipc_router_smd_xprt_config *smd_xprt_config)
{
	int ret;
	int edge;
	int link_id;
	int version;
	char *key;
	const char *ch_name;
	const char *remote_ss;

	key = "qcom,ch-name";
	ch_name = of_get_property(node, key, NULL);
	if (!ch_name)
		goto error;
	strlcpy(smd_xprt_config->ch_name, ch_name, SMD_MAX_CH_NAME_LEN);

	key = "qcom,xprt-remote";
	remote_ss = of_get_property(node, key, NULL);
	if (!remote_ss)
		goto error;
	edge = smd_remote_ss_to_edge(remote_ss);
	if (edge < 0)
		goto error;
	smd_xprt_config->edge = edge;

	key = "qcom,xprt-linkid";
	ret = of_property_read_u32(node, key, &link_id);
	if (ret)
		goto error;
	smd_xprt_config->link_id = link_id;

	key = "qcom,xprt-version";
	ret = of_property_read_u32(node, key, &version);
	if (ret)
		goto error;
	smd_xprt_config->xprt_version = version;

	key = "qcom,fragmented-data";
	smd_xprt_config->xprt_option = of_property_read_bool(node, key);

	key = "qcom,disable-pil-loading";
	smd_xprt_config->disable_pil_loading = of_property_read_bool(node, key);

	scnprintf(smd_xprt_config->xprt_name, XPRT_NAME_LEN, "%s_%s",
			remote_ss, smd_xprt_config->ch_name);

	return 0;

error:
	IPC_RTR_ERR("%s: missing key: %s\n", __func__, key);
	return -ENODEV;
}

static int msm_ipc_router_smd_xprt_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_ipc_router_smd_xprt_config smd_xprt_config;

	if (pdev) {
		if (pdev->dev.of_node) {
			mutex_lock(&smd_remote_xprt_list_lock_lha1);
			ipc_router_smd_xprt_probe_done = 1;
			mutex_unlock(&smd_remote_xprt_list_lock_lha1);

			ret = parse_devicetree(pdev->dev.of_node,
							&smd_xprt_config);
			if (ret) {
				IPC_RTR_ERR("%s: Failed to parse device tree\n",
								__func__);
				return ret;
			}

			ret = msm_ipc_router_smd_config_init(&smd_xprt_config);
			if (ret) {
				IPC_RTR_ERR("%s init failed\n", __func__);
				return ret;
			}
		}
	}
	return 0;
}

static void ipc_router_smd_xprt_probe_worker(struct work_struct *work)
{
	int i, ret;

	BUG_ON(ARRAY_SIZE(smd_xprt_cfg) != NUM_SMD_XPRTS);

	mutex_lock(&smd_remote_xprt_list_lock_lha1);
	if (!ipc_router_smd_xprt_probe_done) {
		mutex_unlock(&smd_remote_xprt_list_lock_lha1);
		for (i = 0; i < ARRAY_SIZE(smd_xprt_cfg); i++) {
			ret = msm_ipc_router_smd_config_init(&smd_xprt_cfg[i]);
			if (ret)
				IPC_RTR_ERR(" %s init failed config idx %d\n",
							__func__, i);
		}
		mutex_lock(&smd_remote_xprt_list_lock_lha1);
	}
	mutex_unlock(&smd_remote_xprt_list_lock_lha1);
}

static struct of_device_id msm_ipc_router_smd_xprt_match_table[] = {
	{ .compatible = "qcom,ipc_router_smd_xprt" },
	{},
};

static struct platform_driver msm_ipc_router_smd_xprt_driver = {
	.probe = msm_ipc_router_smd_xprt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_ipc_router_smd_xprt_match_table,
	 },
};

static int __init msm_ipc_router_smd_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_ipc_router_smd_xprt_driver);
	if (rc) {
		IPC_RTR_ERR(
		"%s: msm_ipc_router_smd_xprt_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	pil_vote_wq = create_singlethread_workqueue("pil_vote_wq");
	if (IS_ERR_OR_NULL(pil_vote_wq)) {
		pr_err("%s: create_singlethread_workqueue failed\n", __func__);
		return -EFAULT;
	}

	INIT_DELAYED_WORK(&ipc_router_smd_xprt_probe_work,
					ipc_router_smd_xprt_probe_worker);
	schedule_delayed_work(&ipc_router_smd_xprt_probe_work,
			msecs_to_jiffies(IPC_ROUTER_SMD_XPRT_WAIT_TIMEOUT));

#ifdef CONFIG_HTC_DEBUG_RIL_PCN0005_HTC_DUMP_SMSM_LOG
#ifdef CONFIG_DEBUG_FS
	do {
		struct dentry *dent;

		dent = debugfs_create_dir("smd_xprt", 0);
		if (!IS_ERR(dent)) {
			debugfs_create_file("dumplog", S_IRUGO, dent, NULL, &smd_xprt_dbg_fops);
		}
	} while(0);
#endif
#endif

	return 0;
}

module_init(msm_ipc_router_smd_xprt_init);
MODULE_DESCRIPTION("IPC Router SMD XPRT");
MODULE_LICENSE("GPL v2");
