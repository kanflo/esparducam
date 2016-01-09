PROGRAM = capture
PROGRAM_SRC_DIR = . arducam
PROGRAM_INC_DIR = . arducam
EXTRA_COMPONENTS=extras/stdin_uart_interrupt extras/i2c extras/spi extras/spiflash extras/past extras/http-upload

include $(EOR_ROOT)/common.mk
