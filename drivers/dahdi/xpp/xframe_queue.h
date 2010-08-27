#ifndef	XFRAME_QUEUE_H
#define	XFRAME_QUEUE_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include "xdefs.h"

#define	XFRAME_QUEUE_MARGIN	10

struct xframe_queue {
	struct list_head	head;
	bool			disabled;
	unsigned int		count;
	unsigned int		max_count;
	unsigned int		steady_state_count;
	spinlock_t		lock;
	const char		*name;
	void			*priv;
	/* statistics */
	unsigned int		worst_count;
	unsigned int		overflows;
	unsigned long		worst_lag_usec;	/* since xframe creation */
};

void xframe_queue_init(struct xframe_queue *q,
	unsigned int steady_state_count, unsigned int max_count,
	const char *name, void *priv);
__must_check bool xframe_enqueue(struct xframe_queue *q, xframe_t *xframe);
__must_check xframe_t *xframe_dequeue(struct xframe_queue *q);
void xframe_queue_clearstats(struct xframe_queue *q);
void xframe_queue_disable(struct xframe_queue *q, bool disabled);
void xframe_queue_clear(struct xframe_queue *q);
uint xframe_queue_count(struct xframe_queue *q);

#endif	/* XFRAME_QUEUE_ */
