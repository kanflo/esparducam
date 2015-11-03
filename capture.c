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
#include "arducam.h"
#include <espressif/spi_flash.h>

#include <espressif/sdk_private.h>
#include <sys/types.h>
#include <unistd.h>

#include <ssid_config.h>

#include <http_upload.h>


//#define CONFIG_NO_WIFI
//#define CONFIG_NO_ARDUCAM
#define CONFIG_NO_PIR
#define CONFIG_NO_CLI

#define ARDUCAM_PWR  (4) // Arducam Mini power enable
#define FLASH_CS     (5) // SPI flash CS
#define ARDUCAM_CS  (15) // Arducam Mini SPI CS


#define OV2640_CHIPID_HIGH  0x0A
#define OV2640_CHIPID_LOW   0x0B

#define delay_ms(t) vTaskDelay((t) / portTICK_RATE_MS)
#define systime_ms() (xTaskGetTickCount()*portTICK_RATE_MS)
#define TIME_MARKER "[%8u] "

#define BUF_SIZE (200)
static char buffer[BUF_SIZE];


#ifndef CONFIG_NO_CLI

void cli_task(void *p)
{
    printf(TIME_MARKER "CLI task\n", systime_ms());

    static char cmd[81];
    int i = 0;
    while(1) {
        char ch = get_char();
        printf("%c", ch);
        if (ch != '\n') {
            if (i < sizeof(cmd)) cmd[i++] = ch;
        } else {
            cmd[i] = 0;
            printf("'%s'\n", cmd);
//            handle_command((char*) cmd);
            i = 0;
        }
    }
}
#endif // CONFIG_NO_CLI

static bool arducam_setup(void)
{
    uint8_t vid, pid, temp;

    arducam(smOV2640);
    printf("portTICK_RATE_MS is at %u\n", portTICK_RATE_MS);
    // Check if the ArduCAM SPI bus is OK
    arducam_write_reg(ARDUCHIP_TEST1, 0x55);
    temp = arducam_read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55) {
        printf(TIME_MARKER "SPI interface error (got 0x%02x)!\n", systime_ms(), temp);
        return false;
    }

    // Change MCU mode
//    arducam_write_reg(ARDUCHIP_MODE, 0x00);

    // Check if the camera module type is OV2640
#if 1
    arducam_i2c_read(OV2640_CHIPID_HIGH, &vid);
    arducam_i2c_read(OV2640_CHIPID_LOW, &pid);
    if((vid != 0x26) || (pid != 0x42)) {
        printf(TIME_MARKER "Error: cannot find OV2640 module (got 0x%02x, 0x%02x)\n", systime_ms(), vid, pid);
        return false;
    } else {
        printf(TIME_MARKER "OV2640 detected\n", systime_ms());
    }
#endif
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

//    arducam_flush_fifo(); // TODO: These are the same
    arducam_clear_fifo_flag(); // TODO: These are the same
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
        printf(TIME_MARKER "Capture done after %ums\n", systime_ms(), systime_ms()-start_time);
    }
    return true;
}

static void arudcam_fifo_read(int client_sock)
{
    uint8_t temp, temp_last = 0, buf_idx = 0;
    uint32_t fifo_size, bytes_read, start_time = systime_ms();

    fifo_size = (arducam_read_reg(0x44) << 16) | (arducam_read_reg(0x43) << 8) | (arducam_read_reg(0x42));
    printf(TIME_MARKER "%u bytes in fifo, according to camera\n", systime_ms(), fifo_size); 

    bytes_read = 1;
    temp = arducam_read_fifo();
//    printf("%02x ", temp);
    buffer[buf_idx++] = temp;
    // Read JPEG data from FIFO
    while((temp != 0xd9) | (temp_last != 0xff)) {
        temp_last = temp;
        temp = arducam_read_fifo();
        buffer[buf_idx++] = temp;
        if (client_sock && buf_idx == BUF_SIZE) {
            int res = write(client_sock, buffer, buf_idx);
            if (res < 0) {
                printf("\nERROR: write returned %d\n", res);
                break;
            }
            buf_idx = 0;
        }
        bytes_read++;
        if (bytes_read > fifo_size) {
            printf("[%8u] Fifo error\n", systime_ms());
            break;
        }
    }
    if (client_sock && buf_idx > 0) {
        int res =  write(client_sock, buffer, buf_idx);
        (void) res;
    }
    printf("[%8u] Done, read %u bytes in %ums\n", systime_ms(), bytes_read, systime_ms()-start_time);
}

void cam_task(void *p)
{
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

#ifndef CONFIG_NO_PIR
extern uint16_t sdk_system_adc_read(void);

void pir_task(void *p)
{
    delay_ms(1000);
    printf(TIME_MARKER "PIR task\n", systime_ms());

    uint16_t threshold = 500;
//    uint8_t pir_pin = 16;
    bool old_state = false;
//    gpio_enable(pir_pin, GPIO_OUTPUT);
//    old_state = gpio_read(pir_pin);
//    __pinMode(pir_pin, INPUT);

    while(1) {
        uint32_t adc = sdk_system_adc_read();
        bool new_state = adc > threshold;
        if (new_state != old_state) {
            printf(TIME_MARKER "%s [%u]\n", systime_ms(), new_state ? "Motion" : "No motion", adc); 
            old_state = new_state;
        }
        delay_ms(10);

#if 0
//        gpio_write(pir_pin, old_state);
        __digitalWrite(pir_pin, old_state);
        printf(TIME_MARKER "%s\n", systime_ms(), old_state ? "On" : "Off"); 
        old_state = !old_state;
        delay_ms(5000);
#endif
    }
}
#endif // CONFIG_NO_PIR


#ifndef CONFIG_NO_ARDUCAM
void server_task(void *p)
{
    bool camera_ok = false;
    camera_ok = arducam_setup();
    if (!camera_ok) {
        printf(TIME_MARKER "Camera init failed!\n", systime_ms()); 
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
                            printf("[%8u] ---\n", systime_ms());
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
#endif // CONFIG_NO_ARDUCAM

void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);
    delay_ms(1000);
    printf("\n\n\n");
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

#ifndef CONFIG_NO_WIFI
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
#endif // CONFIG_NO_WIFI

    // Enable power to the camera
    gpio_enable(ARDUCAM_PWR, GPIO_OUTPUT);
    gpio_write(ARDUCAM_PWR, 1);

//    xTaskCreate(&cam_task, (signed char *) "cam_task", 256, NULL, 2, NULL);
#ifndef CONFIG_NO_ARDUCAM
    xTaskCreate(&server_task, (signed char *) "server_task", 256, NULL, 2, NULL);
#endif // CONFIG_NO_ARDUCAM

#ifndef CONFIG_NO_PIR
    xTaskCreate(&pir_task, (signed char *) "pir_task", 256, NULL, 2, NULL);
#endif // CONFIG_NO_PIR

#ifndef CONFIG_NO_CLI
    xTaskCreate(&cli_task, (signed char *) "cli_task", 256, NULL, 2, NULL);
#endif // CONFIG_NO_CLI

}
