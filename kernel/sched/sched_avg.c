/* Copyright (c) 2012, 2015, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>
#include <trace/events/sched.h>

#include "sched.h"

static DEFINE_PER_CPU(u64, nr_prod_sum);
static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, nr_big_prod_sum);
static DEFINE_PER_CPU(u64, nr);

static DEFINE_PER_CPU(unsigned long, iowait_prod_sum);
static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static s64 last_get_time;

u64 dbg_diff;
u64 dbg_tmp_avg;
u64 dbg_tmp_iowait;
u64 dbg_tmp_big_avg;
void sched_get_nr_running_avg(int *avg, int *iowait_avg, int *big_avg)
{
	int cpu;
	u64 curr_time = sched_clock();
	u64 diff = curr_time - last_get_time;
	u64 tmp_avg = 0, tmp_iowait = 0, tmp_big_avg = 0;

	*avg = 0;
	*iowait_avg = 0;
	*big_avg = 0;

	if (!diff)
		return;
	WARN((s64)diff<0, "[%s] time last:%llu curr:%llu ", __func__, last_get_time, curr_time);

	
	for_each_possible_cpu(cpu) {
		unsigned long flags;

		spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
		curr_time = sched_clock();
		tmp_avg += per_cpu(nr_prod_sum, cpu);
		tmp_avg += per_cpu(nr, cpu) *
			(curr_time - per_cpu(last_time, cpu));

		tmp_big_avg += per_cpu(nr_big_prod_sum, cpu);
		tmp_big_avg += nr_eligible_big_tasks(cpu) *
			(curr_time - per_cpu(last_time, cpu));

		tmp_iowait += per_cpu(iowait_prod_sum, cpu);
		tmp_iowait +=  nr_iowait_cpu(cpu) *
			(curr_time - per_cpu(last_time, cpu));

		per_cpu(last_time, cpu) = curr_time;

		per_cpu(nr_prod_sum, cpu) = 0;
		per_cpu(nr_big_prod_sum, cpu) = 0;
		per_cpu(iowait_prod_sum, cpu) = 0;

		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
	}

	diff = curr_time - last_get_time;
	last_get_time = curr_time;

	*avg = (int)div64_u64(tmp_avg * 100, diff);
	*big_avg = (int)div64_u64(tmp_big_avg * 100, diff);
	*iowait_avg = (int)div64_u64(tmp_iowait * 100, diff);

	if (*avg < 0 || *big_avg < 0 || *iowait_avg < 0) {
		dbg_diff = diff;
		dbg_tmp_avg = tmp_avg;
		dbg_tmp_big_avg = tmp_big_avg;
		dbg_tmp_iowait = tmp_iowait;
	}

	if(*avg < 0) *avg = INT_MAX;
	if(*big_avg < 0) *big_avg = INT_MAX;
	if(*iowait_avg < 0) *iowait_avg = INT_MAX;

	trace_sched_get_nr_running_avg(*avg, *big_avg, *iowait_avg);

	BUG_ON(*avg < 0 || *big_avg < 0 || *iowait_avg < 0);
	pr_debug("%s - avg:%d big_avg:%d iowait_avg:%d\n",
				 __func__, *avg, *big_avg, *iowait_avg);
}
EXPORT_SYMBOL(sched_get_nr_running_avg);

void sched_update_nr_prod(int cpu, long delta, bool inc)
{
	int diff;
	s64 curr_time;
	unsigned long flags, nr_running;

	spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
	nr_running = per_cpu(nr, cpu);
	curr_time = sched_clock();
	diff = curr_time - per_cpu(last_time, cpu);
	per_cpu(last_time, cpu) = curr_time;
	per_cpu(nr, cpu) = nr_running + (inc ? delta : -delta);

	BUG_ON((s64)per_cpu(nr, cpu) < 0);

	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(nr_big_prod_sum, cpu) += nr_eligible_big_tasks(cpu) * diff;
	per_cpu(iowait_prod_sum, cpu) += nr_iowait_cpu(cpu) * diff;
	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);
