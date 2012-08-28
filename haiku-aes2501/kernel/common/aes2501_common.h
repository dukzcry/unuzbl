#ifndef AES_COMMON_H
#define AES_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#include "aes2501_lib.h"

#define AES_VID 0x08ff
#define AES_PID 0x2500

#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DEMODPHASE_NONE		0x00
enum aes2501_regs_common {
	AES2501_REG_CTRL1 = 0x80,
#define AES2501_CTRL1_MASTER_RESET	(1<<0)
#define AES2501_CTRL1_SCAN_RESET	(1<<1) /* stop + restart scan sequencer */
/* 1 = continuously updated, 0 = updated prior to starting a scan */
#define AES2501_CTRL1_REG_UPDATE	(1<<2)
	AES2501_REG_CTRL2 = 0x81,
#define AES2501_CTRL2_SET_ONE_SHOT	0x04
	AES2501_REG_EXCITCTRL = 0x82, /* excitation control */
	AES2501_REG_DETCTRL = 0x83, /* detect control */
	AES2501_REG_COLSCAN = 0x88, /* column scan rate register */
	AES2501_REG_MEASDRV = 0x89, /* measure drive */
	AES2501_REG_MEASFREQ = 0x8a, /* measure frequency */
	AES2501_REG_DEMODPHASE2 = 0x8c,
	AES2501_REG_DEMODPHASE1 = 0x8d,
	AES2501_REG_CHANGAIN = 0x8e, /* channel gain */
	AES2501_REG_ADREFHI = 0x91, /* A/D reference high */
	AES2501_REG_ADREFLO = 0x92, /* A/D reference low */
	AES2501_REG_STRTCOL = 0x95, /* start column */
	AES2501_REG_ENDCOL = 0x96, /* end column */
	AES2501_REG_DATFMT = 0x97, /* data format */
#define AES2501_DATFMT_BIN_IMG	0x10
	AES2501_REG_TREG1 = 0xa1, /* test register 1 */
	AES2501_REG_TREGC = 0xac,
/* Enable the reading of the register in TREGD */
#define AES2501_TREGC_ENABLE	0x01
	AES2501_REG_TREGD = 0xad,
	AES2501_REG_LPONT = 0xb4, /* low power oscillator on time */
#define AES2501_LPONT_MIN_VALUE 0x00	/* 0 ms */
};
enum aes2501_settling_delay {
	AES2501_DETCTRL_SDELAY_31_MS	= 0x00,	/* 31.25ms */
};
enum aes2501_rates_common {
	AES2501_DETCTRL_DRATE_CONTINUOUS 	= 0x00, /* continuously */

    AES2501_COLSCAN_SRATE_128_US	= 0x02,	/* 128us */
};
enum aes2501_mesure {
/* 0 = use mesure drive setting, 1 = when sine wave is selected */
#define AES2501_MEASDRV_MEASURE_SQUARE	0x10
	AES2501_MEASDRV_MDRIVE_0_325	= 0x00,	/* 0.325 Vpp */

	AES2501_MEASFREQ_2M		= 0x05
};
enum aes2501_sensor_gain {
	AES2501_CHANGAIN_STAGE1_16X	= 0x03,
	AES2501_CHANGAIN_STAGE2_4X	= 0x10,
};

#endif // AES_COMMON_H