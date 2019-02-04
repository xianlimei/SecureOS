// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright(c) 2018 Intel Corporation.
 *
 */
#include "iowait.h"
#include "trace_iowait.h"

void iowait_set_flag(struct iowait *wait, u32 flag)
{
	trace_hfi1_iowait_set(wait, flag);
	set_bit(flag, &wait->flags);
}

bool iowait_flag_set(struct iowait *wait, u32 flag)
{
	return test_bit(flag, &wait->flags);
}

inline void iowait_clear_flag(struct iowait *wait, u32 flag)
{
	trace_hfi1_iowait_clear(wait, flag);
	clear_bit(flag, &wait->flags);
}

/**
 * iowait_init() - initialize wait structure
 * @wait: wait struct to initialize
 * @tx_limit: limit for overflow queuing
 * @func: restart function for workqueue
 * @sleep: sleep function for no space
 * @resume: wakeup function for no space
 *
 * This function initializes the iowait
 * structure embedded in the QP or PQ.
 *
 */
void iowait_init(struct iowait *wait, u32 tx_limit,
		 void (*func)(struct work_struct *work),
		 void (*tidfunc)(struct work_struct *work),
		 int (*sleep)(struct sdma_engine *sde,
			      struct iowait_work *wait,
			      struct sdma_txreq *tx,
			      uint seq,
			      bool pkts_sent),
		 void (*wakeup)(struct iowait *wait, int reason),
		 void (*sdma_drained)(struct iowait *wait))
{
	int i;

	wait->count = 0;
	INIT_LIST_HEAD(&wait->list);
	init_waitqueue_head(&wait->wait_dma);
	init_waitqueue_head(&wait->wait_pio);
	atomic_set(&wait->sdma_busy, 0);
	atomic_set(&wait->pio_busy, 0);
	wait->tx_limit = tx_limit;
	wait->sleep = sleep;
	wait->wakeup = wakeup;
	wait->sdma_drained = sdma_drained;
	wait->flags = 0;
	for (i = 0; i < IOWAIT_SES; i++) {
		wait->wait[i].iow = wait;
		INIT_LIST_HEAD(&wait->wait[i].tx_head);
		if (i == IOWAIT_IB_SE)
			INIT_WORK(&wait->wait[i].iowork, func);
		else
			INIT_WORK(&wait->wait[i].iowork, tidfunc);
	}
}

/**
 * iowait_cancel_work - cancel all work in iowait
 * @w: the iowait struct
 */
void iowait_cancel_work(struct iowait *w)
{
	cancel_work_sync(&iowait_get_ib_work(w)->iowork);
	cancel_work_sync(&iowait_get_tid_work(w)->iowork);
}

/**
 * iowait_set_work_flag - set work flag based on leg
 * @w - the iowait work struct
 */
int iowait_set_work_flag(struct iowait_work *w)
{
	if (w == &w->iow->wait[IOWAIT_IB_SE]) {
		iowait_set_flag(w->iow, IOWAIT_PENDING_IB);
		return IOWAIT_IB_SE;
	}
	iowait_set_flag(w->iow, IOWAIT_PENDING_TID);
	return IOWAIT_TID_SE;
}
