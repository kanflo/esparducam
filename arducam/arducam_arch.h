#ifndef __ARDUCAM_ARCH_H__
#define __ARDUCAM_ARCH_H__

#include <stdbool.h>
#include <stdint.h>
#include "arducam.h"

bool arducam_spi_init(void);
bool arducam_i2c_init(uint8_t sensor_addr);

void arducam_spi_write(uint8_t address, uint8_t value);
uint8_t arducam_spi_read(uint8_t address);

// Delay execution for delay milliseconds
void arducam_delay_ms(uint32_t delay);

// Read/write 8 bit value to/from 8 bit register address
uint8_t arducam_i2c_write(uint8_t regID, uint8_t regDat);
uint8_t arducam_i2c_read(uint8_t regID, uint8_t* regDat);

// Read/write 16 bit value to/from 8 bit register address
uint8_t arducam_i2c_write16(uint8_t regID, uint16_t regDat);
uint8_t arducam_i2c_read16(uint8_t regID, uint16_t* regDat);

// Read/write 8 bit value to/from 16 bit register address
uint8_t arducam_i2c_word_write(uint16_t regID, uint8_t regDat);
uint8_t arducam_i2c_word_read(uint16_t regID, uint8_t* regDat);

// Write 8 bit values to 8 bit register address
int arducam_i2c_write_regs(const struct sensor_reg reglist[]);

// Write 16 bit values to 8 bit register address
int arducam_i2c_write_regs16(const struct sensor_reg reglist[]);

// Write 8 bit values to 16 bit register address
int arducam_i2c_write_word_regs(const struct sensor_reg reglist[]);

#endif // __ARDUCAM_ARCH_H__