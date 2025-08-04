// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for OmniVision OX05B1S RGB-IR image sensor
 *
 * Copyright (c) 2024 WavesVision.com
 */

#ifndef __DRIVERS_MEDIA_VIDEO_OX05B1S_H__
#define __DRIVERS_MEDIA_VIDEO_OX05B1S_H__




static const struct regmap_config ox05b1s_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
};


#define OX05B1S_REG_MODE_SEL			0x0100
#define OX05B1S_REG_SW_RESET			0x0103
#define OX05B1S_REG_REG_RESET			0x0107

#define OX05B1S_REG_SENSOR_ID_HI		0x300a

#define OX05B1S_REG_COMMON_MIPI_CFG		0x3010

#define OX05B1S_REG_GRPHOLD_REG08		0x3208	
#define OX05B1S_REG_GRPHOLD_REG0A		0x320A	
#define OX05B1S_REG_GRPHOLD_REG11		0x3211	

#define OX05B1S_REG_EC_A_EXPO_HI		0x3500	
#define OX05B1S_REG_EC_A_EXPO_MID		0x3501	
#define OX05B1S_REG_EC_A_EXPO_LO		0x3502	
#define OX05B1S_REG_EC_A_CTRL03			0x3503	
#define OX05B1S_REG_EC_A_CTRL04			0x3504	
#define OX05B1S_REG_EC_A_MANUAL_AGAIN_HI	0x3508	
#define OX05B1S_REG_EC_A_MANUAL_AGAIN_LO	0x3509	
#define OX05B1S_REG_EC_A_MANUAL_DGAIN_HI	0x350a	
#define OX05B1S_REG_EC_A_MANUAL_DGAIN_MID	0x350b	
#define OX05B1S_REG_EC_A_MANUAL_DGAIN_LO	0x350b	

#define OX05B1S_REG_EC_A_MAX_GAIN_COARSE	0x3523	
#define OX05B1S_REG_EC_A_MAX_GAIN_FINE		0x3524	

#define OX05B1S_REG_TIMING_CTRL_HTS_HI		0x380C
#define OX05B1S_REG_TIMING_CTRL_HTS_LO		0x380D
#define OX05B1S_REG_TIMING_CTRL_VTS_HI		0x380E
#define OX05B1S_REG_TIMING_CTRL_VTS_LO		0x380F

#define OX05B1S_REG_TIMING_CTRL_REG20		0x3820	
#define OX05B1S_REG_TIMING_CTRL_REG21		0x3821	

#define OX05B1S_REG_AEC_CTRL85			0x4685	
#define OX05B1S_REG_AEC_UPDATE_SPEED1		0x4686	
#define OX05B1S_REG_AEC_UPDATE_SPEED23		0x4687	

#define OX05B1S_REG_AEC_MAX_BANDS_60HZ		0x468D	
#define OX05B1S_REG_AEC_MAX_BANDS_50HZ		0x468E	
#define OX05B1S_REG_AEC_WPT			0x468F	
#define OX05B1S_REG_AEC_BPT			0x4690	
#define OX05B1S_REG_AEC_VPT_HIGH		0x4691	

#define OX05B1S_REG_AEC_GAIN_CEIL_HI		0x4698	
#define OX05B1S_REG_AEC_GAIN_CEIL_LO		0x4699	

#define OX05B1S_REG_AEC_WPT2			0x469B	
#define OX05B1S_REG_AEC_BPT2			0x469E	
#define OX05B1S_REG_AEC_VPT_LOW			0x469F	

#define OX05B1S_REG_AEC_MAX2_EXPO_60HZ_HI	0x46A6	
#define OX05B1S_REG_AEC_MAX2_EXPO_60HZ_LO	0x46A7	
#define OX05B1S_REG_AEC_MAX2_EXPO_50HZ_HI	0x46A8	
#define OX05B1S_REG_AEC_MAX2_EXPO_50HZ_LO	0x46A9	

#define OX05B1S_REG_MIPI_CORE_REG13		0x4813	
#define OX05B1S_REG_MIPI_CORE_REG17		0x4817	

#define OX05B1S_REG_ISP_REG00			0x5000
#define OX05B1S_REG_TEST_MODE			0x5100
#define OX05B1S_REG_TEST_MODE2			0x5101
#define OX05B1S_REG_TEST_CONST_REDBLUE_MSB	0x5106	
#define OX05B1S_REG_TEST_CONST_REDBLUE_LSB	0x5107	
#define OX05B1S_REG_TEST_CONST_GREENBLUE_MSB	0x5108
#define OX05B1S_REG_TEST_CONST_GREENBLUE_LSB	0x5109
#define OX05B1S_REG_TEST_CONST_GREENRED_MSB	0x510A
#define OX05B1S_REG_TEST_CONST_GREENRED_LSB	0x510B




#define OX05B1S_BIT_START_STREAMING		(1 << 0)
#define OX05B1S_BIT_SW_RESET			(1 << 0)

#define OX05B1S_BITS_MIPI_LANES			0xf0

#define OX05B1S_BIT_MANUAL_EXPOSURE		(1 << 3)
#define OX05B1S_BIT_MANUAL_GAIN			(1 << 7)
#define OX05B1S_BIT_HORIZONTAL_FLIP		(1 << 2)
#define OX05B1S_BIT_VERTICAL_FLIP		(1 << 2)

#define OX05B1S_BIT_ISP_REG00_ISP		BIT(6)
#define OX05B1S_BIT_ISP_REG00_WIN		BIT(5)
#define OX05B1S_BIT_ISP_REG00_RAWBIN		BIT(4)
#define OX05B1S_BIT_ISP_REG00_DPC		BIT(3)
#define OX05B1S_BIT_ISP_REG00_OTP		BIT(2)
#define OX05B1S_BIT_ISP_REG00_WBG		BIT(1)
#define OX05B1S_BIT_ISP_REG00_PREISP		BIT(0)

#define OX05B1S_BIT_TEST_ENABLE			BIT(7)
#define OX05B1S_BIT_TEST_ROLLING		BIT(6)		
#define OX05B1S_BIT_TEST_TRANSPARENT		BIT(5)
#define OX05B1S_BIT_TEST_SQUARE_BW		BIT(4)		
#define OX05B1S_BIT_TEST_BAR1			(0 << 2)
#define OX05B1S_BIT_TEST_BAR2			(1 << 2)
#define OX05B1S_BIT_TEST_BAR3			(2 << 2)
#define OX05B1S_BIT_TEST_BAR4			(3 << 2)
#define OX05B1S_BIT_TEST_BAR			(0 << 0)
#define OX05B1S_BIT_TEST_RANDOM			(1 << 0)
#define OX05B1S_BIT_TEST_SQUARE			(2 << 0)
#define OX05B1S_BIT_TEST_BLACK			(3 << 0)
#define OX05B1S_BIT_TEST_REDBLUE		(1 << 8)
#define OX05B1S_BIT_TEST_GREEN			(2 << 8)
#define OX05B1S_BIT_TEST_WHITE			(4 << 8)

#define OX05B1S_NAME				"ox05b1s"
#define OX05B1S_SENSOR_ID			0x580542
#define OX05B1S_PWDN_ACTIVE_DELAY_MS		500


#define OX05B1S_NATIVE_WIDTH			2608U
#define OX05B1S_NATIVE_HEIGHT			1960U

#define OX05B1S_PIXEL_ARRAY_LEFT		0U
#define OX05B1S_PIXEL_ARRAY_TOP			0U
#define OX05B1S_PIXEL_ARRAY_WIDTH		2592U
#define OX05B1S_PIXEL_ARRAY_HEIGHT		1944U


#define OX05B1S_CTRL_ANALOGUE_GAIN_DEFAULT	0x10
#define OX05B1S_CTRL_ANALOGUE_GAIN_MAX		0xff
#define OX05B1S_CTRL_DIGITAL_GAIN_DEFAULT	0x400
#define OX05B1S_CTRL_DIGITAL_GAIN_MAX		0x3fff

#define OX05B1S_CTRL_EXPOSURE_MIN		6
#define OX05B1S_CTRL_EXPOSURE_STEP		1
#define OX05B1S_CTRL_EXPOSURE_DEFAULT		1000
#define OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET	30




struct ox05b1s_mode {
	struct v4l2_mbus_framefmt		format;
	struct v4l2_fract			max_fps;
	struct v4l2_rect			crop;
	u64					pixel_rate;
	int					hts;
	int					vts;
	int					link_freq_index;
	const struct reg_sequence		*reg_list;
	unsigned int				num_regs;
};

struct ox05b1s {
	struct regmap			*regmap;
	struct v4l2_subdev		sd;
	struct media_pad		pad;
	struct clk			*xclk;
	bool				clock_ncont;

	// GPIOs
	struct gpio_desc		*pwdn;
	struct gpio_desc		*reset;

	// Sensor config
	u8				csi_lanes;
	const struct ox05b1s_mode	*mode;
	u8				pwr_line_freq;
	bool				streaming;

	// Sensor controls
	struct v4l2_ctrl_handler	ctrls;
	struct v4l2_ctrl		*pixel_rate;
	struct v4l2_ctrl		*hblank;
	struct v4l2_ctrl		*vblank;
	struct v4l2_ctrl		*exposure;
	struct v4l2_ctrl		*again;
	struct v4l2_ctrl		*dgain;
	struct v4l2_ctrl		*auto_exposure;
	struct v4l2_ctrl		*aec_target;
	struct v4l2_ctrl		*aec_stable_in;
	struct v4l2_ctrl		*aec_stable_out;
	struct v4l2_ctrl		*auto_gain;
	struct v4l2_ctrl		*agc_analogue_max;
	struct v4l2_ctrl		*agc_total_max;
	struct v4l2_ctrl		*auto_whitebal;
	struct v4l2_ctrl		*light_freq;
	struct v4l2_ctrl		*hflip;
	struct v4l2_ctrl		*vflip;
	struct v4l2_ctrl		*link_freq;
	struct v4l2_ctrl		*test_pattern;

	// Sensor information
	u32				module_index;
	const char			*module_facing;
	const char			*module_name;
	const char			*len_name;

	struct mutex			lock;
};




#define OX05B1S_VBLANK_MIN			24
#define OX05B1S_VTS_MAX				32767
#define OX05B1S_AUTOEXPO_DEFAULT		V4L2_EXPOSURE_AUTO
#define OX05B1S_AUTOGAIN_DEFAULT		1
#define OX05B1S_HFLIP_DEFAULT			1
#define OX05B1S_VFLIP_DEFAULT			0

#define OX05B1S_AEC_TARGET_DEFAULT		0x73
#define OX05B1S_AEC_TARGET_MAX			0xf0
#define OX05B1S_AEC_TARGET_MIN			0x10
#define OX05B1S_AEC_STABLE_IN_DEFAULT		5
#define OX05B1S_AEC_STABLE_IN_MAX		0x7f
#define OX05B1S_AEC_STABLE_IN_MIN		1
#define OX05B1S_AEC_STABLE_OUT_DEFAULT		5
#define OX05B1S_AEC_STABLE_OUT_MAX		0x7f
#define OX05B1S_AEC_STABLE_OUT_MIN		1

#define OX05B1S_AGC_ANALOGUE_GAIN_MAX_DEFAULT	0xf0
#define OX05B1S_AGC_ANALOGUE_GAIN_MAX_MIN	0x01
#define OX05B1S_AGC_ANALOGUE_GAIN_MAX_MAX	0xff
#define OX05B1S_AGC_TOTAL_GAIN_MAX_DEFAULT	0xf8
#define OX05B1S_AGC_TOTAL_GAIN_MAX_MIN		0x001
#define OX05B1S_AGC_TOTAL_GAIN_MAX_MAX		0x7ff

static const char * const test_pattern_menu[] = {
	"Disabled",
	"Colour bars #1",
	"Colour bars #2",
	"Colour bars #3",
	"Colour bars #4",
	"Color squares",
	"Blank screen",
	"Red/Blue screen",
	"Green screen",
	"White screen",
};

static const u16 test_pattern_val[] = {
	0,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR1 | OX05B1S_BIT_TEST_BAR,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR2 | OX05B1S_BIT_TEST_BAR,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR3 | OX05B1S_BIT_TEST_BAR,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR4 | OX05B1S_BIT_TEST_BAR,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_SQUARE,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BLACK,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR1 | OX05B1S_BIT_TEST_REDBLUE,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR1 | OX05B1S_BIT_TEST_GREEN,
	OX05B1S_BIT_TEST_ENABLE | OX05B1S_BIT_TEST_BAR1 | OX05B1S_BIT_TEST_WHITE,
};



#define V4L2_CID_OX05B1S_BASE			(V4L2_CTRL_CLASS_CAMERA | 0x2000)

#define V4L2_CID_OX05B1S_AEC_TARGET		(V4L2_CID_OX05B1S_BASE + 0x0000)
#define V4L2_CID_OX05B1S_AEC_STABLE_IN		(V4L2_CID_OX05B1S_BASE + 0x0001)
#define V4L2_CID_OX05B1S_AEC_STABLE_OUT		(V4L2_CID_OX05B1S_BASE + 0x0002)
#define V4L2_CID_OX05B1S_AEC_WINDOW_SLOW	(V4L2_CID_OX05B1S_BASE + 0x0003)
#define V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX	(V4L2_CID_OX05B1S_BASE + 0x0004)
#define V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX	(V4L2_CID_OX05B1S_BASE + 0x0005)


static struct v4l2_ctrl_config v4l2ctrl_aec_target = {
	.ops = NULL,
	.id = V4L2_CID_OX05B1S_AEC_TARGET,
	.name = "AEC target",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = OX05B1S_AEC_TARGET_MIN,
	.max = OX05B1S_AEC_TARGET_MAX,
	.step = 1,
	.def = OX05B1S_AEC_TARGET_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_aec_stable_in = {
	.ops = NULL,
	.id = V4L2_CID_OX05B1S_AEC_STABLE_IN,
	.name = "AEC stable-in threshold",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = OX05B1S_AEC_STABLE_IN_MIN,
	.max = OX05B1S_AEC_STABLE_IN_MAX,
	.step = 1,
	.def = OX05B1S_AEC_STABLE_IN_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_aec_stable_out = {
	.ops = NULL,
	.id = V4L2_CID_OX05B1S_AEC_STABLE_OUT,
	.name = "AEC stable-out threshold",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = OX05B1S_AEC_STABLE_OUT_MIN,
	.max = OX05B1S_AEC_STABLE_OUT_MAX,
	.step = 1,
	.def = OX05B1S_AEC_STABLE_OUT_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_agc_analogue_max = {
	.ops = NULL,
	.id = V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX,
	.name = "AGC analogue gain max",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = OX05B1S_AGC_ANALOGUE_GAIN_MAX_MIN,
	.max = OX05B1S_AGC_ANALOGUE_GAIN_MAX_MAX,
	.step = 1,
	.def = OX05B1S_AGC_ANALOGUE_GAIN_MAX_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_agc_total_max = {
	.ops = NULL,
	.id = V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX,
	.name = "AGC total gain max",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = OX05B1S_AGC_TOTAL_GAIN_MAX_MIN,
	.max = OX05B1S_AGC_TOTAL_GAIN_MAX_MAX,
	.step = 1,
	.def = OX05B1S_AGC_TOTAL_GAIN_MAX_DEFAULT,
};




static int ox05b1s_s_hblank(struct v4l2_subdev *sd, u32 val);


#endif  
