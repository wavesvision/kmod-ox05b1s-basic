/*
 * OmniVision OX05B1S Camera Header File
 *
 * Copyright (c) 2024-2025 Waves Vision Ltd. <charlesb@wavesvision.com>
 */

#ifndef __DRIVERS_MEDIA_VIDEO_OX05B1S_H__
#define __DRIVERS_MEDIA_VIDEO_OX05B1S_H__

/* Definitions				*/
/****************************************/
/* Registers */
#define OX05B1S_REG_MODE_SEL			CCI_REG8(0x0100)
#define OX05B1S_REG_MODE_SEL_STREAM_EN					BIT(0)
#define OX05B1S_REG_SW_RESET			CCI_REG8(0x0103)
#define OX05B1S_REG_SW_RESET_BIT                    			BIT(0)
#define OX05B1S_REG_ALT_RESET			CCI_REG8(0x0107)

#define OX05B1S_REG_SENSOR_ID			CCI_REG24(0x300a)
#define OX05B1S_REG_SENSOR_ID_VALUE					0x580542

#define OX05B1S_REG_COMMON_MIPI_CFG		CCI_REG8(0x3010)
#define OX05B1S_REG_COMMON_MIPI_CFG_LANES				0xf0


#define OX05B1S_REG_EC_A_EXPO			CCI_REG24(0x3500)	
#define OX05B1S_REG_EC_A_CTRL03			CCI_REG8(0x3503)	
#define OX05B1S_REG_EC_A_CTRL03_AEC_EN					BIT(3)
#define OX05B1S_REG_EC_A_CTRL03_AGC_EN					BIT(7)
//#define OX05B1S_REG_EC_A_CTRL04			0x3504	
#define OX05B1S_REG_EC_A_MANUAL_AGAIN		CCI_REG16(0x3508)	
#define OX05B1S_REG_EC_A_MANUAL_DGAIN		CCI_REG24(0x350a)	

#define OX05B1S_REG_EC_A_MAX_GAIN_COARSE	CCI_REG16(0x3523)	
#define OX05B1S_REG_EC_A_MAX_GAIN_FINE		CCI_REG8(0x3524)	

#define OX05B1S_REG_TIMING_CTRL_HTS		CCI_REG16(0x380C)
#define OX05B1S_REG_TIMING_CTRL_VTS		CCI_REG16(0x380E)

#define OX05B1S_REG_TIMING_CTRL_REG20		CCI_REG8(0x3820)	
#define OX05B1S_REG_TIMING_CTRL_REG20_VFLIP				(1 << 2)
#define OX05B1S_REG_TIMING_CTRL_REG21		CCI_REG8(0x3821)	
#define OX05B1S_REG_TIMING_CTRL_REG21_HFLIP				(1 << 2)

#define OX05B1S_REG_AEC_CTRL85			CCI_REG8(0x4685)	
//#define OX05B1S_REG_AEC_UPDATE_SPEED1		0x4686	
//#define OX05B1S_REG_AEC_UPDATE_SPEED23		0x4687	

#define OX05B1S_REG_AEC_MAX_BANDS_60HZ		CCI_REG8(0x468D)	
#define OX05B1S_REG_AEC_MAX_BANDS_50HZ		CCI_REG8(0x468E)	
#define OX05B1S_REG_AEC_WPT			CCI_REG8(0x468F)	
#define OX05B1S_REG_AEC_BPT			CCI_REG8(0x4690)	
//#define OX05B1S_REG_AEC_VPT_HIGH		0x4691	

#define OX05B1S_REG_AEC_GAIN_CEIL		CCI_REG16(0x4698)	

#define OX05B1S_REG_AEC_WPT2			CCI_REG8(0x469B)	
#define OX05B1S_REG_AEC_BPT2			CCI_REG8(0x469E)	
//#define OX05B1S_REG_AEC_VPT_LOW			0x469F	

#define OX05B1S_REG_AEC_MAX2_EXPO_60HZ		CCI_REG16(0x46A6)	
#define OX05B1S_REG_AEC_MAX2_EXPO_50HZ		CCI_REG16(0x46A8)	

#define OX05B1S_REG_MIPI_CORE_REG13		CCI_REG8(0x4813)

#define OX05B1S_REG_ISP_REG00			CCI_REG8(0x5000)
#define OX05B1S_REG_ISP_REG00_PREISP					BIT(0)
#define OX05B1S_REG_ISP_REG00_WBG					BIT(1)
#define OX05B1S_REG_ISP_REG00_OTP					BIT(2)
#define OX05B1S_REG_ISP_REG00_DPC					BIT(3)
#define OX05B1S_REG_ISP_REG00_RAWBIN					BIT(4)
#define OX05B1S_REG_ISP_REG00_WIN					BIT(5)
#define OX05B1S_REG_ISP_REG00_ISP					BIT(6)
#define OX05B1S_REG_TEST_MODE			CCI_REG8(0x5100)
#define OX05B1S_REG_TEST_MODE_ENABLE					BIT(7)
#define OX05B1S_REG_TEST_MODE_ROLLING					BIT(6)		
#define OX05B1S_REG_TEST_MODE_TRANSPARENT				BIT(5)
#define OX05B1S_REG_TEST_MODE_SQUARE_BW					BIT(4)		
#define OX05B1S_REG_TEST_MODE_BAR1					(0 << 2)
#define OX05B1S_REG_TEST_MODE_BAR2					(1 << 2)
#define OX05B1S_REG_TEST_MODE_BAR3					(2 << 2)
#define OX05B1S_REG_TEST_MODE_BAR4					(3 << 2)
#define OX05B1S_REG_TEST_MODE_BAR					(0 << 0)
#define OX05B1S_REG_TEST_MODE_RANDOM					(1 << 0)
#define OX05B1S_REG_TEST_MODE_SQUARE					(2 << 0)
#define OX05B1S_REG_TEST_MODE_BLACK					(3 << 0)
#define OX05B1S_REG_TEST_MODE_RED					(1 << 8)	
#define OX05B1S_REG_TEST_MODE_GREEN					(2 << 8)	
#define OX05B1S_REG_TEST_MODE_BLUE					(4 << 8)
#define OX05B1S_REG_TEST_MODE2			CCI_REG8(0x5101)
#define OX05B1S_REG_TEST_MODE2_CONST_CLR				BIT(6)
#define OX05B1S_REG_TEST_CONST_BLUE		CCI_REG16(0x5106)
#define OX05B1S_REG_TEST_CONST_GREENBLUE	CCI_REG16(0x5108)
#define OX05B1S_REG_TEST_CONST_GREENRED		CCI_REG16(0x510A)
#define OX05B1S_REG_TEST_CONST_RED		CCI_REG16(0x510C)


#define OX05B1S_SENSOR_NATIVE_HEIGHT		1960U
#define OX05B1S_SENSOR_NATIVE_WIDTH		2608U

#define OX05B1S_SENSOR_PIXEL_ARRAY_TOP		0U
#define OX05B1S_SENSOR_PIXEL_ARRAY_LEFT		0U
#define OX05B1S_SENSOR_PIXEL_ARRAY_HEIGHT	1944U
#define OX05B1S_SENSOR_PIXEL_ARRAY_WIDTH	2592U

#define	OX05B1S_XCLK_FREQ_MIN			6000000
#define	OX05B1S_XCLK_FREQ_MAX			27000000

#define OX05B1S_CFG_HBLANK			0
#define OX05B1S_CFG_VBLANK			0
#define OX05B1S_CFG_CAM_ORIENTATION		V4L2_CAMERA_ORIENTATION_EXTERNAL
#define OX05B1S_CFG_PIXEL_RATE			192000000
#define OX05B1S_CFG_LINK_FREQ_450M		450000000
#define OX05B1S_CFG_LINK_FREQ_225M		225000000

#define IMAGE_PAD				0


static const s64 ox05b1s_link_freq_2lanes[] = {
	OX05B1S_CFG_LINK_FREQ_450M,
};

static const s64 ox05b1s_link_freq_4lanes[] = {
	OX05B1S_CFG_LINK_FREQ_225M,
	OX05B1S_CFG_LINK_FREQ_450M,
};

static const s64 ox05b1s_link_freq_menu[] = {
	OX05B1S_CFG_LINK_FREQ_225M,
	OX05B1S_CFG_LINK_FREQ_450M,
};

static const int ox05b1s_linkfreq_2_hts[] = {
	0x2f0,					// 30 FPS
	0x170,					// 60 FPS
};


/* Regulators				*/
/****************************************/
static const char * const ox05b1s_supply_names[] = {
        "vsensor",		
};
#define OX05B1S_NUM_SUPPLIES		ARRAY_SIZE(ox05b1s_supply_names)


/* Structures				*/
/****************************************/
struct ox05b1s_mode {
	struct v4l2_mbus_framefmt	format;
	u32				vts_def;
	const struct cci_reg_sequence	*reg_list;
	u32				reg_list_size;
};

struct ox05b1s_sensor {
	
	struct device			*dev;
	struct clk			*xclk;
	int				xclk_freq;
	struct i2c_client		*client;
	struct regmap			*regmap;
	struct regulator_bulk_data	supplies[OX05B1S_NUM_SUPPLIES];
	unsigned int			num_lanes;
	unsigned long			link_freq_bitmap;
	u8				pwr_line_freq;

	
	struct v4l2_subdev		subdev;
	struct media_pad		pad;
	struct v4l2_ctrl_handler	ctrl_handler;

	
	struct v4l2_ctrl		*v4l2_exposure;
	struct v4l2_ctrl		*v4l2_auto_exposure;
	struct v4l2_ctrl		*v4l2_again;
	struct v4l2_ctrl		*v4l2_dgain;

	
	struct v4l2_ctrl		*v4l2_aec_target;
	struct v4l2_ctrl		*v4l2_aec_stable_in;
	struct v4l2_ctrl		*v4l2_aec_stable_out;

	
	struct v4l2_ctrl		*v4l2_auto_gain;
	struct v4l2_ctrl		*v4l2_agc_analogue_max;
	struct v4l2_ctrl		*v4l2_agc_total_max;

	
	struct v4l2_ctrl		*v4l2_auto_whitebal;
	struct v4l2_ctrl		*v4l2_light_freq;
	struct v4l2_ctrl		*v4l2_hblank;
	struct v4l2_ctrl		*v4l2_vblank;
	struct v4l2_ctrl		*v4l2_orientation;
	struct v4l2_ctrl		*v4l2_rotation;
	struct v4l2_ctrl		*v4l2_pixel_rate;
	struct v4l2_ctrl		*v4l2_link_freq;
	struct v4l2_ctrl		*v4l2_hflip;
	struct v4l2_ctrl		*v4l2_vflip;
	struct v4l2_ctrl		*v4l2_test_pattern;

	const struct ox05b1s_mode	*current_mode;
	struct v4l2_fract		max_fps;
	u32				test_mode;

	struct mutex			lock;				
};

/* V4L2					*/
/****************************************/
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

/*
 * Exposure control
 * Set max value as 0x850 (frame length = 2128) - 30 = 0x0832 for 16.66 ms exposure
 * Set default value to be 1000 (0x03E8).
 */
#define OX05B1S_CTRL_ANALOGUE_GAIN_DEFAULT	0x10
#define OX05B1S_CTRL_ANALOGUE_GAIN_MAX		0xff
#define OX05B1S_CTRL_DIGITAL_GAIN_DEFAULT	0x400
#define OX05B1S_CTRL_DIGITAL_GAIN_MAX		0x3fff

#define OX05B1S_CTRL_EXPOSURE_MIN		6
#define OX05B1S_CTRL_EXPOSURE_STEP		1
#define OX05B1S_CTRL_EXPOSURE_DEFAULT		1000
#define OX05B1S_CTRL_EXPOSURE_MAX_VTS_OFFSET	30


static const char * const ox05b1s_test_pattern_menu[] = {
	"Disabled",
	"Colour bars #1",
	"Colour bars #2",
	"Colour bars #3",
	"Colour bars #4",
	"Color squares",
	"Blank screen",
	"Red screen",
	"Green screen",
	"Blue screen",
};

static const u16 ox05b1s_test_pattern_val[] = {
	0,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BAR1 | OX05B1S_REG_TEST_MODE_BAR,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BAR2 | OX05B1S_REG_TEST_MODE_BAR,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BAR3 | OX05B1S_REG_TEST_MODE_BAR,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BAR4 | OX05B1S_REG_TEST_MODE_BAR,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_SQUARE,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BLACK,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BLACK | OX05B1S_REG_TEST_MODE_RED,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BLACK | OX05B1S_REG_TEST_MODE_GREEN,
	OX05B1S_REG_TEST_MODE_ENABLE | OX05B1S_REG_TEST_MODE_BLACK | OX05B1S_REG_TEST_MODE_BLUE,
};

/* V4L2 custom controls			*/
/****************************************/
#define OX05B1S_V4L2_CTRLS_MAX		20

#define V4L2_CID_OX05B1S_BASE			(V4L2_CTRL_CLASS_CAMERA | 0x2000)
#define V4L2_CID_OX05B1S_AEC_TARGET		(V4L2_CID_OX05B1S_BASE + 0x0000)
#define V4L2_CID_OX05B1S_AEC_STABLE_IN		(V4L2_CID_OX05B1S_BASE + 0x0001)
#define V4L2_CID_OX05B1S_AEC_STABLE_OUT		(V4L2_CID_OX05B1S_BASE + 0x0002)
#define V4L2_CID_OX05B1S_AEC_WINDOW_SLOW	(V4L2_CID_OX05B1S_BASE + 0x0003)
#define V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX	(V4L2_CID_OX05B1S_BASE + 0x0004)
#define V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX	(V4L2_CID_OX05B1S_BASE + 0x0005)


static struct v4l2_ctrl_config v4l2ctrl_aec_target = {
	.ops	= NULL,
	.id	= V4L2_CID_OX05B1S_AEC_TARGET,
	.name	= "AEC target",
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.min	= OX05B1S_AEC_TARGET_MIN,
	.max	= OX05B1S_AEC_TARGET_MAX,
	.step	= 1,
	.def	= OX05B1S_AEC_TARGET_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_aec_stable_in = {
	.ops	= NULL,
	.id	= V4L2_CID_OX05B1S_AEC_STABLE_IN,
	.name	= "AEC stable-in threshold",
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.min	= OX05B1S_AEC_STABLE_IN_MIN,
	.max	= OX05B1S_AEC_STABLE_IN_MAX,
	.step	= 1,
	.def	= OX05B1S_AEC_STABLE_IN_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_aec_stable_out = {
	.ops	= NULL,
	.id	= V4L2_CID_OX05B1S_AEC_STABLE_OUT,
	.name	= "AEC stable-out threshold",
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.min	= OX05B1S_AEC_STABLE_OUT_MIN,
	.max	= OX05B1S_AEC_STABLE_OUT_MAX,
	.step	= 1,
	.def	= OX05B1S_AEC_STABLE_OUT_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_agc_analogue_max = {
	.ops	= NULL,
	.id	= V4L2_CID_OX05B1S_AGC_ANALOGUE_GAIN_MAX,
	.name	= "AGC analogue gain max",
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.min	= OX05B1S_AGC_ANALOGUE_GAIN_MAX_MIN,
	.max	= OX05B1S_AGC_ANALOGUE_GAIN_MAX_MAX,
	.step	= 1,
	.def	= OX05B1S_AGC_ANALOGUE_GAIN_MAX_DEFAULT,
};

static struct v4l2_ctrl_config v4l2ctrl_agc_total_max = {
	.ops	= NULL,
	.id	= V4L2_CID_OX05B1S_AGC_TOTAL_GAIN_MAX,
	.name	= "AGC total gain max",
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.min	= OX05B1S_AGC_TOTAL_GAIN_MAX_MIN,
	.max	= OX05B1S_AGC_TOTAL_GAIN_MAX_MAX,
	.step	= 1,
	.def	= OX05B1S_AGC_TOTAL_GAIN_MAX_DEFAULT,
};


/* Sensor config						*/
/****************************************************************/
#include "ox05b1s_regs_2592x1944.h"

static const struct ox05b1s_mode ox05b1s_supported_modes[] = {
	
	{
		.format		= {
					.code		= MEDIA_BUS_FMT_SGRBG10_1X10,
					.colorspace	= V4L2_COLORSPACE_RAW,
					.field		= V4L2_FIELD_NONE,
					.width		= OX05B1S_SENSOR_PIXEL_ARRAY_WIDTH,
					.height		= OX05B1S_SENSOR_PIXEL_ARRAY_HEIGHT,
				},
		.vts_def	= 0x850,
		.reg_list	= &ox05b1s_linear_2592x1944_full[0],
		.reg_list_size	= ARRAY_SIZE(ox05b1s_linear_2592x1944_full),
        },
};

#define OX05B1S_V4L2_DEFAULT_MODE	ox05b1s_supported_modes[0]

#endif  
