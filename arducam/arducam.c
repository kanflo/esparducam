/*
 * arducam.c
 *
 *  Created on: 2015.01.16
 *      Author: Lee
 */


/*
  ArduCAM.cpp - Arduino library support for CMOS Image Sensor
  Copyright (C)2011-2013 ArduCAM.com. All right reserved

  Basic functionality of this library are based on the demo-code provided by
  ArduCAM.com. You can find the latest version of the library at
  http://www.ArduCAM.com

  Now supported controllers:
		-	OV7670
		-	MT9D111
		-	OV7675
		-	OV2640
		-	OV3640
		-	OV5642
		-	OV7660
		-	OV7725

	We will add support for many other sensors in next release.

  Supported MCU platform
 		-	Theoretically support all Arduino families
  		-	Arduino UNO R3			(Tested)
  		-	Arduino MEGA2560 R3		(Tested)
  		-	Arduino Leonardo R3		(Tested)
  		-	Arduino Nano			(Tested)
  		-	Arduino DUE				(Tested)
  		-	Arduino Yun				(Tested)
  		-	Raspberry Pi			(Tested)

  If you make any modifications or improvements to the code, I would appreciate
  that you share the code with me so that I might include it in the next release.
  I can be contacted through http://www.ArduCAM.com

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*------------------------------------
	Revision History:
	2015/01/16 	V1.0	by Lee	first release for Raspberry Pi
--------------------------------------*/

#include "arducam.h"
#include "arducam_arch.h"
#include "memorysaver.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static struct CAM myCAM;

int arducam(sensor_model_t model)
{
	myCAM.sensor_model = model;
	switch(myCAM.sensor_model)
	{
		case smOV7660:
		case smOV7670:
		case smOV7675:
		case smOV7725:
			myCAM.sensor_addr = 0x21;
			break;
		case smMT9D111:
			myCAM.sensor_addr = 0x5d;
			break;
		case smOV3640:
		case smOV5642:
			myCAM.sensor_addr = 0x3c;
			break;
		case smOV2640:
		case smOV9655:
			myCAM.sensor_addr = 0x30;
			break;
		case smMT9M112:
			myCAM.sensor_addr = 0x5d;
			break;
		default:
			myCAM.sensor_addr = 0x21;
			break;
	}
	if (!arducam_spi_init()) {
		printf("ERROR: SPI init failed\n");
		return 0;
	}

	// initialize i2c:
	if (!arducam_i2c_init(myCAM.sensor_addr)) {
		printf("ERROR: I2C init failed\n");
		return 0;
	}
	return 1;
}

void arducam_init()
{
	switch(myCAM.sensor_model) {
#ifdef OV7660_CAM
		case smOV7660:
		{
			arducam_i2c_write(0x12, 0x80);
			arducam_delay_ms(100);
			(void) arducam_i2c_write_regs(OV7660_QVGA);
			break;
		}
#endif

#ifdef OV7725_CAM
		case smOV7725:
		{
			uint8_t reg_val;
			arducam_i2c_write(0x12, 0x80);
			arducam_delay_ms(100);
			(void) arducam_i2c_write_regs(OV7725_QVGA);
			arducam_i2c_read(0x15,&reg_val);
			arducam_i2c_write(0x15, (reg_val | 0x02));
			break;
		}
#endif

#ifdef OV7670_CAM
		case smOV7670:
		{
			arducam_i2c_write(0x12, 0x80);
			arducam_delay_ms(100);
			(void) arducam_i2c_write_regs(OV7670_QVGA);
			break;
		}
#endif

#ifdef OV7675_CAM
		case smOV7675:
		{
			arducam_i2c_write(0x12, 0x80);
			arducam_delay_ms(100);
			(void) arducam_i2c_write_regs(OV7675_QVGA);
			break;
		}
#endif

#ifdef MT9D111_CAM
		case smMT9D111:
		{
			//arducam_i2c_write_regs16(MT9D111_QVGA_3fps);
			arducam_i2c_write_regs16(MT9D111_QVGA_15fps);
			//arducam_i2c_write_regs16(MT9D111_QVGA_30fps);
			arducam_delay_ms(1000);
			arducam_i2c_write16(0x97, 0x0020);
			arducam_i2c_write16(0xf0, 0x00);
			arducam_i2c_write16(0x21, 0x8403); //Mirror Column
			arducam_i2c_write16(0xC6, 0xA103);//SEQ_CMD
        	arducam_i2c_write16(0xC8, 0x0005); //SEQ_CMD
			break;
		}
#endif

#ifdef OV5642_CAM
		case smOV5642:
		{
			uint8_t reg_val;
			arducam_i2c_word_write(0x3008, 0x80);

			arducam_delay_ms(100);
			if(myCAM.m_fmt == fmtJPEG)
			{
				arducam_i2c_write_word_regs(OV5642_1080P_Video_setting);
				arducam_i2c_word_read(0x3818,&reg_val);
				arducam_i2c_word_write(0x3818, (reg_val | 0x20) & 0xBf);
				arducam_i2c_word_read(0x3621,&reg_val);
				arducam_i2c_word_write(0x3621, reg_val | 0x20);
			}
			else
			{
				arducam_i2c_write_word_regs(OV5642_RGB_QVGA);
				arducam_i2c_word_read(0x3818,&reg_val);
				arducam_i2c_word_write(0x3818, (reg_val | 0x60) & 0xff);
				arducam_i2c_word_read(0x3621,&reg_val);
				arducam_i2c_word_write(0x3621, reg_val & 0xdf);
			}
			break;
		}
#endif

#ifdef OV3640_CAM
		case smOV3640:
		{
			(void) arducam_i2c_write_word_regs(OV3640_QVGA);
			break;
		}
#endif

#ifdef OV2640_CAM
		case smOV2640:
		{
			arducam_i2c_write(0xff, 0x01);
			arducam_i2c_write(0x12, 0x80);
			arducam_delay_ms(100);
			if(myCAM.m_fmt == fmtJPEG)
			{
				arducam_i2c_write_regs(OV2640_JPEG_INIT);
				arducam_i2c_write_regs(OV2640_YUV422);
				arducam_i2c_write_regs(OV2640_JPEG);
				arducam_i2c_write(0xff, 0x01);
				arducam_i2c_write(0x15, 0x00);
				arducam_i2c_write_regs(OV2640_320x240_JPEG);
				//arducam_i2c_write(0xff, 0x00);
				//arducam_i2c_write(0x44, 0x32);
			}
			else
			{
				arducam_i2c_write_regs(OV2640_QVGA);
			}
			break;
		}
#endif

		case smOV9655:
		{

			break;
		}
		case smMT9M112:
		{

			break;
		}

		default:

			break;
	}
}

void arducam_flush_fifo(void)
{
	arducam_write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

void arducam_start_capture(void)
{
	arducam_write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

void arducam_clear_fifo_flag(void)
{
	arducam_write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

uint8_t arducam_read_fifo(void)
{
	uint8_t data;
	data = arducam_spi_read(0x3D);
	return data;
}

uint8_t arducam_read_reg(uint8_t addr)
{
	uint8_t data;
	data = arducam_spi_read(addr & 0x7F);
	return data;
}

void arducam_write_reg(uint8_t addr, uint8_t data)
{
	arducam_spi_write(addr | 0x80, data);
}

void arducam_set_jpeg_size(jpeg_size_t size)
{
#ifdef OV2640_CAM
	switch(size)
	{
		case sz160x120:
			arducam_i2c_write_regs(OV2640_160x120_JPEG);
			break;
		case sz176x144:
			arducam_i2c_write_regs(OV2640_176x144_JPEG);
			break;
		case sz320x240:
			arducam_i2c_write_regs(OV2640_320x240_JPEG);
			break;
		case sz352x288:
			arducam_i2c_write_regs(OV2640_352x288_JPEG);
			break;
		case sz640x480:
			arducam_i2c_write_regs(OV2640_640x480_JPEG);
			break;
		case sz800x600:
			arducam_i2c_write_regs(OV2640_800x600_JPEG);
			break;
		case sz1024x768:
			arducam_i2c_write_regs(OV2640_1024x768_JPEG);
			break;
		case sz1280x1024:
			arducam_i2c_write_regs(OV2640_1280x1024_JPEG);
			break;
		case sz1600x1200:
			arducam_i2c_write_regs(OV2640_1600x1200_JPEG);
			break;
		default:
			arducam_i2c_write_regs(OV2640_320x240_JPEG);
			break;
	}
#endif
}

void arducam_set_format(image_format_t fmt)
{
	myCAM.m_fmt = fmt;
}
