/*
 * OmniVision OX05B1S Camera Header File
 *
 * Copyright (c) 2024-2025 Waves Vision Ltd. <charlesb@wavesvision.com>
 */

#ifndef __DRIVERS_MEDIA_VIDEO_OX05B1S_REGS_2592X1944_30FPS_H__
#define __DRIVERS_MEDIA_VIDEO_OX05B1S_REGS_2592X1944_30FPS_H__

/*
 * Full resolution
 */

static const struct cci_reg_sequence ox05b1s_linear_2592x1944_full[] = {

	{ CCI_REG8(0x3808), 0x0a },
	{ CCI_REG8(0x3809), 0x20 },
	{ CCI_REG8(0x380a), 0x07 },
	{ CCI_REG8(0x380b), 0x98 },

	{ CCI_REG8(0x5280), 0x00 },
	{ CCI_REG8(0x5289), 0x49 },
};

#endif
