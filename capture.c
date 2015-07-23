#include "espressif/esp_common.h"
#include "espressif/sdk_private.h"
#include <espressif/esp_misc.h> // sdk_os_delay_us
#include <lwip/api.h>
//#include <lwip/err.h>
#include <lwip/sockets.h>
//#include <lwip/dns.h>
//#include <lwip/netdb.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "hspi.h"
#include "i2c.h"
#include "arducam.h"

#if !defined(WIFI_SSID) || !defined(WIFI_PASS)
#error "Please define macros WIFI_SSID & WIFI_PASS (here, or better in a local.h file at root level or in program dir."
#endif

#define OV2640_CHIPID_HIGH  0x0A
#define OV2640_CHIPID_LOW   0x0B

#define delay_ms(t) vTaskDelay((t) / portTICK_RATE_MS)
#define systime_ms() (xTaskGetTickCount()*portTICK_RATE_MS)
#define TIME_MARKER "[%8lu] "

#define BUF_SIZE (200)
static char buffer[BUF_SIZE];


#if 0
static void arducam_test(void)
{
    uint8_t vid, pid;
    uint8_t temp;

    arducam(smOV2640);

    // Check if the ArduCAM SPI bus is OK
    arducam_write_reg(ARDUCHIP_TEST1, 0x55);
    temp = arducam_read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55) {
        printf("SPI interface error!\n");
    } else {
        printf("SPI interface to camera OK!\n");
    }

    // Change MCU mode
    arducam_write_reg(ARDUCHIP_MODE, 0x00);

    // Check if the camera module type is OV2640
    arducam_i2c_read(OV2640_CHIPID_HIGH, &vid);
    arducam_i2c_read(OV2640_CHIPID_LOW, &pid);
    if((vid != 0x26) || (pid != 0x42)) {
        printf("Error: cannot find OV2640 module\n");
    } else {
        printf("OV2640 detected\n");
    }
}
#endif

static bool arducam_setup(void)
{
    uint8_t vid, pid, temp;

    arducam(smOV2640);
    printf("portTICK_RATE_MS is at %lu\n", portTICK_RATE_MS);
    // Check if the ArduCAM SPI bus is OK
    arducam_write_reg(ARDUCHIP_TEST1, 0x55);
    temp = arducam_read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55) {
        printf(TIME_MARKER "SPI interface error!\n", systime_ms());
        return false;
    }

    // Change MCU mode
    arducam_write_reg(ARDUCHIP_MODE, 0x00);

    // Check if the camera module type is OV2640
    arducam_i2c_read(OV2640_CHIPID_HIGH, &vid);
    arducam_i2c_read(OV2640_CHIPID_LOW, &pid);
    if((vid != 0x26) || (pid != 0x42)) {
        printf(TIME_MARKER "Error: cannot find OV2640 module (got 0x%02x, 0x%02x)\n", systime_ms(), vid, pid);
        return false;
    } else {
        printf(TIME_MARKER "OV2640 detected\n", systime_ms());
    }

    printf(TIME_MARKER "Setting JPEG\n", systime_ms());
    arducam_set_format(fmtJPEG);
    printf(TIME_MARKER "Init\n", systime_ms());
    arducam_init(); // Note to self. Must call set_format before init.
    printf(TIME_MARKER "Setting size\n", systime_ms());
    arducam_set_jpeg_size(sz640x480);
    // Allow for auto exposure loop to adapt to after resolution change
    printf(TIME_MARKER "Autoexposure working...\n", systime_ms());
    delay_ms(1000);
    printf(TIME_MARKER "Done...\n", systime_ms());
    return true;
}

static bool arducam_capture(void)
{
    uint8_t temp;
    uint32_t start_time = systime_ms();
    arducam_flush_fifo();
    arducam_clear_fifo_flag();
    printf(TIME_MARKER "Start capture\n", systime_ms());
    arducam_start_capture();
    temp = arducam_read_reg(ARDUCHIP_TRIG);
    if (!temp) {
        printf(TIME_MARKER "Failed to start capture!\n", systime_ms());
        return false;
    } else {
        while (!(arducam_read_reg(ARDUCHIP_TRIG) & CAP_DONE_MASK)) {
            delay_ms(1);
        }
        printf(TIME_MARKER "Capture done after %lums\n", systime_ms(), systime_ms()-start_time);
    }
    return true;
}

static void arudcam_fifo_read(int client_sock)
{
    uint8_t temp, temp_last = 0, buf_idx = 0;
    uint32_t fifo_size, bytes_read, start_time = systime_ms();

    fifo_size = (arducam_read_reg(0x44) << 16) | (arducam_read_reg(0x43) << 8) | (arducam_read_reg(0x42));
    printf(TIME_MARKER "%lu bytes in fifo, according to camera\n", systime_ms(), fifo_size); 

    bytes_read = 1;
    temp = arducam_read_fifo();
    buffer[buf_idx++] = temp;
    // Read JPEG data from FIFO
    while((temp != 0xd9) | (temp_last != 0xff)) {
        temp_last = temp;
        temp = arducam_read_fifo();
        buffer[buf_idx++] = temp;
        if (client_sock && buf_idx == BUF_SIZE) {
            (void) write(client_sock, buffer, buf_idx);
            buf_idx = 0;
        }
        bytes_read++;
        if (bytes_read > fifo_size) {
            printf("[%8lu] Fifo error\n", systime_ms());
            break;
        }
    }
    if (client_sock && buf_idx > 0) {
        (void) write(client_sock, buffer, buf_idx);
    }
    printf("[%8lu] Done, read %lu bytes in %lums\n", systime_ms(), bytes_read, systime_ms()-start_time);
}


/*
static void hspi_test(void)
{
    uint8_t data[] = {0x81, 0x55};
    printf("hspi_init\n");
    hspi_init(32); // Slow
    printf("hspi_waitReady\n");
    hspi_waitReady();
    printf("hspi_Tx\n");
    hspi_Tx(data, sizeof(data));
    printf("done\n");
}

static void i2c_test(void)
{
    printf("i2c test\n");
    i2c_init();
    i2c_start();
    i2c_writeByte(0x30 << 1);
    if(!i2c_check_ack()) {
        printf("Write ack missing\n");
        i2c_stop();
    }
    i2c_stop();
}
*/

void cam_task(void *p)
{
//    hspi_test();
//    i2c_test();
//    arducam_test();

    do {
        if (!arducam_setup())
            break;
        if (!arducam_capture())
            break;
        arudcam_fifo_read(0);
    } while(0);

    while(1) {
        delay_ms(1000);
    }
}

void server_task(void *p)
{
    bool camera_ok = false;
    camera_ok = arducam_setup();
 
    while (1) {
        struct sockaddr_in server_addr, client_addr;
        int server_sock, client_sock;
        socklen_t sin_size;
        bzero(&server_addr, sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(80);

        int recbytes;

        do {
            if (-1 == (server_sock = socket(AF_INET, SOCK_STREAM, 0))) {
                printf(TIME_MARKER "Socket error\n", systime_ms());
                break;
            }

            if (-1 == bind(server_sock, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr))) {
                printf(TIME_MARKER "bind fail\n", systime_ms());
                break;
            }

            printf(TIME_MARKER " bind port: %d\n", systime_ms(), ntohs(server_addr.sin_port));

            if (-1 == listen(server_sock, 5)) {
                printf(TIME_MARKER "listen fail\n", systime_ms());
                break;
            }

            sin_size = sizeof(client_addr);

            for (;;) {
                if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
                    printf(TIME_MARKER "accept fail\n", systime_ms());
                    continue;
                }

                printf(TIME_MARKER "Client from %s:%d\n", systime_ms(), inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));

                int opt = 50;
                if (lwip_setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(int)) < 0) {
                    printf(TIME_MARKER "failed to set timeout on socket\n", systime_ms());
                }
                while ((recbytes = read(client_sock, buffer, BUF_SIZE)) > 0) {
                    buffer[recbytes] = 0;
                }

                sprintf(buffer, "HTTP/1.1 200 OK\nContent-Type: image/jpeg; charset=utf-8\nConnection: close\n\n");

                if (write(client_sock, buffer, strlen(buffer)) > 0) {
                    if (camera_ok) {
                        uint32_t retries = 4;
                        bool success;
                        while(retries--) {
                            success = arducam_capture();
                            if (success) {
                                break;
                            } else {
                                printf(TIME_MARKER "Capture retry\n", systime_ms()); 
                                delay_ms(10); // Arbitrary value
                            }
                        }
                        if (success) {
                            printf(TIME_MARKER "Reading fifo\n", systime_ms()); 
                            arudcam_fifo_read(client_sock);
                            printf("[%8lu] ---\n", systime_ms());
                        }
                    }
                }

                if (recbytes <= 0) {
                    close(client_sock);
                }
            }
        } while (0);
    }
}

void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

//    xTaskCreate(&cam_task, (signed char *) "cam_task", 256, NULL, 2, NULL);
    xTaskCreate(&server_task, (signed char *) "server_task", 256, NULL, 2, NULL);
}
