/* SPDX-License-Identifier: GPL-2.0 */
/*
 * imx283_mode_tbls.h - imx283 sensor driver
 *
 * Copyright (c) 2016-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2026, UAB Kurokesu. All rights reserved.
 */

#ifndef __IMX283_MODE_TBLS_H__
#define __IMX283_MODE_TBLS_H__

#include <media/camera_common.h>
#include <linux/miscdevice.h>

#define IMX283_TABLE_WAIT_MS 0
#define IMX283_TABLE_END 1
#define IMX283_WAIT_MS 1

#define IMX283_REG_STANDBY 0x3000
#define IMX283_REG_CLAMP 0x3001
#define IMX283_REG_PLSTMG08 0x3003
#define IMX283_REG_MDSEL1 0x3004
#define IMX283_REG_MDSEL2 0x3005
#define IMX283_REG_MDSEL3 0x3006
#define IMX283_REG_MDSEL4 0x3007
#define IMX283_REG_SVR_LSB 0x3009
#define IMX283_REG_SVR_MSB 0x300A
#define IMX283_REG_HTRIMMING 0x300B
#define IMX283_REG_VWINPOS_LSB 0x300F
#define IMX283_REG_VWINPOS_MSB 0x3010
#define IMX283_REG_VWIDCUT_LSB 0x3011
#define IMX283_REG_VWIDCUT_MSB 0x3012
#define IMX283_REG_TCLKPOST 0x3018
#define IMX283_REG_THSPREPARE 0x301A
#define IMX283_REG_THSZERO 0x301C
#define IMX283_REG_THSTRAIL 0x301E
#define IMX283_REG_TCLKTRAIL 0x3020
#define IMX283_REG_TCLKPREPARE 0x3022
#define IMX283_REG_TCLKZERO_LSB 0x3024
#define IMX283_REG_TCLKZERO_MSB 0x3025
#define IMX283_REG_TLPX 0x3026
#define IMX283_REG_THSEXIT 0x3028
#define IMX283_REG_TCLKPRE 0x302A
#define IMX283_REG_Y_OUT_SIZE_LSB 0x302F
#define IMX283_REG_Y_OUT_SIZE_MSB 0x3030
#define IMX283_REG_WRITE_VSIZE_LSB 0x3031
#define IMX283_REG_WRITE_VSIZE_MSB 0x3032
#define IMX283_REG_OB_SIZE_V 0x3033
#define IMX283_REG_HMAX_LSB 0x3036
#define IMX283_REG_HMAX_MSB 0x3037
#define IMX283_REG_VMAX_LSB 0x3038
#define IMX283_REG_VMAX_MID 0x3039
#define IMX283_REG_VMAX_MSB 0x303A
#define IMX283_REG_SHR_LSB 0x303B
#define IMX283_REG_SHR_MSB 0x303C
#define IMX283_REG_ANALOG_GAIN_LSB 0x3042
#define IMX283_REG_ANALOG_GAIN_MSB 0x3043
#define IMX283_REG_DIGITAL_GAIN 0x3044
#define IMX283_REG_HTRIMMING_START_LSB 0x3058
#define IMX283_REG_HTRIMMING_START_MSB 0x3059
#define IMX283_REG_HTRIMMING_END_LSB 0x305A
#define IMX283_REG_HTRIMMING_END_MSB 0x305B
#define IMX283_REG_SYSMODE 0x3104
#define IMX283_REG_XMSTA 0x3105
#define IMX283_REG_SYNCDRV 0x3107
#define IMX283_REG_TPG_CTRL 0x3156
#define IMX283_REG_TPG_PAT 0x3157
#define IMX283_REG_STBPL 0x320B
#define IMX283_REG_PLSTMG02 0x36AA
#define IMX283_REG_PLRD1 0x36C1
#define IMX283_REG_PLRD2_LSB 0x36C2
#define IMX283_REG_PLRD2_MSB 0x36C3
#define IMX283_REG_PLRD3 0x36F7
#define IMX283_REG_PLRD4 0x36F8
#define IMX283_REG_EBD_X_OUT_SIZE_LSB 0x3A54
#define IMX283_REG_EBD_X_OUT_SIZE_MSB 0x3A55

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
 *   - MIPI timing for 720 Mbps data rate (360 MHz link freq)
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
	{ IMX283_REG_STANDBY, IMX283_STBLOGIC | IMX283_STBDV },

	/*
	 * PLL input frequency config for 24 MHz INCK.
	 * Values from RPi driver imx283_frequencies[24 MHz].
	 */
	{ IMX283_REG_PLRD1, 0x02 },
	{ IMX283_REG_PLRD2_LSB, 0xF0 },
	{ IMX283_REG_PLRD2_MSB, 0x00 },
	{ IMX283_REG_PLRD3, 0x02 },
	{ IMX283_REG_PLRD4, 0xC0 },

	/* Communication init */
	{ IMX283_REG_PLSTMG08, 0x77 },
	{ IMX283_REG_PLSTMG02, 0x00 },

	/* Enable PLL (must be the last write before the 1 ms wait) */
	{ IMX283_REG_STBPL, 0x00 },

	/*
	 * MIPI timing for 720 Mbps data rate (360 MHz link freq).
	 * Values from RPi driver mipi_data_rate_720Mbps[].
	 *
	 * Bit rate halved from the IMX283 default 1440 Mbps to keep the
	 * Tegra DPHY data eye open for payloads with multi-bit runs;
	 * 1440 Mbps is marginal on this hardware path and produces
	 * pattern-dependent payload CRC errors in non-test data.
	 *
	 * Line burst at 720 Mbps: 5568 px * 12 bit / 4 lanes = 16704
	 * bits/lane = 23.2 us. HMAX must therefore be >= 1670 (in
	 * 72 MHz units); the mode table below uses 1800 for ~7% margin.
	 *
	 * 0x36C5 and 0x3AC4 are undocumented sensor registers per the
	 * RPi driver; their values differ between rate presets and
	 * must be paired with the matching DPHY HS timings.
	 */
	{ 0x36C5, 0x01 },
	{ 0x3AC4, 0x01 },
	{ IMX283_REG_TCLKPOST, 0x77 },
	{ IMX283_REG_THSPREPARE, 0x37 },
	{ IMX283_REG_THSZERO, 0x67 },
	{ IMX283_REG_THSTRAIL, 0x37 },
	{ IMX283_REG_TCLKTRAIL, 0x37 },
	{ IMX283_REG_TCLKPREPARE, 0x37 },
	{ IMX283_REG_TCLKZERO_LSB, 0xDF },
	{ IMX283_REG_TCLKZERO_MSB, 0x00 },
	{ IMX283_REG_TLPX, 0x2F },
	{ IMX283_REG_THSEXIT, 0x47 },
	{ IMX283_REG_TCLKPRE, 0x0F },
	{ IMX283_REG_SYSMODE, 0x02 },

	/* 1st stabilisation period (>= 1 ms) */
	{ IMX283_TABLE_WAIT_MS, IMX283_WAIT_MS * 2 },

	/* Activate (exit standby) */
	{ IMX283_REG_STANDBY, 0x00 },

	/* 2nd stabilisation period (>= 19 ms) */
	{ IMX283_TABLE_WAIT_MS, IMX283_WAIT_MS * 20 },

	{ IMX283_TABLE_END, 0x00 },
};

/*
 * Mode 0: 12-bit, 5568 x 3648 on-wire (96 HOB/OB + 5472 effective
 * aperture) x V-crop output. HTRIMMING_START/END only crops the inner
 * 5472-pixel effective region; the 96 leading HOB cols still appear
 * on MIPI as zero-valued RAW12 pixels in every pixel-data line (DT
 * 0x2C) and userspace can crop them at display.
 *
 * Mode 0 H-trim constraint (datasheet page 68, "Horizontal Arbitrary
 * Cropping Function"):
 *
 *     HTRIMMING_START = HOST + N * step
 *     HTRIMMING_END   = HOST + HNUM - M * step
 *     HTRIMMING_END - HTRIMMING_START >= MinH
 *     (M, N integers >= 0)
 *
 *   For Mode 0:  HOST = 120,  HNUM = 5496,  step = 4,  MinH = 240
 *
 * Note: the RPi imx283 driver hard-codes HTRIMMING_START=108,
 * HTRIMMING_END=5580 -- those are Mode 6 values (HOST=108) and are
 * illegal for Mode 0 (would require N = -3). With out-of-range
 * HTRIMMING_START the sensor silently disables H-trim and emits the
 * full HNUM=5496 pixels per line.
 *
 * To get a clean 5472-pixel effective line width with Mode 0:
 *   HTRIMMING_START = 120 + 0 * 4   = 120   (N = 0)
 *   HTRIMMING_END   = 5616 - 6 * 4  = 5592  (M = 6)
 *   width = 5592 - 120 = 5472 (>= MinH = 240 OK)
 *
 * Datasheet Mode 0 register table (pages 39-40), with V-crop enabled
 * to clamp output to the recommended-recording sub-area:
 *
 *   MDSEL1          = 04h
 *   MDSEL2          = 03h
 *   MDSEL3          = 30h    (= 10h | 20h, VCROP_EN bit set)
 *   MDSEL4          = 50h    (= 00h | 50h, VCROP_EN bit set)
 *   SVR             = 0000h
 *   HTRIMMING       = 30h    ([7:5]=001 reserved, [4]=1 enable)
 *   VWINPOS         = 0014h  (20, V-crop start offset)
 *   VWIDCUT         = 0017h  (23, V-crop trim count)
 *   Y_OUT_SIZE      = 0E40h  (3648 lines, recommended recording Veff)
 *   WRITE_VSIZE     = 0E50h  (3664 = 3648 + 16 OB)
 *   OB_SIZE_V       = 10h    (16 vertical OB lines)
 *   HMAX            = 0708h  (1800 @ 72 MHz, ~10 fps at 720 Mbps)
 *   VMAX            = 00FA0h (4000)
 *   SHR             = 000Bh  (11, minimum for Mode 0)
 *   HTRIMMING_START = 0078h  (120, Mode 0 minimum)
 *   HTRIMMING_END   = 15D8h  (5592, gives 5472-pix line width)
 *
 * V-crop output line count math (datasheet p.65):
 *   For Mode 0: Vst = 0, Vct = 0, Veff = 3694
 *   output_lines = Veff - (VWIDCUT - Vct) * 2 = 3694 - 46 = 3648 OK
 *   This matches Y_OUT_SIZE and DTS active_h exactly.
 *
 * HTRIMMING (0x300B) = 0x30 preserves reserved bits [7:5]=001 per
 * the datasheet "Set the default value" requirement; the RPi driver
 * writes 0x10 here, clobbering them.
 *
 * Sensor wire output per frame (datasheet p.47):
 *   - FS short packet.
 *   - 1 Embedded Data line (DT 0x12), EBD_X_OUT_SIZE = 0.
 *   - 16 Optical Black lines (DT 0x37), absorbed by NVCSI.
 *   - 3648 image lines (DT 0x2C, RAW12), 5568 pixels each.
 *   - FE short packet.
 */
static imx283_reg imx283_mode_5472x3648_12bit[] = {
	/*
	 * Readout mode 0 (12-bit), with V-crop enabled to clamp output to
	 * the datasheet-recommended 5472 x 3648 recording sub-area.
	 *
	 * MDSEL3 [5] = VCROP_EN; MDSEL4 [6:4] are V-crop control bits.
	 * Values (0x30, 0x50) match the RPi imx283 driver's mode-0 entry.
	 */
	{ IMX283_REG_MDSEL1, 0x04 },
	{ IMX283_REG_MDSEL2, 0x03 },
	{ IMX283_REG_MDSEL3, 0x10 | IMX283_MDSEL3_VCROP_EN },
	{ IMX283_REG_MDSEL4, 0x00 | IMX283_MDSEL4_VCROP_EN },

	/* SVR = 0 (not used) */
	{ IMX283_REG_SVR_LSB, 0x00 },
	{ IMX283_REG_SVR_MSB, 0x00 },

	/*
	 * 0x300B HTRIMMING:
	 *   [7:5] = 0b001 reserved (POR default; preserve)
	 *   [4]   = 1     HTRIMMING_EN
	 *   [3:1] = 0     reserved
	 *   [0]   = 0     MDVREV (vertical flip)
	 */
	{ IMX283_REG_HTRIMMING, 0x30 },

	/* VWINPOS = 20 (V-crop start offset, RPi value) */
	{ IMX283_REG_VWINPOS_LSB, 0x14 },
	{ IMX283_REG_VWINPOS_MSB, 0x00 },

	/* VWIDCUT = 23 (V-crop trim count, RPi value) */
	{ IMX283_REG_VWIDCUT_LSB, 0x17 },
	{ IMX283_REG_VWIDCUT_MSB, 0x00 },

	/* Y_OUT_SIZE = 3648 (0x0E40, recommended-recording Veff) */
	{ IMX283_REG_Y_OUT_SIZE_LSB, 0x40 },
	{ IMX283_REG_Y_OUT_SIZE_MSB, 0x0E },

	/* WRITE_VSIZE = 3664 (0x0E50, = Y_OUT_SIZE + 16 OB) */
	{ IMX283_REG_WRITE_VSIZE_LSB, 0x50 },
	{ IMX283_REG_WRITE_VSIZE_MSB, 0x0E },

	/* OB_SIZE_V = 16 vertical optical-black lines (datasheet default) */
	{ IMX283_REG_OB_SIZE_V, 0x10 },

	/*
	 * HMAX = 1800 (72 MHz clocks = 25 us/line). Sized for the
	 * 720 Mbps preset: line burst is 5568 px * 12 bit / 4 lanes =
	 * 16704 bits/lane = 23.2 us, so HMAX >= 1670 in 72 MHz units.
	 * 1800 gives ~7% margin. Frame time = 1800 * 4000 / 72e6 =
	 * 100 ms ~= 10 fps.
	 */
	{ IMX283_REG_HMAX_LSB, 0x08 },  /* 1800 & 0xFF */
	{ IMX283_REG_HMAX_MSB, 0x07 },  /* 1800 >> 8   */

	/* VMAX = 4000 (~10 fps at 720 Mbps with HMAX=1800) */
	{ IMX283_REG_VMAX_LSB, 0xA0 },
	{ IMX283_REG_VMAX_MID, 0x0F },
	{ IMX283_REG_VMAX_MSB, 0x00 },

	/* SHR = 11 (minimum for mode 0) */
	{ IMX283_REG_SHR_LSB, 0x0B },
	{ IMX283_REG_SHR_MSB, 0x00 },

	/*
	 * HTRIMMING_START = 120 (0x0078, Mode 0 minimum = HOST).
	 * Mode 0 requires HTRIMMING_START = 120 + N*4 with N>=0; the
	 * RPi value of 108 (Mode 6's HOST) violates this and silently
	 * disables H-trim, making the sensor emit full HNUM=5496 pixels
	 * per line and triggering err_data 256 PIXEL_LONG_LINE on Tegra.
	 */
	{ IMX283_REG_HTRIMMING_START_LSB, 0x78 },
	{ IMX283_REG_HTRIMMING_START_MSB, 0x00 },

	/*
	 * HTRIMMING_END = 5592 (0x15D8, M=6: 5616 - 6*4).
	 * Width = HTRIMMING_END - HTRIMMING_START = 5472 pixels.
	 */
	{ IMX283_REG_HTRIMMING_END_LSB, 0xD8 },
	{ IMX283_REG_HTRIMMING_END_MSB, 0x15 },

	/*
	 * EBD_X_OUT_SIZE = 0 -- matches RPi upstream and the datasheet
	 * POR default. Writing 0 may not fully suppress the DT 0x12 EBD
	 * long packet on the wire (the sensor may still emit a short
	 * header/footer-only payload), so this MUST be paired with DTS
	 * `embedded_metadata_height = "0"` so Tegra VI's chansel does
	 * not allocate or expect a metadata line.
	 */
	{ IMX283_REG_EBD_X_OUT_SIZE_LSB, 0x00 },
	{ IMX283_REG_EBD_X_OUT_SIZE_MSB, 0x00 },

	{ IMX283_TABLE_END, 0x00 },
};

/*
 * Standby cancel phase 3 (datasheet page 77, "Normal operation"):
 *   - CLPSQRST = 1
 *   - XMSTA    = 0  (master mode start -> MIPI output begins)
 *   - SYNCDRV  = 2  (XHS/XVS drive enabled)
 */
static imx283_reg imx283_start[] = {
	{ IMX283_REG_CLAMP, 0x10 },
	{ IMX283_REG_XMSTA, 0x00 },
	{ IMX283_REG_SYNCDRV, 0xA2 },
	{ IMX283_TABLE_END, 0x00 },
};

/* Stop streaming: enter standby */
static imx283_reg imx283_stop[] = {
	{ IMX283_REG_STANDBY, IMX283_STBLOGIC },
	{ IMX283_TABLE_WAIT_MS, IMX283_WAIT_MS * 30 },
	{ IMX283_TABLE_END, 0x00 },
};

enum {
	IMX283_MODE_5568X3648,
	IMX283_MODE_COMMON,
	IMX283_START_STREAM,
	IMX283_STOP_STREAM,
};

static imx283_reg *mode_table[] = {
	[IMX283_MODE_5568X3648] = imx283_mode_5472x3648_12bit,
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
	/*
	 * 5568 x 3648 = 96 HOB + (HTRIMMING_END - HTRIMMING_START) =
	 * full on-wire line width, x V-crop output line count. Both
	 * dimensions are even as required by Tegra VI.
	 */
	{ { 5568, 3648 }, imx283_20fps, 1, 0, IMX283_MODE_5568X3648 },
};

#endif /* __IMX283_MODE_TBLS_H__ */
