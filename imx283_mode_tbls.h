/* SPDX-License-Identifier: GPL-2.0 */
/*
 * imx283_mode_tbls.h - imx283 sensor driver
 *
 * Copyright (c) 2016-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2026, UAB Kurokesu. All rights reserved.
 *
 * Register values derived from the RPi IMX283 V4L2 driver by Will Whang,
 * Kieran Bingham, and Umang Jain (Ideas on Board / Raspberry Pi).
 */

#ifndef __IMX283_MODE_TBLS_H__
#define __IMX283_MODE_TBLS_H__

#include <media/camera_common.h>
#include <linux/miscdevice.h>

#define IMX283_TABLE_WAIT_MS 0
#define IMX283_TABLE_END 1
#define IMX283_WAIT_MS 1

/* Standby / control */
#define IMX283_STANDBY 0x3000
#define IMX283_CLAMP 0x3001
#define IMX283_PLSTMG08 0x3003
#define IMX283_XMSTA 0x3105
#define IMX283_SYNCDRV 0x3107

/* Readout mode */
#define IMX283_MDSEL1 0x3004
#define IMX283_MDSEL2 0x3005
#define IMX283_MDSEL3 0x3006
#define IMX283_MDSEL4 0x3007

/* SVR (16-bit LE) */
#define IMX283_SVR_LSB 0x3009
#define IMX283_SVR_MSB 0x300A

/* Horizontal trimming */
#define IMX283_HTRIMMING 0x300B

/* VWINPOS (16-bit LE) */
#define IMX283_VWINPOS_LSB 0x300F
#define IMX283_VWINPOS_MSB 0x3010

/* VWIDCUT (16-bit LE) */
#define IMX283_VWIDCUT_LSB 0x3011
#define IMX283_VWIDCUT_MSB 0x3012

/* MIPI timing */
#define IMX283_TCLKPOST 0x3018
#define IMX283_THSPREPARE 0x301A
#define IMX283_THSZERO 0x301C
#define IMX283_THSTRAIL 0x301E
#define IMX283_TCLKTRAIL 0x3020
#define IMX283_TCLKPREPARE 0x3022
#define IMX283_TCLKZERO_LSB 0x3024
#define IMX283_TCLKZERO_MSB 0x3025
#define IMX283_TLPX 0x3026
#define IMX283_THSEXIT 0x3028
#define IMX283_TCLKPRE 0x302A

/* Image size */
#define IMX283_Y_OUT_SIZE_LSB 0x302F
#define IMX283_Y_OUT_SIZE_MSB 0x3030
#define IMX283_WRITE_VSIZE_LSB 0x3031
#define IMX283_WRITE_VSIZE_MSB 0x3032
#define IMX283_OB_SIZE_V 0x3033

/* Timing */
#define IMX283_HMAX_LSB 0x3036
#define IMX283_HMAX_MSB 0x3037
#define IMX283_VMAX_LSB 0x3038
#define IMX283_VMAX_MID 0x3039
#define IMX283_VMAX_MSB 0x303A
#define IMX283_SHR_LSB 0x303B
#define IMX283_SHR_MSB 0x303C

/* Gain */
#define IMX283_ANALOG_GAIN_LSB 0x3042
#define IMX283_ANALOG_GAIN_MSB 0x3043
#define IMX283_DIGITAL_GAIN 0x3044

/* Horizontal trimming range */
#define IMX283_HTRIMMING_START_LSB 0x3058
#define IMX283_HTRIMMING_START_MSB 0x3059
#define IMX283_HTRIMMING_END_LSB 0x305A
#define IMX283_HTRIMMING_END_MSB 0x305B

/* SYSMODE */
#define IMX283_SYSMODE 0x3104

/* PLL */
#define IMX283_STBPL 0x320B
#define IMX283_PLSTMG02 0x36AA
#define IMX283_PLRD1 0x36C1
#define IMX283_PLRD2_LSB 0x36C2
#define IMX283_PLRD2_MSB 0x36C3
#define IMX283_PLRD3 0x36F7
#define IMX283_PLRD4 0x36F8

/* Test pattern */
#define IMX283_TPG_CTRL 0x3156
#define IMX283_TPG_PAT 0x3157

/* Embedded data */
#define IMX283_EBD_X_OUT_SIZE_LSB 0x3A54
#define IMX283_EBD_X_OUT_SIZE_MSB 0x3A55

/* Readout mode field values for Mode 0 (12-bit) */
#define IMX283_MDSEL3_VCROP_EN 0x20
#define IMX283_MDSEL4_VCROP_EN 0x50

/* Standby register bit fields */
#define IMX283_STBLOGIC 0x02
#define IMX283_STBDV 0x08

#define imx283_reg struct reg_8

/*
 * Standby cancel phase 1 + 2 (datasheet page 77):
 *   - Partial standby (STBLOGIC=1, STBDV=1)
 *   - PLL input frequency config for 24 MHz INCK
 *   - Communication init (PLSTMG)
 *   - Enable PLL (STBPL=0)
 *   - MIPI timing for 1440 Mbps data rate (720 MHz link freq)
 *   - >= 1 ms stabilisation
 *   - Exit standby (STANDBY=0)
 *   - >= 19 ms stabilisation
 *
 * After this table the sensor is fully active but not yet in master
 * mode (XMSTA defaults to 1 = master mode stop per datasheet). The
 * transition to "Normal operation" (CLAMP/XMSTA/SYNCDRV) is deferred
 * to imx283_start so that MIPI output begins exactly when tegracam
 * calls start_streaming, matching the datasheet phase-3 sequence.
 */
static imx283_reg imx283_mode_common[] = {
	/* Partial standby */
	{ IMX283_STANDBY, IMX283_STBLOGIC | IMX283_STBDV },

	/*
	 * PLL input frequency config for 24 MHz INCK.
	 * Values from RPi driver imx283_frequencies[24 MHz].
	 */
	{ IMX283_PLRD1, 0x02 },
	{ IMX283_PLRD2_LSB, 0xF0 },
	{ IMX283_PLRD2_MSB, 0x00 },
	{ IMX283_PLRD3, 0x02 },
	{ IMX283_PLRD4, 0xC0 },

	/* Communication init */
	{ IMX283_PLSTMG08, 0x77 },
	{ IMX283_PLSTMG02, 0x00 },

	/* Enable PLL (must be the last write before the 1 ms wait) */
	{ IMX283_STBPL, 0x00 },

	/*
	 * MIPI timing for 1440 Mbps data rate (720 MHz link freq).
	 * Values from RPi driver mipi_data_rate_1440Mbps[].
	 *
	 * Bandwidth rationale: mode 0 line time is HMAX=900 @ 72 MHz
	 * (12.5 us). Each line carries 5472 pixels x 12 bits = 65664
	 * bits. Four lanes @ 1440 Mbps = 5.76 Gbps total, so the line
	 * transmits in 11.4 us (fits). 720 Mbps only gives 2.88 Gbps
	 * which needs 22.8 us per line - impossible within HMAX=900
	 * and causes the sensor's MIPI TX to stay silent.
	 */
	{ 0x36C5, 0x00 },
	{ 0x3AC4, 0x00 },
	{ IMX283_TCLKPOST, 0xA7 },
	{ IMX283_THSPREPARE, 0x6F },
	{ IMX283_THSZERO, 0x9F },
	{ IMX283_THSTRAIL, 0x5F },
	{ IMX283_TCLKTRAIL, 0x5F },
	{ IMX283_TCLKPREPARE, 0x6F },
	{ IMX283_TCLKZERO_LSB, 0x7F },
	{ IMX283_TCLKZERO_MSB, 0x01 },
	{ IMX283_TLPX, 0x4F },
	{ IMX283_THSEXIT, 0x47 },
	{ IMX283_TCLKPRE, 0x07 },
	{ IMX283_SYSMODE, 0x02 },

	/* 1st stabilisation period (>= 1 ms) */
	{ IMX283_TABLE_WAIT_MS, IMX283_WAIT_MS * 2 },

	/* Activate (exit standby) */
	{ IMX283_STANDBY, 0x00 },

	/* 2nd stabilisation period (>= 19 ms) */
	{ IMX283_TABLE_WAIT_MS, IMX283_WAIT_MS * 20 },

	{ IMX283_TABLE_END, 0x00 },
};

/*
 * Mode 0: 12-bit, 5472 x 3648, all-pixel readout.
 *
 * Derived from RPi driver supported_modes_12bit[0] and
 * imx283_readout_modes[IMX283_MODE_0].
 *
 * Image parameters:
 *   y_out_size  = 3648  (0x0E40)
 *   write_vsize = 3664  (0x0E50)  = y_out_size + vertical_ob(16)
 *   vwinpos     = 20    (0x0014)  = (crop.top / vbin) / 2 + vst
 *   vwidcut     = 23    (0x0017)  = (veff - y_out_size) / 2 + vct
 *   htrim_start = 108   (0x006C)  = crop.left
 *   htrim_end   = 5580  (0x15CC)  = crop.left + crop.width
 *   hmax        = 900   (0x0384)  72 MHz clocks (= 6000 pix @ 480 Mpix/s)
 *   vmax        = 4000  (0x0FA0)  lines  (~20 fps)
 *   shr         = 11    (0x000B)  minimum for mode 0
 */
static imx283_reg imx283_mode_5472x3648_12bit[] = {
	/* Readout mode 0 (12-bit) with vertical cropping */
	{ IMX283_MDSEL1, 0x04 },
	{ IMX283_MDSEL2, 0x03 },
	{ IMX283_MDSEL3, 0x10 | IMX283_MDSEL3_VCROP_EN },
	{ IMX283_MDSEL4, 0x00 | IMX283_MDSEL4_VCROP_EN },

	/* SVR = 0 (not used) */
	{ IMX283_SVR_LSB, 0x00 },
	{ IMX283_SVR_MSB, 0x00 },

	/* Horizontal trimming enable (no VFLIP) */
	{ IMX283_HTRIMMING, 0x10 },

	/* VWINPOS = 20 */
	{ IMX283_VWINPOS_LSB, 0x14 },
	{ IMX283_VWINPOS_MSB, 0x00 },

	/* VWIDCUT = 23 */
	{ IMX283_VWIDCUT_LSB, 0x17 },
	{ IMX283_VWIDCUT_MSB, 0x00 },

	/* Y_OUT_SIZE = 3648 */
	{ IMX283_Y_OUT_SIZE_LSB, 0x40 },
	{ IMX283_Y_OUT_SIZE_MSB, 0x0E },

	/* WRITE_VSIZE = 3664 */
	{ IMX283_WRITE_VSIZE_LSB, 0x50 },
	{ IMX283_WRITE_VSIZE_MSB, 0x0E },

	/* OB_SIZE_V = 16 */
	{ IMX283_OB_SIZE_V, 0x10 },

	/* HMAX = 900 (72 MHz clocks) */
	{ IMX283_HMAX_LSB, 0x84 },
	{ IMX283_HMAX_MSB, 0x03 },

	/* VMAX = 4000 */
	{ IMX283_VMAX_LSB, 0xA0 },
	{ IMX283_VMAX_MID, 0x0F },
	{ IMX283_VMAX_MSB, 0x00 },

	/* SHR = 11 (minimum for mode 0) */
	{ IMX283_SHR_LSB, 0x0B },
	{ IMX283_SHR_MSB, 0x00 },

	/* HTRIMMING_START = 108 (crop.left) */
	{ IMX283_HTRIMMING_START_LSB, 0x6C },
	{ IMX283_HTRIMMING_START_MSB, 0x00 },

	/* HTRIMMING_END = 5580 (crop.left + crop.width) */
	{ IMX283_HTRIMMING_END_LSB, 0xCC },
	{ IMX283_HTRIMMING_END_MSB, 0x15 },

	/* Disable embedded data */
	{ IMX283_EBD_X_OUT_SIZE_LSB, 0x00 },
	{ IMX283_EBD_X_OUT_SIZE_MSB, 0x00 },

	{ IMX283_TABLE_END, 0x00 },
};

/*
 * Standby cancel phase 3 (datasheet page 77, "Normal operation"):
 *   - CLPSQRST = 1
 *   - XMSTA    = 0  (master mode start -> MIPI output begins)
 *   - SYNCDRV  = 2  (XHS/XVS drive enabled)
 */
static imx283_reg imx283_start[] = {
	{ IMX283_CLAMP, 0x10 },
	{ IMX283_XMSTA, 0x00 },
	{ IMX283_SYNCDRV, 0xA2 },
	{ IMX283_TABLE_END, 0x00 },
};

/* Stop streaming: enter standby */
static imx283_reg imx283_stop[] = {
	{ IMX283_STANDBY, IMX283_STBLOGIC },
	{ IMX283_TABLE_WAIT_MS, IMX283_WAIT_MS * 30 },
	{ IMX283_TABLE_END, 0x00 },
};

enum {
	IMX283_MODE_5472X3648,
	IMX283_MODE_COMMON,
	IMX283_START_STREAM,
	IMX283_STOP_STREAM,
};

static imx283_reg *mode_table[] = {
	[IMX283_MODE_5472X3648] = imx283_mode_5472x3648_12bit,
	[IMX283_MODE_COMMON] = imx283_mode_common,
	[IMX283_START_STREAM] = imx283_start,
	[IMX283_STOP_STREAM] = imx283_stop,
};

static const int imx283_20fps[] = {
	20,
};

/*
 * WARNING: frmfmt ordering needs to match mode definition in
 * device tree!
 */
static const struct camera_common_frmfmt imx283_frmfmt[] = {
	{ { 5472, 3648 }, imx283_20fps, 1, 0, IMX283_MODE_5472X3648 },
};

#endif /* __IMX283_MODE_TBLS_H__ */
