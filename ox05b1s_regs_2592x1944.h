// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for OmniVision OX05B1S RGB-IR image sensor
 *
 * Copyright (c) 2024 WavesVision.com
 */

#ifndef __DRIVERS_MEDIA_VIDEO_OX05B1S_REGS_2592X1944_30FPS_H__
#define __DRIVERS_MEDIA_VIDEO_OX05B1S_REGS_2592X1944_30FPS_H__

/*
 * Full resolution
 */

static const struct reg_sequence ox05b1s_linear_2592x1944_full[] = {

	{0x3800, 0x00}, 
	{0x3801, 0x00}, 
	{0x3802, 0x00}, 
	{0x3803, 0x00}, 

	{0x3804, 0x0a}, 
	{0x3805, 0x2f}, 
	{0x3806, 0x07}, 
	{0x3807, 0xaf}, 

	{0x3808, 0x0a}, 
	{0x3809, 0x20}, 
	{0x380a, 0x07}, 
	{0x380b, 0x98}, 

	{0x3810, 0x00},	
	{0x3811, 0x08},	
	{0x3812, 0x00},	
	{0x3813, 0x08},	

};

#endif  

