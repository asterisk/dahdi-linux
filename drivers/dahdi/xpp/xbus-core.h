/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef	XBUS_CORE_H
#define	XBUS_CORE_H

#include <linux/wait.h>
#include <linux/interrupt.h>	/* for tasklets */
#include <linux/kref.h>
#include "xpd.h"
#include "xframe_queue.h"
#include "xbus-pcm.h"

#define	MAX_BUSES	128
#define	XFRAME_DATASIZE	512
#define	MAX_ENV_STR	40

/* forward declarations */
struct xbus_workqueue;

#ifdef	__KERNEL__

struct xbus_ops {
	int (*xframe_send_pcm)(xbus_t *xbus, xframe_t *xframe);
	int (*xframe_send_cmd)(xbus_t *xbus, xframe_t *xframe);
	xframe_t *(*alloc_xframe)(xbus_t *xbus, gfp_t gfp_flags);
	void (*free_xframe)(xbus_t *xbus, xframe_t *xframe);
};

/*
 * XBUS statistics counters
 */
enum {
	XBUS_N_UNITS,
	XBUS_N_TX_XFRAME_PCM,
	XBUS_N_RX_XFRAME_PCM,
	XBUS_N_TX_PACK_PCM,
	XBUS_N_RX_PACK_PCM,
	XBUS_N_TX_BYTES,
	XBUS_N_RX_BYTES,
	XBUS_N_TX_PCM_FRAG,
	XBUS_N_RX_CMD,
	XBUS_N_TX_CMD,
};

#define	XBUS_COUNTER(xbus, counter)	((xbus)->counters[XBUS_N_ ## counter])

#define	C_(x)	[ XBUS_N_ ## x ] = { #x }

/* yucky, make an instance so we can size it... */
static struct xbus_counters {
	char	*name;
} xbus_counters[] = {
	C_(UNITS),
	C_(TX_XFRAME_PCM),
	C_(RX_XFRAME_PCM),
	C_(TX_PACK_PCM),
	C_(RX_PACK_PCM),
	C_(TX_BYTES),
	C_(RX_BYTES),
	C_(TX_PCM_FRAG),
	C_(RX_CMD),
	C_(TX_CMD),
};

#undef C_

#define	XBUS_COUNTER_MAX	ARRAY_SIZE(xbus_counters)

enum xbus_state {
	XBUS_STATE_START,
	XBUS_STATE_IDLE,
	XBUS_STATE_SENT_REQUEST,
	XBUS_STATE_RECVD_DESC,
	XBUS_STATE_READY,
	XBUS_STATE_DEACTIVATING,
	XBUS_STATE_DEACTIVATED,
	XBUS_STATE_FAIL,
};

const char *xbus_statename(enum xbus_state st);

struct xbus_transport {
	struct xbus_ops		*ops;
	void			*priv;
	struct device		*transport_device;
	ushort			max_send_size;
	enum xbus_state		xbus_state;
	unsigned long		transport_flags;
	spinlock_t		state_lock;
	atomic_t		transport_refcount;
	wait_queue_head_t	transport_unused;
	spinlock_t		lock;
	char			model_string[MAX_ENV_STR];
};

#define	MAX_SEND_SIZE(xbus)	((xbus)->transport.max_send_size)
#define	XBUS_STATE(xbus)	((xbus)->transport.xbus_state)
#define	XBUS_IS(xbus, st)	(XBUS_STATE(xbus) == XBUS_STATE_ ## st)
#define	TRANSPORT_EXIST(xbus)	((xbus)->transport.ops != NULL)

#define	XBUS_FLAG_CONNECTED	0
#define	XBUS_FLAGS(xbus, flg)	test_bit(XBUS_FLAG_ ## flg, &((xbus)->transport.transport_flags))

struct xbus_ops *transportops_get(xbus_t *xbus);
void transportops_put(xbus_t *xbus);

/*
 * Encapsulate all poll related data of a single xbus.
 */
struct xbus_workqueue {
	struct workqueue_struct	*wq;
	struct work_struct	xpds_init_work;
	bool			xpds_init_done;
	struct list_head	card_list;
	int			num_units;
	int			num_units_initialized;
	wait_queue_head_t	wait_for_xpd_initialization;
	spinlock_t		worker_lock;
	struct semaphore	running_initialization;
};

/*
 * Allocate/Free an xframe from pools of empty xframes.
 * Calls to {get,put}_xframe are wrapped in
 * the macros bellow, so we take/return it
 * to the correct pool.
 */
xframe_t *get_xframe(struct xframe_queue *q);
void put_xframe(struct xframe_queue *q, xframe_t *xframe);

#define	ALLOC_SEND_XFRAME(xbus)	get_xframe(&(xbus)->send_pool)
#define	ALLOC_RECV_XFRAME(xbus)	get_xframe(&(xbus)->receive_pool)
#define	FREE_SEND_XFRAME(xbus, xframe)	put_xframe(&(xbus)->send_pool, (xframe))
#define	FREE_RECV_XFRAME(xbus, xframe)	put_xframe(&(xbus)->receive_pool, (xframe))

xbus_t	*xbus_num(uint num);
xbus_t	*get_xbus(const char *msg, uint num);
void	put_xbus(const char *msg, xbus_t *xbus);
int	refcount_xbus(xbus_t *xbus);

/*
 * Echo canceller related data
 */
#define	ECHO_TIMESLOTS	128

struct echoops {
	int	(*ec_set)(xpd_t *xpd, int pos, bool on);
	int	(*ec_get)(xpd_t *xpd, int pos);
	int	(*ec_update)(xbus_t *xbus);
	void	(*ec_dump)(xbus_t *xbus);
};

struct xbus_echo_state {
	const struct echoops	*echoops;
	byte			timeslots[ECHO_TIMESLOTS];
	int			xpd_idx;
	struct device_attribute	*da[MAX_XPDS];
};
#define	ECHOOPS(xbus)			((xbus)->echo_state.echoops)
#define	EC_METHOD(name, xbus)		(ECHOOPS(xbus)->name)
#define	CALL_EC_METHOD(name, xbus, ...)	(EC_METHOD(name, (xbus))(__VA_ARGS__))

/*
 * An xbus is a transport layer for Xorcom Protocol commands
 */
struct xbus {
	char			busname[XBUS_NAMELEN];	/* set by xbus_new() */

	/* low-level bus drivers set these 2 fields */
	char			connector[XBUS_DESCLEN];
	char			label[LABEL_SIZE];
	byte			revision;		/* Protocol revision */
	struct xbus_transport	transport;
	struct dahdi_device	*ddev;

	int			num;
	struct xpd		*xpds[MAX_XPDS];
	struct xbus_echo_state	echo_state;

	int			command_tick_counter;
	int			usec_nosend;		/* Firmware flow control */
	struct xframe_queue	command_queue;
	wait_queue_head_t	command_queue_empty;

	struct xframe_queue	send_pool;		/* empty xframes for send */
	struct xframe_queue	receive_pool;		/* empty xframes for receive */

	/* tasklet processing */
	struct xframe_queue	receive_queue;
	struct tasklet_struct	receive_tasklet;
	int			cpu_rcv_intr[NR_CPUS];
	int			cpu_rcv_tasklet[NR_CPUS];

	struct quirks {
		unsigned int has_fxo:1;
		unsigned int has_digital_span:1;
	}			quirks;
	bool			self_ticking;
	enum sync_mode		sync_mode;
	/* Managed by low-level drivers: */
	enum sync_mode		sync_mode_default;
	struct timer_list	command_timer;
	unsigned int		xbus_frag_count;
	struct xframe_queue	pcm_tospan;

	struct xpp_ticker	ticker;		/* for tick rate */
	struct xpp_drift	drift;		/* for tick offset */

	atomic_t		pcm_rx_counter;
	unsigned int		global_counter;

	/* Device-Model */
	struct device		astribank;
#define	dev_to_xbus(dev)	container_of(dev, struct xbus, astribank)
	struct kref		kref;
#define kref_to_xbus(k) container_of(k, struct xbus, kref)

	spinlock_t		lock;

	/* PCM metrics */
	struct timeval		last_tx_sync;
	struct timeval		last_rx_sync;
	unsigned long		max_tx_sync;
	unsigned long		min_tx_sync;
	unsigned long		max_rx_sync;
	unsigned long		min_rx_sync;
	unsigned long		max_rx_process;		/* packet processing time (usec) */
#ifdef	SAMPLE_TICKS
#define	SAMPLE_SIZE	1000
	int			sample_ticks[SAMPLE_SIZE];
	bool			sample_running;
	int			sample_pos;
#endif

	struct xbus_workqueue	worker;

	/*
	 * Sync adjustment
	 */
	int			sync_adjustment;
	int			sync_adjustment_offset;
	long			pll_updated_at;

	atomic_t		num_xpds;

#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*proc_xbus_dir;
	struct proc_dir_entry	*proc_xbus_summary;
#ifdef	PROTOCOL_DEBUG
	struct proc_dir_entry	*proc_xbus_command;
#endif
#endif

	/* statistics */
	int		counters[XBUS_COUNTER_MAX];
};
#endif

#define	XFRAME_MAGIC	123456L

struct xframe {
	unsigned long		xframe_magic;
	struct list_head	frame_list;
	atomic_t		frame_len;
	xbus_t			*xbus;
	struct timeval		tv_created;
	struct timeval		tv_queued;
	struct timeval		tv_submitted;
	struct timeval		tv_received;
	/* filled by transport layer */
	size_t			frame_maxlen;
	byte			*packets;	/* max XFRAME_DATASIZE */
	byte			*first_free;
	int			usec_towait;	/* prevent overflowing AB */
	void			*priv;
};

void xframe_init(xbus_t *xbus, xframe_t *xframe, void *buf, size_t maxsize, void *priv);

#define XFRAME_LEN(frame)	atomic_read(&(frame)->frame_len)

int xbus_core_init(void);		/* Initializer */
void xbus_core_shutdown(void);		/* Terminator */

/* Frame handling */
void dump_xframe(const char msg[], const xbus_t *xbus, const xframe_t *xframe, int debug);
int send_cmd_frame(xbus_t *xbus, xframe_t *xframe);

/*
 * Return pointer to next packet slot in the frame
 * or NULL if the frame is full.
 */
xpacket_t *xframe_next_packet(xframe_t *xframe, int len);

/* XBUS handling */

/*
 * Map: unit+subunit <--> index in xbus->xpds[]
 */
#define	XPD_IDX(unit,subunit)	((unit) * MAX_SUBUNIT + (subunit))
#define	XBUS_UNIT(idx)		((idx) / MAX_SUBUNIT)
#define	XBUS_SUBUNIT(idx)	((idx) % MAX_SUBUNIT)

xpd_t	*xpd_of(const xbus_t *xbus, int xpd_num);
xpd_t	*xpd_byaddr(const xbus_t *xbus, uint unit, uint subunit);
int	xbus_check_unique(xbus_t *xbus);
bool	xbus_setstate(xbus_t *xbus, enum xbus_state newstate);
bool	xbus_setflags(xbus_t *xbus, int flagbit, bool on);
xbus_t	*xbus_new(struct xbus_ops *ops, ushort max_send_size, struct device *transport_device, void *priv);
void	xbus_free(xbus_t *xbus);
int	xbus_connect(xbus_t *xbus);
int	xbus_activate(xbus_t *xbus);
void	xbus_deactivate(xbus_t *xbus);
void	xbus_disconnect(xbus_t *xbus);
void	xbus_receive_xframe(xbus_t *xbus, xframe_t *xframe);
int	xbus_process_worker(xbus_t *xbus);
int	waitfor_xpds(xbus_t *xbus, char *buf);

int	xbus_xpd_bind(xbus_t *xbus, xpd_t *xpd, int unit, int subunit);
int	xbus_xpd_unbind(xbus_t *xbus, xpd_t *xpd);

/* sysfs */
int	xpd_device_register(xbus_t *xbus, xpd_t *xpd);
void	xpd_device_unregister(xpd_t *xpd);
int	echocancel_xpd(xpd_t *xpd, int on);

int	xbus_is_registered(xbus_t *xbus);
int	xbus_register_dahdi_device(xbus_t *xbus);
void	xbus_unregister_dahdi_device(xbus_t *xbus);

int	xpp_driver_init(void);
void	xpp_driver_exit(void);
int	xbus_sysfs_transport_create(xbus_t *xbus);
void	xbus_sysfs_transport_remove(xbus_t *xbus);
int	xbus_sysfs_create(xbus_t *xbus);
void	xbus_sysfs_remove(xbus_t *xbus);

#ifdef	OLD_HOTPLUG_SUPPORT_269
/* Copy from new kernels lib/kobject_uevent.c */
enum kobject_action {
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
	KOBJ_MOUNT,
	KOBJ_UMOUNT,
	KOBJ_OFFLINE,
	KOBJ_ONLINE,
};
#endif

void	astribank_uevent_send(xbus_t *xbus, enum kobject_action act);

#endif	/* XBUS_CORE_H */

