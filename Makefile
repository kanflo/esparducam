PROGRAM = capture
PROGRAM_SRC_DIR = . arducam
PROGRAM_INC_DIR = . arducam
OTA=1
EXTRA_COMPONENTS=extras/stdin_uart_interrupt extras/i2c extras/spi extras/spiflash extras/http-upload extras/rboot-ota
include $(EOR_ROOT)/common.mk
