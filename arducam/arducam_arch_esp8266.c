//#define GPIO16_HACK

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
//#include "ets_sys.h"
//#include "osapi.h"

#define ARDUCAM_CS  (4)
//#define ARDUCAM_CS  (16)

static uint8_t _sensor_addr;

#ifdef GPIO16_HACK
static void esp8266_pin16Mode(uint8_t mode);
static void esp8266_digital16Write(uint8_t val);
//static int esp8266_digital16Read(void);
 #define INPUT             0x00
 #define INPUT_PULLUP      0x02
 #define INPUT_PULLDOWN_16 0x04 // PULLDOWN only possible for pin16
 #define OUTPUT            0x01
 #define OUTPUT_OPEN_DRAIN 0x03
 #define WAKEUP_PULLUP     0x05
 #define WAKEUP_PULLDOWN   0x07
 #define SPECIAL           0xF8 //defaults to the usable BUSes uart0rx/tx uart1tx and hspi
 #define FUNCTION_0        0x08
 #define FUNCTION_1        0x18
 #define FUNCTION_2        0x28
 #define FUNCTION_3        0x38
 #define FUNCTION_4        0x48
#endif // GPIO16_HACK


//#define CONFIG_VERIFY

#define I2C_SDA_PIN (2)
#define I2C_SCK_PIN (0) 


static void spi_chip_select(uint8_t cs_pin);
static void spi_chip_unselect(uint8_t cs_pin);


bool arducam_spi_init(void)
{
	printf("arducam_spi_init\n");
#ifdef GPIO16_HACK
    if (ARDUCAM_CS == 16) {
    	printf("ARDUCAM_CS is GPIO 16\n");
		esp8266_pin16Mode(OUTPUT);
		esp8266_digital16Write(0);
	} else
#endif // GPIO16_HACK
	gpio_enable(ARDUCAM_CS, GPIO_OUTPUT);
    gpio_write(ARDUCAM_CS, 1);
	spi_init(iHSPI);

#ifdef GPIO16_HACK
	esp8266_digital16Write(0); arducam_delay_ms(10);
	esp8266_digital16Write(1); arducam_delay_ms(10);
	esp8266_digital16Write(0); arducam_delay_ms(10);
	esp8266_digital16Write(1); arducam_delay_ms(10);
	esp8266_digital16Write(0); arducam_delay_ms(10);
	esp8266_digital16Write(1); arducam_delay_ms(10);
#endif // GPIO16_HACK
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
//	hspi_Tx(ARDUCAM_CS, data, sizeof(data));
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
#ifdef GPIO16_HACK
    if (ARDUCAM_CS == 16)
		esp8266_digital16Write(0);
	else
#endif // GPIO16_HACK
    gpio_write(cs_pin, 0);
}

static void spi_chip_unselect(uint8_t cs_pin)
{
#ifdef GPIO16_HACK
    if (ARDUCAM_CS == 16)
		esp8266_digital16Write(1);
	else
#endif // GPIO16_HACK
    gpio_write(cs_pin, 1);
}



#ifdef GPIO16_HACK
#define ESP8266_REG(addr) *((volatile uint32_t *)(0x60000000+(addr)))

//GPIO 16 Control Registers
#define GP16O  ESP8266_REG(0x768)
#define GP16E  ESP8266_REG(0x774)
#define GP16I  ESP8266_REG(0x78C)

//GPIO 16 PIN Control Register
#define GP16C  ESP8266_REG(0x790)
#define GPC16  GP16C

//GPIO 16 PIN Function Register
#define GP16F  ESP8266_REG(0x7A0)
#define GPF16  GP16F

//GPIO 16 PIN Function Bits
#define GP16FFS0 0 //Function Select bit 0
#define GP16FFS1 1 //Function Select bit 1
#define GP16FPD  3 //Pulldown
#define GP16FSPD 5 //Sleep Pulldown
#define GP16FFS2 6 //Function Select bit 2
#define GP16FFS(f) (((f) & 0x03) | (((f) & 0x04) << 4))

#define GPFFS_GPIO(p) (((p)==0||(p)==2||(p)==4||(p)==5)?0:((p)==16)?1:3)


static void esp8266_pin16Mode(uint8_t mode) {
  GPF16 = GP16FFS(GPFFS_GPIO(16));//Set mode to GPIO
  GPC16 = 0;
  if(mode == INPUT || mode == INPUT_PULLDOWN_16){
    if(mode == INPUT_PULLDOWN_16){
      GPF16 |= (1 << GP16FPD);//Enable Pulldown
    }
    GP16E &= ~1;
  } else if(mode == OUTPUT){
    GP16E |= 1;
  }
}

static void esp8266_digital16Write(uint8_t val) {
  if(val) GP16O |= 1;
  else GP16O &= ~1;
}
/*
static int esp8266_digital16Read(void) {
  return GP16I & 0x01;
}
*/
#endif // GPIO16_HACK
