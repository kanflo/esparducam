/* 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Johan Kanflo (github.com/kanflo)
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

#include "espressif/esp_common.h"
#include "espressif/sdk_private.h"
#include <espressif/esp_misc.h>
#include <lwip/api.h>
#include <lwip/sockets.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <spi.h>
#include <i2c/i2c.h>
#include <http_upload.h>
#include <arducam.h>
#include <espressif/spi_flash.h>
#include <espressif/sdk_private.h>
#include <sys/types.h>
#include <unistd.h>
#include "camdriver.h"
#include "cli.h"
#include "timeutils.h"
#include "ota-tftp.h"
#include "rboot-api.h"

#include <ssid_config.h>



//#define CONFIG_NO_WIFI
//#define CONFIG_NO_PIR

#define ARDUCAM_PWR  (15) // Arducam Mini power enable

#define BUF_SIZE (200)
static char buffer[BUF_SIZE];



#ifndef CONFIG_NO_PIR
extern uint16_t sdk_system_adc_read(void);

void pir_task(void *p)
{
    delay_ms(1000);
    printf("PIR task\n");

    uint16_t threshold = 500;
    bool old_state = false;

    while(1) {
        if (cli_motion_enabled()) {
            uint32_t adc = sdk_system_adc_read();
            bool new_state = adc > threshold;
            if (new_state != old_state) {
                if (new_state) {
                    printf("Motion detected!\n");
                    if (arducam_capture()) {
                        arudcam_upload_fifo(cli_motion_upload_ip(), 8000);
                    } else {
                        printf("Error: capture failed\n");
                    }
                }
                old_state = new_state;
            }
        } else {
            old_state = false;
        }
        delay_ms(10);
    }
}
#endif // CONFIG_NO_PIR

void server_task(void *p)
{
    bool camera_ok = false;

    // Power cycle the the camera
    gpio_enable(ARDUCAM_PWR, GPIO_OUTPUT);
    printf("Camera power cycle\n");
    gpio_write(ARDUCAM_PWR, 1);
    delay_ms(250);
    gpio_write(ARDUCAM_PWR, 0);
    delay_ms(250);

    camera_ok = arducam_setup();
    if (!camera_ok) {
        printf("Camera init failed!\n"); 
    }

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
                printf("Socket error\n");
                break;
            }

            if (-1 == bind(server_sock, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr))) {
                printf("bind fail\n");
                break;
            }

            printf(" bind port: %d\n", ntohs(server_addr.sin_port));

            if (-1 == listen(server_sock, 5)) {
                printf("listen fail\n");
                break;
            }

            sin_size = sizeof(client_addr);

            for (;;) {
                if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
                    printf("accept fail\n");
                    continue;
                }

                printf("Client from %s:%d\n", inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port));

                int opt = 50;
                if (lwip_setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &opt, sizeof(int)) < 0) {
                    printf("failed to set timeout on socket\n");
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
                                printf("Capture retry\n"); 
                                delay_ms(10); // Arbitrary value
                            }
                        }
                        if (success) {
                            printf("Reading fifo\n"); 
                            arudcam_fifo_to_socket(client_sock);
                            printf("---\n");
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
    rboot_config conf = rboot_get_config();
    printf("\n\n\n");
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    printf("Currently running on flash slot %d / %d.\r\n\r\n", conf.current_rom, conf.count);
    printf("Image addresses in flash:\r\n");
    for(int i = 0; i <conf.count; i++) {
        printf("%c%d: offset 0x%08x\r\n", i == conf.current_rom ? '*':' ', i, conf.roms[i]);
    }

    cli_init();
    ota_tftp_init_server(TFTP_PORT);

#ifndef CONFIG_NO_WIFI
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
#endif // CONFIG_NO_WIFI

    xTaskCreate(&server_task, (signed char *) "server_task", 256, NULL, 2, NULL);
#ifndef CONFIG_NO_PIR
    xTaskCreate(&pir_task, (signed char *) "pir_task", 256, NULL, 2, NULL);
#endif // CONFIG_NO_PIR
}
