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

// ProSlic Header File

typedef struct {
	unsigned char address;
	unsigned char altaddr;
	char *name;
	unsigned short initial;
} alpha;

typedef struct {
	unsigned char chip_number;
	unsigned char DTMF_digit;
	unsigned char interrupt_line;
	unsigned char hook_status;
	unsigned long half_pulses[20]; // Contains the time stamps of incomming half pulses.
	unsigned char half_pulses_detected; // Contains the number of half pulses detected.
	unsigned char Pulse_digit;
	unsigned long On_Hook_time;
	unsigned long Off_Hook_time;
} chipStruct;

// Defines
#define LPT 0X378

/* Proslic Linefeed options for register 64 - Linefeed Control */
#define SLIC_LF_OPEN		0x0
#define SLIC_LF_ACTIVE_FWD	0x1
#define SLIC_LF_OHTRAN_FWD	0x2 /* Forward On Hook Transfer */
#define SLIC_LF_TIP_OPEN	0x3
#define SLIC_LF_RINGING		0x4
#define SLIC_LF_ACTIVE_REV	0x5
#define SLIC_LF_OHTRAN_REV	0x6 /* Reverse On Hook Transfer */
#define SLIC_LF_RING_OPEN	0x7

#define SLIC_LF_SETMASK		0x7
#define SLIC_LF_OPPENDING 	0x10

/* Mask used to reverse the linefeed mode between forward and
 * reverse polarity. */
#define SLIC_LF_REVMASK 	0x4

#define IDA_LO  28
#define IDA_HI	29

#define IAA 30
#define ID_ACCES_STATUS 31
#define IAS_BIT 1

#define	I_STATUS	31

#define	SPI_MODE	0
#define	PCM_MODE	1
#define	PCM_XMIT_START_COUNT_LSB	2
#define	PCM_XMIT_START_COUNT_MSB	3
#define	PCM_RCV_START_COUNT_LSB	4
#define	PCM_RCV_START_COUNT_MSB	5
#define	DIO	6

#define	AUDIO_LOOPBACK	8
#define	AUDIO_GAIN	9
#define	LINE_IMPEDANCE	10
#define	HYBRID	11
#define	RESERVED12	12
#define	RESERVED13	13
#define	PWR_DOWN1	14
#define	PWR_DOWN2	15
#define	RESERVED16	16
#define	RESERVED17	17
#define	INTRPT_STATUS1	18
#define	INTRPT_STATUS2	19
#define	INTRPT_STATUS3	20
#define	INTRPT_MASK1	21
#define	INTRPT_MASK2	22
#define	INTRPT_MASK3	23
#define	DTMF_DIGIT	24
#define	RESERVED25	25
#define	RESERVED26	26
#define	RESERVED27	27
#define	I_DATA_LOW	28
#define	I_DATA_HIGH	29
#define	I_ADDRESS	30
#define	I_STATUS	31
#define	OSC1	32
#define	OSC2	33
#define	RING_OSC_CTL	34
#define	PULSE_OSC	35
#define	OSC1_ON__LO	36
#define	OSC1_ON_HI	37
#define	OSC1_OFF_LO	38
#define	OSC1_OFF_HI	39
#define	OSC2_ON__LO	40
#define	OSC2_ON_HI	41
#define	OSC2_OFF_LO	42
#define	OSC2_OFF_HI	43
#define	PULSE_ON__LO	44
#define	PULSE_ON_HI	45
#define	PULSE_OFF_LO	46
#define	PULSE_OFF_HI	47
#define	RING_ON__LO	48
#define	RING_ON_HI	49
#define	RING_OFF_LO	50
#define	RING_OFF_HI	51
#define	RESERVED52	52
#define	RESERVED53	53
#define	RESERVED54	54
#define	RESERVED55	55
#define	RESERVED56	56
#define	RESERVED57	57
#define	RESERVED58	58
#define	RESERVED59	59
#define	RESERVED60	60
#define	RESERVED61	61
#define	RESERVED62	62
#define	RESERVED63	63
#define	LINE_STATE	64
#define			ACTIVATE_LINE 0x11
#define			RING_LINE     0x44
#define	BIAS_SQUELCH	65
#define	BAT_FEED	66
#define	AUTO_STATE	67
#define	LOOP_STAT	68
#define	LOOP_DEBOUCE	69
#define	RT_DEBOUCE	70
#define	LOOP_I_LIMIT	71
#define	OFF_HOOK_V	72
#define	COMMON_V	73
#define	BAT_V_HI	74
#define	BAT_V_LO	75
#define	PWR_STAT_DEV	76
#define	PWR_STAT	77
#define	LOOP_V_SENSE	78
#define	LOOP_I_SENSE	79
#define	TIP_V_SENSE	80
#define	RING_V_SENSE	81
#define	BAT_V_HI_SENSE	82
#define	BAT_V_LO_SENSE	83
#define	IQ1	84
#define	IQ2	85
#define	IQ3	86
#define	IQ4	87
#define	IQ5	88
#define	IQ6	89
#define	RESERVED90	90
#define	RESERVED91	91
#define	DCDC_PWM_OFF	92
#define	DCDC	93
#define	DCDC_PW_OFF	94
#define	RESERVED95	95
#define	CALIBR1	96
#define CALIBRATE_LINE 0x78
#define NORMAL_CALIBRATION_COMPLETE 0x20
#define	CALIBR2	97
#define	RING_GAIN_CAL	98
#define	TIP_GAIN_CAL	99
#define	DIFF_I_CAL	100
#define	COMMON_I_CAL	101
#define	I_LIMIT_GAIN_CAL	102
#define	ADC_OFFSET_CAL	103
#define	DAC_ADC_OFFSET	104
#define	DAC_OFFSET_CAL	105
#define	COMMON_BAL_CAL	106
#define	DC_PEAK_CAL	107

//		Indirect Register (decimal)
#define	DTMF_ROW_0_PEAK	0
#define	DTMF_ROW_1_PEAK	1
#define	DTMF_ROW2_PEAK	2
#define	DTMF_ROW3_PEAK	3
#define	DTMF_COL1_PEAK	4
#define	DTMF_FWD_TWIST	5
#define	DTMF_RVS_TWIST	6
#define	DTMF_ROW_RATIO_THRESH	7
#define	DTMF_COL_RATIO_THRESH	8
#define	DTMF_ROW_2ND_HARM	9
#define	DTMF_COL_2ND_HARM	10
#define	DTMF_PWR_MIN_THRESH	11
#define	DTMF_HOT_LIM_THRESH	12
#define	OSC1_COEF	13
#define	OSC1X	14
#define	OSC1Y	15
#define	OSC2_COEF	16
#define	OSC2X	17
#define	OSC2Y	18
#define	RING_V_OFF	19
#define	RING_OSC_COEF	20
#define	RING_X	21
#define	RING_Y	22
#define	PULSE_ENVEL	23
#define	PULSE_X	24
#define	PULSE_Y	25
#define	RECV_DIGITAL_GAIN	26
#define	XMIT_DIGITAL_GAIN	27
#define	LOOP_CLOSE_THRESH	28
#define	RING_TRIP_THRESH	29
#define	COMMON_MIN_THRESH	30
#define	COMMON_MAX_THRESH	31
#define	PWR_ALARM_Q1Q2	32
#define	PWR_ALARM_Q3Q4	33
#define	PWR_ALARM_Q5Q6	34
#define	LOOP_CLOSURE_FILTER	35
#define	RING_TRIP_FILTER	36
#define	THERM_LP_POLE_Q1Q2	37
#define	THERM_LP_POLE_Q3Q4	38
#define	THERM_LP_POLE_Q5Q6	39
#define	CM_BIAS_RINGING	40
#define	DCDC_MIN_V	41
#define	DCDC_XTRA	42
