/**
 @file hspi.c

 @brief HSPI driver for ESP8255
 Based on initial work from Sem 2015 - mostly rewrittem
 Added code to handle proper aligned reads and writes
 @par Copyright &copy; 2015 Sem
 @par Copyright &copy; 2015 Mike Gore, GPL License
 @par You are free to use this code under the terms of GPL
  Please retain a copy of this notice in any code you use it in.

  This is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option)
  any later version.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//#include <user_config.h>
#include <stdint.h>
#include <espressif/esp8266/esp8266.h>
#include <espressif/esp8266/spi_register.h>
#include <esp/gpio.h>
#include <hspi.h>

#define SOFTCS
#define CS_PIN  (4)

// @brief HSPI Prescale value - You should set this in your Makefile
#ifndef HSPI_PRESCALER
#define HSPI_PRESCALER 16
#endif

static uint16_t _f_tx_ind = 0;                    // fifo buffer index
static uint8_t _f_tx_buf[HSPI_FIFO_SIZE+2];       // fifo buffer, same as HSPI FIFO

/// @brief HSPI Initiaization - with automatic chip sellect
/// Pins:
/// 	MISO GPIO12
/// 	MOSI GPIO13
/// 	CLK GPIO14
/// 	CS GPIO15
/// 	DC GPIO2
/// @return  void
void hspi_init(uint16_t prescale)
{
    WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105);        //clear bit9

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);    // HSPIQ MISO GPIO12
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);    // HSPID MOSI GPIO13
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);    // CLK        GPIO14

#ifdef SOFTCS
/* Enable GPIO on the specified pin, and set it to input/output/ with
 *  pullup as needed
 */
    gpio_enable(CS_PIN, GPIO_OUTPUT);
//    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    gpio_write(CS_PIN, 1);
//    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);
    //gpio_write(GPIO_ID_PIN(15),1);
#else
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);    // CS         GPIO15
#endif

// SPI clock = CPU clock / 10 / 4
// time length HIGHT level = (CPU clock / 10 / 2) ^ -1,
// time length LOW level = (CPU clock / 10 / 2) ^ -1
    WRITE_PERI_REG(SPI_CLOCK(HSPI),
        (((prescale - 1) & SPI_CLKDIV_PRE) << SPI_CLKDIV_PRE_S) |
        ((1 & SPI_CLKCNT_N) << SPI_CLKCNT_N_S) |
        ((0 & SPI_CLKCNT_H) << SPI_CLKCNT_H_S) |
        ((1 & SPI_CLKCNT_L) << SPI_CLKCNT_L_S));

    WRITE_PERI_REG(SPI_CTRL1(HSPI), 0);
}


/// @brief HSPI Configuration for tranasmit and receive
/// @param[in] configState: CONFIG_FOR_RX_TX or CONFIG_FOR_RX
/// @return  void
void hspi_config(int configState)
{
    uint32_t valueOfRegisters = 0;

    hspi_waitReady();

    if (configState == CONFIG_FOR_TX)
    {
        valueOfRegisters |=  SPI_USR_MOSI;
//clear bit 2 see example IoT_Demo
        valueOfRegisters &= \
            ~(BIT2 | SPI_USR_ADDR | SPI_USR_DUMMY | \
            SPI_USR_MISO | SPI_USR_COMMAND | SPI_DOUTDIN);
    }
    else if  (configState == CONFIG_FOR_RX_TX)
    {
        valueOfRegisters |=  SPI_USR_MOSI | SPI_DOUTDIN | SPI_CK_I_EDGE;
//clear bit 2 see example IoT_Demo
        valueOfRegisters &= \
            ~(BIT2 | SPI_USR_ADDR | SPI_USR_DUMMY | \
            SPI_USR_MISO | SPI_USR_COMMAND);
    }
    else
    {
        return;                                   // Error
    }
    WRITE_PERI_REG(SPI_USER(HSPI), valueOfRegisters);

}


/// @brief HSPI FIFO send or receive byte count
/// @param[in] bytes: bytes to send or receive
// Only called from hspi_writeFIFO or hspi_readFIFO
// So bounds testing has already been done
/// @return  void
void hspi_setBits(uint16_t bytes)
{
    uint16_t bits = (bytes << 3) - 1;
    WRITE_PERI_REG(SPI_USER1(HSPI),
        ( (bits & SPI_USR_MOSI_BITLEN) << SPI_USR_MOSI_BITLEN_S ) |
        ( (bits & SPI_USR_MISO_BITLEN) << SPI_USR_MISO_BITLEN_S ) );
}


/// @brief HSPI Start Send
/// @return  void
void hspi_startSend(void)
{
#ifdef SOFTCS
    gpio_write(CS_PIN, 0);
//    gpio_write(GPIO_ID_PIN(15), 0);
#endif
    SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
}


/// @brief HSPI Ready wait
/// @return  void
void hspi_waitReady(void)
{
    while (READ_PERI_REG(SPI_CMD(HSPI)) & SPI_USR) {};
#ifdef SOFTCS
    gpio_write(CS_PIN, 1);
//    gpio_write(GPIO_ID_PIN(15), 1);
#endif
}


/// @brief HSPI FIFO write in units of 32 bits (4 byte words)
/// @param[in] write_data: write buffer
/// @param[in] bytes: bytes to write
/// @return  void
void hspi_writeFIFO(uint8_t *write_data, uint16_t bytes)
{
    uint8_t word_ind = 0;

    if(bytes > HSPI_FIFO_SIZE)                    // TODO set error status
        return;

    hspi_setBits(bytes);                          // Update FIFO with number of bits we will send

// First do a fast write with 32 bit chunks at a time
    while(bytes >= 4)
    {
// Cast both source and destination to 4 byte word pointers
        ((uint32_t *)SPI_W0(HSPI)) [word_ind] = \
            ((uint32_t *)write_data) [word_ind];

        bytes -= 4;
        word_ind++;
    }

// Next write remaining bytes (if any) to FIFO as a single word
// Note: We follow the good practice of avoiding access past the end of an array.
//	(ie. protected memory, PIC using retw from flash ... could crash some CPU's)
    if(bytes)                                     // Valid counts are now: 3,2,1 bytes
    {
        uint32_t last_word = 0;                   // Last word to send
        uint16_t byte_ind = word_ind << 2;        // Convert to Byte index

// Working MSB to LSB allows assigment to LSB without shifting
// Compiler can optimize powers of 8 into byte swaps, etc... (on some CPU's)
        while(bytes--)                            // index is now 2,1,0 matching required storage offset
        {
            last_word <<= 8;
// 2,1,0
            last_word |= ((uint32_t) 0xffU & write_data[byte_ind + bytes ]);
        }
// Send last partial word
        ((uint32_t *)SPI_W0(HSPI)) [word_ind] = last_word;
    }
}


/// @brief HSPI FIFO Read in units of 32 bits (4 byte words)
/// @param[in] read_data: read buffer
/// @param[in] bytes: bytes to write
/// @return  void
void hspi_readFIFO(uint8_t *read_data, uint16_t bytes)
{

    uint8_t word_ind = 0;

    if(bytes > HSPI_FIFO_SIZE)                    // TODO set error status
        return;

// Update FIFO with number of bits to read ?
    hspi_setBits(bytes);

// First do a fast read 32 bit chunks at a time
    while(bytes >= 4)
    {
// Cast both source and destination to 4 byte word pointers
        ((uint32_t *)read_data) [word_ind] = \
            ((uint32_t *)SPI_W0(HSPI)) [word_ind];
        bytes -= 4;
        word_ind++;
    }

// Next read remaining bytes (if any) from FIFO as a single word
// Note: We follow the good practice of avoiding access past the end of an array.
//	(ie. protected memory, PIC using retw from flash ... could crash some CPU's)
    if(bytes)                                     // Valid counts are now: 3,2,1 bytes
    {
        uint32_t last_word = ((uint32_t *)SPI_W0(HSPI)) [word_ind];
        uint16_t byte_ind = word_ind << 2;        // Convert to Byte index

        while(bytes--)                            // index is now 2,1,0 matching required storage offset
        {
// 2,1,0 order
            read_data[byte_ind++] = (uint8_t) 0xff & last_word;
            last_word >>= 8;
        }
// Send last partial word
    }
}


/// @brief Used only for small (less then HSPI_FIFO_SIZE) write / read
/// Write and Read uses same buffer and same byte count
/// @param[in,out] data: buffer
/// @param[in] bytes: bytes to write and or read
/// @return  void
void hspi_TxRx(uint8_t *data, uint16_t bytes)
{
    if(bytes > HSPI_FIFO_SIZE)
        return;                                   // Error
    hspi_config(CONFIG_FOR_RX_TX);
    hspi_writeFIFO(data, bytes);
    hspi_startSend();
    hspi_waitReady();
// read result
    hspi_readFIFO(data, bytes);
}


/// @brief Used only for small (less then HSPI_FIFO_SIZE) write
/// Used for small (less then HSPI_FIFO_SIZE) packet send only
/// @param[in] data: buffer
/// @param[in] bytes: bytes to write
/// @return  void
void hspi_Tx(uint8_t *data, uint16_t bytes)
{
    if(bytes > HSPI_FIFO_SIZE)
        return;                                   // Error
    hspi_config(CONFIG_FOR_TX);                   // Does hspi_waitReady(); first
    hspi_writeFIFO(data, bytes);
    hspi_startSend();
    hspi_waitReady();
}


/// =================================================================
/// @brief
/// SPI buffered write functions

/// @brief HSPI stream byte to buffer
/// We use the fifo - or a buffer to queue spi writes
/// The overhead of N writes done at once is less N writes done one at a time
/// @param[in] data: byte to add to buffer
/// @return  void
void hspi_TX_stream_byte(uint8_t data)
{
// Queue sent data
    _f_tx_buf[_f_tx_ind] = data;
// Send a full FIFO block - or - remaining data
    if(++_f_tx_ind >= HSPI_FIFO_SIZE)
    {
        hspi_TX_stream_flush();
    }
}


/// @brief HSPI stream init
/// We use the fifo - or a buffer to queue spi writes
/// The overhead of N writes done at once is less N writes done one at a time
void hspi_TX_stream_init(void)
{
    _f_tx_ind = 0;
    hspi_waitReady();
}


/// @brief HSPI stream flush
/// We use the fifo - or a buffer to queue spi writes
/// The overhead of N writes done at once is less N writes done one at a time
/// @return  void
void hspi_TX_stream_flush( void )
{
// Send a full FIFO block - or - remaining data
    if(_f_tx_ind )
    {
        hspi_config(CONFIG_FOR_TX);               // Does hspi_waitReady(); first
        hspi_writeFIFO(_f_tx_buf, _f_tx_ind);
        hspi_startSend();
        _f_tx_ind = 0;
    }
    hspi_waitReady();
}
