/*
 * DAHDI Telephony Interface
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and architectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001 - 2010 Digium, Inc.
 *
 * All rights reserved.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

/*!
 * \file
 * \brief DAHDI kernel interface definitions
 */

#ifndef _DAHDI_KERNEL_H
#define _DAHDI_KERNEL_H

#include <dahdi/user.h>
#include <dahdi/fasthdlc.h>

#include <dahdi/dahdi_config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/ioctl.h>

#ifdef CONFIG_DAHDI_NET	
#include <linux/hdlc.h>
#endif

#ifdef CONFIG_DAHDI_PPP
#include <linux/ppp_channel.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#endif
#include <linux/device.h>
#include <linux/sysfs.h>

#include <linux/poll.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
#define dahdi_pci_module pci_register_driver
#else
#define dahdi_pci_module pci_module_init
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
#define DAHDI_IRQ_HANDLER(a) static irqreturn_t a(int irq, void *dev_id)
#else
#define DAHDI_IRQ_HANDLER(a) static irqreturn_t a(int irq, void *dev_id, struct pt_regs *regs)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
#ifdef CONFIG_PCI
#include <linux/pci-aspm.h>
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
#define HAVE_NET_DEVICE_OPS
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#define DAHDI_IRQ_SHARED IRQF_SHARED
#define DAHDI_IRQ_DISABLED IRQF_DISABLED
#define DAHDI_IRQ_SHARED_DISABLED IRQF_SHARED | IRQF_DISABLED
#else
#define DAHDI_IRQ_SHARED SA_SHIRQ
#define DAHDI_IRQ_DISABLED SA_INTERRUPT
#define DAHDI_IRQ_SHARED_DISABLED SA_SHIRQ | SA_INTERRUPT
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16)
#ifndef dev_notice
#define dev_notice(dev, format, arg...)         \
        dev_printk(KERN_NOTICE , dev , format , ## arg)
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#  ifdef RHEL_RELEASE_VERSION
#    if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(5, 6)
#define dev_name(dev)		((dev)->bus_id)
#define dev_set_name(dev, format, ...) \
	snprintf((dev)->bus_id, BUS_ID_SIZE, format, ## __VA_ARGS__)
#    else
#define dev_set_name(dev, format, ...) \
	do { \
		kobject_set_name(&(dev)->kobj, format, ## __VA_ARGS__); \
		snprintf((dev)->bus_id, BUS_ID_SIZE, \
			kobject_name(&(dev)->kobj));	\
	} while (0)
#    endif
#  else
#define dev_name(dev)		((dev)->bus_id)
#define dev_set_name(dev, format, ...) \
	snprintf((dev)->bus_id, BUS_ID_SIZE, format, ## __VA_ARGS__)
#  endif
#endif

/*! Default chunk size for conferences and such -- static right now, might make
   variable sometime.  8 samples = 1 ms = most frequent service interval possible
   for a USB device */
#define DAHDI_CHUNKSIZE		 8
#define DAHDI_MIN_CHUNKSIZE	 DAHDI_CHUNKSIZE
#define DAHDI_DEFAULT_CHUNKSIZE	 DAHDI_CHUNKSIZE
#define DAHDI_MAX_CHUNKSIZE 	 DAHDI_CHUNKSIZE
#define DAHDI_CB_SIZE		 2

/* DAHDI operates at 8Khz by default */
#define DAHDI_MS_TO_SAMPLES(ms) ((ms) * 8)

#define DAHDI_MSECS_PER_CHUNK	(DAHDI_CHUNKSIZE/DAHDI_MS_TO_SAMPLES(1))

#define RING_DEBOUNCE_TIME	2000	/*!< 2000 ms ring debounce time */

typedef struct
{
    int32_t gain;
    int32_t a1;
    int32_t a2;
    int32_t b1;
    int32_t b2;

    int32_t z1;
    int32_t z2;
} biquad2_state_t;

typedef struct
{
    biquad2_state_t notch;
    int notch_level;
    int channel_level;
    int tone_present;
    int tone_cycle_duration;
    int good_cycles;
    int hit;
} echo_can_disable_detector_state_t;

struct sf_detect_state {
	long	x1;
	long	x2;
	long	y1;
	long	y2;
	long	e1;
	long	e2;
	int	samps;
	int	lastdetect;
};

struct dahdi_tone_state {
	int v1_1;
	int v2_1;
	int v3_1;
	int v1_2;
	int v2_2;
	int v3_2;
	int modulate;
};

/*! \brief Conference queue structure */
struct confq {
	u_char buffer[DAHDI_CHUNKSIZE * DAHDI_CB_SIZE];
	u_char *buf[DAHDI_CB_SIZE];
	int inbuf;
	int outbuf;
};

struct dahdi_chan;
struct dahdi_echocan_state;

/*! Features a DAHDI echo canceler (software or hardware) can provide to the DAHDI core. */
struct dahdi_echocan_features {

	/*! Able to detect CED tone (2100 Hz with phase reversals) in the transmit direction.
	 * If the echocan can detect this tone, it may report it it as an event (see
	 * the events.CED_tx_detected field of dahdi_echocan_state), and if it will automatically
	 * disable itself or its non-linear processor, then the NLP_automatic feature flag should also
	 * be set so that the DAHDI core doesn't bother trying to do so.
	*/
	u32 CED_tx_detect:1;

	/*! Able to detect CED tone (2100 Hz with phase reversals) in the receive direction.
	 * If the echocan can detect this tone, it may report it it as an event (see
	 * the events.CED_rx_detected field of dahdi_echocan_state), and if it will automatically
	 * disable itself or its non-linear processor, then the NLP_automatic flag feature should also
	 * be set so that the DAHDI core doesn't bother trying to do so.
	*/
	u32 CED_rx_detect:1;

	/*! Able to detect CNG tone (1100 Hz) in the transmit direction. */
	u32 CNG_tx_detect:1;

	/*! Able to detect CNG tone (1100 Hz) in the receive direction. */
	u32 CNG_rx_detect:1;

	/*! If the echocan's NLP can be enabled and disabled without requiring destruction
	 * and recreation of the state structure, this feature flag should be set and the
	 * echocan_NLP_toggle field of the dahdi_echocan_ops structure should be filled with a
	 * pointer to the function to perform that operation.
	 */
	u32 NLP_toggle:1;

	/*! If the echocan will automatically disable itself (or even just its NLP) based on
	 * detection of a CED tone in either direction, this feature flag should be set (along
	 * with the tone detection feature flags).
	 */
	u32 NLP_automatic:1;
};

/*! Operations (methods) that can be performed on a DAHDI echo canceler instance (state
 * structure) after it has been created, by either a software or hardware echo canceller.
 * The echo canceler must populate the owner field of the dahdi_echocan_state structure
 * with a pointer to the relevant operations structure for that instance.
 */
struct dahdi_echocan_ops {

	/*! \brief Free an echocan state structure.
	 * \param[in,out] ec Pointer to the state structure to free.
	 *
	 * \return Nothing.
	 */
	void (*echocan_free)(struct dahdi_chan *chan, struct dahdi_echocan_state *ec);

	/*! \brief Process an array of audio samples through the echocan.
	 * \param[in,out] ec Pointer to the state structure.
	 * \param[in,out] isig The receive direction data (will be modified).
	 * \param[in] iref The transmit direction data.
	 * \param[in] size The number of elements in the isig and iref arrays.
	 *
	 * Note: This function can also return events in the events field of the
	 * dahdi_echocan_state structure. If it can do so, then the echocan does
	 * not need to provide the echocan_events function.
	 *
	 * \return Nothing.
	 */
	void (*echocan_process)(struct dahdi_echocan_state *ec, short *isig, const short *iref, u32 size);

	/*! \brief Retrieve events from the echocan.
	 * \param[in,out] ec Pointer to the state structure.
	 *
	 *
	 * If any events have occurred, the events field of the dahdi_echocan_state
	 * structure should be updated to include them.
	 *
	 * \return Nothing.
	 */
	void (*echocan_events)(struct dahdi_echocan_state *ec);

	/*! \brief Feed a sample (and its position) for echocan training.
	 * \param[in,out] ec Pointer to the state structure.
	 * \param[in] pos The tap position to be 'trained'.
	 * \param[in] val The receive direction sample for the specified tap position.
	 *
	 * \retval Zero if training should continue.
	 * \retval Non-zero if training is complete.
	 */
	int (*echocan_traintap)(struct dahdi_echocan_state *ec, int pos, short val);

	/*! \brief Enable or disable non-linear processing (NLP) in the echocan.
	 * \param[in,out] ec Pointer to the state structure.
	 * \param[in] enable Zero to disable, non-zero to enable.
	 *
	 * \return Nothing.
	 */
	void (*echocan_NLP_toggle)(struct dahdi_echocan_state *ec, unsigned int enable);

#ifdef CONFIG_DAHDI_ECHOCAN_PROCESS_TX
	/*! \brief Process an array of TX audio samples.
	 *
	 * \return Nothing.
	 */
	void (*echocan_process_tx)(struct dahdi_echocan_state *ec,
				   short *tx, u32 size);
#endif
};

/*! A factory for creating instances of software echo cancelers to be used on DAHDI channels. */
struct dahdi_echocan_factory {

	/*! Get the name of the factory. */
	const char *(*get_name)(const struct dahdi_chan *chan);

	/*! Pointer to the module that owns this factory; the module's reference count will be
	 * incremented/decremented by the DAHDI core as needed.
	 */
	struct module *owner;

	/*! \brief Function to create an instance of the echocan.
	 * \param[in] ecp Structure defining parameters to be used for the instance creation.
	 * \param[in] p Pointer to the beginning of an (optional) array of user-defined parameters.
	 * \param[out] ec Pointer to the state structure that is created, if any.
	 *
	 * \retval Zero on success.
	 * \retval Non-zero on failure (return value will be returned to userspace so it should be a
	 * standard error number).
	 */
	int (*echocan_create)(struct dahdi_chan *chan, struct dahdi_echocanparams *ecp,
			      struct dahdi_echocanparam *p, struct dahdi_echocan_state **ec);
};

/*! \brief Register an echo canceler factory with the DAHDI core.
 * \param[in] ec Pointer to the dahdi_echocan_factory structure to be registered.
 *
 * \retval Zero on success.
 * \retval Non-zero on failure (return value will be a standard error number).
 */
int dahdi_register_echocan_factory(const struct dahdi_echocan_factory *ec);

/*! \brief Unregister a previously-registered echo canceler factory from the DAHDI core.
 * \param[in] ec Pointer to the dahdi_echocan_factory structure to be unregistered.
 *
 * \return Nothing.
 */
void dahdi_unregister_echocan_factory(const struct dahdi_echocan_factory *ec);

enum dahdi_echocan_mode {
	__ECHO_MODE_MUTE = 1 << 8,
	ECHO_MODE_IDLE = 0,
	ECHO_MODE_PRETRAINING = 1 | __ECHO_MODE_MUTE,
	ECHO_MODE_STARTTRAINING = 2 | __ECHO_MODE_MUTE,
	ECHO_MODE_AWAITINGECHO = 3 | __ECHO_MODE_MUTE,
	ECHO_MODE_TRAINING = 4 | __ECHO_MODE_MUTE,
	ECHO_MODE_ACTIVE = 5,
	ECHO_MODE_FAX = 6,
};

/*! An instance of a DAHDI echo canceler (software or hardware). */
struct dahdi_echocan_state {

	/*! Pointer to a dahdi_echocan_ops structure of operations that can be
	 * performed on this instance.
	 */
	const struct dahdi_echocan_ops *ops;

	/*! State data used by the DAHDI core's CED detector for the transmit
	 * direction, if needed.
	 */
	echo_can_disable_detector_state_t txecdis;

	/*! State data used by the DAHDI core's CED detector for the receive
	 * direction, if needed.
	 */
	echo_can_disable_detector_state_t rxecdis;

	/*! Features offered by the echo canceler that provided this instance. */
	struct dahdi_echocan_features features;

	struct {
		/*! The mode the echocan is currently in. */
		enum dahdi_echocan_mode mode;

		/*! The last tap position that was fed to the echocan's training function. */
		u32 last_train_tap;

		/*! How many samples to wait before beginning the training operation. */
		u32 pretrain_timer;
	} status;

	/*! This structure contains event flags, allowing the echocan to report
	 * events that occurred as it processed the transmit and receive streams
	 * of samples. Each call to the echocan_process operation for this
	 * instance may report events, so the structure should be cleared before
	 * calling that operation.
	 */
	union dahdi_echocan_events {
		u32 all;
		struct {
			/*! CED tone was detected in the transmit direction. If the
			 * echocan automatically disables its NLP when this occurs,
			 * it must also signal the NLP_auto_disabled event during the *same*
			 * call to echocan_process that reports the CED detection.
			 */
			u32 CED_tx_detected:1;

			/*! CED tone was detected in the receive direction. If the
			 * echocan automatically disables its NLP when this occurs,
			 * it must also signal the NLP_auto_disabled event during the *same*
			 * call to echocan_process that reports the CED detection.
			 */
			u32 CED_rx_detected:1;

			/*! CNG tone was detected in the transmit direction. */
			u32 CNG_tx_detected:1;

			/*! CNG tone was detected in the receive direction. */
			u32 CNG_rx_detected:1;

			/*! The echocan disabled its NLP automatically.
			 */
			u32 NLP_auto_disabled:1;

			/*! The echocan enabled its NLP automatically.
			 */
			u32 NLP_auto_enabled:1;
		} bit;
	} events;
};

struct dahdi_chan {
#ifdef CONFIG_DAHDI_NET
	/*! \note Must be first */
	struct dahdi_hdlc *hdlcnetdev;
#endif
#ifdef CONFIG_DAHDI_PPP
	struct ppp_channel *ppp;
	struct tasklet_struct ppp_calls;
	int do_ppp_wakeup;
	int do_ppp_error;
	struct sk_buff_head ppp_rq;
#endif
#ifdef BUFFER_DEBUG
	int statcount;
	int lastnumbufs;
#endif
	spinlock_t lock;
	char name[40];
	/* Specified by DAHDI */
	/*! \brief DAHDI channel number */
	int channo;
	int chanpos;
	unsigned long flags;
	long rxp1;
	long rxp2;
	long rxp3;
	int txtone;
	int tx_v2;
	int tx_v3;
	int v1_1;
	int v2_1;
	int v3_1;
	int toneflags;
	struct sf_detect_state rd;

	struct dahdi_chan *master;	/*!< Our Master channel (could be us) */
	/*! \brief Next slave (if appropriate) */
	struct dahdi_chan *nextslave;

	u_char *writechunk;						/*!< Actual place to write to */
	u_char swritechunk[DAHDI_MAX_CHUNKSIZE];	/*!< Buffer to be written */
	u_char *readchunk;						/*!< Actual place to read from */
	u_char sreadchunk[DAHDI_MAX_CHUNKSIZE];	/*!< Preallocated static area */
	short *readchunkpreec;

	/* Channel from which to read when DACSed. */
	struct dahdi_chan *dacs_chan;

	/*! Pointer to tx and rx gain tables */
	const u_char *rxgain;
	const u_char *txgain;
	
	/* Specified by driver, readable by DAHDI */
	void *pvt;			/*!< Private channel data */
	struct file *file;	/*!< File structure */
	
	
#ifdef CONFIG_DAHDI_MIRROR
	struct dahdi_chan	*rxmirror;  /*!< channel we mirror reads to */
	struct dahdi_chan	*txmirror;  /*!< channel we mirror writes to */
	struct dahdi_chan	*srcmirror; /*!< channel we mirror from */
#endif /* CONFIG_DAHDI_MIRROR */
	struct dahdi_span	*span;			/*!< Span we're a member of */
	int		sig;			/*!< Signalling */
	int		sigcap;			/*!< Capability for signalling */
	__u32		chan_alarms;		/*!< alarms status */

	wait_queue_head_t waitq;

	/* Used only by DAHDI -- NO DRIVER SERVICEABLE PARTS BELOW */
	/* Buffer declarations */
	u_char		*readbuf[DAHDI_MAX_NUM_BUFS];	/*!< read buffer */
	int		inreadbuf;
	int		outreadbuf;

	u_char		*writebuf[DAHDI_MAX_NUM_BUFS]; /*!< write buffers */
	int		inwritebuf;
	int		outwritebuf;
	
	int		blocksize;	/*!< Block size */

	int		eventinidx;  /*!< out index in event buf (circular) */
	int		eventoutidx;  /*!< in index in event buf (circular) */
	unsigned int	eventbuf[DAHDI_MAX_EVENTSIZE];  /*!< event circ. buffer */
	
	int		readn[DAHDI_MAX_NUM_BUFS];  /*!< # of bytes ready in read buf */
	int		readidx[DAHDI_MAX_NUM_BUFS];  /*!< current read pointer */
	int		writen[DAHDI_MAX_NUM_BUFS];  /*!< # of bytes ready in write buf */
	int		writeidx[DAHDI_MAX_NUM_BUFS];  /*!< current write pointer */
	
	int		numbufs;			/*!< How many buffers in channel */
	int		txbufpolicy;			/*!< Buffer policy */
	int		rxbufpolicy;			/*!< Buffer policy */
	int		txdisable;				/*!< Disable transmitter */
	int 	rxdisable;				/*!< Disable receiver */
	
	
	/* Tone zone stuff */
	struct dahdi_zone *curzone;		/*!< Zone for selecting tones */
	struct dahdi_tone *curtone;		/*!< Current tone we're playing (if any) */
	int		tonep;					/*!< Current position in tone */
	struct dahdi_tone_state ts;		/*!< Tone state */

	/* Pulse dial stuff */
	int	pdialcount;			/*!< pulse dial count */

	/*! Ring cadence */
	int ringcadence[DAHDI_MAX_CADENCE];
	int firstcadencepos;				/*!< Where to restart ring cadence */

	/* Digit string dialing stuff */
	int		digitmode;			/*!< What kind of tones are we sending? */
	char	txdialbuf[DAHDI_MAX_DTMF_BUF];
	int 	dialing;
	int	afterdialingtimer;
	int		cadencepos;				/*!< Where in the cadence we are */

	/* I/O Mask */	
	unsigned int iomask;  /*! I/O Mux signal mask */
	
	/* HDLC state machines */
	struct fasthdlc_state txhdlc;
	struct fasthdlc_state rxhdlc;
	int infcs;

	/* Conferencing stuff */
	int		confna;	/*! conference number (alias) */
	int		_confn;	/*! Actual conference number */
	int		confmode;  /*! conference mode */
	int		confmute; /*! conference mute mode */
	struct dahdi_chan *conf_chan;

	/* Incoming and outgoing conference chunk queues for
	   communicating between DAHDI master time and
	   other boards */
	struct confq confin;
	struct confq confout;

	short	getlin[DAHDI_MAX_CHUNKSIZE];			/*!< Last transmitted samples */
	unsigned char getraw[DAHDI_MAX_CHUNKSIZE];		/*!< Last received raw data */
	short	getlin_lastchunk[DAHDI_MAX_CHUNKSIZE];	/*!< Last transmitted samples from last chunk */
	short	putlin[DAHDI_MAX_CHUNKSIZE];			/*!< Last received samples */
	unsigned char putraw[DAHDI_MAX_CHUNKSIZE];		/*!< Last received raw data */
	short	conflast[DAHDI_MAX_CHUNKSIZE];			/*!< Last conference sample -- base part of channel */
	short	conflast1[DAHDI_MAX_CHUNKSIZE];		/*!< Last conference sample  -- pseudo part of channel */
	short	conflast2[DAHDI_MAX_CHUNKSIZE];		/*!< Previous last conference sample -- pseudo part of channel */


	/*! The echo canceler module that should be used to create an
	   instance when this channel needs one */
	const struct dahdi_echocan_factory *ec_factory;
	/*! The echo canceler module that owns the instance currently
	   on this channel, if one is present */
	const struct dahdi_echocan_factory *ec_current;
	/*! The state data of the echo canceler instance in use */
	struct dahdi_echocan_state *ec_state;

	/* RBS timings  */
	int		prewinktime;  /*!< pre-wink time (ms) */
	int		preflashtime;	/*!< pre-flash time (ms) */
	int		winktime;  /*!< wink time (ms) */
	int		flashtime;  /*!< flash time (ms) */
	int		starttime;  /*!< start time (ms) */
	int		rxwinktime;  /*!< rx wink time (ms) */
	int		rxflashtime; /*!< rx flash time (ms) */
	int		debouncetime;  /*!< FXS GS sig debounce time (ms) */
	int		pulsebreaktime; /*!< pulse line open time (ms) */
	int		pulsemaketime;  /*!< pulse line closed time (ms) */
	int		pulseaftertime; /*!< pulse time between digits (ms) */

	/*! RING debounce timer */
	int	ringdebtimer;
	
	/*! RING trailing detector to make sure a RING is really over */
	int ringtrailer;

	/* PULSE digit receiver stuff */
	int	pulsecount;
	int	pulsetimer;

	/* RBS timers */
	int 	itimerset;		/*!< what the itimer was set to last */
	int 	itimer;
	int 	otimer;
	
	/* RBS state */
	int gotgs;
	int txstate;
	int rxsig;
	int txsig;
	int rxsigstate;

	/* non-RBS rx state */
	int rxhooksig;
	int txhooksig;
	int kewlonhook;

	/*! Idle signalling if CAS signalling */
	int idlebits;

	int deflaw;		/*! 1 = mulaw, 2=alaw, 0=undefined */
	short *xlaw;
#ifdef	OPTIMIZE_CHANMUTE
	int chanmute;		/*!< no need for PCM data */
#endif
#ifdef CONFIG_CALC_XLAW
	unsigned char (*lineartoxlaw)(short a);
#else
	unsigned char *lin2x;
#endif
};

#ifdef CONFIG_DAHDI_NET
struct dahdi_hdlc {
	struct net_device *netdev;
	struct dahdi_chan *chan;
};
#endif

/*! Define the maximum block size */
#define DAHDI_MAX_BLOCKSIZE	8192


#define DAHDI_DEFAULT_WINKTIME	150	/*!< 150 ms default wink time */
#define DAHDI_DEFAULT_FLASHTIME	750	/*!< 750 ms default flash time */

#define DAHDI_DEFAULT_PREWINKTIME	50	/*!< 50 ms before wink */
#define DAHDI_DEFAULT_PREFLASHTIME 50	/*!< 50 ms before flash */
#define DAHDI_DEFAULT_STARTTIME 1500	/*!< 1500 ms of start */
#define DAHDI_DEFAULT_RINGTIME 2000	/*!< 2000 ms of ring on (start, FXO) */
#if 0
#define DAHDI_DEFAULT_RXWINKTIME 250	/*!< 250ms longest rx wink */
#endif
#define DAHDI_DEFAULT_RXWINKTIME 300	/*!< 300ms longest rx wink (to work with the Atlas) */
#define DAHDI_DEFAULT_RXFLASHTIME 1250	/*!< 1250ms longest rx flash */
#define DAHDI_DEFAULT_DEBOUNCETIME 600	/*!< 600ms of FXS GS signalling debounce */
#define DAHDI_DEFAULT_PULSEMAKETIME 50	/*!< 50 ms of line closed when dial pulsing */
#define DAHDI_DEFAULT_PULSEBREAKTIME 50	/*!< 50 ms of line open when dial pulsing */
#define DAHDI_DEFAULT_PULSEAFTERTIME 750	/*!< 750ms between dial pulse digits */

#define DAHDI_MINPULSETIME (15 * 8)	/*!< 15 ms minimum */

#ifdef SHORT_FLASH_TIME
#define DAHDI_MAXPULSETIME (80 * 8)	/*!< we need 80 ms, not 200ms, as we have a short flash */
#else
#define DAHDI_MAXPULSETIME (200 * 8)	/*!< 200 ms maximum */
#endif

#define DAHDI_PULSETIMEOUT ((DAHDI_MAXPULSETIME / 8) + 50)

#define DAHDI_RINGTRAILER (50 * 8)	/*!< Don't consider a ring "over" until it's been gone at least this
									   much time */

#define DAHDI_LOOPCODE_TIME 10000		/*!< send loop codes for 10 secs */
#define DAHDI_ALARMSETTLE_TIME	5000	/*!< allow alarms to settle for 5 secs */
#define DAHDI_AFTERSTART_TIME 500		/*!< 500ms after start */

#define DAHDI_RINGOFFTIME 4000		/*!< Turn off ringer for 4000 ms */
#define DAHDI_KEWLTIME 500		/*!< 500ms for kewl pulse */
#define DAHDI_AFTERKEWLTIME 300    /*!< 300ms after kewl pulse */

#define DAHDI_MAX_PRETRAINING   1000	/*!< 1000ms max pretraining time */

#ifdef	FXSFLASH
#define DAHDI_FXSFLASHMINTIME	450	/*!< min 450ms */
#define DAHDI_FXSFLASHMAXTIME	550	/*!< max 550ms */
#endif


struct dahdi_chardev {
	const char *name;
	__u8 minor;
};

int dahdi_register_chardev(struct dahdi_chardev *dev);
int dahdi_unregister_chardev(struct dahdi_chardev *dev);

/*! \brief defines for transmit signalling */
enum dahdi_txsig {
	DAHDI_TXSIG_ONHOOK,  /*!< On hook */
	DAHDI_TXSIG_OFFHOOK, /*!< Off hook */
	DAHDI_TXSIG_START,   /*!< Start / Ring */
	DAHDI_TXSIG_KEWL,     /*!< Drop battery if possible */
	/*! Leave this as the last entry */
	DAHDI_TXSIG_TOTAL,
};

enum dahdi_rxsig {
	DAHDI_RXSIG_ONHOOK,
	DAHDI_RXSIG_OFFHOOK,
	DAHDI_RXSIG_START,
	DAHDI_RXSIG_RING,
	DAHDI_RXSIG_INITIAL
};
	
enum {
	/* Span flags */
	DAHDI_FLAGBIT_REGISTERED= 0,
	DAHDI_FLAGBIT_RUNNING	= 1,
	DAHDI_FLAGBIT_RBS	= 12,	/*!< Span uses RBS signalling */

	/* Channel flags */
	DAHDI_FLAGBIT_DTMFDECODE= 2,	/*!< Channel supports native DTMF decode */
	DAHDI_FLAGBIT_MFDECODE	= 3,	/*!< Channel supports native MFr2 decode */
	DAHDI_FLAGBIT_ECHOCANCEL= 4,	/*!< Channel supports native echo cancellation */
	DAHDI_FLAGBIT_HDLC	= 5,	/*!< Perform HDLC */
#ifdef CONFIG_DAHDI_NET
	DAHDI_FLAGBIT_NETDEV	= 6,	/*!< Send to network */
#endif
	DAHDI_FLAGBIT_CLEAR	= 8,	/*!< Clear channel */
	DAHDI_FLAGBIT_AUDIO	= 9,	/*!< Audio mode channel */
	DAHDI_FLAGBIT_OPEN	= 10,	/*!< Channel is open */
	DAHDI_FLAGBIT_FCS	= 11,	/*!< Calculate FCS */
	/* Reserve 12 for uniqueness with span flags */
	DAHDI_FLAGBIT_LINEAR	= 13,	/*!< Talk to user space in linear */
	DAHDI_FLAGBIT_PPP	= 14,	/*!< PPP is available */
	DAHDI_FLAGBIT_T1PPP	= 15,
	DAHDI_FLAGBIT_SIGFREEZE	= 16,	/*!< Freeze signalling */
	DAHDI_FLAGBIT_NOSTDTXRX	= 17,	/*!< Do NOT do standard transmit and receive on every interrupt */
	DAHDI_FLAGBIT_LOOPED	= 18,	/*!< Loopback the receive data from the channel to the transmit */
	DAHDI_FLAGBIT_MTP2	= 19,	/*!< Repeats last message in buffer and also discards repeating messages sent to us */
	DAHDI_FLAGBIT_HDLC56	= 20,	/*!< Sets the given channel (if in HDLC mode) to use 56K HDLC instead of 64K  */
	DAHDI_FLAGBIT_BUFEVENTS	= 21,	/*!< Report buffer events */
	DAHDI_FLAGBIT_TXUNDERRUN = 22,	/*!< Transmit underrun condition */
	DAHDI_FLAGBIT_RXOVERRUN = 23,	/*!< Receive overrun condition */
	DAHDI_FLAGBIT_DEVFILE	= 25,	/*!< Channel has a sysfs dev file */
};

#ifdef CONFIG_DAHDI_NET
/**
 * have_netdev() - Return true if a channel has an associated network device.
 * @chan:	   Then channel to check.
 *
 */
static inline int dahdi_have_netdev(const struct dahdi_chan *chan)
{
	return test_bit(DAHDI_FLAGBIT_NETDEV, &chan->flags);
}
#else
static inline int dahdi_have_netdev(const struct dahdi_chan *chan) { return 0; }
#endif

struct dahdi_count {
	__u32 fe;		/*!< Framing error counter */
	__u32 cv;		/*!< Coding violations counter */
	__u32 bpv;		/*!< Bipolar Violation counter */
	__u32 crc4;		/*!< CRC4 error counter */
	__u32 ebit;		/*!< current E-bit error count */
	__u32 fas;		/*!< current FAS error count */
	__u32 be;		/*!< current bit error count */
	__u32 prbs;		/*!< current PRBS detected pattern */
	__u32 errsec;		/*!< errored seconds */
};

/* map flagbits to flag masks */
#define	DAHDI_FLAG(x)	(1 << (DAHDI_FLAGBIT_ ## x))

/*! This is a redefinition of the flags from above to allow use of the 
 * legacy drivers that do not use the kernel atomic bit testing and 
 * changing routines.
 * 
 * See the above descriptions for DAHDI_FLAGBIT_....  for documentation 
 * about function. */
/* Span flags */
#define DAHDI_FLAG_REGISTERED	DAHDI_FLAG(REGISTERED)
#define DAHDI_FLAG_RUNNING	DAHDI_FLAG(RUNNING)
#define DAHDI_FLAG_RBS		DAHDI_FLAG(RBS)

/* Channel flags */
#define DAHDI_FLAG_DTMFDECODE	DAHDI_FLAG(DTMFDECODE)
#define DAHDI_FLAG_MFDECODE	DAHDI_FLAG(MFDECODE)
#define DAHDI_FLAG_ECHOCANCEL	DAHDI_FLAG(ECHOCANCEL)

#define DAHDI_FLAG_HDLC		DAHDI_FLAG(HDLC)
/* #define DAHDI_FLAG_NETDEV	DAHDI_FLAG(NETDEV) */
#define DAHDI_FLAG_CLEAR	DAHDI_FLAG(CLEAR)
#define DAHDI_FLAG_AUDIO	DAHDI_FLAG(AUDIO)

#define DAHDI_FLAG_OPEN		DAHDI_FLAG(OPEN)
#define DAHDI_FLAG_FCS		DAHDI_FLAG(FCS)
/* Reserve 12 for uniqueness with span flags */
#define DAHDI_FLAG_LINEAR	DAHDI_FLAG(LINEAR)
#define DAHDI_FLAG_PPP		DAHDI_FLAG(PPP)
#define DAHDI_FLAG_T1PPP	DAHDI_FLAG(T1PPP)
#define DAHDI_FLAG_SIGFREEZE	DAHDI_FLAG(SIGFREEZE)
#define DAHDI_FLAG_NOSTDTXRX	DAHDI_FLAG(NOSTDTXRX)
#define DAHDI_FLAG_LOOPED	DAHDI_FLAG(LOOPED)
#define DAHDI_FLAG_MTP2		DAHDI_FLAG(MTP2)
#define DAHDI_FLAG_HDLC56	DAHDI_FLAG(HDLC56)
#define DAHDI_FLAG_BUFEVENTS	DAHDI_FLAG(BUFEVENTS)
#define DAHDI_FLAG_TXUNDERRUN	DAHDI_FLAG(TXUNDERRUN)
#define DAHDI_FLAG_RXOVERRUN	DAHDI_FLAG(RXOVERRUN)

struct file;

struct dahdi_span_ops {
	struct module *owner;		/*!< Which module is exporting this span. */

	/*   ==== Span Callback Operations ====   */
	/*! Req: Set the requested chunk size.  This is the unit in which you must
	   report results for conferencing, etc */
	int (*setchunksize)(struct dahdi_span *span, int chunksize);

	/*! Opt: Configure the span (if appropriate) */
	int (*spanconfig)(struct file *file, struct dahdi_span *span,
			  struct dahdi_lineconfig *lc);
	
	/*! Opt: Start the span */
	int (*startup)(struct file *file, struct dahdi_span *span);
	
	/*! Opt: Shutdown the span */
	int (*shutdown)(struct dahdi_span *span);
	
	/*! Opt: Enable maintenance modes */
	int (*maint)(struct dahdi_span *span, int mode);

#ifdef	DAHDI_SYNC_TICK
	/*! Opt: send sync to spans. Called in hard_irq context with chan_lock
	 *       held.*/
	void (*sync_tick)(struct dahdi_span *span, int is_master);
#endif
	/* ====  Channel Callback Operations ==== */
	/*! Opt: Set signalling type (if appropriate) */
	int (*chanconfig)(struct file *file, struct dahdi_chan *chan,
			  int sigtype);

	/*! Opt: Prepare a channel for I/O */
	int (*open)(struct dahdi_chan *chan);

	/*! Opt: Close channel for I/O */
	int (*close)(struct dahdi_chan *chan);
	
	/*! Opt: IOCTL */
	int (*ioctl)(struct dahdi_chan *chan, unsigned int cmd, unsigned long data);
	
	/* Okay, now we get to the signalling.  You have several options: */

	/* Option 1: If you're a T1 like interface, you can just provide a
	   rbsbits function and we'll assert robbed bits for you.  Be sure to 
	   set the DAHDI_FLAG_RBS in this case.  */

	/*! Opt: If the span uses A/B bits, set them here */
	int (*rbsbits)(struct dahdi_chan *chan, int bits);
	
	/*! Option 2: If you don't know about sig bits, but do have their
	   equivalents (i.e. you can disconnect battery, detect off hook,
	   generate ring, etc directly) then you can just specify a
	   sethook function, and we'll call you with appropriate hook states
	   to set.  Still set the DAHDI_FLAG_RBS in this case as well */
	int (*hooksig)(struct dahdi_chan *chan, enum dahdi_txsig hookstate);
	
	/*! Option 3: If you can't use sig bits, you can write a function
	   which handles the individual hook states  */
	int (*sethook)(struct dahdi_chan *chan, int hookstate);
	
	/*! Opt: Used to tell an onboard HDLC controller that there is data ready to transmit */
	void (*hdlc_hard_xmit)(struct dahdi_chan *chan);

	/*! If the watchdog detects no received data, it will call the
	   watchdog routine */
	int (*watchdog)(struct dahdi_span *span, int cause);

#ifdef	DAHDI_AUDIO_NOTIFY
	/*! Opt: audio is used, don't optimize out */
	int (*audio_notify)(struct dahdi_chan *chan, int yes);
#endif

	/*! Opt: Enable preechocan stream from inline HW echocanceler. */
	int (*enable_hw_preechocan)(struct dahdi_chan *chan);

	/*! Opt: Disable preechocan stream from inline HW echocanceler. */
	void (*disable_hw_preechocan)(struct dahdi_chan *chan);

	/*! Opt: Dacs the contents of chan2 into chan1 if possible */
	int (*dacs)(struct dahdi_chan *chan1, struct dahdi_chan *chan2);

	/*! Opt: Provide echo cancellation on a channel */
	int (*echocan_create)(struct dahdi_chan *chan,
			      struct dahdi_echocanparams *ecp,
			      struct dahdi_echocanparam *p,
			      struct dahdi_echocan_state **ec);

	/*! Opt: Provide the name of the echo canceller on a channel */
	const char *(*echocan_name)(const struct dahdi_chan *chan);

	/*! When using "pinned_spans", this function is called back when this
	 * span has been assigned with the system. */
	void (*assigned)(struct dahdi_span *span);

	/*! Called when the spantype / linemode is changed before the span is
	 * assigned a number. */
	int (*set_spantype)(struct dahdi_span *span, const char *spantype);
};

/**
 * dahdi_device - Represents a device that can contain one or more spans.
 *
 * @spans:        List of child spans.
 * @manufacturer: Device manufacturer.
 * @location:     The location of this device. This should not change if
 *                the device is replaced (e.g: in the same PCI slot)
 * @hardware_id:  The hardware_id of this device (NULL for devices without
 *                a hardware_id). This should not change if the device is
 *                relocated to a different location (e.g: different PCI slot)
 * @devicetype:   What type of device this is.
 * @irqmisses:    Count of "interrupt misses" for this device.
 *
 */
struct dahdi_device {
	struct list_head spans;
	const char *manufacturer;
	const char *location;
	const char *hardware_id;
	const char *devicetype;
	struct device dev;
	unsigned int irqmisses;
};

struct dahdi_span {
	spinlock_t lock;
	char name[40];			/*!< Span name */
	char desc[80];			/*!< Span description */
	const char *spantype;		/*!< span type in text form */
	int deflaw;			/*!< Default law (DAHDI_MULAW or DAHDI_ALAW) */
	int alarms;			/*!< Pending alarms on span */
	unsigned long flags;
	u8 cannot_provide_timing:1;
	int lbo;			/*!< Span Line-Buildout */
	int lineconfig;			/*!< Span line configuration */
	int linecompat;			/*!< Span line compatibility (0 for
					     analog spans)*/
	int channels;			/*!< Number of channels in span */
	int txlevel;			/*!< Tx level */
	int rxlevel;			/*!< Rx level */
	int syncsrc;			/*!< current sync src (gets copied here) */
	struct dahdi_count count;	/*!< Performance and Error counters */

	int maintstat;			/*!< Maintenance state */
	int mainttimer;			/*!< Maintenance timer */

	int timingslips;		/*!< Clock slips */

	struct dahdi_chan **chans;	/*!< Member channel structures */

	const struct dahdi_span_ops *ops;	/*!< span callbacks. */

	/* Used by DAHDI only -- no user servicable parts inside */
	int spanno;			/*!< Span number for DAHDI */
	int offset;			/*!< Offset within a given card */
	int lastalarms;			/*!< Previous alarms */

#ifdef CONFIG_DAHDI_WATCHDOG
	int watchcounter;
	int watchstate;
#endif	

#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_entry;
#endif
	struct list_head spans_node;

	struct dahdi_device *parent;
	struct list_head device_node;
	struct device *span_device;
};

struct dahdi_transcoder_channel {
	void *pvt;
	struct dahdi_transcoder *parent;
	wait_queue_head_t ready;
	__u32 built_fmts;
#define DAHDI_TC_FLAG_BUSY		1
#define DAHDI_TC_FLAG_CHAN_BUILT	2
#define DAHDI_TC_FLAG_NONBLOCK		3
#define DAHDI_TC_FLAG_DATA_WAITING	4
	unsigned long flags;
	u32 dstfmt;
	u32 srcfmt;
};

int dahdi_is_sync_master(const struct dahdi_span *span);

static inline int 
dahdi_tc_is_built(struct dahdi_transcoder_channel *dtc) {
	return test_bit(DAHDI_TC_FLAG_CHAN_BUILT, &dtc->flags);
}
static inline void
dahdi_tc_set_built(struct dahdi_transcoder_channel *dtc) {
	set_bit(DAHDI_TC_FLAG_CHAN_BUILT, &dtc->flags);
}
static inline void 
dahdi_tc_clear_built(struct dahdi_transcoder_channel *dtc) {
	clear_bit(DAHDI_TC_FLAG_CHAN_BUILT, &dtc->flags);
}
static inline int 
dahdi_tc_is_nonblock(struct dahdi_transcoder_channel *dtc) {
	return test_bit(DAHDI_TC_FLAG_NONBLOCK, &dtc->flags);
}
static inline void 
dahdi_tc_set_nonblock(struct dahdi_transcoder_channel *dtc) {
	set_bit(DAHDI_TC_FLAG_NONBLOCK, &dtc->flags);
}
static inline void 
dahdi_tc_clear_nonblock(struct dahdi_transcoder_channel *dtc) {
	clear_bit(DAHDI_TC_FLAG_NONBLOCK, &dtc->flags);
}
static inline int 
dahdi_tc_is_data_waiting(struct dahdi_transcoder_channel *dtc) {
	return test_bit(DAHDI_TC_FLAG_DATA_WAITING, &dtc->flags);
}
static inline int 
dahdi_tc_is_busy(struct dahdi_transcoder_channel *dtc) {
	return test_bit(DAHDI_TC_FLAG_BUSY, &dtc->flags);
}
static inline void 
dahdi_tc_set_busy(struct dahdi_transcoder_channel *dtc) {
	set_bit(DAHDI_TC_FLAG_BUSY, &dtc->flags);
}
static inline void 
dahdi_tc_clear_busy(struct dahdi_transcoder_channel *dtc) {
	clear_bit(DAHDI_TC_FLAG_BUSY, &dtc->flags);
}
static inline void 
dahdi_tc_set_data_waiting(struct dahdi_transcoder_channel *dtc) {
	set_bit(DAHDI_TC_FLAG_DATA_WAITING, &dtc->flags);
}
static inline void 
dahdi_tc_clear_data_waiting(struct dahdi_transcoder_channel *dtc) {
	clear_bit(DAHDI_TC_FLAG_DATA_WAITING, &dtc->flags);
}

struct dahdi_transcoder {
	struct list_head active_list_node;
	struct list_head registration_list_node;
	char name[80];
	int numchannels;
	unsigned int srcfmts;
	unsigned int dstfmts;
	struct file_operations fops;
	int (*allocate)(struct dahdi_transcoder_channel *channel);
	int (*release)(struct dahdi_transcoder_channel *channel);
	/* Transcoder channels */
	struct dahdi_transcoder_channel channels[0];
};

#define DAHDI_WATCHDOG_NOINTS		(1 << 0)

#define DAHDI_WATCHDOG_INIT			1000

#define DAHDI_WATCHSTATE_UNKNOWN		0
#define DAHDI_WATCHSTATE_OK			1
#define DAHDI_WATCHSTATE_RECOVERING	2
#define DAHDI_WATCHSTATE_FAILED		3


struct dahdi_dynamic {
	char addr[40];
	char dname[20];
	int err;
	struct kref kref;
	long rxjif;
	unsigned short txcnt;
	unsigned short rxcnt;
	struct dahdi_device *ddev;
	struct dahdi_span span;
	struct dahdi_chan *chans[256];
	struct dahdi_dynamic_driver *driver;
	void *pvt;
	int timing;
	int master;
	unsigned char *msgbuf;
	struct device *dev;

	struct list_head list;
};

struct dahdi_dynamic_driver {
	/*! Driver name (e.g. Eth) */
	const char *name;

	/*! Driver description */
	const char *desc;

	/*! Create a new transmission pipe */
	int (*create)(struct dahdi_dynamic *d, const char *address);

	/*! Destroy a created transmission pipe */
	void (*destroy)(struct dahdi_dynamic *d);

	/*! Transmit a given message */
	void (*transmit)(struct dahdi_dynamic *d, u8 *msg, size_t msglen);

	/*! Flush any pending messages */
	int (*flush)(void);

	struct list_head list;
	struct module *owner;

	/*! Numberic id of next device created by this driver. */
	unsigned int id;
};

/*! \brief Receive a dynamic span message */
void dahdi_dynamic_receive(struct dahdi_span *span, unsigned char *msg, int msglen);

/*! \brief Register a dynamic driver */
int dahdi_dynamic_register_driver(struct dahdi_dynamic_driver *driver);

/*! \brief Unregister a dynamic driver */
void dahdi_dynamic_unregister_driver(struct dahdi_dynamic_driver *driver);

int _dahdi_receive(struct dahdi_span *span);

/*! Receive on a span.  The DAHDI interface will handle all the calculations for
   all member channels of the span, pulling the data from the readchunk buffer */
static inline int dahdi_receive(struct dahdi_span *span)
{
	unsigned long flags;
	int ret;
	local_irq_save(flags);
	ret = _dahdi_receive(span);
	local_irq_restore(flags);
	return ret;
}

int _dahdi_transmit(struct dahdi_span *span);

/*! Prepare writechunk buffers on all channels for this span */
static inline int dahdi_transmit(struct dahdi_span *span)
{
	unsigned long flags;
	int ret;
	local_irq_save(flags);
	ret = _dahdi_transmit(span);
	local_irq_restore(flags);
	return ret;
}

static inline int dahdi_is_digital_span(const struct dahdi_span *s)
{
	return (s->linecompat > 0);
}

static inline int dahdi_is_t1_span(const struct dahdi_span *s)
{
	return (s->linecompat & (DAHDI_CONFIG_D4 | DAHDI_CONFIG_ESF |
				 DAHDI_CONFIG_B8ZS)) > 0;
}

static inline int dahdi_is_e1_span(const struct dahdi_span *s)
{
	return dahdi_is_digital_span(s) && !dahdi_is_t1_span(s);
}

/*! Abort the buffer currently being receive with event "event" */
void dahdi_hdlc_abort(struct dahdi_chan *ss, int event);

/*! Indicate to DAHDI that the end of frame was received and rotate buffers */
void dahdi_hdlc_finish(struct dahdi_chan *ss);

/*! Put a chunk of data into the current receive buffer */
void dahdi_hdlc_putbuf(struct dahdi_chan *ss, unsigned char *rxb, int bytes);

/*! Get a chunk of data from the current transmit buffer.  Returns -1 if no data
 * is left to send, 0 if there is data remaining in the current message to be sent
 * and 1 if the currently transmitted message is now done */
int dahdi_hdlc_getbuf(struct dahdi_chan *ss, unsigned char *bufptr, unsigned int *size);

/*! Register a device.  Returns 0 on success, -1 on failure. */
struct dahdi_device *dahdi_create_device(void);
int dahdi_register_device(struct dahdi_device *ddev, struct device *parent);
void dahdi_unregister_device(struct dahdi_device *ddev);
void dahdi_free_device(struct dahdi_device *ddev);
void dahdi_init_span(struct dahdi_span *span);

/*! Allocate / free memory for a transcoder */
struct dahdi_transcoder *dahdi_transcoder_alloc(int numchans);
void dahdi_transcoder_free(struct dahdi_transcoder *ztc);

/*! \brief Register a transcoder */
int dahdi_transcoder_register(struct dahdi_transcoder *tc);

/*! \brief Unregister a transcoder */
int dahdi_transcoder_unregister(struct dahdi_transcoder *tc);

/*! \brief Alert a transcoder */
int dahdi_transcoder_alert(struct dahdi_transcoder_channel *ztc);

/*! \brief Gives a name to an LBO */
const char *dahdi_lboname(int lbo);

/*! \brief Tell DAHDI about changes in received rbs bits */
void dahdi_rbsbits(struct dahdi_chan *chan, int bits);

/*! \brief Tell DAHDI abou changes in received signalling */
void dahdi_hooksig(struct dahdi_chan *chan, enum dahdi_rxsig rxsig);

/*! \brief Queue an event on a channel */
void dahdi_qevent_nolock(struct dahdi_chan *chan, int event);

/*! \brief Queue an event on a channel, locking it first */
void dahdi_qevent_lock(struct dahdi_chan *chan, int event);

/*! \brief Notify a change possible change in alarm status on a channel */
void dahdi_alarm_channel(struct dahdi_chan *chan, int alarms);

/*! \brief Notify a change possible change in alarm status on a span */
void dahdi_alarm_notify(struct dahdi_span *span);

/*! \brief Initialize a tone state */
void dahdi_init_tone_state(struct dahdi_tone_state *ts, struct dahdi_tone *zt);

/*! \brief Get a given MF tone struct, suitable for dahdi_tone_nextsample. */
struct dahdi_tone *dahdi_mf_tone(const struct dahdi_chan *chan, char digit, int digitmode);

/* Echo cancel a receive and transmit chunk for a given channel.  This
   should be called by the low-level driver as close to the interface
   as possible.  ECHO CANCELLATION IS NO LONGER AUTOMATICALLY DONE
   AT THE DAHDI LEVEL.  dahdi_ec_chunk will not echo cancel if it should
   not be doing so.  rxchunk is modified in-place */
void __dahdi_ec_chunk(struct dahdi_chan *ss, u8 *rxchunk,
		      const u8 *preecchunk, const u8 *txchunk);

static inline void _dahdi_ec_chunk(struct dahdi_chan *chan,
				   u8 *rxchunk, const u8 *txchunk)
{
	__dahdi_ec_chunk(chan, rxchunk, rxchunk, txchunk);
}

static inline void dahdi_ec_chunk(struct dahdi_chan *ss, unsigned char *rxchunk,
				  const unsigned char *txchunk)
{
	unsigned long flags;
	local_irq_save(flags);
	_dahdi_ec_chunk(ss, rxchunk, txchunk);
	local_irq_restore(flags);
}

void _dahdi_ec_span(struct dahdi_span *span);
static inline void dahdi_ec_span(struct dahdi_span *span)
{
	unsigned long flags;
	local_irq_save(flags);
	_dahdi_ec_span(span);
	local_irq_restore(flags);
}

extern struct file_operations *dahdi_transcode_fops;

/* Don't use these directly -- they're not guaranteed to
   be there. */
extern short __dahdi_mulaw[256];
extern short __dahdi_alaw[256];
#ifdef CONFIG_CALC_XLAW
u_char __dahdi_lineartoulaw(short a);
u_char __dahdi_lineartoalaw(short a);
#else
extern u_char __dahdi_lin2mu[16384];
extern u_char __dahdi_lin2a[16384];
#endif

struct dahdi_dynamic_ops {
	struct module *owner;
	int (*ioctl)(unsigned int cmd, unsigned long data);
};

/*! \brief Used by dynamic DAHDI -- don't use directly */
void dahdi_set_dynamic_ops(const struct dahdi_dynamic_ops *ops);

/*! \brief Used by DAHDI HPEC module -- don't use directly */
void dahdi_set_hpec_ioctl(int (*func)(unsigned int cmd, unsigned long data));

/*! \brief Used privately by DAHDI.  Avoid touching directly */
struct dahdi_tone {
	int fac1;
	int init_v2_1;
	int init_v3_1;

	int fac2;
	int init_v2_2;
	int init_v3_2;

	int tonesamples;		/*!< How long to play this tone before 
					   going to the next (in samples) */
	struct dahdi_tone *next;		/* Next tone in this sequence */

	int modulate;
};

static inline short dahdi_tone_nextsample(struct dahdi_tone_state *ts, struct dahdi_tone *zt)
{
	/* follow the curves, return the sum */

	int p;

	ts->v1_1 = ts->v2_1;
	ts->v2_1 = ts->v3_1;
	ts->v3_1 = (zt->fac1 * ts->v2_1 >> 15) - ts->v1_1;

	ts->v1_2 = ts->v2_2;
	ts->v2_2 = ts->v3_2;
	ts->v3_2 = (zt->fac2 * ts->v2_2 >> 15) - ts->v1_2;

	/* Return top 16 bits */
	if (!ts->modulate) return ts->v3_1 + ts->v3_2;
	/* we are modulating */
	p = ts->v3_2 - 32768;
	if (p < 0) p = -p;
	p = ((p * 9) / 10) + 1;
	return (ts->v3_1 * p) >> 15;

}

static inline short dahdi_txtone_nextsample(struct dahdi_chan *ss)
{
	/* follow the curves, return the sum */

	ss->v1_1 = ss->v2_1;
	ss->v2_1 = ss->v3_1;
	ss->v3_1 = (ss->txtone * ss->v2_1 >> 15) - ss->v1_1;
	return ss->v3_1;
}

/* These are the right functions to use.  */

#define DAHDI_MULAW(a) (__dahdi_mulaw[(a)])
#define DAHDI_ALAW(a) (__dahdi_alaw[(a)])
#define DAHDI_XLAW(a,c) (c->xlaw[(a)])

#ifdef CONFIG_CALC_XLAW
#define DAHDI_LIN2MU(a) (__dahdi_lineartoulaw((a)))
#define DAHDI_LIN2A(a) (__dahdi_lineartoalaw((a)))

#define DAHDI_LIN2X(a,c) ((c)->lineartoxlaw((a)))

#else
/* Use tables */
#define DAHDI_LIN2MU(a) (__dahdi_lin2mu[((unsigned short)(a)) >> 2])
#define DAHDI_LIN2A(a) (__dahdi_lin2a[((unsigned short)(a)) >> 2])

/* Manipulate as appropriate for x-law */
#define DAHDI_LIN2X(a,c) ((c)->lin2x[((unsigned short)(a)) >> 2])

#endif /* CONFIG_CALC_XLAW */

/* Data formats for capabilities and frames alike (from Asterisk) */
/*! G.723.1 compression */
#define DAHDI_FORMAT_G723_1	(1 << 0)
/*! GSM compression */
#define DAHDI_FORMAT_GSM		(1 << 1)
/*! Raw mu-law data (G.711) */
#define DAHDI_FORMAT_ULAW		(1 << 2)
/*! Raw A-law data (G.711) */
#define DAHDI_FORMAT_ALAW		(1 << 3)
/*! ADPCM (G.726, 32kbps) */
#define DAHDI_FORMAT_G726		(1 << 4)
/*! ADPCM (IMA) */
#define DAHDI_FORMAT_ADPCM		(1 << 5)
/*! Raw 16-bit Signed Linear (8000 Hz) PCM */
#define DAHDI_FORMAT_SLINEAR	(1 << 6)
/*! LPC10, 180 samples/frame */
#define DAHDI_FORMAT_LPC10		(1 << 7)
/*! G.729A audio */
#define DAHDI_FORMAT_G729A		(1 << 8)
/*! SpeeX Free Compression */
#define DAHDI_FORMAT_SPEEX		(1 << 9)
/*! iLBC Free Compression */
#define DAHDI_FORMAT_ILBC		(1 << 10)
/*! Maximum audio format */
#define DAHDI_FORMAT_MAX_AUDIO	(1 << 15)
/*! Maximum audio mask */
#define DAHDI_FORMAT_AUDIO_MASK	((1 << 16) - 1)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
#define KERN_CONT ""
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#ifndef clamp
#define clamp(x, low, high) min(max(low, x), high)
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)

/* Some distributions backported fatal_signal_pending so we'll use a macro to
 * override the inline function definition. */
#define fatal_signal_pending(p) \
	(signal_pending((p)) && sigismember(&(p)->pending.signal, SIGKILL))

#ifdef CONFIG_PCI
#ifndef PCIE_LINK_STATE_L0S
#define PCIE_LINK_STATE_L0S	1
#define PCIE_LINK_STATE_L1	2
#define PCIE_LINK_STATE_CLKPM	4
#endif
#define pci_disable_link_state dahdi_pci_disable_link_state
void dahdi_pci_disable_link_state(struct pci_dev *pdev, int state);
#endif /* CONFIG_PCI */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)

#ifndef __packed                                                                                         
#define __packed  __attribute__((packed))                                                                
#endif 

#include <linux/ctype.h>
/* A define of 'clamp_val' happened to be added in the patch
 * linux-2.6-sata-prep-work-for-rhel5-3.patch kernel-2.6.spec that also
 * backported support for strcasecmp to some later RHEL/Centos kernels.
 * If you have an older kernel that breaks because strcasecmp is already
 * defined, somebody out-smarted us. In that case, replace the line below
 * with '#if 0' to get the code building, and file a bug report at
 * https://issues.asterisk.org/ .
 */
#ifndef clamp_val
static inline int strcasecmp(const char *s1, const char *s2)
{
	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 == c2 && c1 != 0);
	return c1 - c2;
}
#endif /* clamp_val */
#endif /* 2.6.22 */
#endif /* 2.6.25 */
#endif /* 2.6.26 */
#endif /* 2.6.31 */

#ifndef CONFIG_TRACING
#define trace_printk printk
#endif

#ifndef DEFINE_SPINLOCK
#define DEFINE_SPINLOCK(x)      spinlock_t x = SPIN_LOCK_UNLOCKED
#endif

#ifndef DEFINE_SEMAPHORE
#define DEFINE_SEMAPHORE(name) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)
#endif

#ifndef DEFINE_MUTEX
struct mutex {
	struct semaphore sem;
};
#define DEFINE_MUTEX(name)					\
	struct mutex name = {					\
		.sem = __SEMAPHORE_INITIALIZER((name).sem, 1),	\
	}
#define mutex_lock(_x) down(&(_x)->sem)
#define mutex_unlock(_x) up(&(_x)->sem)
#define mutex_init(_x) sema_init(&(_x)->sem, 1)
#endif

#ifndef DEFINE_PCI_DEVICE_TABLE
#define DEFINE_PCI_DEVICE_TABLE(_x) \
	const struct pci_device_id _x[] __devinitdata
#endif

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#endif

/* WARN_ONCE first showed up in the kernel in 2.6.27 but it may have been
 * backported. */
#ifndef WARN_ONCE
#define WARN_ONCE(condition, format...) WARN_ON_ONCE(condition)
#endif

#define	DAHDI_CTL	0
#define	DAHDI_TRANSCODE	250
#define	DAHDI_TIMER	253
#define	DAHDI_CHANNEL	254
#define	DAHDI_PSEUDO	255

/* prink-wrapper macros */
#define	DAHDI_PRINTK(level, category, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: " fmt, #level, category, \
			THIS_MODULE->name, ## __VA_ARGS__)
#define	span_printk(level, category, span, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: span-%d: " fmt, #level,	\
		category, THIS_MODULE->name, (span)->spanno, ## __VA_ARGS__)
#define	chan_printk(level, category, chan, fmt, ...)	\
	printk(KERN_ ## level "%s%s-%s: %d: " fmt, #level,	\
		category, THIS_MODULE->name, (chan)->channo, ## __VA_ARGS__)
#define	dahdi_err(fmt, ...)	DAHDI_PRINTK(ERR, "", fmt, ## __VA_ARGS__)
#define	span_info(span, fmt, ...)	span_printk(INFO, "", span, fmt, \
						## __VA_ARGS__)
#define	span_notice(span, fmt, ...)	span_printk(NOTICE, "", span, fmt, \
						## __VA_ARGS__)
#define	span_err(span, fmt, ...)	span_printk(ERR, "", span, fmt, \
						## __VA_ARGS__)
#define	chan_notice(chan, fmt, ...)	chan_printk(NOTICE, "", chan, fmt, \
						## __VA_ARGS__)
#define	chan_err(chan, fmt, ...)	chan_printk(ERR, "", chan, fmt, \
						## __VA_ARGS__)

#ifndef pr_err
#define pr_err(fmt, ...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_warning
#define pr_warning(fmt, ...) \
	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_warn
#define pr_warn pr_warning
#endif
#ifndef pr_notice
#define pr_notice(fmt, ...) \
	printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_info
#define pr_info(fmt, ...) \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif

/* The dbg_* ones use a magical variable 'debug' and the user should be
 * aware of that.
*/
#ifdef DAHDI_PRINK_MACROS_USE_debug
#ifndef	BIT	/* added in 2.6.24 */
#define	BIT(i)		(1UL << (i))
#endif
/* Standard debug bit values. Any module may define others. They must
 * be of the form DAHDI_DBG_*
 */
#define	DAHDI_DBG_GENERAL	BIT(0)
#define	DAHDI_DBG_ASSIGN	BIT(1)
#define	DAHDI_DBG_DEVICES	BIT(7)	/* instantiation/destruction etc. */
#define	dahdi_dbg(bits, fmt, ...)	\
	((void)((debug & (DAHDI_DBG_ ## bits)) && DAHDI_PRINTK(DEBUG, \
			"-" #bits, "%s: " fmt, __func__, ## __VA_ARGS__)))
#define	span_dbg(bits, span, fmt, ...)	\
			((void)((debug & (DAHDI_DBG_ ## bits)) && \
				span_printk(DEBUG, "-" #bits, span, "%s: " \
					fmt, __func__, ## __VA_ARGS__)))
#define	chan_dbg(bits, chan, fmt, ...)	\
			((void)((debug & (DAHDI_DBG_ ## bits)) && \
				chan_printk(DEBUG, "-" #bits, chan, \
					"%s: " fmt, __func__, ## __VA_ARGS__)))
#define dahdi_dev_dbg(bits, dev, fmt, ...)         \
			((void)((debug & (DAHDI_DBG_ ## bits)) && \
			dev_printk(KERN_DEBUG, dev, \
			"DBG-%s(%s): " fmt, #bits, __func__, ## __VA_ARGS__)))
#endif /* DAHDI_PRINK_MACROS_USE_debug */

#endif /* _DAHDI_KERNEL_H */
