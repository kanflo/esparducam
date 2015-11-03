PROGRAM = capture
PROGRAM_SRC_DIR = . arducam past http-upload
PROGRAM_INC_DIR = . arducam past http-upload
EXTRA_COMPONENTS=extras/stdin_uart_interrupt extras/i2c extras/spi extras/spiflash
#EXTRA_COMPONENTS=extras/serial-driver extras/past extras/http-upload

include /Users/jhn/github/esp8266/kanflo-open-rtos/common.mk
