// SPDX-License-Identifier: GPL-2.0
/*
 * nv_imx283.c - imx283 sensor driver
 *
 * Copyright (c) 2016-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * Copyright (c) 2026, UAB Kurokesu. All rights reserved.
 */

#define DEBUG

#include <nvidia/conftest.h>

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra_v4l2_camera.h>
#include <media/tegracam_core.h>
#include "imx283_mode_tbls.h"

/*
 * Chip "ID" - standby register power-on default.
 */
#define IMX283_CHIP_ID 0x0B

/* Timing limits */
#define IMX283_MIN_FRAME_LENGTH (3793)
#define IMX283_MAX_FRAME_LENGTH (0xFFFF)
#define IMX283_MIN_SHR (11)
#define IMX283_DEFAULT_FRAME_LENGTH (4000)
#define IMX283_DEFAULT_HMAX (900)

/* Analog gain: 11-bit value, 0..1957 */
#define IMX283_ANA_GAIN_MAX 1957

/* TPG patterns (written to 0x3157) */
#define IMX283_TPG_PAT_ALL_000 0x00
#define IMX283_TPG_PAT_ALL_FFF 0x01
#define IMX283_TPG_PAT_ALL_555 0x02
#define IMX283_TPG_PAT_ALL_AAA 0x03
#define IMX283_TPG_PAT_H_COLOR_BARS 0x0A
#define IMX283_TPG_PAT_V_COLOR_BARS 0x0B

static const u8 imx283_tpg_val[] = {
	IMX283_TPG_PAT_ALL_000,	     IMX283_TPG_PAT_ALL_FFF,
	IMX283_TPG_PAT_ALL_555,	     IMX283_TPG_PAT_ALL_AAA,
	IMX283_TPG_PAT_H_COLOR_BARS, IMX283_TPG_PAT_V_COLOR_BARS,
};

static const struct of_device_id imx283_of_match[] = {
	{ .compatible = "sony,imx283" },
	{},
};

MODULE_DEVICE_TABLE(of, imx283_of_match);

static int test_mode;
module_param(test_mode, int, 0644);
MODULE_PARM_DESC(
	test_mode,
	"Test pattern: 0=off 1=000 2=FFF 3=555 4=AAA 5=H-bars 6=V-bars");

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

struct imx283 {
	struct i2c_client *i2c_client;
	struct v4l2_subdev *subdev;
	struct camera_common_data *s_data;
	struct tegracam_device *tc_dev;
	u32 frame_length;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.use_single_read = true,
	.use_single_write = true,
};

static inline int imx283_read_reg(struct camera_common_data *s_data, u16 addr,
				  u8 *val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xFF;

	return err;
}

static inline int imx283_write_reg(struct camera_common_data *s_data, u16 addr,
				   u8 val)
{
	int err = 0;

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(s_data->dev, "%s: i2c write failed, 0x%x = %x",
			__func__, addr, val);

	return err;
}

static int imx283_write_table(struct imx283 *priv, const imx283_reg table[])
{
	int err = 0;

	dev_dbg(priv->s_data->dev, "%s: Writing register table\n", __func__);

	err = regmap_util_write_table_8(priv->s_data->regmap, table, NULL, 0,
					IMX283_TABLE_WAIT_MS, IMX283_TABLE_END);

	if (err) {
		dev_err(priv->s_data->dev, "%s: Failed to write table (%d)\n",
			__func__, err);
	} else {
		dev_dbg(priv->s_data->dev,
			"%s: Register table written successfully\n", __func__);
	}

	return err;
}

static inline void imx283_get_vmax_regs(imx283_reg *regs, u32 vmax)
{
	regs->addr = IMX283_REG_VMAX_MSB;
	regs->val = (vmax >> 16) & 0xFF;

	(regs + 1)->addr = IMX283_REG_VMAX_MID;
	(regs + 1)->val = (vmax >> 8) & 0xFF;

	(regs + 2)->addr = IMX283_REG_VMAX_LSB;
	(regs + 2)->val = vmax & 0xFF;
}

static inline void imx283_get_shr_regs(imx283_reg *regs, u16 shr)
{
	regs->addr = IMX283_REG_SHR_MSB;
	regs->val = (shr >> 8) & 0xFF;

	(regs + 1)->addr = IMX283_REG_SHR_LSB;
	(regs + 1)->val = shr & 0xFF;
}

static int imx283_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	/*
	 * Stub: gain stays at the mode-table default (register 0 =
	 * unity). The tegracam control plumbing requires this hook
	 * but writing IMX283_ANALOG_GAIN_LSB/MSB during Argus session
	 * setup is currently destabilising capture; restore once that
	 * path is verified.
	 */
	dev_dbg(tc_dev->dev, "%s: stub (val=%lld ignored)\n", __func__, val);
	return 0;
}

static int __maybe_unused imx283_set_coarse_time(struct imx283 *priv, s64 val)
{
	struct camera_common_data *s_data = priv->s_data;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode];
	struct device *dev = priv->tc_dev->dev;
	imx283_reg reg_list[2];
	u32 coarse_time;
	u16 shr;
	int err, i;

	if (mode->control_properties.exposure_factor == 0 ||
	    mode->image_properties.line_length == 0) {
		dev_err(dev, "%s: line_len=%d, exposure_factor=%d\n", __func__,
			mode->image_properties.line_length,
			mode->control_properties.exposure_factor);
		return -EINVAL;
	}

	coarse_time = DIV_ROUND_CLOSEST(
		mode->signal_properties.pixel_clock.val * val /
			mode->image_properties.line_length,
		mode->control_properties.exposure_factor);

	if (priv->frame_length == 0)
		priv->frame_length = IMX283_DEFAULT_FRAME_LENGTH;

	/*
	 * SHR = VMAX - coarse_time.
	 * Clamp so that SHR >= IMX283_MIN_SHR.
	 */
	if (coarse_time > (priv->frame_length - IMX283_MIN_SHR))
		coarse_time = priv->frame_length - IMX283_MIN_SHR;
	if (coarse_time < 1)
		coarse_time = 1;

	shr = priv->frame_length - coarse_time;

	dev_dbg(dev, "%s: coarse_time:%u, SHR:%u, FL:%u\n", __func__,
		coarse_time, shr, priv->frame_length);

	imx283_get_shr_regs(reg_list, shr);

	for (i = 0; i < 2; i++) {
		err = imx283_write_reg(s_data, reg_list[i].addr,
				       reg_list[i].val);
		if (err) {
			dev_dbg(dev, "%s: SHR write error\n", __func__);
			return err;
		}
	}

	return 0;
}

static int imx283_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	/*
	 * Stub: exposure stays at the SHR value programmed by the mode
	 * table (SHR = 11, near-full-frame open shutter at 10 fps).
	 *
	 * The previous implementation called imx283_set_coarse_time()
	 * which computes SHR = priv->frame_length - coarse_time; that
	 * runs against priv->frame_length BEFORE set_frame_rate
	 * (tegracam call order: gain -> exposure -> frame_rate), so
	 * SHR ends up referenced to a frame_length that frame_rate
	 * may later overwrite with a different VMAX. Restore once
	 * either the call ordering is fixed or set_frame_rate
	 * recomputes SHR.
	 */
	dev_dbg(tc_dev->dev, "%s: stub (val=%lld ignored)\n", __func__, val);
	return 0;
}

static int imx283_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	/*
	 * Stub: frame rate stays at the VMAX value programmed by the
	 * mode table (VMAX = 4000 lines, ~10 fps with HMAX = 1800).
	 *
	 * The previous implementation wrote IMX283_VMAX_LSB/MID/MSB
	 * computed from the requested rate but left SHR unchanged, so
	 * a sequence of set_exposure -> set_frame_rate could leave
	 * SHR referenced to the previous VMAX. Restore once SHR/VMAX
	 * are kept consistent across the setter call sequence.
	 */
	dev_dbg(tc_dev->dev, "%s: stub (val=%lld ignored)\n", __func__, val);
	return 0;
}

static int imx283_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	int err = 0;

	err = imx283_write_reg(s_data, IMX283_REG_REGHOLD, val);
	if (err)
		dev_dbg(dev, "%s: Group hold control error\n", __func__);

	return err;
}

static struct tegracam_ctrl_ops imx283_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = imx283_set_gain,
	.set_exposure = imx283_set_exposure,
	.set_frame_rate = imx283_set_frame_rate,
	.set_group_hold = imx283_set_group_hold,
};

static int imx283_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power on\n", __func__);
	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	if (pw->reset_gpio) {
		if (gpiod_cansleep(gpio_to_desc(pw->reset_gpio)))
			gpio_set_value_cansleep(pw->reset_gpio, 0);
		else
			gpio_set_value(pw->reset_gpio, 0);
	}

	if (unlikely(!(pw->avdd || pw->iovdd || pw->dvdd)))
		goto skip_power_seqn;

	usleep_range(10, 20);

	if (pw->avdd) {
		err = regulator_enable(pw->avdd);
		if (err)
			goto imx283_avdd_fail;
	}

	if (pw->iovdd) {
		err = regulator_enable(pw->iovdd);
		if (err)
			goto imx283_iovdd_fail;
	}

	if (pw->dvdd) {
		err = regulator_enable(pw->dvdd);
		if (err)
			goto imx283_dvdd_fail;
	}

	usleep_range(10, 20);

skip_power_seqn:
	if (pw->reset_gpio) {
		if (gpiod_cansleep(gpio_to_desc(pw->reset_gpio)))
			gpio_set_value_cansleep(pw->reset_gpio, 1);
		else
			gpio_set_value(pw->reset_gpio, 1);
	}

	usleep_range(1000, 2000);

	pw->state = SWITCH_ON;

	return 0;

imx283_dvdd_fail:
	regulator_disable(pw->iovdd);

imx283_iovdd_fail:
	regulator_disable(pw->avdd);

imx283_avdd_fail:
	dev_err(dev, "%s failed.\n", __func__);

	return -ENODEV;
}

static int imx283_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power off\n", __func__);

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (err) {
			dev_err(dev, "%s failed.\n", __func__);
			return err;
		}
	} else {
		if (pw->reset_gpio) {
			if (gpiod_cansleep(gpio_to_desc(pw->reset_gpio)))
				gpio_set_value_cansleep(pw->reset_gpio, 0);
			else
				gpio_set_value(pw->reset_gpio, 0);
		}

		usleep_range(10, 10);

		if (pw->dvdd)
			regulator_disable(pw->dvdd);
		if (pw->iovdd)
			regulator_disable(pw->iovdd);
		if (pw->avdd)
			regulator_disable(pw->avdd);
	}

	pw->state = SWITCH_OFF;

	return 0;
}

static int imx283_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->dvdd))
		devm_regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		devm_regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		devm_regulator_put(pw->iovdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;

	if (likely(pw->reset_gpio))
		gpio_free(pw->reset_gpio);

	return 0;
}

static int imx283_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct clk *parent;
	int err = 0;

	if (!pdata) {
		dev_err(dev, "pdata missing\n");
		return -EFAULT;
	}

	/* Sensor MCLK (aka. INCK) */
	if (pdata->mclk_name) {
		pw->mclk = devm_clk_get(dev, pdata->mclk_name);
		if (IS_ERR(pw->mclk)) {
			dev_err(dev, "unable to get clock %s\n",
				pdata->mclk_name);
			return PTR_ERR(pw->mclk);
		}

		if (pdata->parentclk_name) {
			parent = devm_clk_get(dev, pdata->parentclk_name);
			if (IS_ERR(parent)) {
				dev_err(dev, "unable to get parent clock %s",
					pdata->parentclk_name);
			} else
				clk_set_parent(pw->mclk, parent);
		}
	}

	/* analog 2.9v */
	if (pdata->regulators.avdd)
		err |= camera_common_regulator_get(dev, &pw->avdd,
						   pdata->regulators.avdd);
	/* IO 1.8v */
	if (pdata->regulators.iovdd)
		err |= camera_common_regulator_get(dev, &pw->iovdd,
						   pdata->regulators.iovdd);
	/* dig 1.2v */
	if (pdata->regulators.dvdd)
		err |= camera_common_regulator_get(dev, &pw->dvdd,
						   pdata->regulators.dvdd);
	if (err) {
		dev_err(dev, "%s: unable to get regulator(s)\n", __func__);
		goto done;
	}

	/* Reset or ENABLE GPIO */
	pw->reset_gpio = pdata->reset_gpio;
	err = gpio_request(pw->reset_gpio, "cam_reset_gpio");
	if (err < 0) {
		dev_err(dev, "%s: unable to request reset_gpio (%d)\n",
			__func__, err);
		goto done;
	}

done:
	pw->state = SWITCH_OFF;

	return err;
}

static struct camera_common_pdata *
imx283_parse_dt(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *np = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	struct camera_common_pdata *ret = NULL;
	int err = 0;
	int gpio;

	if (!np)
		return NULL;

	match = of_match_device(imx283_of_match, dev);
	if (!match) {
		dev_err(dev, "failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata =
		devm_kzalloc(dev, sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "reset-gpios not found\n");
		goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err)
		dev_dbg(dev,
			"mclk name not present, assume sensor driven externally\n");

	err = of_property_read_string(np, "avdd-reg",
				      &board_priv_pdata->regulators.avdd);
	err |= of_property_read_string(np, "iovdd-reg",
				       &board_priv_pdata->regulators.iovdd);
	err |= of_property_read_string(np, "dvdd-reg",
				       &board_priv_pdata->regulators.dvdd);
	if (err)
		dev_dbg(dev,
			"avdd, iovdd and/or dvdd reglrs. not present, assume sensor powered independently\n");

	board_priv_pdata->has_eeprom = of_property_read_bool(np, "has-eeprom");

	return board_priv_pdata;

error:
	devm_kfree(dev, board_priv_pdata);

	return ret;
}

static int imx283_set_mode(struct tegracam_device *tc_dev)
{
	struct imx283 *priv = (struct imx283 *)tegracam_get_privdata(tc_dev);
	int err = 0;

	dev_dbg(tc_dev->dev, "%s:\n", __func__);

	err = imx283_write_table(priv, mode_table[IMX283_MODE_COMMON]);
	if (err)
		return err;

	err = imx283_write_table(priv, mode_table[IMX283_MODE_5568X3648]);
	if (err)
		return err;

	priv->frame_length = IMX283_DEFAULT_FRAME_LENGTH;

	return 0;
}

static int imx283_start_streaming(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	int err = 0;

	dev_dbg(tc_dev->dev, "%s:\n", __func__);

	if (test_mode) {
		u8 pat_idx;

		dev_dbg(tc_dev->dev, "test pattern mode %d\n", test_mode);

		pat_idx = (test_mode >= 1 &&
			   test_mode <= ARRAY_SIZE(imx283_tpg_val)) ?
				  test_mode - 1 :
				  0;

		err = imx283_write_reg(s_data, IMX283_REG_TPG_PAT,
				       imx283_tpg_val[pat_idx]);
		if (err)
			return err;

		err = imx283_write_reg(s_data, IMX283_REG_TPG_CTRL, 0x11);
		if (err)
			return err;
	}

	err = imx283_write_reg(s_data, IMX283_REG_XMSTA, 0x00);
	if (err)
		return err;

	/*
	 * Read back the registers that determine frame geometry and
	 * master-mode state so dmesg shows the live sensor config at
	 * stream start. Useful when chasing chansel mismatches since
	 * Tegra's per-line/per-frame counters must match the on-wire
	 * line width and line count.
	 */
	{
		u8 standby = 0xff, xmsta = 0xff;
		u8 mdsel3 = 0, mdsel4 = 0, htrim = 0;
		u8 yout_l = 0, yout_h = 0, wvs_l = 0, wvs_h = 0, obv = 0;
		u8 hmax_l = 0, hmax_h = 0, vmax_l = 0, vmax_m = 0, vmax_h = 0;
		u8 hts_l = 0, hts_h = 0, hte_l = 0, hte_h = 0;
		u8 vwp_l = 0, vwp_h = 0, vwc_l = 0, vwc_h = 0;
		u8 ebd_l = 0, ebd_h = 0;
		u8 tpg_ctrl = 0, tpg_pat = 0;

		imx283_read_reg(s_data, IMX283_REG_STANDBY, &standby);
		imx283_read_reg(s_data, IMX283_REG_XMSTA, &xmsta);
		imx283_read_reg(s_data, IMX283_REG_MDSEL3, &mdsel3);
		imx283_read_reg(s_data, IMX283_REG_MDSEL4, &mdsel4);
		imx283_read_reg(s_data, IMX283_REG_HTRIMMING, &htrim);
		imx283_read_reg(s_data, IMX283_REG_Y_OUT_SIZE_LSB, &yout_l);
		imx283_read_reg(s_data, IMX283_REG_Y_OUT_SIZE_MSB, &yout_h);
		imx283_read_reg(s_data, IMX283_REG_WRITE_VSIZE_LSB, &wvs_l);
		imx283_read_reg(s_data, IMX283_REG_WRITE_VSIZE_MSB, &wvs_h);
		imx283_read_reg(s_data, IMX283_REG_OB_SIZE_V, &obv);
		imx283_read_reg(s_data, IMX283_REG_HMAX_LSB, &hmax_l);
		imx283_read_reg(s_data, IMX283_REG_HMAX_MSB, &hmax_h);
		imx283_read_reg(s_data, IMX283_REG_VMAX_LSB, &vmax_l);
		imx283_read_reg(s_data, IMX283_REG_VMAX_MID, &vmax_m);
		imx283_read_reg(s_data, IMX283_REG_VMAX_MSB, &vmax_h);
		imx283_read_reg(s_data, IMX283_REG_HTRIMMING_START_LSB, &hts_l);
		imx283_read_reg(s_data, IMX283_REG_HTRIMMING_START_MSB, &hts_h);
		imx283_read_reg(s_data, IMX283_REG_HTRIMMING_END_LSB, &hte_l);
		imx283_read_reg(s_data, IMX283_REG_HTRIMMING_END_MSB, &hte_h);
		imx283_read_reg(s_data, IMX283_REG_VWINPOS_LSB, &vwp_l);
		imx283_read_reg(s_data, IMX283_REG_VWINPOS_MSB, &vwp_h);
		imx283_read_reg(s_data, IMX283_REG_VWIDCUT_LSB, &vwc_l);
		imx283_read_reg(s_data, IMX283_REG_VWIDCUT_MSB, &vwc_h);
		imx283_read_reg(s_data, IMX283_REG_EBD_X_OUT_SIZE_LSB, &ebd_l);
		imx283_read_reg(s_data, IMX283_REG_EBD_X_OUT_SIZE_MSB, &ebd_h);
		imx283_read_reg(s_data, IMX283_REG_TPG_CTRL, &tpg_ctrl);
		imx283_read_reg(s_data, IMX283_REG_TPG_PAT, &tpg_pat);

		dev_info(
			tc_dev->dev,
			"start_streaming: STANDBY=0x%02x XMSTA=0x%02x (expect 0x00)\n",
			standby, xmsta);
		dev_info(tc_dev->dev,
			 "  MDSEL3=0x%02x MDSEL4=0x%02x HTRIMMING=0x%02x\n",
			 mdsel3, mdsel4, htrim);
		dev_info(tc_dev->dev,
			 "  HTRIM_START=%u HTRIM_END=%u (line_width=%u)\n",
			 hts_l | (hts_h << 8), hte_l | (hte_h << 8),
			 (hte_l | (hte_h << 8)) - (hts_l | (hts_h << 8)));
		dev_info(tc_dev->dev,
			 "  Y_OUT=%u WRITE_VSIZE=%u OB_V=%u EBD_X=%u\n",
			 yout_l | (yout_h << 8), wvs_l | (wvs_h << 8), obv,
			 ebd_l | (ebd_h << 8));
		dev_info(tc_dev->dev, "  VWINPOS=%u VWIDCUT=%u\n",
			 vwp_l | (vwp_h << 8), vwc_l | (vwc_h << 8));
		dev_info(tc_dev->dev, "  HMAX=%u VMAX=%u\n",
			 hmax_l | (hmax_h << 8),
			 vmax_l | (vmax_m << 8) | (vmax_h << 16));
		dev_info(tc_dev->dev,
			 "  TPG_CTRL=0x%02x TPG_PAT=0x%02x (test_mode=%d)\n",
			 tpg_ctrl, tpg_pat, test_mode);
	}

	return 0;
}

static int imx283_stop_streaming(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	int err = 0;

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	err = imx283_write_reg(s_data, IMX283_REG_STANDBY, IMX283_STBLOGIC);

	return err;
}

static struct camera_common_sensor_ops imx283_common_ops = {
	.numfrmfmts = ARRAY_SIZE(imx283_frmfmt),
	.frmfmt_table = imx283_frmfmt,
	.power_on = imx283_power_on,
	.power_off = imx283_power_off,
	.write_reg = imx283_write_reg,
	.read_reg = imx283_read_reg,
	.parse_dt = imx283_parse_dt,
	.power_get = imx283_power_get,
	.power_put = imx283_power_put,
	.set_mode = imx283_set_mode,
	.start_streaming = imx283_start_streaming,
	.stop_streaming = imx283_stop_streaming,
};

static int imx283_board_setup(struct imx283 *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	u8 reg_val;
	int err = 0;

	/* Skip mclk enable as this camera module has an on-board oscillator */

	err = imx283_power_on(s_data);
	if (err) {
		dev_err(dev, "error during power on sensor (%d)\n", err);
		goto done;
	}

	err = imx283_read_reg(s_data, IMX283_REG_STANDBY, &reg_val);
	if (err) {
		dev_err(dev, "%s: error during i2c read probe (%d)\n", __func__,
			err);
		goto err_reg_probe;
	}

	if (reg_val != IMX283_CHIP_ID) {
		dev_err(dev,
			"%s: unexpected standby reg value: 0x%02x (expected 0x%02x)\n",
			__func__, reg_val, IMX283_CHIP_ID);
		err = -ENODEV;
		goto err_reg_probe;
	}

err_reg_probe:
	imx283_power_off(s_data);

done:
	return err;
}

static int imx283_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops imx283_subdev_internal_ops = {
	.open = imx283_open,
};

#if defined(NV_I2C_DRIVER_STRUCT_PROBE_WITHOUT_I2C_DEVICE_ID_ARG) /* Linux 6.3 */
static int imx283_probe(struct i2c_client *client)
#else
static int imx283_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
#endif
{
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	struct imx283 *priv;
	int err = 0;

	dev_dbg(dev, "probing v4l2 sensor at addr 0x%0x\n", client->addr);

	if (!IS_ENABLED(CONFIG_OF) || !client->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct imx283), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	tc_dev = devm_kzalloc(dev, sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;
	strncpy(tc_dev->name, "imx283", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &imx283_common_ops;
	tc_dev->v4l2sd_internal_ops = &imx283_subdev_internal_ops;
	tc_dev->tcctrl_ops = &imx283_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	err = imx283_board_setup(priv);
	if (err) {
		dev_err(dev, "board setup failed\n");
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_dbg(dev, "detected imx283 sensor\n");

	return 0;
}

#if defined(NV_I2C_DRIVER_STRUCT_REMOVE_RETURN_TYPE_INT) /* Linux 6.1 */
static int imx283_remove(struct i2c_client *client)
#else
static void imx283_remove(struct i2c_client *client)
#endif
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct imx283 *priv;

	if (!s_data) {
		dev_err(&client->dev, "camera common data is NULL\n");
#if defined(NV_I2C_DRIVER_STRUCT_REMOVE_RETURN_TYPE_INT) /* Linux 6.1 */
		return -EINVAL;
#else
		return;
#endif
	}
	priv = (struct imx283 *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);
#if defined(NV_I2C_DRIVER_STRUCT_REMOVE_RETURN_TYPE_INT) /* Linux 6.1 */
	return 0;
#endif
}

static const struct i2c_device_id imx283_id[] = { { "imx283", 0 }, {} };

MODULE_DEVICE_TABLE(i2c, imx283_id);

static struct i2c_driver imx283_i2c_driver = {
	.driver = {
		.name = "imx283",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(imx283_of_match),
	},
	.probe = imx283_probe,
	.remove = imx283_remove,
	.id_table = imx283_id,
};

module_i2c_driver(imx283_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for Sony IMX283");
MODULE_AUTHOR("UAB Kurokesu");
MODULE_LICENSE("GPL v2");
