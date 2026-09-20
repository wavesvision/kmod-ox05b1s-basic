// SPDX-License-Identifier: GPL-2.0
/*
 * OmniVision OX05B1S RGB-IR Image Sensor driver
 *
 * Copyright (c) 2024-2025 Waves Vision Ltd. <charlesb@wavesvision.com>
 *
 * Adapated from imx219.c
 *
 * Register settings taken from TI driver, copyright (c) 2023-2024 Abhishek Sharma <abhishek.sharma@ti.com>
 *   https://openbeagle.org/beagleboard/linux/-/raw/v6.1.80-ti-arm64-r56/drivers/media/i2c/ox05b1s.c?ref_type=heads
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>

#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>

#include "ox05b1s.h"
#include "ox05b1s_regs_common.h"
#include "ox05b1s_regs_2592x1944.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 2, 0)


static inline struct ox05b1s_sensor *to_ox05b1s(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct ox05b1s_sensor, subdev);
}

/* Sensor: Misc							*/
/****************************************************************/
static long __ox05b1s_get_link_frequency(struct ox05b1s_sensor *sensor)
{
	switch (sensor->num_lanes) {
	case 2:
		return ox05b1s_link_freq_2lanes[__ffs(sensor->link_freq_bitmap)];
		break;
	case 4:
		return ox05b1s_link_freq_4lanes[__ffs(sensor->link_freq_bitmap)];
		break;
	default:
		dev_err(sensor->dev, "%s: invalid number of MIPI lanes: %u\n", __func__, sensor->num_lanes);
		return 0;
	}

	return 0;
}

static unsigned long __ox05b1s_get_pixel_rate(struct ox05b1s_sensor *sensor)
{
	switch (sensor->num_lanes) {
	case 2:
		switch (__ox05b1s_get_link_frequency(sensor)) {
		case OX05B1S_CFG_LINK_FREQ_450M:
			return OX05B1S_CFG_PIXEL_RATE;
		default:
			dev_err(sensor->dev, "%s: invalid 2 MIPI lane link frequency\n", __func__);
			return 0;
		}

		break;
	case 4:
		switch (__ox05b1s_get_link_frequency(sensor)) {
		case OX05B1S_CFG_LINK_FREQ_450M:
			return OX05B1S_CFG_PIXEL_RATE * 2;
		case OX05B1S_CFG_LINK_FREQ_225M:
			return OX05B1S_CFG_PIXEL_RATE;
		default:
			dev_err(sensor->dev, "%s: invalid 4 MIPI lane link frequency\n", __func__);
			return 0;
		}

		break;
	default:
		dev_err(sensor->dev, "%s: invalid number of MIPI lanes: %u\n", __func__, sensor->num_lanes);
		return 0;
	}

	return 0;
}

static int __ox05b1s_set_exposure_auto(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Turn on AEC by clearing bit 3.*/
	regval = val == V4L2_EXPOSURE_MANUAL ? OX05B1S_REG_EC_A_CTRL03_AEC_EN : 0;
	return cci_update_bits(sensor->regmap, OX05B1S_REG_EC_A_CTRL03, OX05B1S_REG_EC_A_CTRL03_AEC_EN, regval, NULL);
}

static int __ox05b1s_set_exposure_aec_cfg(struct ox05b1s_sensor *sensor)
{
	int error = 0;
	s32 regval;

	/* Configure stable-in window (upper) */
	regval = sensor->v4l2_aec_target->val + sensor->v4l2_aec_stable_in->val;
	if (regval > 0xff) regval = 0xff;
	dev_dbg(sensor->dev, "Setting AEC WPT: %u\n", regval);
	cci_write(sensor->regmap, OX05B1S_REG_AEC_WPT, regval, &error);
	/* Configure stable-in window (lower) */
	regval = sensor->v4l2_aec_target->val - sensor->v4l2_aec_stable_in->val;
	if (regval < 0) regval = 0;
	dev_dbg(sensor->dev, "Setting AEC BPT: %u\n", regval);
	cci_write(sensor->regmap, OX05B1S_REG_AEC_BPT, regval, &error);

	/* Configure stable-out window (upper) */
	regval = sensor->v4l2_aec_target->val + sensor->v4l2_aec_stable_out->val;
	if (regval > 0xff) regval = 0xff;
	dev_dbg(sensor->dev, "Setting AEC WPT2: %u\n", regval);
	cci_write(sensor->regmap, OX05B1S_REG_AEC_WPT2, regval, &error);
	/* Configure stable-out window (lower) */
	regval = sensor->v4l2_aec_target->val - sensor->v4l2_aec_stable_out->val;
	if (regval < 0) regval = 0;
	dev_dbg(sensor->dev, "Setting AEC BPT2: %u\n", regval);
	cci_write(sensor->regmap, OX05B1S_REG_AEC_BPT2, regval, &error);

	return error;
}

static int __ox05b1s_set_exposure(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Limit value to 16 bit */
	regval = val & 0xffff;

	/* Update exposure */
	return cci_write(sensor->regmap, OX05B1S_REG_EC_A_EXPO, regval, NULL);
}

static int __ox05b1s_set_flip_horizontal(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;
	int error = 0;

	/* Turn on horizontal flip by setting bit 2 */
	regval = val ? OX05B1S_REG_TIMING_CTRL_REG21_HFLIP : 0;
	cci_update_bits(sensor->regmap, OX05B1S_REG_TIMING_CTRL_REG21, OX05B1S_REG_TIMING_CTRL_REG21_HFLIP, regval, &error);

	return error;
}

static int __ox05b1s_set_flip_vertical(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;
	int error = 0;

	/* Turn on vertical flip by setting bit 2 */
	regval = val ? OX05B1S_REG_TIMING_CTRL_REG20_VFLIP : 0;
	cci_update_bits(sensor->regmap, OX05B1S_REG_TIMING_CTRL_REG20, OX05B1S_REG_TIMING_CTRL_REG20_VFLIP, regval, &error);

	return error;
}

static int __ox05b1s_set_gain_auto(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Turn on AGC by clearing bit 7.*/
	regval = val ? 0 : OX05B1S_REG_EC_A_CTRL03_AGC_EN;
	return cci_update_bits(sensor->regmap, OX05B1S_REG_EC_A_CTRL03, OX05B1S_REG_EC_A_CTRL03_AGC_EN, regval, NULL);
}

static int __ox05b1s_set_gain_agc_cfg(struct ox05b1s_sensor *sensor)
{
	int error = 0;
	u32 regval;

	/* Set max total gain */
	regval = sensor->v4l2_agc_total_max->val & 0x7ff;
	dev_dbg(sensor->dev, "Setting AGC total gain max: 0x%04x\n", regval);
	cci_write(sensor->regmap, OX05B1S_REG_AEC_GAIN_CEIL, regval, &error);

	/* Set max analogue gain, register split */
	regval = ((sensor->v4l2_agc_analogue_max->val & 0xf0) << 4) | (sensor->v4l2_agc_analogue_max->val & 0x0f);
	dev_dbg(sensor->dev, "Setting AGC analogue gain max: 0x%04x\n", regval);
	cci_write(sensor->regmap, OX05B1S_REG_EC_A_MAX_GAIN_COARSE, regval, &error);

	return error;
}

static int __ox05b1s_set_gain_analogue(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Limit value to 8 bits */
	regval = val & 0xff;
	/* Value is split between 2 registers */
	regval = regval << 4;

	/* Update analagoue gain */
	return cci_write(sensor->regmap, OX05B1S_REG_EC_A_MANUAL_AGAIN, regval, NULL);
}

static int __ox05b1s_set_gain_digital(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Limit value to 14 bits */
	regval = val & 0x3fff;
	/* Value is split between 3 registers */
	regval = regval << 6;

	/* Update digital gain */
	return cci_write(sensor->regmap, OX05B1S_REG_EC_A_MANUAL_DGAIN, regval, NULL);
}

static int __ox05b1s_s_hblank(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Limit value to 16 bit */
	regval = val & 0xffff;

	/* Update horizontal blanking */
	return cci_write(sensor->regmap, OX05B1S_REG_TIMING_CTRL_HTS, regval, NULL);
}

static int __ox05b1s_set_light_freq(struct ox05b1s_sensor *sensor, int value) {
	int error = 0, exposure_max;
	u32 regval;

	/* Calculate max exposure */
	exposure_max = sensor->current_mode->format.height + sensor->v4l2_vblank->val - OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET;

	if (value == V4L2_CID_POWER_LINE_FREQUENCY_60HZ) {
		/* Set max exposure: 60Hz */
		regval = exposure_max & 0x0fff;
		cci_write(sensor->regmap, OX05B1S_REG_AEC_MAX2_EXPO_60HZ, regval, &error);

		/* Calculate the banding value: 60Hz */
		regval = (120 / (sensor->max_fps.denominator / sensor->max_fps.numerator)) * 2;
		cci_write(sensor->regmap, OX05B1S_REG_AEC_MAX_BANDS_60HZ, regval, &error);

		/* Set auto step, and auto step value: 60Hz */
		regval = 0x30;
		cci_write(sensor->regmap, OX05B1S_REG_AEC_CTRL85, regval, &error);

	} else if (value == V4L2_CID_POWER_LINE_FREQUENCY_50HZ) {
		/* Set max exposure: 50Hz */
		regval = exposure_max & 0x0fff;
		cci_write(sensor->regmap, OX05B1S_REG_AEC_MAX2_EXPO_50HZ, regval, &error);

		/* Calculate the banding value: 50Hz */
		regval = (100 / (sensor->max_fps.denominator / sensor->max_fps.numerator)) * 2;
		cci_write(sensor->regmap, OX05B1S_REG_AEC_MAX_BANDS_50HZ, regval, &error);

		/* Set auto step, and auto step value: 50Hz */
		regval = 0xb0;
		cci_write(sensor->regmap, OX05B1S_REG_AEC_CTRL85, regval, &error);

	} else {
		/* Line frequency setting disabled */
	}

	return error;
}

static int __ox05b1s_set_mipi_lanes(struct ox05b1s_sensor *sensor)
{
	u32 regval;

	/* Check that value is valid */
	if ((sensor->num_lanes != 2) && (sensor->num_lanes != 4))
		return -EINVAL;

	/* Set number of MIPI lanes to use */
	regval = sensor->num_lanes << 4;
	return cci_update_bits(sensor->regmap, OX05B1S_REG_COMMON_MIPI_CFG, OX05B1S_REG_COMMON_MIPI_CFG_LANES, regval, NULL);
}

static int __ox05b1s_set_test_pattern(struct ox05b1s_sensor *sensor, int value)
{
	int error = 0;

	if (value) {
		/* Enable PRE_ISP module */
		cci_write(sensor->regmap, OX05B1S_REG_ISP_REG00, 0x6f, &error);
	} else {
		/* Disable PRE_ISP module */
		cci_write(sensor->regmap, OX05B1S_REG_ISP_REG00, 0x6e, &error);
	}
	if ((value & OX05B1S_REG_TEST_MODE_RED) > 0) {
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_RED, 0x0fff, &error);
	} else {
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_RED, 0x0000, &error);
	}
	if ((value & OX05B1S_REG_TEST_MODE_GREEN) > 0) {
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_GREENRED, 0x0fff, &error);
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_GREENBLUE, 0x0fff, &error);
	} else {
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_GREENRED, 0x0000, &error);
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_GREENBLUE, 0x0000, &error);
	}
	if ((value & OX05B1S_REG_TEST_MODE_BLUE) > 0) {
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_BLUE, 0x0fff, &error);
	} else {
		cci_write(sensor->regmap, OX05B1S_REG_TEST_CONST_BLUE, 0x0000, &error);
	}
	if ((value & (OX05B1S_REG_TEST_MODE_RED | OX05B1S_REG_TEST_MODE_GREEN | OX05B1S_REG_TEST_MODE_BLUE)) > 0) {
		/* Enable constant colour */
		cci_update_bits(sensor->regmap, OX05B1S_REG_TEST_MODE2, OX05B1S_REG_TEST_MODE2_CONST_CLR, OX05B1S_REG_TEST_MODE2_CONST_CLR, &error);
	} else {
		/* Disable constant colour */
		cci_update_bits(sensor->regmap, OX05B1S_REG_TEST_MODE2, OX05B1S_REG_TEST_MODE2_CONST_CLR, 0x0, &error);
	}
	cci_write(sensor->regmap, OX05B1S_REG_TEST_MODE, (ox05b1s_test_pattern_val[value] & 0xff), &error);

	return error;
}

static int __ox05b1s_set_vblank(struct ox05b1s_sensor *sensor, u32 val)
{
	u32 regval;

	/* Limit value to 16 bit */
	regval = val & 0xffff;

	/* Update vertical blanking */
	return cci_write(sensor->regmap, OX05B1S_REG_TIMING_CTRL_VTS, regval, NULL);
}

static int __ox05b1s_set_virtual_channel(struct ox05b1s_sensor *sensor, int channel)
{
	return cci_write(sensor->regmap, OX05B1S_REG_MIPI_CORE_REG13, channel, NULL);
}

static int __ox05b1s_stream_mode(struct ox05b1s_sensor *sensor)
{
	const struct ox05b1s_mode *mode = sensor->current_mode;
	u64 regval;
	int ret;

	/* Check that the device is not streaming */
	ret = cci_read(sensor->regmap, OX05B1S_REG_MODE_SEL, &regval, NULL);
	if (ret) {
		dev_err(sensor->dev, "%s: failed to read enable streaming register\n", __func__);
		return ret;
	}
	if ((regval & OX05B1S_REG_MODE_SEL_STREAM_EN) != 0)
		return -EBUSY;

	/* Change configuration depending on the number of MIPI lanes available */
	switch (sensor->num_lanes) {
	case 2:
		/* Configure PLLs (PLLs need to be first) */
		switch (__ox05b1s_get_link_frequency(sensor)) {
		case OX05B1S_CFG_LINK_FREQ_450M:
			/* Set FPS: 30 */
			sensor->max_fps.denominator = 300000;
			sensor->max_fps.numerator = 10000;
			ret = cci_multi_reg_write(sensor->regmap, ox05b1s_pllcfg_30fps, ARRAY_SIZE(ox05b1s_pllcfg_30fps), NULL);
			break;
		default:
			dev_err(sensor->dev, "%s: invalid link frequency selected\n", __func__);
			break;
		}
		/* Set HTS */
		if (__ffs(sensor->link_freq_bitmap) <= ARRAY_SIZE(ox05b1s_linkfreq_2_hts)) {
			ret |= __ox05b1s_s_hblank(sensor, ox05b1s_linkfreq_2_hts[__ffs(sensor->link_freq_bitmap)]);
		} else {
			dev_err(sensor->dev, "%s: invalid link frequency bitmap\n", __func__);
			return -EINVAL;
		}
		break;
	case 4:
		/* Configure PLLs (PLLs need to be first) */
		switch (__ox05b1s_get_link_frequency(sensor)) {
		case OX05B1S_CFG_LINK_FREQ_225M:
			/* Set FPS: 30 */
			sensor->max_fps.denominator = 300000;
			sensor->max_fps.numerator = 10000;
			ret = cci_multi_reg_write(sensor->regmap, ox05b1s_pllcfg_30fps, ARRAY_SIZE(ox05b1s_pllcfg_30fps), NULL);
			break;
		case OX05B1S_CFG_LINK_FREQ_450M:
			/* Set FPS: 30 */
			sensor->max_fps.denominator = 600000;
			sensor->max_fps.numerator = 10000;
			ret = cci_multi_reg_write(sensor->regmap, ox05b1s_pllcfg_60fps, ARRAY_SIZE(ox05b1s_pllcfg_60fps), NULL);
			break;
		default:
			dev_err(sensor->dev, "%s: invalid link frequency selected\n", __func__);
			break;
		}
		/* Set HTS */
		if (__ffs(sensor->link_freq_bitmap) <= ARRAY_SIZE(ox05b1s_linkfreq_2_hts)) {
			ret |= __ox05b1s_s_hblank(sensor, ox05b1s_linkfreq_2_hts[__ffs(sensor->link_freq_bitmap)]);
		} else {
			dev_err(sensor->dev, "%s: invalid link frequency bitmap\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		dev_err(sensor->dev, "%s: incorrect number of MIPI lanes selected!\n", __func__);
		return -EINVAL;
	}
	if (ret) {
		dev_err(sensor->dev, "%s: failed to write sensor PLL registers\n", __func__);
		return ret;
	}

	/* Configure common values */
	ret = cci_multi_reg_write(sensor->regmap, ox05b1s_linear_common, ARRAY_SIZE(ox05b1s_linear_common), NULL);
	if (ret) {
		dev_err(sensor->dev, "%s: failed to write to sensor default registers\n", __func__);
		return ret;
	}

	/* Configure MIPI lanes */
	ret = __ox05b1s_set_mipi_lanes(sensor);
	if (ret) {
		dev_err(sensor->dev, "%s: failed to set number of MIPI lanes to %d\n", __func__, sensor->num_lanes);
		return ret;
	}

	/* Set default virtual channel */
	ret = __ox05b1s_set_virtual_channel(sensor, 0);
	if (ret) {
		dev_err(sensor->dev, "%s: failed to set virtual channel\n", __func__);
		return ret;
	}

	/* Configure sensor resolution */
	ret = cci_multi_reg_write(sensor->regmap, mode->reg_list, mode->reg_list_size, NULL);
	if (ret) {
		dev_err(sensor->dev, "%s: failed to write sensor mode registers\n", __func__);
		return ret;
	}

	/* Set test pattern if applicable */
	ret = __ox05b1s_set_test_pattern(sensor, sensor->test_mode);
	if (ret) {
		dev_err(sensor->dev, "%s: failed to configure test mode registers\n", __func__);
		return ret;
	}

	return 0;
}

static int __ox05b1s_stream_start(struct ox05b1s_sensor *sensor)
{
	int ret;

	/* Apply register settings according to mode */
	ret = __ox05b1s_stream_mode(sensor);
	if (ret)
		return ret;

	/* Apply customized values from user when stream starts. */
	ret =  __v4l2_ctrl_handler_setup(sensor->subdev.ctrl_handler);
	if (ret)
		return ret;

	/* Set active */
	ret = cci_write(sensor->regmap, OX05B1S_REG_MODE_SEL, OX05B1S_REG_MODE_SEL_STREAM_EN, NULL);
	if (ret < 0)
		return ret;

	/* No communication is possible for a while after exiting standby.
	 * we want the sensor to have sufficient time to process
	 * the new configurations. The same type of delays have been programmed
	 * in the OV2312 driver.
	 * TODO: check if there is a status register to poll for sensor readiness.
	 */
	msleep(20);

	return 0;
}

static int __ox05b1s_stream_stop(struct ox05b1s_sensor *sensor)
{
	int ret;

	/* Set standby */
	ret = cci_write(sensor->regmap, OX05B1S_REG_MODE_SEL, 0, NULL);
	if (ret < 0)
		return ret;

	/* No communication is possible for a while after entering standby */
	usleep_range(10000, 20000);
	return 0;
}


/* Sensor: Video ops						*/
/****************************************************************/
static const struct v4l2_subdev_video_ops ox05b1s_video_ops = {
	.s_stream		= v4l2_subdev_s_stream_helper,
};


/* Sensor: Pad ops						*/
/****************************************************************/
/*
 * The supported formats.
 * This table MUST contain 4 entries per format, to cover the various flip
 * combinations in the order
 * - no flip
 * - h flip
 * - v flip
 * - h&v flips
 */
static const u32 ox05b1s_mbus_formats[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,			/* No h/v flip */
	MEDIA_BUS_FMT_SGRBG10_1X10,			/* Horizontal flip */
	MEDIA_BUS_FMT_SGBRG10_1X10,			/* Vertical flip */
	MEDIA_BUS_FMT_SRGGB10_1X10,			/* Horizontal & vertical flip */
};

/* Get bayer format based on flip setting. */
static u32 __ox05b1s_get_format_code(struct ox05b1s_sensor *sensor, u32 code)
{
	unsigned int i = 0;

	/* Try to match format that was passed to function */
	for (i = 0; i < ARRAY_SIZE(ox05b1s_mbus_formats); i++)
		if (ox05b1s_mbus_formats[i] == code)
			break;

	/* Check whether it's matched the requested format */
	if (i >= ARRAY_SIZE(ox05b1s_mbus_formats))
		i = 0;

	/* Strip flip selection from selected format, and re-apply */
	i = (i & ~3) | (sensor->v4l2_vflip->val ? 2 : 0) | (sensor->v4l2_hflip->val ? 1 : 0);

	return ox05b1s_mbus_formats[i];
}

static void __ox05b1s_update_pad_format(struct ox05b1s_sensor *sensor,
				     const struct ox05b1s_mode *mode,
				     struct v4l2_mbus_framefmt *fmt, u32 code)
{
	/* Bayer order varies with flips */
	fmt->code		= __ox05b1s_get_format_code(sensor, code);
	fmt->width		= mode->format.width;
	fmt->height		= mode->format.height;
	fmt->field		= V4L2_FIELD_NONE;
	fmt->colorspace		= V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc		= V4L2_YCBCR_ENC_601;
	fmt->quantization	= V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func		= V4L2_XFER_FUNC_NONE;
}

static int ox05b1s_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);

	if (code->index >= (ARRAY_SIZE(ox05b1s_mbus_formats) / 4))
		return -EINVAL;

	code->code = __ox05b1s_get_format_code(sensor, ox05b1s_mbus_formats[code->index * 4]);

	return 0;
}

static int ox05b1s_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *format = v4l2_subdev_state_get_format(state, 0);
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);
	const struct ox05b1s_mode *mode;


	mode = v4l2_find_nearest_size(ox05b1s_supported_modes,
				      ARRAY_SIZE(ox05b1s_supported_modes),
				      format.width, format.height,
				      fmt->format.width, fmt->format.height);

	__ox05b1s_update_pad_format(sensor, mode, &fmt->format, fmt->format.code);
	*format = fmt->format;







	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {


		int pixel_rate;

















		/* Scale the pixel rate based on the mode specific factor */
		pixel_rate = __ox05b1s_get_pixel_rate(sensor);
		__v4l2_ctrl_modify_range(sensor->v4l2_pixel_rate, pixel_rate,
					 pixel_rate, 1, pixel_rate);
	}

	/* Update selected link frequency */
	for (int i = 0; i < ARRAY_SIZE(ox05b1s_link_freq_menu); i++) {
		if (__ox05b1s_get_link_frequency(sensor) == ox05b1s_link_freq_menu[i])
			__v4l2_ctrl_s_ctrl(sensor->v4l2_link_freq, i);
	}

	return 0;
}

static int ox05b1s_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *v4l2_subdev_state_get_crop(state, 0);
		return 0;

	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = OX05B1S_SENSOR_NATIVE_WIDTH;
		sel->r.height = OX05B1S_SENSOR_NATIVE_HEIGHT;

		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = OX05B1S_SENSOR_PIXEL_ARRAY_TOP;
		sel->r.left = OX05B1S_SENSOR_PIXEL_ARRAY_LEFT;
		sel->r.width = OX05B1S_SENSOR_PIXEL_ARRAY_WIDTH;
		sel->r.height = OX05B1S_SENSOR_PIXEL_ARRAY_HEIGHT;

		return 0;
	}

	return -EINVAL;
}

static int ox05b1s_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);
	u32 code;

	if (fse->index >= ARRAY_SIZE(ox05b1s_supported_modes))
		return -EINVAL;

	code = __ox05b1s_get_format_code(sensor, fse->code);
	if (fse->code != code)
		return -EINVAL;

	fse->min_width = ox05b1s_supported_modes[fse->index].format.width;
	fse->max_width = fse->min_width;
	fse->min_height = ox05b1s_supported_modes[fse->index].format.height;
	fse->max_height = fse->min_height;

	return 0;
}

static int ox05b1s_enable_streams(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);
	int ret;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	/* Set stream on */
	ret = __ox05b1s_stream_start(sensor);
	if (ret)
		goto ox05b1s_enable_streams_error_rpm_put;

	/* Disable vflip and hflip as they cannot change during streaming */
	__v4l2_ctrl_grab(sensor->v4l2_vflip, true);
	__v4l2_ctrl_grab(sensor->v4l2_hflip, true);

	return 0;

ox05b1s_enable_streams_error_rpm_put:
	pm_runtime_put_autosuspend(sensor->dev);
	return ret;
}

static int ox05b1s_disable_streams(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);
	int ret = 0;

	/* Set stream off */
	ret = __ox05b1s_stream_stop(sensor);
	if (ret)
		dev_err(sensor->dev, "%s failed to set stream\n", __func__);

	/* Re-enable flip controls */
	__v4l2_ctrl_grab(sensor->v4l2_vflip, false);
	__v4l2_ctrl_grab(sensor->v4l2_hflip, false);

	pm_runtime_put_autosuspend(sensor->dev);

	return ret;
}

static const struct v4l2_subdev_pad_ops ox05b1s_pad_ops = {
	.enum_mbus_code		= ox05b1s_enum_mbus_code,
	.get_fmt		= v4l2_subdev_get_fmt,
	.set_fmt		= ox05b1s_set_pad_format,
	.get_selection		= ox05b1s_get_selection,
	.enum_frame_size	= ox05b1s_enum_frame_size,
	.enable_streams		= ox05b1s_enable_streams,
	.disable_streams	= ox05b1s_disable_streams,
};

static const struct v4l2_subdev_ops ox05b1s_subdev_ops = {
	.video			= &ox05b1s_video_ops,
	.pad			= &ox05b1s_pad_ops,
};


/* Sensor: Internal ops						*/
/****************************************************************/
static int ox05b1s_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
		.format = {
			.code = OX05B1S_V4L2_DEFAULT_MODE.format.code,
			.width = OX05B1S_V4L2_DEFAULT_MODE.format.width,
			.height = OX05B1S_V4L2_DEFAULT_MODE.format.height,
		},
	};

	ox05b1s_set_pad_format(sd, state, &fmt);

	return 0;
}

static const struct v4l2_subdev_internal_ops ox05b1s_internal_ops = {
	.init_state	= ox05b1s_init_state,
};


/* Sensor: V4L2 controls					*/
/****************************************************************/
static int ox05b1s_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ox05b1s_sensor *sensor = container_of(ctrl->handler, struct ox05b1s_sensor, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->subdev);
	int ret = 0;

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = sensor->current_mode->format.height + ctrl->val - OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET;
		exposure_def = min(exposure_max, OX05B1S_CTRL_EXPOSURE_DEFAULT);
		__v4l2_ctrl_modify_range(sensor->v4l2_exposure,
					sensor->v4l2_exposure->minimum,
					exposure_max, sensor->v4l2_exposure->step,
					exposure_def);
	}

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret =  __ox05b1s_set_gain_analogue(sensor, ctrl->val);
		break;
	case V4L2_CID_AUTOGAIN:
		ret = __ox05b1s_set_gain_auto(sensor, ctrl->val);
		break;



	case V4L2_CID_DIGITAL_GAIN:
		ret =  __ox05b1s_set_gain_digital(sensor, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ret = __ox05b1s_set_exposure_auto(sensor, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = __ox05b1s_set_exposure(sensor, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		__ox05b1s_set_flip_horizontal(sensor, ctrl->val);
		break;
	case V4L2_CID_OX05B1S_AEC_TARGET:
	case V4L2_CID_OX05B1S_AEC_STABLE_IN:
	case V4L2_CID_OX05B1S_AEC_STABLE_OUT:
		ret = __ox05b1s_set_exposure_aec_cfg(sensor);
		break;
	case V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX:
	case V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX:
		ret = __ox05b1s_set_gain_agc_cfg(sensor);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ret = __ox05b1s_set_light_freq(sensor, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		/* Save the selected test pattern mode */
		sensor->test_mode = ctrl->val;
		__ox05b1s_set_test_pattern(sensor, sensor->test_mode);
		break;
	case V4L2_CID_VBLANK:
		ret = __ox05b1s_set_vblank(sensor, sensor->current_mode->format.height + ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		__ox05b1s_set_flip_vertical(sensor, ctrl->val);
		break;

	/* Read-only, but we adjust it based on mode. */
	case V4L2_CID_PIXEL_RATE:
	case V4L2_CID_HBLANK:
		/* Read-only, but we adjust it based on mode. */
		break;

	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static int ox05b1s_try_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ox05b1s_sensor *sensor = container_of(ctrl->handler, struct ox05b1s_sensor, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_OX05B1S_AEC_STABLE_IN:
		/* Check whether stable in value is greater than current stable out value */
		if (ctrl->val > sensor->v4l2_aec_stable_out->val)
			ctrl->val = sensor->v4l2_aec_stable_out->val;
		break;
	case V4L2_CID_OX05B1S_AEC_STABLE_OUT:
		/* Check whether stable out value is less than current stable in value */
		if (ctrl->val < sensor->v4l2_aec_stable_in->val)
			ctrl->val = sensor->v4l2_aec_stable_in->val;
		break;
	case V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX:
		/* Check whether analogue gain is greater than total gain */
		if (ctrl->val > sensor->v4l2_agc_total_max->val)
			ctrl->val = sensor->v4l2_agc_total_max->val;
		break;
	case V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX:
		/* Check whether total gain is less than analogue gain */
		if (ctrl->val < sensor->v4l2_agc_analogue_max->val)
			ctrl->val = sensor->v4l2_agc_analogue_max->val;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ox05b1s_ctrl_ops = {
	.s_ctrl		= ox05b1s_set_ctrl,
	.try_ctrl	= ox05b1s_try_ctrl,
};

/* Initialize control handlers */
static int __ox05b1s_init_controls(struct ox05b1s_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->subdev);
	struct device *dev = &client->dev;
	const struct ox05b1s_mode *mode = sensor->current_mode;
	struct v4l2_ctrl_handler *ctrl_hdlr;
	
	int exposure_max, exposure_def;
	int ret;

	ctrl_hdlr = &sensor->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, OX05B1S_V4L2_CTRLS_MAX);
	if (ret) {
		dev_err(dev, "%s: failed to initialise V4L2 control handler\n", __func__);
		return ret;
	}

	/* Added controls for manual adjustment */
	exposure_max = sensor->current_mode->vts_def - OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET;
	exposure_def = min(exposure_max, OX05B1S_CTRL_EXPOSURE_MIN);
	sensor->v4l2_exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_EXPOSURE,
							OX05B1S_CTRL_EXPOSURE_MIN,
							exposure_max, OX05B1S_CTRL_EXPOSURE_STEP,
							exposure_def);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: exposure [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	sensor->v4l2_auto_exposure = v4l2_ctrl_new_std_menu(ctrl_hdlr, &ox05b1s_ctrl_ops,
								V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL,
								0, OX05B1S_AUTOEXPO_DEFAULT);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: auto exposure [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Add custom controls for AEC */
	v4l2ctrl_aec_target.ops = &ox05b1s_ctrl_ops;
	sensor->v4l2_aec_target = v4l2_ctrl_new_custom(ctrl_hdlr, &v4l2ctrl_aec_target, NULL);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: AEC target [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	v4l2ctrl_aec_stable_in.ops = &ox05b1s_ctrl_ops;
	sensor->v4l2_aec_stable_in = v4l2_ctrl_new_custom(ctrl_hdlr, &v4l2ctrl_aec_stable_in, NULL);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: AEC stable in [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	v4l2ctrl_aec_stable_out.ops = &ox05b1s_ctrl_ops;
	sensor->v4l2_aec_stable_out = v4l2_ctrl_new_custom(ctrl_hdlr, &v4l2ctrl_aec_stable_out, NULL);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: AEC stable out [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Analogue gain */
	sensor->v4l2_again = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
						V4L2_CID_ANALOGUE_GAIN, 0,
						OX05B1S_CTRL_ANALOGUE_GAIN_MAX, 1,
						OX05B1S_CTRL_ANALOGUE_GAIN_DEFAULT);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: analogue gain [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Digital gain */
	sensor->v4l2_dgain = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
						V4L2_CID_DIGITAL_GAIN, 0,
						OX05B1S_CTRL_DIGITAL_GAIN_MAX, 1,
						OX05B1S_CTRL_DIGITAL_GAIN_DEFAULT);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: digital gain [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Auto gain */
	sensor->v4l2_auto_gain = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_AUTOGAIN, 0, 1, 1, OX05B1S_AUTOGAIN_DEFAULT);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: auto gain [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Added custom controls for AGC */
	v4l2ctrl_agc_analogue_max.ops = &ox05b1s_ctrl_ops;
	sensor->v4l2_agc_analogue_max = v4l2_ctrl_new_custom(ctrl_hdlr, &v4l2ctrl_agc_analogue_max, NULL);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: AGC analogue max [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	v4l2ctrl_agc_total_max.ops = &ox05b1s_ctrl_ops;
	sensor->v4l2_agc_total_max = v4l2_ctrl_new_custom(ctrl_hdlr, &v4l2ctrl_agc_total_max, NULL);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: ACC total max [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}










	/* Light flicker frequency */
	sensor->v4l2_light_freq = v4l2_ctrl_new_std_menu(ctrl_hdlr, &ox05b1s_ctrl_ops,
								V4L2_CID_POWER_LINE_FREQUENCY,
								V4L2_CID_POWER_LINE_FREQUENCY_60HZ, ~0x7,
								sensor->pwr_line_freq == 60 ? V4L2_CID_POWER_LINE_FREQUENCY_60HZ : V4L2_CID_POWER_LINE_FREQUENCY_50HZ);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: light frequency [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Horizontal Flip */
	sensor->v4l2_hflip = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (sensor->v4l2_hflip)
		sensor->v4l2_hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: horizontal flip [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Vertical Flip */
	sensor->v4l2_vflip = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (sensor->v4l2_vflip)
		sensor->v4l2_vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: vertical flip [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Test Pattern */
	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &ox05b1s_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ox05b1s_test_pattern_menu) - 1,
				     0, 0, ox05b1s_test_pattern_menu);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: test pattern[%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* By default, PIXEL_RATE is read only */
	sensor->v4l2_pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_PIXEL_RATE,
							__ox05b1s_get_pixel_rate(sensor),
							__ox05b1s_get_pixel_rate(sensor), 1,
							__ox05b1s_get_pixel_rate(sensor));
	if (sensor->v4l2_pixel_rate)
		sensor->v4l2_pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: pixel rate [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Link frequency */
	sensor->v4l2_link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_LINK_FREQ,
							ARRAY_SIZE(ox05b1s_link_freq_menu) - 1, 0,
							ox05b1s_link_freq_menu);
	if (sensor->v4l2_link_freq)
		sensor->v4l2_link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: link frequency [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

	/* Vertical Blanking */
	sensor->v4l2_vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ox05b1s_ctrl_ops,
							V4L2_CID_VBLANK, OX05B1S_VBLANK_MIN,
							OX05B1S_VTS_MAX - mode->format.height, 1,
							mode->vts_def - mode->format.height);
	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&sensor->client->dev, "failed to init V4L2 control: vertical blanking [%d]\n", ret);
		goto __ox05b1s_init_controls_error_free_handler;
	}

















	/* Save control handler */
	sensor->subdev.ctrl_handler = ctrl_hdlr;

	return 0;

__ox05b1s_init_controls_error_free_handler:
	v4l2_ctrl_handler_free(ctrl_hdlr);

	return ret;
}

static void ox05b1s_free_controls(struct ox05b1s_sensor *sensor)
{
	v4l2_ctrl_handler_free(sensor->subdev.ctrl_handler);
}


/* Sensor: Power						*/
/****************************************************************/
static int ox05b1s_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);
	int ret;

	ret = regulator_bulk_enable(OX05B1S_NUM_SUPPLIES, sensor->supplies);
	if (ret) {
		dev_err(dev, "%s: failed to enable regulators\n", __func__);
		return ret;
	}

	ret = clk_prepare_enable(sensor->xclk);
	if (ret) {
		dev_err(dev, "%s: failed to enable clock\n", __func__);
		goto ox05b1s_power_on_error_reg_off;
	}

	return 0;

ox05b1s_power_on_error_reg_off:
	regulator_bulk_disable(OX05B1S_NUM_SUPPLIES, sensor->supplies);

	return ret;
}

static int ox05b1s_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);

	regulator_bulk_disable(OX05B1S_NUM_SUPPLIES, sensor->supplies);
	clk_disable_unprepare(sensor->xclk);

	return 0;
}

static const struct dev_pm_ops ox05b1s_pm_ops = {
	SET_RUNTIME_PM_OPS(ox05b1s_power_off, ox05b1s_power_on, NULL)
};


/* Sensor: Initialisation					*/
/****************************************************************/
static int __ox05b1s_check_hwcfg(struct device *dev, struct ox05b1s_sensor *sensor)
{
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "endpoint node not found\n");

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		ret = dev_err_probe(dev, -EINVAL, "could not parse endpoint\n");
		goto __ox05b1s_check_hwcfg_error_out;
	}

	/* Check the number of MIPI CSI2 data lanes */
	if ((ep_cfg.bus.mipi_csi2.num_data_lanes != 2) && (ep_cfg.bus.mipi_csi2.num_data_lanes != 4)) {
		ret = dev_err_probe(dev, -EINVAL, "only 2 or 4 data lanes are currently supported: %u\n", ep_cfg.bus.mipi_csi2.num_data_lanes);
		goto __ox05b1s_check_hwcfg_error_out;
	}
	sensor->num_lanes = ep_cfg.bus.mipi_csi2.num_data_lanes;

	/* Check the link frequency set in device tree */
	switch (sensor->num_lanes) {
	case 2:
		ret = v4l2_link_freq_to_bitmap(dev,
					       ep_cfg.link_frequencies,
					       ep_cfg.nr_of_link_frequencies,
					       ox05b1s_link_freq_2lanes,
					       ARRAY_SIZE(ox05b1s_link_freq_2lanes),
					       &sensor->link_freq_bitmap);
		break;
	case 4:
		ret = v4l2_link_freq_to_bitmap(dev,
					       ep_cfg.link_frequencies,
					       ep_cfg.nr_of_link_frequencies,
					       ox05b1s_link_freq_4lanes,
					       ARRAY_SIZE(ox05b1s_link_freq_4lanes),
					       &sensor->link_freq_bitmap);
		break;
	}
	/* Check whether link frequency supported */
	if (ret) {
		ret =  dev_err_probe(dev, -EINVAL,
					"Link frequency not supported: %lld\n",
					ep_cfg.link_frequencies[0]);
	}

	/* Get power line frequency if present */
	of_property_read_u8(dev->of_node, "pwr_line_frequency", &sensor->pwr_line_freq);
	/* Set default, if value not/improperly set */
	if ((sensor->pwr_line_freq != 50) && (sensor->pwr_line_freq != 60)) {
		sensor->pwr_line_freq = 60;
	}

__ox05b1s_check_hwcfg_error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}

static int __ox05b1s_get_regulators(struct ox05b1s_sensor *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->subdev);
	unsigned int i;

	for (i = 0; i < OX05B1S_NUM_SUPPLIES; i++)
		sensor->supplies[i].supply = ox05b1s_supply_names[i];

	return devm_regulator_bulk_get(&client->dev,
				       OX05B1S_NUM_SUPPLIES,
				       sensor->supplies);
}

static int __ox05b1s_detect(struct ox05b1s_sensor *sensor)
{
	int ret;
	u64 id;

	/* Reset sensor (software) */
	ret = cci_update_bits(sensor->regmap, OX05B1S_REG_SW_RESET, OX05B1S_REG_SW_RESET_BIT, OX05B1S_REG_SW_RESET_BIT, NULL);
	if (ret < 0)
		return ret;

	ret = cci_read(sensor->regmap, OX05B1S_REG_SENSOR_ID, &id, NULL);
	if (ret < 0)
		return dev_err_probe(sensor->dev, ret, "failed to read chip id %x\n", OX05B1S_REG_SENSOR_ID_VALUE);

	if (id != OX05B1S_REG_SENSOR_ID_VALUE) {
		return dev_err_probe(sensor->dev, -ENODEV, "chip id mismatch: %x!=%llx\n", OX05B1S_REG_SENSOR_ID_VALUE, id);
	}

	dev_info(sensor->dev, "%s: detected chip ID 0x%04llx\n", __func__, id);

	/* Release reset (software) */
	ret = cci_update_bits(sensor->regmap, OX05B1S_REG_SW_RESET, OX05B1S_REG_SW_RESET_BIT, 0, NULL);
	if (ret < 0)
		return ret;

	/* Set alternate reset */
	/* This was done in previous driver, but register is marked reserved in manual */
	return cci_update_bits(sensor->regmap, OX05B1S_REG_ALT_RESET, 1, 1, NULL);
}

static int ox05b1s_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ox05b1s_sensor *sensor;
	int ret = 0;

	/* Allocate internal struct */
	sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(dev, "%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	sensor->dev = dev;

	/* Initialize the subdev and its controls. */
	v4l2_i2c_subdev_init(&sensor->subdev, client, &ox05b1s_subdev_ops);
	sensor->subdev.internal_ops = &ox05b1s_internal_ops;

	/* Check the hardware configuration in device tree */
	if (__ox05b1s_check_hwcfg(dev, sensor)) {
		dev_err(dev, "%s: failed to parse devicetree\n", __func__);
		return -EINVAL;
	}

	/* Initialize CCI I2C Regmap */
	sensor->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(sensor->regmap))
		return dev_err_probe(dev, PTR_ERR(sensor->regmap), "failed to initialize CCI\n");

	/* Get system clock (xclk) */
	sensor->xclk = devm_v4l2_sensor_clk_get(dev, "inck");
	if (IS_ERR(sensor->xclk))
		return dev_err_probe(dev, PTR_ERR(sensor->xclk), "failed to get xclk\n");
	sensor->xclk_freq = clk_get_rate(sensor->xclk);
	if ((sensor->xclk_freq < OX05B1S_XCLK_FREQ_MIN) || (sensor->xclk_freq > OX05B1S_XCLK_FREQ_MAX))
		return dev_err_probe(dev, -EINVAL, "xclk frequency not supported: %d Hz\n", sensor->xclk_freq);

	/* Get regulators */
	ret = __ox05b1s_get_regulators(sensor);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get regulators\n");

	/*
	 * The sensor must be powered for ox05b1s_identify_module()
	 * to be able to read the CHIP_ID register
	 */
	ret = ox05b1s_power_on(dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable regulators\n");

	ret = __ox05b1s_detect(sensor);
	if (ret)
		goto ox05b1s_probe_error_power_off;

	/* Set default mode */
	sensor->current_mode = &OX05B1S_V4L2_DEFAULT_MODE;

	/* Initialise V4L2 controls */
	ret = __ox05b1s_init_controls(sensor);
	if (ret) {
		dev_err(dev, "%s: failed to initialise V4L2 controls\n", __func__);
		goto ox05b1s_probe_error_power_off;
	}

	/* Initialize subdev */
	sensor->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&sensor->subdev.entity, 1, &sensor->pad);
	if (ret) {
		dev_err_probe(dev, ret, "failed to init entity pads\n");
		goto ox05b1s_probe_error_handler_free;
	}

	sensor->subdev.state_lock = sensor->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&sensor->subdev);
	if (ret < 0) {
		dev_err_probe(dev, ret, "subdev init error\n");
		goto ox05b1s_probe_error_media_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	ret = v4l2_async_register_subdev_sensor(&sensor->subdev);
	if (ret < 0) {
		dev_err_probe(dev, ret, "failed to register sensor sub-device\n");
		goto ox05b1s_probe_error_subdev_cleanup;
	}
	dev_info(dev, "registered device\n");

	/* Print debug info */
	dev_dbg(dev, "ox05b1s sensor:\n");
	dev_dbg(dev, "	data lanes: %u\n", sensor->num_lanes);
	dev_dbg(dev, "	link frequency: %ld\n", __ox05b1s_get_link_frequency(sensor));
	dev_dbg(dev, "	power line frequency: %u\n", sensor->pwr_line_freq);

	pm_runtime_idle(dev);
	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);

	return 0;

ox05b1s_probe_error_subdev_cleanup:
	v4l2_subdev_cleanup(&sensor->subdev);
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);

ox05b1s_probe_error_media_entity:
	media_entity_cleanup(&sensor->subdev.entity);

ox05b1s_probe_error_handler_free:
	ox05b1s_free_controls(sensor);

ox05b1s_probe_error_power_off:
	ox05b1s_power_off(dev);

	return ret;
}

static void ox05b1s_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ox05b1s_sensor *sensor = to_ox05b1s(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	ox05b1s_free_controls(sensor);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev)) {
		ox05b1s_power_off(&client->dev);
		pm_runtime_set_suspended(&client->dev);
	}
}


static const struct of_device_id ox05b1s_of_match[] = {
	{ .compatible = "ovti,ox05b1s" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ox05b1s_of_match);


static struct i2c_driver ox05b1s_i2c_driver = {
	.driver = {
		.name = "ox05b1s",
		.of_match_table	= ox05b1s_of_match,
		.pm = &ox05b1s_pm_ops,
	},
	.probe = ox05b1s_probe,
	.remove = ox05b1s_remove,
};
module_i2c_driver(ox05b1s_i2c_driver);


MODULE_AUTHOR("Charles Burgoyne <charlesb@wavesvision.com>");
MODULE_DESCRIPTION("OX05B1S RGB-IR Image Sensor driver");
MODULE_LICENSE("GPL");
