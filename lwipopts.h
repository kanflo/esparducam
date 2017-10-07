// Temporary hack until https://github.com/SuperHouse/esp-open-rtos/issues/445
// gets sorted.

#include <../lwip/include/lwipopts.h>
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
