/* 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Johan Kanflo (github.com/kanflo)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <FreeRTOS.h>
#include "task.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <spi.h>
#include <i2c/i2c.h>
#include "arducam.h"
#include "arducam_arch.h"

#define ARDUCAM_CS  (4)

static uint8_t _sensor_addr;

//#define CONFIG_VERIFY

#define I2C_SDA_PIN (2)
#define I2C_SCK_PIN (0) 


static void spi_chip_select(uint8_t cs_pin);
static void spi_chip_unselect(uint8_t cs_pin);


bool arducam_spi_init(void)
{
	printf("arducam_spi_init\n");
	gpio_enable(ARDUCAM_CS, GPIO_OUTPUT);
    gpio_write(ARDUCAM_CS, 1);
	spi_init(iHSPI);
	return true;
}

bool arducam_i2c_init(uint8_t sensor_addr)
{
	printf("arducam_i2c_init\n");
	i2c_init(I2C_SCK_PIN, I2C_SDA_PIN);
	_sensor_addr = sensor_addr;
	return true;
}

void arducam_delay_ms(uint32_t delay)
{
	vTaskDelay(delay / portTICK_RATE_MS);
}

void arducam_spi_write(uint8_t address, uint8_t value)
{
#ifdef CONFIG_VERIFY
	static int counter = 0;
#endif // CONFIG_VERIFY

	uint8_t data[2] = {address, value};
	spi_chip_select(ARDUCAM_CS);
	spi_tx16(iHSPI, data[0] << 8 | data[1]);
	spi_chip_unselect(ARDUCAM_CS);
#ifdef CONFIG_VERIFY
	data[0] = arducam_spi_read(address & 0x7f);
//	printf("arducam_spi_write: [0x%02x] = 0x%02x\n", address & 0x7f, value);
	if (data[0] != value) {
		printf("arducam_spi_write: verify failed after %d for reg 0x%02x (0x%02x should be 0x%02x)\n", counter, address & 0x7f, data[0], value);
	}
	counter++;
#endif // CONFIG_VERIFY
}

uint8_t arducam_spi_read(uint8_t address)
{
	uint8_t data[2] = {address, 0x00};
	spi_chip_select(ARDUCAM_CS);
	spi_tx8(iHSPI, data[0]);
//	spi_chip_unselect(ARDUCAM_CS); // The HW SPI does this but things does not work if we do the same
//	spi_chip_select(ARDUCAM_CS);
	data[1] = (uint8_t) spi_rx8(iHSPI);
	spi_chip_unselect(ARDUCAM_CS);
  	return data[1];
}

uint8_t arducam_i2c_write(uint8_t regID, uint8_t regDat)
{
	uint8_t data[] = {regID, regDat};
	return i2c_slave_write(_sensor_addr, data, sizeof(data));
}

uint8_t arducam_i2c_read(uint8_t regID, uint8_t* regDat)
{
	return i2c_slave_read(_sensor_addr, regID, regDat, 1);
}

uint8_t arducam_i2c_write16(uint8_t regID, uint16_t regDat)
{
	return 0;
}

uint8_t arducam_i2c_read16(uint8_t regID, uint16_t* regDat)
{
	return 0;
}

uint8_t arducam_i2c_word_write(uint16_t regID, uint8_t regDat)
{
	return 0;
}

uint8_t arducam_i2c_word_read(uint16_t regID, uint8_t* regDat)
{
	return 0;
}

int arducam_i2c_write_regs(const struct sensor_reg reglist[])
{
	uint16_t reg_addr = 0, reg_idx = 0;
	uint16_t reg_val = 0;
	const struct sensor_reg *next = reglist;

	while ((reg_addr != 0xff) | (reg_val != 0xff))
	{
		reg_addr = pgm_read_word(&next->reg);
		reg_val = pgm_read_word(&next->val);
		if (!arducam_i2c_write(reg_addr, reg_val)) {
			printf("arducam_i2c_write_regs failed at register %d\n", reg_idx);
			return 0;
		}
	   	next++;
	   	reg_idx++;
	}

	return 1;
}


int arducam_i2c_write_regs16(const struct sensor_reg reglist[])
{
	unsigned int reg_addr = 0, reg_val = 0;
	const struct sensor_reg *next = reglist;
	while ((reg_addr != 0xff) | (reg_val != 0xffff))
	{
		reg_addr = pgm_read_word(&next->reg);
		reg_val = pgm_read_word(&next->val);
		if (!arducam_i2c_write16(reg_addr, reg_val)) {
			return 0;
		}
	   	next++;
	}

	return 1;
}

int arducam_i2c_write_word_regs(const struct sensor_reg reglist[])
{
	unsigned int reg_addr = 0, reg_val = 0, reg_idx = 0;
	const struct sensor_reg *next = reglist;

	while ((reg_addr != 0xffff) | (reg_val != 0xff))
	{
		reg_addr = pgm_read_word(&next->reg);
		reg_val = pgm_read_word(&next->val);
		if (!arducam_i2c_write16(reg_addr, reg_val)) {
			printf("arducam_i2c_write_word_regs failed at register %d\n", reg_idx);
			return 0;
		}
	   	next++;
	   	reg_idx++;
	}

	return 1;
}

static void spi_chip_select(uint8_t cs_pin)
{
    gpio_write(cs_pin, 0);
}

static void spi_chip_unselect(uint8_t cs_pin)
{
    gpio_write(cs_pin, 1);
}
