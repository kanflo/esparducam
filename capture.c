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
#include "uhej.h"
#include "camdriver.h"
#include "cli.h"
#include "timeutils.h"
#include "ota-tftp.h"
#include "rboot-api.h"
#include "config.h"

#include <ssid_config.h>

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
    gpio_enable(ARDUCAM_nPWR, GPIO_OUTPUT);
    printf("Camera power cycle\n");
    gpio_write(ARDUCAM_nPWR, 1);
    delay_ms(250);
    gpio_write(ARDUCAM_nPWR, 0);
    delay_ms(250);

#ifdef CONFIG_TARGET_ESPARDUCAM_MINI
    gpio_enable(LED_nPWR, GPIO_OUTPUT);
    gpio_write(LED_nPWR, 1);
#endif // CONFIG_TARGET_ESPARDUCAM_MINI

    camera_ok = arducam_setup();
    if (!camera_ok) {
        printf("Camera init failed!\n"); 
        while (1) {
#ifdef CONFIG_TARGET_ESPARDUCAM_MINI
            gpio_write(LED_nPWR, 1);
            delay_ms(1000);
            gpio_write(LED_nPWR, 0);
#endif // CONFIG_TARGET_ESPARDUCAM_MINI
            delay_ms(1000);
        }
    }

    if (!(uhej_server_init() &&
        uhej_announce_udp("tftp", 69) &&
        uhej_announce_udp("esparducam", 80)))
    {
       printf("uHej registration failed\n");
    }

    struct netconn *client = NULL;
    struct netconn *nc = netconn_new(NETCONN_TCP);
    if (nc == NULL) {
        printf("Failed to allocate socket\n");
        vTaskDelete(NULL);
    }
    netconn_bind(nc, IP_ADDR_ANY, 80);
    netconn_listen(nc);
    while (1) {
        err_t err = netconn_accept(nc, &client);
        if (err == ERR_OK) {
            struct netbuf *nb;

            ip_addr_t client_addr;
            uint16_t port_ignore;
            netconn_peer(client, &client_addr, &port_ignore);
            printf("---\n");
            printf("Client from %d.%d.%d.%d\n", ip4_addr1(&client_addr), ip4_addr2(&client_addr), ip4_addr3(&client_addr), ip4_addr4(&client_addr));

            if ((err = netconn_recv(client, &nb)) == ERR_OK) {
                char *data;
                uint16_t len;
                err = netbuf_data(nb, (void*) &data, &len);
                if (err == ERR_OK) {
                    printf("Received %d bytes\n", len);
                } else {
                    printf("netbuf_data returned error %d\n", err);
                }

                sprintf(buffer, "HTTP/1.1 200 OK\nContent-Type: image/jpeg; charset=utf-8\nConnection: close\n\n");

#ifdef CONFIG_TARGET_ESPARDUCAM_MINI
                gpio_write(LED_nPWR, 0);
#endif // CONFIG_TARGET_ESPARDUCAM_MINI

                netconn_write(client, buffer, strlen(buffer), NETCONN_COPY);

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
                        arudcam_fifo_to_netcon(client);
                    }
                }
#ifdef CONFIG_TARGET_ESPARDUCAM_MINI
                gpio_write(LED_nPWR, 1);
#endif // CONFIG_TARGET_ESPARDUCAM_MINI
            }
            netbuf_delete(nb);
        }
        printf("Closing connection\n");
        netconn_close(client);
        netconn_delete(client);
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

    xTaskCreate(&server_task, "server_task", 512, NULL, 2, NULL);
#ifndef CONFIG_NO_PIR
    xTaskCreate(&pir_task, "pir_task", 256, NULL, 2, NULL);
#endif // CONFIG_NO_PIR
}
