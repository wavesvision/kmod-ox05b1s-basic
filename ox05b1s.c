// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for OmniVision OX05B1S RGB-IR image sensor
 *
 * Copyright (c) 2024 WavesVision.com
 *
 * Based on OmniVision OV5647 driver
 * Copyright (C) 2016, Synopsys, Inc.
 *
 * Referenced:
 *	https://github.com/nxp-imx/linux-imx/blob/lf-6.12.y/drivers/media/i2c/ox05b1s/ox05b1s_mipi.c
 *	https://git.ti.com/cgit/ti-linux-kernel/ti-linux-kernel/tree/drivers/media/i2c/ox05b1s.c?h=ti-linux-6.1.y
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>

#include "ox05b1s.h"
#include "ox05b1s_regs_common.h"
#include "ox05b1s_regs_2592x1944.h"


#define OX05B1S_DRIVER_VERSION		KERNEL_VERSION(0, 0x1, 0x0)


static const s64 link_freq_menu_items[] = {
	756000000,
	378000000,
};

static const struct ox05b1s_mode ox05b1s_modes[] = {
/* 60 FPS */
	/* 2592x1944 full resolution full FOV 10-bit mode. */
	{
		.format = {
			.code		= MEDIA_BUS_FMT_SGRBG10_1X10,
			.colorspace	= V4L2_COLORSPACE_RAW,
			.field		= V4L2_FIELD_NONE,
			.width		= OX05B1S_PIXEL_ARRAY_WIDTH,
			.height		= OX05B1S_PIXEL_ARRAY_HEIGHT
		},
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.crop = {
			.left		= OX05B1S_PIXEL_ARRAY_LEFT,
			.top		= OX05B1S_PIXEL_ARRAY_TOP,
			.width		= OX05B1S_PIXEL_ARRAY_WIDTH,
			.height		= OX05B1S_PIXEL_ARRAY_HEIGHT
		},
		.pixel_rate		= 120000000,
		.hts			= 0x170,
		.vts			= 0x850,
		.link_freq_index	= 0,
		.reg_list		= ox05b1s_linear_2592x1944_full,
		.num_regs		= ARRAY_SIZE(ox05b1s_linear_2592x1944_full)
	},
/* 30 FPS */
	/* 2592x1944 full resolution full FOV 10-bit mode. */
	{
		.format = {
			.code		= MEDIA_BUS_FMT_SGRBG10_1X10,
			.colorspace	= V4L2_COLORSPACE_RAW,
			.field		= V4L2_FIELD_NONE,
			.width		= OX05B1S_PIXEL_ARRAY_WIDTH,
			.height		= OX05B1S_PIXEL_ARRAY_HEIGHT
		},
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.crop = {
			.left		= OX05B1S_PIXEL_ARRAY_LEFT,
			.top		= OX05B1S_PIXEL_ARRAY_TOP,
			.width		= OX05B1S_PIXEL_ARRAY_WIDTH,
			.height		= OX05B1S_PIXEL_ARRAY_HEIGHT
		},
		.pixel_rate		= 120000000,
		.hts			= 0x2f0,
		.vts			= 0x850,
		.link_freq_index	= 0,
		.reg_list		= ox05b1s_linear_2592x1944_full,
		.num_regs		= ARRAY_SIZE(ox05b1s_linear_2592x1944_full)
	},
};

/* Default sensor mode is 2592x1944 SGRBG10 1X10 */
#define OX05B1S_DEFAULT_MODE		(ox05b1s_modes[1])
#define OX05B1S_DEFAULT_FORMAT		(ox05b1s_modes[1].format)
#define OX05B1S_DEFAULT_MODE_30FPS	(ox05b1s_modes[1])

static inline struct ox05b1s *to_sensor(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ox05b1s, sd);
}

static int ox05b1s_reg_read(struct ox05b1s *sensor, u16 addr, u32 *val, size_t nbytes)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	int ret;
	__be32 val_be = 0;

	/* Check nbytes is valid */
	if ((nbytes == 0) || (nbytes > 4))
		return -EINVAL;

	ret = regmap_bulk_read(sensor->regmap, addr, ((uint8_t *) &val_be)+(4-nbytes), nbytes);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to read reg 0x%04x: %d\n", __func__, addr, ret);
		return ret;
	}

	*val = be32_to_cpu(val_be);
	return 0;
}

static int ox05b1s_reg_write(struct ox05b1s *sensor, u16 addr, u32 val, size_t nbytes)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	int ret;
	__be32 val_be = cpu_to_be32(val);

	/* Check nbytes is valid */
	if ((nbytes == 0) || (nbytes > 4))
		return -EINVAL;

	ret = regmap_bulk_write(sensor->regmap, addr, ((uint8_t *) &val_be)+(4-nbytes), nbytes);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to write reg 0x%04x: %d\n", __func__, addr, ret);
	}
	return ret;
}

static int ox05b1s_reg_write_table(struct ox05b1s *sensor, const struct reg_sequence *regs, unsigned int nr_regs)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	int ret;

	ret = regmap_multi_reg_write(sensor->regmap, regs, nr_regs);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to write reg table (%d)!\n", __func__, ret);
	}
	return ret;
}

static int ox05b1s_reg_update_bits(struct ox05b1s *sensor, u16 addr, u8 mask, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	int ret = 0;

	ret = regmap_update_bits(sensor->regmap, addr, mask, val);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to update reg addr 0x%04x with 0x%02x\n", addr, val);
	}

	return ret;
}

static int ox05b1s_set_mipi_lanes(struct ox05b1s *sensor, int num_lanes)
{
	u32 regval;

	/* Check that value is valid */
	if ((num_lanes != 2) && (num_lanes != 4))
		return -EINVAL;

	/* Set number of MIPI lanes to use */
	regval = num_lanes << 4;
	return ox05b1s_reg_update_bits(sensor, OX05B1S_REG_COMMON_MIPI_CFG, OX05B1S_BITS_MIPI_LANES, regval);
}

static int ox05b1s_set_virtual_channel(struct ox05b1s *sensor, int virtual_channel)
{
	/* Check that value is valid */
	if ((virtual_channel < 0) || (virtual_channel >= 4))
		return -EINVAL;

	/* Update virtual channel ID */
	return ox05b1s_reg_write(sensor, OX05B1S_REG_MIPI_CORE_REG13, virtual_channel, 1);
}

static int ox05b1s_set_mode(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	u32 rdval;
	int ret;

	/* Check that the device is not streaming */
	ret = ox05b1s_reg_read(sensor, OX05B1S_REG_MODE_SEL, &rdval, 1);
	if (ret < 0)
		return ret;
	if ((rdval & OX05B1S_BIT_START_STREAMING) != 0)
		return -EBUSY;

	/* Change configuration depending on the number of MIPI lanes available */
	switch (sensor->csi_lanes) {
	case 2:
		/* Configure PLLs (PLLs need to be first) */
		ret = ox05b1s_reg_write_table(sensor, ox05b1s_pllcfg_2lane, ARRAY_SIZE(ox05b1s_pllcfg_2lane));
		/* Set HTS */
		ret |= ox05b1s_s_hblank(sd, OX05B1S_DEFAULT_MODE_30FPS.hts);
		break;
	case 4:
		/* Configure PLLs (PLLs need to be first) */
		ret = ox05b1s_reg_write_table(sensor, ox05b1s_pllcfg_4lane, ARRAY_SIZE(ox05b1s_pllcfg_4lane));
		/* Set HTS */
		ret |= ox05b1s_s_hblank(sd, OX05B1S_DEFAULT_MODE.hts);
		break;
	default:
		dev_err(&client->dev, "incorrect number of MIPI lanes selected!\n");
		return -EINVAL;
	}
	if (ret < 0) {
		dev_err(&client->dev, "write sensor PLL regs error\n");
		return ret;
	}

	/* Configure common values */
	ret = ox05b1s_reg_write_table(sensor, ox05b1s_linear_common, ARRAY_SIZE(ox05b1s_linear_common));
	if (ret < 0) {
		dev_err(&client->dev, "write sensor default regs error\n");
		return ret;
	}

	/* Configure MIPI lanes */
	ret = ox05b1s_set_mipi_lanes(sensor, sensor->csi_lanes);
	if (ret) {
		dev_err(&client->dev, "Failed to set number of MIPI lanes to %d\n", sensor->csi_lanes);
		return ret;
	}

	/* Set default virtual channel */
	ret = ox05b1s_set_virtual_channel(sensor, 0);
	if (ret < 0)
		return ret;

	/* Configure default sensor resolution */
	ret = ox05b1s_reg_write_table(sensor, OX05B1S_DEFAULT_MODE.reg_list, OX05B1S_DEFAULT_MODE.num_regs);
	if (ret < 0) {
		dev_err(&client->dev, "write sensor mode regs error\n");
		return ret;
	}

	return 0;
}

static int ox05b1s_stream_on(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;

	ret = ox05b1s_set_mode(sd);
	if (ret) {
		dev_err(&client->dev, "Failed to program sensor mode: %d\n", ret);
		return ret;
	}

	/* Apply customized values from user when stream starts. */
	ret =  __v4l2_ctrl_handler_setup(sd->ctrl_handler);
	if (ret)
		return ret;

	/* Start streaming */
	ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_MODE_SEL, OX05B1S_BIT_START_STREAMING, OX05B1S_BIT_START_STREAMING);
	if (ret < 0) {
		dev_dbg(&client->dev, "Failed to start streaming\n");
		return ret;
	}

	return 0;
}

static int ox05b1s_stream_off(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;

	/* Enter software standby */
	ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_MODE_SEL, OX05B1S_BIT_START_STREAMING, 0);
	if (ret < 0) {
		dev_dbg(&client->dev, "Failed to stop stream\n");
		return ret;
	}

	return 0;
}

static int ox05b1s_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;

	dev_info(dev, "OX05B1S power on\n");

	if (sensor->pwdn) {
		gpiod_set_value_cansleep(sensor->pwdn, 0);
		msleep(OX05B1S_PWDN_ACTIVE_DELAY_MS);
	}

	ret = clk_prepare_enable(sensor->xclk);
	if (ret < 0) {
		dev_err(dev, "clk prepare enable failed\n");
		goto error_pwdn;
	}

	return 0;

error_pwdn:
	gpiod_set_value_cansleep(sensor->pwdn, 1);

	return ret;
}

static int ox05b1s_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;

	dev_info(dev, "OX05B1S power off\n");

	/* Enter software standby */
	ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_MODE_SEL, OX05B1S_BIT_START_STREAMING, 0);
	if (ret < 0)
		dev_dbg(dev, "Failed to set software standby mode\n");

	clk_disable_unprepare(sensor->xclk);
	gpiod_set_value_cansleep(sensor->pwdn, 1);

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ox05b1s_sensor_get_register(struct v4l2_subdev *sd,
				      struct v4l2_dbg_register *reg)
{
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;
	u32 val;

	ret = ox05b1s_reg_read(sensor, reg->reg & 0xffff, &val, 1);
	if (ret < 0)
		return ret;

	reg->val = val & 0xff;
	reg->size = 1;

	return 0;
}

static int ox05b1s_sensor_set_register(struct v4l2_subdev *sd,
				      const struct v4l2_dbg_register *reg)
{
	struct ox05b1s *sensor = to_sensor(sd);
	return ox05b1s_reg_write(sensor, reg->reg & 0xffff, reg->val & 0xff, 1);
}
#endif

static void ox05b1s_get_module_inf(struct ox05b1s *sensor,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, OX05B1S_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sensor->module_name, sizeof(inf->base.module));
	strscpy(inf->base.lens, sensor->len_name, sizeof(inf->base.lens));
}

static int ox05b1s_get_channel_info(struct ox05b1s *sensor, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;

	switch (ch_info->index) {
	default:
		ch_info->vc = 0;
		ch_info->width = sensor->mode->format.width;
		ch_info->height = sensor->mode->format.height;
		ch_info->bus_fmt = sensor->mode->format.code;
		break;
	}

	return 0;
}

static long ox05b1s_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ox05b1s *sensor = to_sensor(sd);
	struct rkmodule_channel_info *ch_info;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		ox05b1s_get_module_inf(sensor, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = ox05b1s_get_channel_info(sensor, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ox05b1s_compat_ioctl32(struct v4l2_subdev *sd,
					unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_channel_info *ch_info;
	long ret;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ox05b1s_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}
		ret = ox05b1s_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

/* Subdev core operations registration */
static const struct v4l2_subdev_core_ops ox05b1s_subdev_core_ops = {
	.log_status		= v4l2_ctrl_subdev_log_status,
	.subscribe_event	= v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event	= v4l2_event_subdev_unsubscribe,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= ox05b1s_sensor_get_register,
	.s_register		= ox05b1s_sensor_set_register,
#endif
	.ioctl			= ox05b1s_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32		= ox05b1s_compat_ioctl32,
#endif
};

static int ox05b1s_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;

	mutex_lock(&sensor->lock);
	if (sensor->streaming == enable) {
		mutex_unlock(&sensor->lock);
		return 0;
	}

	if (enable) {
		dev_info(&client->dev, "stream start\n");
		ret = pm_runtime_resume(&client->dev);
		if (ret < 0)
		{
			dev_err(&client->dev, "pm_runtime_resume failed: %d\n", ret);
			goto error_unlock;
		}
		ret = pm_runtime_get(&client->dev);
		if (ret < 0)
		{
			dev_err(&client->dev, "pm_runtime_get failed: %d\n", ret);
			goto error_unlock;
		}
		ret = ox05b1s_stream_on(sd);
		if (ret < 0) {
			dev_err(&client->dev, "stream start failed: %d\n", ret);
			goto error_pm;
		}
	} else {
		dev_info(&client->dev, "stream stop\n");
		ret = ox05b1s_stream_off(sd);
		if (ret < 0) {
			dev_err(&client->dev, "stream stop failed: %d\n", ret);
			goto error_pm;
		}
		pm_runtime_put(&client->dev);
	}

	sensor->streaming = enable;
	mutex_unlock(&sensor->lock);

	return 0;

error_pm:
	pm_runtime_put(&client->dev);
error_unlock:
	mutex_unlock(&sensor->lock);

	return ret;
}

static int ox05b1s_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct ox05b1s *sensor = to_sensor(sd);
	const struct ox05b1s_mode *mode = sensor->mode;

	mutex_lock(&sensor->lock);
	fi->interval = mode->max_fps;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ox05b1s_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	*status = 0;
	return 0;
}

static const struct v4l2_subdev_video_ops ox05b1s_subdev_video_ops = {
	.s_stream		= ox05b1s_s_stream,
	.g_input_status		= ox05b1s_g_input_status,
	.g_frame_interval	= ox05b1s_g_frame_interval,
};

/* This function returns the mbus code for the current settings of the
   HFLIP and VFLIP controls. */
static u32 ox05b1s_get_mbus_code(struct v4l2_subdev *sd)
{
	struct ox05b1s *sensor = to_sensor(sd);
	/* The control values are only 0 or 1. */
	int index =  sensor->hflip->val | (sensor->vflip->val << 1);

	/* Using standard bayer codes in place of ones denoting IR capability */
	static const u32 codes[4] = {
		MEDIA_BUS_FMT_SBGGR10_1X10,
		MEDIA_BUS_FMT_SGRBG10_1X10,
		MEDIA_BUS_FMT_SGBRG10_1X10,
		MEDIA_BUS_FMT_SRGGB10_1X10,
	};

	return codes[index];
}

static int ox05b1s_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = ox05b1s_get_mbus_code(sd);

	return 0;
}

static int ox05b1s_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(ox05b1s_modes))
		return -EINVAL;

	fie->code = ox05b1s_get_mbus_code(sd);

	fie->width = ox05b1s_modes[fie->index].format.width;
	fie->height = ox05b1s_modes[fie->index].format.height;
	fie->interval = ox05b1s_modes[fie->index].max_fps;
	return 0;
}

static int ox05b1s_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	const struct v4l2_mbus_framefmt *fmt;

	if (fse->code != ox05b1s_get_mbus_code(sd) ||
	    fse->index >= ARRAY_SIZE(ox05b1s_modes))
		return -EINVAL;

	fmt = &ox05b1s_modes[fse->index].format;
	fse->min_width = fmt->width;
	fse->max_width = fmt->width;
	fse->min_height = fmt->height;
	fse->max_height = fmt->height;

	return 0;
}

static int ox05b1s_get_pad_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state,
			      struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;
	const struct v4l2_mbus_framefmt *sensor_format;
	struct ox05b1s *sensor = to_sensor(sd);

	mutex_lock(&sensor->lock);
	switch (format->which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		sensor_format = v4l2_subdev_get_try_format(sd, sd_state,
							   format->pad);
		break;
	default:
		sensor_format = &sensor->mode->format;
		break;
	}

	*fmt = *sensor_format;
	/* The code we pass back must reflect the current h/vflips. */
	fmt->code = ox05b1s_get_mbus_code(sd);
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ox05b1s_set_pad_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state,
			      struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;
	struct ox05b1s *sensor = to_sensor(sd);
	const struct ox05b1s_mode *mode;

	mode = v4l2_find_nearest_size(ox05b1s_modes, ARRAY_SIZE(ox05b1s_modes),
				      format.width, format.height,
				      fmt->width, fmt->height);

	/* Update the sensor mode and apply at it at streamon time. */
	mutex_lock(&sensor->lock);
	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, sd_state, format->pad) = mode->format;
	} else {
		sensor->mode = mode;
		__v4l2_ctrl_modify_range(sensor->pixel_rate, mode->pixel_rate,
					 mode->pixel_rate, 1, mode->pixel_rate);
	}
	*fmt = mode->format;
	/* The code we pass back must reflect the current h/vflips. */
	fmt->code = ox05b1s_get_mbus_code(sd);

	__v4l2_ctrl_s_ctrl(sensor->link_freq, mode->link_freq_index);
	v4l2_info(sd, "%s res wxh:%dx%d, link freq:%llu", __func__,
			fmt->width, fmt->height, link_freq_menu_items[mode->link_freq_index]);
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ox05b1s_get_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct ox05b1s *sensor = to_sensor(sd);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = sensor->csi_lanes;

	return 0;
}

static const struct v4l2_subdev_pad_ops ox05b1s_subdev_pad_ops = {
	.enum_mbus_code		= ox05b1s_enum_mbus_code,
	.enum_frame_interval	= ox05b1s_enum_frame_interval,
	.enum_frame_size	= ox05b1s_enum_frame_size,
	.set_fmt		= ox05b1s_set_pad_fmt,
	.get_fmt		= ox05b1s_get_pad_fmt,
	.get_mbus_config	= ox05b1s_get_mbus_config,
};

static const struct v4l2_subdev_ops ox05b1s_subdev_ops = {
	.core		= &ox05b1s_subdev_core_ops,
	.video		= &ox05b1s_subdev_video_ops,
	.pad		= &ox05b1s_subdev_pad_ops,
};

static int ox05b1s_detect(struct ox05b1s *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	u32 sensorid;
	int ret;

	/* Reset sensor (software) */
	ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_SW_RESET, OX05B1S_BIT_SW_RESET, OX05B1S_BIT_SW_RESET);
	if (ret < 0)
		return ret;

	/* Read sensor ID */
	ret = ox05b1s_reg_read(sensor, OX05B1S_REG_SENSOR_ID_HI, &sensorid, 3);
	if (ret < 0)
		return ret;

	/* Check sensor ID */
	if (sensorid != OX05B1S_SENSOR_ID) {
		dev_err(&client->dev, "Unexpected sensor ID expected %x, got %x\n", OX05B1S_SENSOR_ID, sensorid);
		return -ENODEV;
	}

	/* Release reset (software) */
	ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_SW_RESET, OX05B1S_BIT_SW_RESET, 0);
	if (ret < 0)
		return ret;

	/* Set alternate reset */
	/* This was done in previous driver, but register is marked reserved in manual */
	return ox05b1s_reg_update_bits(sensor, OX05B1S_REG_REG_RESET, 1, 1);
}

static int ox05b1s_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *format =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	struct v4l2_rect *crop = v4l2_subdev_get_try_crop(sd, fh->state, 0);

	crop->left = OX05B1S_PIXEL_ARRAY_LEFT;
	crop->top = OX05B1S_PIXEL_ARRAY_TOP;
	crop->width = format->width;
	crop->height = format->height;

	*format = OX05B1S_DEFAULT_FORMAT;
	format->code = ox05b1s_get_mbus_code(sd);

	return 0;
}

static const struct v4l2_subdev_internal_ops ox05b1s_subdev_internal_ops = {
	.open = ox05b1s_open,
};

static int ox05b1s_s_exposure_auto(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Turn on AEC by clearing bit 3.*/
	regval = val == V4L2_EXPOSURE_MANUAL ? OX05B1S_BIT_MANUAL_EXPOSURE : 0;
	return ox05b1s_reg_update_bits(sensor, OX05B1S_REG_EC_A_CTRL03, OX05B1S_BIT_MANUAL_EXPOSURE, regval);
}

static int ox05b1s_s_exposure_aec_cfg(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;
	s32 regval;

	/* Configure stable-in window (upper) */
	regval = sensor->aec_target->val + sensor->aec_stable_in->val;
	if (regval > 0xff) regval = 0xff;
	dev_dbg(&client->dev, "Setting AEC WPT: %u\n", regval);
	ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_WPT, regval, 1);
	if (ret)
		return ret;
	/* Configure stable-in window (lower) */
	regval = sensor->aec_target->val - sensor->aec_stable_in->val;
	if (regval < 0) regval = 0;
	dev_dbg(&client->dev, "Setting AEC BPT: %u\n", regval);
	ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_BPT, regval, 1);
	if (ret)
		return ret;

	/* Configure stable-out window (upper) */
	regval = sensor->aec_target->val + sensor->aec_stable_out->val;
	if (regval > 0xff) regval = 0xff;
	dev_dbg(&client->dev, "Setting AEC WPT2: %u\n", regval);
	ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_WPT2, regval, 1);
	if (ret)
		return ret;
	/* Configure stable-out window (lower) */
	regval = sensor->aec_target->val - sensor->aec_stable_out->val;
	if (regval < 0) regval = 0;
	dev_dbg(&client->dev, "Setting AEC BPT2: %u\n", regval);
	return ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_BPT2, regval, 1);
}

static int ox05b1s_s_exposure(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Limit value to 16 bit */
	regval = val & 0xffff;

	/* Update exposure */
	return ox05b1s_reg_write(sensor, OX05B1S_REG_EC_A_EXPO_HI, regval, 3);
}

static int ox05b1s_s_gain_auto(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Turn on AGC by clearing bit 7.*/
	regval = val ? 0 : OX05B1S_BIT_MANUAL_GAIN;
	return ox05b1s_reg_update_bits(sensor, OX05B1S_REG_EC_A_CTRL03, OX05B1S_BIT_MANUAL_GAIN, regval);
}

static int ox05b1s_s_gain_agc_cfg(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	int ret;
	u32 regval;

	/* Set max total gain */
	regval = sensor->agc_total_max->val & 0x7ff;
	dev_dbg(&client->dev, "Setting AGC total gain max: 0x%04x\n", regval);
	ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_GAIN_CEIL_HI, regval, 2);
	if (ret)
		return ret;

	/* Set max analogue gain, register split */
	regval = ((sensor->agc_analogue_max->val & 0xf0) << 4) | (sensor->agc_analogue_max->val & 0x0f);
	dev_dbg(&client->dev, "Setting AGC analogue gain max: 0x%04x\n", regval);
	return ox05b1s_reg_write(sensor, OX05B1S_REG_EC_A_MAX_GAIN_COARSE, regval, 2);
}

static int ox05b1s_s_gain_analogue(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Limit value to 8 bits */
	regval = val & 0xff;
	/* Value is split between 2 registers */
	regval = regval << 4;

	/* Update analagoue gain */
	return ox05b1s_reg_write(sensor, OX05B1S_REG_EC_A_MANUAL_AGAIN_HI, regval, 2);
}

static int ox05b1s_s_gain_digital(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Limit value to 14 bits */
	regval = val & 0x3fff;
	/* Value is split between 3 registers */
	regval = regval << 6;

	/* Update digital gain */
	return ox05b1s_reg_write(sensor, OX05B1S_REG_EC_A_MANUAL_DGAIN_HI, regval, 3);
}

static int ox05b1s_s_hblank(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Limit value to 16 bit */
	regval = val & 0xffff;

	/* Update horizontal blanking */
	return ox05b1s_reg_write(sensor, OX05B1S_REG_TIMING_CTRL_HTS_HI, regval, 2);
}

static int ox05b1s_s_vblank(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Limit value to 16 bit */
	regval = val & 0xffff;

	/* Update vertical blanking */
	return ox05b1s_reg_write(sensor, OX05B1S_REG_TIMING_CTRL_VTS_HI, regval, 2);
}

static int ox05b1s_s_light_freq(struct v4l2_subdev *sd, int value) {
	struct ox05b1s *sensor = to_sensor(sd);
	int ret, exposure_max;
	u32 regval;

	/* Calculate max exposure */
	exposure_max = sensor->mode->format.height + sensor->vblank->val - OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET;

	if (value == V4L2_CID_POWER_LINE_FREQUENCY_60HZ) {
		/* Set max exposure: 60Hz */
		regval = exposure_max & 0x0fff;
		ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_MAX2_EXPO_60HZ_HI, regval, 2);
		if (ret)
			return ret;

		/* Calculate the banding value: 60Hz */
		regval = (120 / (sensor->mode->max_fps.denominator / sensor->mode->max_fps.numerator)) * 2;
		ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_MAX_BANDS_60HZ, regval, 1);
		if (ret)
			return ret;

		/* Set auto step, and auto step value: 60Hz */
		regval = 0x30;
		ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_CTRL85, regval, 1);
		if (ret)
			return ret;

	} else if (value == V4L2_CID_POWER_LINE_FREQUENCY_50HZ) {
		/* Set max exposure: 50Hz */
		regval = exposure_max & 0x0fff;
		ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_MAX2_EXPO_50HZ_HI, regval, 2);
		if (ret)
			return ret;

		/* Calculate the banding value: 50Hz */
		regval = (100 / (sensor->mode->max_fps.denominator / sensor->mode->max_fps.numerator)) * 2;
		ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_MAX_BANDS_50HZ, regval, 1);
		if (ret)
			return ret;

		/* Set auto step, and auto step value: 50Hz */
		regval = 0xb0;
		ret = ox05b1s_reg_write(sensor, OX05B1S_REG_AEC_CTRL85, regval, 1);
		if (ret)
			return ret;

	} else {
		// Line frequency setting disabled
	}

	return 0;
}

static int ox05b1s_s_hflip(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Turn on horizontal flip by setting bit 2 */
	regval = val ? OX05B1S_BIT_HORIZONTAL_FLIP : 0;
	return ox05b1s_reg_update_bits(sensor, OX05B1S_REG_TIMING_CTRL_REG21, OX05B1S_BIT_HORIZONTAL_FLIP, regval);
}

static int ox05b1s_s_vflip(struct v4l2_subdev *sd, u32 val)
{
	struct ox05b1s *sensor = to_sensor(sd);
	u32 regval;

	/* Turn on vertical flip by setting bit 2 */
	regval = val ? OX05B1S_BIT_VERTICAL_FLIP : 0;
	return ox05b1s_reg_update_bits(sensor, OX05B1S_REG_TIMING_CTRL_REG20, OX05B1S_BIT_VERTICAL_FLIP, regval);
}

static int ox05b1s_s_test_pattern(struct v4l2_subdev *sd, u32 test_idx)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ox05b1s *sensor = to_sensor(sd);
	u32 val;
	int ret;

	// Check that index is valid
	if (test_idx >= ARRAY_SIZE(test_pattern_val))  return -EINVAL;
	val = test_pattern_val[test_idx];

	if (val) {
		// Enable PRE_ISP module
		ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_ISP_REG00, OX05B1S_BIT_ISP_REG00_PREISP, OX05B1S_BIT_ISP_REG00_PREISP);
		if (ret < 0) {
			dev_dbg(&client->dev, "Failed to enable Pre-ISP\n");
			return ret;
		}
	} else {
		// Disable PRE_ISP module
		ret = ox05b1s_reg_update_bits(sensor, OX05B1S_REG_ISP_REG00, OX05B1S_BIT_ISP_REG00_PREISP, 0);
		if (ret < 0) {
			dev_dbg(&client->dev, "Failed to disable Pre-ISP\n");
			return ret;
		}
	}
	if ((val & (OX05B1S_BIT_TEST_REDBLUE | OX05B1S_BIT_TEST_WHITE)) > 0) {
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_CONST_REDBLUE_MSB, 0x0fff, 2);
	} else {
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_CONST_REDBLUE_MSB, 0x0000, 2);
	}
	if ((val & (OX05B1S_BIT_TEST_GREEN | OX05B1S_BIT_TEST_WHITE)) > 0) {
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_CONST_GREENRED_MSB, 0x0fff, 2);
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_CONST_GREENBLUE_MSB, 0x0fff, 2);
	} else {
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_CONST_GREENRED_MSB, 0x0000, 2);
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_CONST_GREENBLUE_MSB, 0x0000, 2);
	}
	if ((val & (OX05B1S_BIT_TEST_REDBLUE | OX05B1S_BIT_TEST_GREEN | OX05B1S_BIT_TEST_WHITE)) > 0) {
		// Enable constant colour
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_MODE2, 0x41, 1);
	} else {
		// Disable constant colour
		ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_MODE2, 0x01, 1);
	}
	return ox05b1s_reg_write(sensor, OX05B1S_REG_TEST_MODE, val & 0xff, 1);
}

static int ox05b1s_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ox05b1s *sensor = container_of(ctrl->handler,
					    struct ox05b1s, ctrls);
	struct v4l2_subdev *sd = &sensor->sd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;


	/* v4l2_ctrl_lock() locks our own mutex */

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = sensor->mode->format.height + ctrl->val - OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET;
		exposure_def = min(exposure_max, OX05B1S_CTRL_EXPOSURE_DEFAULT);
		__v4l2_ctrl_modify_range(sensor->exposure,
					 sensor->exposure->minimum,
					 exposure_max, sensor->exposure->step,
					 exposure_def);
	}

	/*
	 * If the device is not powered up do not apply any controls
	 * to H/W at this time. Instead the controls will be restored
	 * at s_stream(1) time.
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret =  ox05b1s_s_gain_analogue(sd, ctrl->val);
		break;
	case V4L2_CID_AUTOGAIN:
		ret = ox05b1s_s_gain_auto(sd, ctrl->val);
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
//		ret = ox05b1s_s_auto_white_balance(sd, ctrl->val);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret =  ox05b1s_s_gain_digital(sd, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ret = ox05b1s_s_exposure_auto(sd, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = ox05b1s_s_exposure(sd, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ox05b1s_s_hflip(sd, ctrl->val);
		break;
	case V4L2_CID_OX05B1S_AEC_TARGET:
	case V4L2_CID_OX05B1S_AEC_STABLE_IN:
	case V4L2_CID_OX05B1S_AEC_STABLE_OUT:
		ret = ox05b1s_s_exposure_aec_cfg(sd);
		break;
	case V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX:
	case V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX:
		ret = ox05b1s_s_gain_agc_cfg(sd);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ret = ox05b1s_s_light_freq(sd, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ox05b1s_s_test_pattern(sd, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = ox05b1s_s_vblank(sd, sensor->mode->format.height + ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		ox05b1s_s_vflip(sd, ctrl->val);
		break;

	/* Read-only, but we adjust it based on mode. */
	case V4L2_CID_PIXEL_RATE:
	case V4L2_CID_HBLANK:
		/* Read-only, but we adjust it based on mode. */
		break;

	default:
		dev_info(&client->dev,
			 "Control (id:0x%x, val:0x%x) not supported\n",
			 ctrl->id, ctrl->val);
		return -EINVAL;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static int ox05b1s_t_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ox05b1s *sensor = container_of(ctrl->handler, struct ox05b1s, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_OX05B1S_AEC_STABLE_IN:
		/* Check whether stable in value is greater than current stable out value */
		if (ctrl->val > sensor->aec_stable_out->val)
			ctrl->val = sensor->aec_stable_out->val;
		break;
	case V4L2_CID_OX05B1S_AEC_STABLE_OUT:
		/* Check whether stable out value is less than current stable in value */
		if (ctrl->val < sensor->aec_stable_in->val)
			ctrl->val = sensor->aec_stable_in->val;
		break;
	case V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX:
		/* Check whether analogue gain is greater than total gain */
		if (ctrl->val > sensor->agc_total_max->val)
			ctrl->val = sensor->agc_total_max->val;
		break;
	case V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX:
		/* Check whether total gain is less than analogue gain */
		if (ctrl->val < sensor->agc_analogue_max->val)
			ctrl->val = sensor->agc_analogue_max->val;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ox05b1s_ctrl_ops = {
	.s_ctrl		= ox05b1s_s_ctrl,
	.try_ctrl	= ox05b1s_t_ctrl,
};

static int ox05b1s_init_controls(struct ox05b1s *sensor)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);
	int exposure_max, exposure_def;

	v4l2_ctrl_handler_init(&sensor->ctrls, 8);

	/* Added controls for manual adjustment */
	exposure_max = sensor->mode->vts - OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET;
	exposure_def = min(exposure_max, OX05B1S_CTRL_EXPOSURE_MIN);
	sensor->exposure = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_EXPOSURE,
						OX05B1S_CTRL_EXPOSURE_MIN,
						exposure_max, OX05B1S_CTRL_EXPOSURE_STEP,
						exposure_def);

	sensor->auto_exposure = v4l2_ctrl_new_std_menu(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL,
						0, OX05B1S_AUTOEXPO_DEFAULT);

	/* Added custom controls for AEC */
	v4l2ctrl_aec_target.ops = &ox05b1s_ctrl_ops;
	sensor->aec_target = v4l2_ctrl_new_custom(&sensor->ctrls, &v4l2ctrl_aec_target, NULL);
	v4l2ctrl_aec_stable_in.ops = &ox05b1s_ctrl_ops;
	sensor->aec_stable_in = v4l2_ctrl_new_custom(&sensor->ctrls, &v4l2ctrl_aec_stable_in, NULL);
	v4l2ctrl_aec_stable_out.ops = &ox05b1s_ctrl_ops;
	sensor->aec_stable_out = v4l2_ctrl_new_custom(&sensor->ctrls, &v4l2ctrl_aec_stable_out, NULL);

	sensor->again = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_ANALOGUE_GAIN, 0,
						OX05B1S_CTRL_ANALOGUE_GAIN_MAX, 1,
						OX05B1S_CTRL_ANALOGUE_GAIN_DEFAULT);

	sensor->dgain = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_DIGITAL_GAIN, 0,
						OX05B1S_CTRL_DIGITAL_GAIN_MAX, 1,
						OX05B1S_CTRL_DIGITAL_GAIN_DEFAULT);

	sensor->auto_gain = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_AUTOGAIN, 0, 1, 1, OX05B1S_AUTOGAIN_DEFAULT);

	/* Added custom controls for AGC */
	v4l2ctrl_agc_analogue_max.ops = &ox05b1s_ctrl_ops;
	sensor->agc_analogue_max = v4l2_ctrl_new_custom(&sensor->ctrls, &v4l2ctrl_agc_analogue_max, NULL);
	v4l2ctrl_agc_total_max.ops = &ox05b1s_ctrl_ops;
	sensor->agc_total_max = v4l2_ctrl_new_custom(&sensor->ctrls, &v4l2ctrl_agc_total_max, NULL);

	sensor->auto_whitebal = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 0);


	sensor->light_freq = v4l2_ctrl_new_std_menu(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_POWER_LINE_FREQUENCY,
						V4L2_CID_POWER_LINE_FREQUENCY_60HZ, ~0x7,
						sensor->pwr_line_freq == 60 ? V4L2_CID_POWER_LINE_FREQUENCY_60HZ : V4L2_CID_POWER_LINE_FREQUENCY_50HZ);


	sensor->test_pattern = v4l2_ctrl_new_std_menu_items(&sensor->ctrls, &ox05b1s_ctrl_ops, V4L2_CID_TEST_PATTERN,
						ARRAY_SIZE(test_pattern_menu) - 1,
						0, 0, test_pattern_menu);

	sensor->hflip = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_HFLIP, 0, 1, 1, OX05B1S_HFLIP_DEFAULT);
	if (sensor->hflip)
		sensor->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	sensor->vflip = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_VFLIP, 0, 1, 1, OX05B1S_VFLIP_DEFAULT);
	if (sensor->vflip)
		sensor->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	/* By default, LINK_FREQ is read only, but it does change per mode */
	sensor->link_freq = v4l2_ctrl_new_int_menu(&sensor->ctrls, NULL,
						V4L2_CID_LINK_FREQ,
				      		ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	if (sensor->link_freq)
		sensor->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* By default, PIXEL_RATE is read only, but it does change per mode */
	sensor->pixel_rate = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_PIXEL_RATE,
						sensor->mode->pixel_rate,
						sensor->mode->pixel_rate, 1,
						sensor->mode->pixel_rate);
	if (sensor->pixel_rate)
		sensor->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

//	/* By default, HBLANK is read only, but it does change per mode. */
//	hblank = sensor->mode->hts - sensor->mode->format.width;
//	sensor->hblank = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
//					   V4L2_CID_HBLANK, hblank, hblank, 1,
//					   hblank);
//	if (sensor->hblank)
//		sensor->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	sensor->vblank = v4l2_ctrl_new_std(&sensor->ctrls, &ox05b1s_ctrl_ops,
						V4L2_CID_VBLANK, OX05B1S_VBLANK_MIN,
						OX05B1S_VTS_MAX - sensor->mode->format.height, 1,
						sensor->mode->vts - sensor->mode->format.height);

	if (sensor->ctrls.error)
		goto handler_free;

	sensor->sd.ctrl_handler = &sensor->ctrls;

	return 0;

handler_free:
	dev_err(&client->dev, "%s Controls initialization failed (%d)\n", __func__, sensor->ctrls.error);
	v4l2_ctrl_handler_free(&sensor->ctrls);

	return sensor->ctrls.error;
}

static int ox05b1s_get_hwcfg(struct ox05b1s *sensor, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	struct device_node *ep;
	int ret;

	ep = of_graph_get_next_endpoint(np, NULL);
	if (!ep)
		return -EINVAL;

	/* Get CSI bus information */
	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep), &bus_cfg);
	if (ret)
		goto out;

	/* Check if bus clock non-continuous */
	sensor->clock_ncont = bus_cfg.bus.mipi_csi2.flags & V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK;

	/* Get number of data lanes */
	sensor->csi_lanes = bus_cfg.bus.mipi_csi2.num_data_lanes;
	if ((sensor->csi_lanes != 2) && (sensor->csi_lanes != 4)) {
		dev_err(dev, "number of CSI2 data lanes %d is not supported\n", sensor->csi_lanes);
		ret = -EINVAL;
		goto out;
	}
	dev_info(dev, "Using %u data lanes\n", sensor->csi_lanes);

	/* Get sensor module information */
	ret = of_property_read_u32(np, RKMODULE_CAMERA_MODULE_INDEX, &sensor->module_index);
	ret |= of_property_read_string(np, RKMODULE_CAMERA_MODULE_FACING, &sensor->module_facing);
	ret |= of_property_read_string(np, RKMODULE_CAMERA_MODULE_NAME, &sensor->module_name);
	ret |= of_property_read_string(np, RKMODULE_CAMERA_LENS_NAME, &sensor->len_name);
	if (ret) {
		dev_err(dev, "Could not get sensor module information!\n");
		ret = -EINVAL;
		goto out;
	}

	/* Get line frequency if present */
	of_property_read_u8(np, "pwr_line_frequency", &sensor->pwr_line_freq);
	/* Set default, if value not/improperly set */
	if ((sensor->pwr_line_freq != 50) && (sensor->pwr_line_freq != 60)) {
		sensor->pwr_line_freq = 60;
	}

out:
	of_node_put(ep);

	return ret;
}

static int ox05b1s_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct device *dev = &client->dev;
	struct ox05b1s *sensor;
	struct v4l2_subdev *sd;
	u32 xclk_freq;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x\n",
		OX05B1S_DRIVER_VERSION >> 16,
		(OX05B1S_DRIVER_VERSION & 0xff00) >> 8,
		OX05B1S_DRIVER_VERSION & 0x00ff);

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	/* Initialize I2C Regmap */
	sensor->regmap = devm_regmap_init_i2c(client, &ox05b1s_regmap_config);
	if (IS_ERR(sensor->regmap))
		return PTR_ERR(sensor->regmap);

	/* Parse devicetree data */
	if (IS_ENABLED(CONFIG_OF)) {
		ret = ox05b1s_get_hwcfg(sensor, dev);
		if (ret) {
			dev_err(dev, "Error parsing devicetree: %d\n", ret);
			return ret;
		}
	}

	/* Get sensor clock */
	sensor->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(sensor->xclk)) {
		dev_err(dev, "Could not get xclk\n");
		return PTR_ERR(sensor->xclk);
	}
	/* Check sensor clock frequency */
	xclk_freq = clk_get_rate(sensor->xclk);
	if (xclk_freq != 24000000) {
		dev_err(dev, "Unsupported clock frequency: %u\n", xclk_freq);
		return -EINVAL;
	}

	/* Request the power down GPIO asserted. */
	sensor->pwdn = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_HIGH);
	if (IS_ERR(sensor->pwdn)) {
		dev_err(dev, "Failed to get 'pwdn' gpio\n");
		return -EINVAL;
	}

	mutex_init(&sensor->lock);

	sensor->mode = &OX05B1S_DEFAULT_MODE;

	sd = &sensor->sd;
	v4l2_i2c_subdev_init(sd, client, &ox05b1s_subdev_ops);
	sd->internal_ops = &ox05b1s_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sensor->pad);
	if (ret < 0)
		goto mutex_destroy;

	/* Initialise V4L2 controls */
	ret = ox05b1s_init_controls(sensor);
	if (ret)
		goto entity_cleanup;

	ret = ox05b1s_power_on(dev);
	if (ret)
		goto ctrl_handler_free;

	ret = ox05b1s_detect(sensor);
	if (ret < 0)
		goto power_off;

	memset(facing, 0, sizeof(facing));
	if (strcmp(sensor->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	/* Update device name */
	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sensor->module_index, facing,
		 OX05B1S_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret < 0)
		goto power_off;

	/* Enable runtime PM and turn off the device */
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	dev_info(dev, "OmniVision OX05B1S camera probed\n");

	return 0;

power_off:
	ox05b1s_power_off(dev);
ctrl_handler_free:
	v4l2_ctrl_handler_free(&sensor->ctrls);
entity_cleanup:
	media_entity_cleanup(&sd->entity);
mutex_destroy:
	mutex_destroy(&sensor->lock);

	return ret;
}

static void ox05b1s_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ox05b1s *sensor = to_sensor(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls);
	v4l2_device_unregister_subdev(sd);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&sensor->lock);
}

static const struct dev_pm_ops ox05b1s_pm_ops = {
	SET_RUNTIME_PM_OPS(ox05b1s_power_off, ox05b1s_power_on, NULL)
};

static const struct i2c_device_id ox05b1s_id[] = {
	{ "ox05b1s", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, ox05b1s_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ox05b1s_of_match[] = {
	{ .compatible = "ovti,ox05b1s" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ox05b1s_of_match);
#endif

static struct i2c_driver ox05b1s_driver = {
	.driver = {
		.of_match_table = of_match_ptr(ox05b1s_of_match),
		.name	= OX05B1S_NAME,
		.pm	= &ox05b1s_pm_ops,
	},
	.probe		= ox05b1s_probe,
	.remove		= ox05b1s_remove,
	.id_table	= ox05b1s_id,
};

module_i2c_driver(ox05b1s_driver);

MODULE_AUTHOR("Charles Burgoyne <charlesb@wavesvision.com>");
MODULE_DESCRIPTION("V4L2 driver for OmniVision OX05B1S");
MODULE_LICENSE("GPL v2");
