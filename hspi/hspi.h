/*
 * hspi.h
 *
 *  Created on: 12 џэт. 2015 у.
 *      Author: Sem
 */

#ifndef INCLUDE_HSPI_H_
#define INCLUDE_HSPI_H_

//#include <ets_sys.h>
//#include <osapi.h>
//#include <os_type.h>
//#include <gpio.h>

#define SPI         0
#define HSPI        1
#define HSPI_FIFO_SIZE  64

#define CONFIG_FOR_TX           1
#define CONFIG_FOR_RX_TX        2

/* hspi.c */
void hspi_init ( uint16_t prescale );
void hspi_config ( int configState );
void hspi_setBits ( uint16_t bytes );
void hspi_startSend ( void );
void hspi_waitReady ( void );
void hspi_writeFIFO ( uint8_t *write_data , uint16_t bytes );
void hspi_readFIFO ( uint8_t *read_data , uint16_t bytes );
void hspi_TxRx ( uint8_t *data , uint16_t bytes );
void hspi_Tx ( uint8_t *data , uint16_t bytes );
void hspi_TX_stream_byte ( uint8_t data );
void hspi_TX_stream_init ( void );
void hspi_TX_stream_flush ( void );
#endif                                            /* INCLUDE_HSPI_H_ */
