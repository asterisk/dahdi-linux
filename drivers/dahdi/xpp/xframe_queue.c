#include "xframe_queue.h"
#include "xbus-core.h"
#include "dahdi_debug.h"

extern int debug;

static xframe_t *transport_alloc_xframe(xbus_t *xbus, gfp_t gfp_flags);
static void transport_free_xframe(xbus_t *xbus, xframe_t *xframe);

void xframe_queue_init(struct xframe_queue *q, unsigned int steady_state_count, unsigned int max_count, const char *name, void *priv)
{
	memset(q, 0, sizeof(*q));
	spin_lock_init(&q->lock);
	INIT_LIST_HEAD(&q->head);
	q->max_count = XFRAME_QUEUE_MARGIN + max_count;
	q->steady_state_count = XFRAME_QUEUE_MARGIN + steady_state_count;
	q->name = name;
	q->priv = priv;
}

void xframe_queue_clearstats(struct xframe_queue *q)
{
	q->worst_count = 0;
	//q->overflows = 0;	/* Never clear overflows */
	q->worst_lag_usec = 0L;
}

static void __xframe_dump_queue(struct xframe_queue *q)
{
	xframe_t	*xframe;
	int		i = 0;
	char		prefix[30];
	struct timeval	now;

	do_gettimeofday(&now);
	printk(KERN_DEBUG "%s: dump queue '%s' (first packet in each frame)\n",
		THIS_MODULE->name,
		q->name);
	list_for_each_entry_reverse(xframe, &q->head, frame_list) {
		xpacket_t	*pack = (xpacket_t *)&xframe->packets[0];
		long		usec = usec_diff(&now, &xframe->tv_queued);
		snprintf(prefix, ARRAY_SIZE(prefix), "  %3d> %5ld.%03ld msec",
			i++, usec / 1000, usec % 1000);
		dump_packet(prefix, pack, 1);
	}
}

static bool __xframe_enqueue(struct xframe_queue *q, xframe_t *xframe)
{
	int			ret = 1;
	static int		overflow_cnt = 0;

	if(unlikely(q->disabled)) {
		ret = 0;
		goto out;
	}
	if(q->count >= q->max_count) {
		q->overflows++;
		if ((overflow_cnt++ % 1000) < 5) {
			NOTICE("Overflow of %-15s: counts %3d, %3d, %3d worst %3d, overflows %3d worst_lag %02ld.%ld ms\n",
				q->name,
				q->steady_state_count,
				q->count,
				q->max_count,
				q->worst_count,
				q->overflows,
				q->worst_lag_usec / 1000,
				q->worst_lag_usec % 1000);
			__xframe_dump_queue(q);
		}
		ret = 0;
		goto out;
	}
	if(++q->count > q->worst_count)
		q->worst_count = q->count;
	list_add_tail(&xframe->frame_list, &q->head);
	do_gettimeofday(&xframe->tv_queued);
out:
	return ret;
}

bool xframe_enqueue(struct xframe_queue *q, xframe_t *xframe)
{
	unsigned long	flags;
	int		ret;

	spin_lock_irqsave(&q->lock, flags);
	ret = __xframe_enqueue(q, xframe);
	spin_unlock_irqrestore(&q->lock, flags);
	return ret;
}

static xframe_t *__xframe_dequeue(struct xframe_queue *q)
{
	xframe_t		*frm = NULL;
	struct list_head	*h;
	struct timeval		now;
	unsigned long		usec_lag;

	if(list_empty(&q->head))
		goto out;
	h = q->head.next;
	list_del_init(h);
	--q->count;
	frm = list_entry(h, xframe_t, frame_list);
	do_gettimeofday(&now);
	usec_lag =	
		(now.tv_sec - frm->tv_queued.tv_sec)*1000*1000 +
		(now.tv_usec - frm->tv_queued.tv_usec);
	if(q->worst_lag_usec < usec_lag)
		q->worst_lag_usec = usec_lag;
out:
	return frm;
}

xframe_t *xframe_dequeue(struct xframe_queue *q)
{
	unsigned long	flags;
	xframe_t	*frm;

	spin_lock_irqsave(&q->lock, flags);
	frm = __xframe_dequeue(q);
	spin_unlock_irqrestore(&q->lock, flags);
	return frm;
}
void xframe_queue_disable(struct xframe_queue *q, bool disabled)
{
	q->disabled = disabled;
}

void xframe_queue_clear(struct xframe_queue *q)
{
	xframe_t	*xframe;
	xbus_t		*xbus = q->priv;
	int		i = 0;

	xframe_queue_disable(q, 1);
	while((xframe = xframe_dequeue(q)) != NULL) {
		transport_free_xframe(xbus, xframe);
		i++;
	}
	XBUS_DBG(DEVICES, xbus, "%s: finished queue clear (%d items)\n", q->name, i);
}

uint xframe_queue_count(struct xframe_queue *q)
{
	return q->count;
}

/*------------------------- Frame Alloc/Dealloc --------------------*/

static xframe_t *transport_alloc_xframe(xbus_t *xbus, gfp_t gfp_flags)
{
	struct xbus_ops	*ops;
	xframe_t	*xframe;
	unsigned long	flags;

	BUG_ON(!xbus);
	ops = transportops_get(xbus);
	if(unlikely(!ops)) {
		XBUS_ERR(xbus, "Missing transport\n");
		return NULL;
	}
	spin_lock_irqsave(&xbus->transport.lock, flags);
	//XBUS_INFO(xbus, "%s (transport_refcount=%d)\n", __FUNCTION__, atomic_read(&xbus->transport.transport_refcount));
	xframe = ops->alloc_xframe(xbus, gfp_flags);
	if(!xframe) {
		static int rate_limit;

		if((rate_limit++ % 3001) == 0)
			XBUS_ERR(xbus,
				"Failed xframe allocation from transport (%d)\n",
				rate_limit);
		transportops_put(xbus);
		/* fall through */
	}
	spin_unlock_irqrestore(&xbus->transport.lock, flags);
	return xframe;
}

static void transport_free_xframe(xbus_t *xbus, xframe_t *xframe)
{
	struct xbus_ops	*ops;
	unsigned long	flags;

	BUG_ON(!xbus);
	ops = xbus->transport.ops;
	BUG_ON(!ops);
	spin_lock_irqsave(&xbus->transport.lock, flags);
	//XBUS_INFO(xbus, "%s (transport_refcount=%d)\n", __FUNCTION__, atomic_read(&xbus->transport.transport_refcount));
	ops->free_xframe(xbus, xframe);
	transportops_put(xbus);
	spin_unlock_irqrestore(&xbus->transport.lock, flags);
}

static bool xframe_queue_adjust(struct xframe_queue *q)
{
	xbus_t		*xbus;
	xframe_t	*xframe;
	int		delta;
	unsigned long	flags;
	int		ret = 0;

	BUG_ON(!q);
	xbus = q->priv;
	BUG_ON(!xbus);
	spin_lock_irqsave(&q->lock, flags);
	delta = q->count - q->steady_state_count;
	if(delta < -XFRAME_QUEUE_MARGIN) {
		/* Increase pool by one frame */
		//XBUS_INFO(xbus, "%s(%d): Allocate one\n", q->name, delta);
		xframe = transport_alloc_xframe(xbus, GFP_ATOMIC);
		if(!xframe) {
			static int rate_limit;

			if((rate_limit++ % 3001) == 0)
				XBUS_ERR(xbus, "%s: failed frame allocation\n", q->name);
			goto out;
		}
		if(!__xframe_enqueue(q, xframe)) {
			static int rate_limit;

			if((rate_limit++ % 3001) == 0)
				XBUS_ERR(xbus, "%s: failed enqueueing frame\n", q->name);
			transport_free_xframe(xbus, xframe);
			goto out;
		}
	} else if(delta > XFRAME_QUEUE_MARGIN) {
		/* Decrease pool by one frame */
		//XBUS_INFO(xbus, "%s(%d): Free one\n", q->name, delta);
		xframe = __xframe_dequeue(q);
		if(!xframe) {
			static int rate_limit;

			if((rate_limit++ % 3001) == 0)
				XBUS_ERR(xbus, "%s: failed dequeueing frame\n", q->name);
			goto out;
		}
		transport_free_xframe(xbus, xframe);
	}
	ret = 1;
out:
	spin_unlock_irqrestore(&q->lock, flags);
	return ret;
}

xframe_t *get_xframe(struct xframe_queue *q)
{
	xframe_t	*xframe;
	xbus_t		*xbus;

	BUG_ON(!q);
	xbus = (xbus_t *)q->priv;
	BUG_ON(!xbus);
	xframe_queue_adjust(q);
	xframe = xframe_dequeue(q);
	if(!xframe) {
		static int rate_limit;

		if((rate_limit++ % 3001) == 0)
			XBUS_ERR(xbus, "%s STILL EMPTY (%d)\n", q->name, rate_limit);
		return NULL;
	}
	BUG_ON(xframe->xframe_magic != XFRAME_MAGIC);
	atomic_set(&xframe->frame_len, 0);
	xframe->first_free = xframe->packets;
	do_gettimeofday(&xframe->tv_created);
	/*
	 * If later parts bother to correctly initialize their
	 * headers, there is no need to memset() the whole data.
	 *
	 * ticket:403
	 *
	 * memset(xframe->packets, 0, xframe->frame_maxlen);
	 */
	//XBUS_INFO(xbus, "%s\n", __FUNCTION__);
	return xframe;
}

void put_xframe(struct xframe_queue *q, xframe_t *xframe)
{
	xbus_t		*xbus;

	BUG_ON(!q);
	xbus = (xbus_t *)q->priv;
	BUG_ON(!xbus);
	//XBUS_INFO(xbus, "%s\n", __FUNCTION__);
	BUG_ON(!TRANSPORT_EXIST(xbus));
	if(unlikely(!xframe_enqueue(q, xframe))) {
		XBUS_ERR(xbus, "Failed returning xframe to %s\n", q->name);
		transport_free_xframe(xbus, xframe);
		return;
	}
	xframe_queue_adjust(q);
}


EXPORT_SYMBOL(xframe_queue_init);
EXPORT_SYMBOL(xframe_queue_clearstats);
EXPORT_SYMBOL(xframe_enqueue);
EXPORT_SYMBOL(xframe_dequeue);
EXPORT_SYMBOL(xframe_queue_disable);
EXPORT_SYMBOL(xframe_queue_clear);
EXPORT_SYMBOL(xframe_queue_count);
EXPORT_SYMBOL(get_xframe);
EXPORT_SYMBOL(put_xframe);
