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

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <arducam.h>
#include <http_upload.h>
#include "cli.h"
#include "camdriver.h"


#define MAX_ARGC (10)

static bool motion_enabled = false;
static char upload_ip[16];

bool cli_motion_enabled(void)
{
    return motion_enabled;
}

char* cli_motion_upload_ip(void)
{
    return (char*) upload_ip;
}

static void cmd_motion(uint32_t argc, char *argv[])
{
    if (argc >= 2) {
        motion_enabled = strcmp(argv[1], "on") == 0;
        if (motion_enabled) {
            if (argc == 3)
                strcpy(upload_ip, argv[2]);
            else
                printf("Error: missing upload ip number.\n");
        }
    } else {
        printf("Error: wrong number of parameters.\n");
    }

}

static void cmd_capture(uint32_t argc, char *argv[])
{
    if (arducam_capture()) {
        arudcam_fifo_to_devnull();
    } else {
        printf("Capture failed\n");
    }
}

static void cmd_set_size(uint32_t argc, char *argv[])
{
    if (argc == 2) {
        jpeg_size_t size;
        if (strcmp(argv[1], "160x120") == 0) size = sz160x120;
        else if (strcmp(argv[1], "320x240") == 0) size = sz320x240;
        else if (strcmp(argv[1], "640x480") == 0) size = sz640x480;
        else if (strcmp(argv[1], "800x600") == 0) size = sz800x600;
        else if (strcmp(argv[1], "1024x768") == 0) size = sz1024x768;
        else if (strcmp(argv[1], "1280x1024") == 0) size = sz1280x1024;
        else if (strcmp(argv[1], "1600x1200") == 0) size = sz1600x1200;
        else {
            printf("Error: unknown JPEG size, valid sizes are:\n");
            printf("  160x120\n");
            printf("  320x240\n");
            printf("  640x480\n");
            printf("  800x600\n");
            printf("  1024x768\n");
            printf("  1280x1024\n");
            printf("  1600x1200\n");
            return;
        }
        arducam_set_jpeg_size(size);
        // Allow for auto exposure loop to adapt to after resolution change
        printf("ok\n");

    } else {
        printf("Error: missing image size.\n");
    }
}

static void cmd_capture_upload(uint32_t argc, char *argv[])
{
    if (argc == 2) {
        if (arducam_capture()) {
            arudcam_upload_fifo(argv[1], 8000);
            printf("ok\n");
        } else {
            printf("Capture failed\n");
        }
    } else {
        printf("Error: missing host ip.\n");
    }
}

static void cmd_on(uint32_t argc, char *argv[])
{
    if (argc >= 2) {
        for(int i=1; i<argc; i++) {
            uint8_t gpio_num = atoi(argv[i]);
            gpio_enable(gpio_num, GPIO_OUTPUT);
            gpio_write(gpio_num, true);
            printf("On %d\n", gpio_num);
        }
    } else {
        printf("Error: missing gpio number.\n");
    }
}

static void cmd_off(uint32_t argc, char *argv[])
{
    if (argc >= 2) {
        for(int i=1; i<argc; i++) {
            uint8_t gpio_num = atoi(argv[i]);
            gpio_enable(gpio_num, GPIO_OUTPUT);
            gpio_write(gpio_num, false);
            printf("Off %d\n", gpio_num);
        }
    } else {
        printf("Error: missing gpio number.\n");
    }
}

static void cmd_help(uint32_t argc, char *argv[])
{
    printf("motion:<on|off>[:<ip number>]         Enable/disable image upload on motion detection\n");
    printf("size:<size>                           Set JPEG size (see below)\n");
    printf("upload:<hostname>                     Capture and upload image to host\n");
    printf("capture                               Capture image to '/dev/null'. Used for FPS benchmarking\n");
    printf("on:<gpio number>[:<gpio number>]+     Set gpio to 1\n");
    printf("off:<gpio number>[:<gpio number>]+    Set gpio to 0\n");
    printf("\nExample:\n");

    printf("  motion:on:172.16.3.107<enter> when motion is detected, captures and uploads image to host running server.py\n");
    printf("  motion:off<enter> disable motion detection\n");
    printf("  size:1600x1200<enter> set JPEG size to 1600x1200 pixels\n");
    printf("  upload:172.16.3.107<enter> captures and uploads image to host running server.py\n");
    printf("  on:0<enter> switches on gpio 0\n");
    printf("  on:0:2:4<enter> switches on gpios 0, 2 and 4\n");

    printf("\n");
    printf("Valid JPEG sizes are:\n");
    printf("   160x120\n");
    printf("   320x240\n");
    printf("   640x480\n");
    printf("   800x600\n");
    printf("   1024x768\n");
    printf("   1280x1024\n");
    printf("   1600x1200\n");
}

static void handle_command(char *cmd)
{
    char *argv[MAX_ARGC];
    int argc = 1;
    char *temp, *rover;
    memset((void*) argv, 0, sizeof(argv));
    argv[0] = cmd;
    rover = cmd;
    // Split string "<command> <argument 1> <argument 2>  ...  <argument N>"
    // into argv, argc style
    while(argc < MAX_ARGC && (temp = strstr(rover, ":"))) {
        rover = &(temp[1]);
        argv[argc++] = rover;
        *temp = 0;
    }

    if (strlen(argv[0]) > 0) {
        if (strcmp(argv[0], "help") == 0) cmd_help(argc, argv);
        else if (strcmp(argv[0], "motion") == 0) cmd_motion(argc, argv);
        else if (strcmp(argv[0], "size") == 0) cmd_set_size(argc, argv);
        else if (strcmp(argv[0], "capture") == 0) cmd_capture(argc, argv);
        else if (strcmp(argv[0], "upload") == 0) cmd_capture_upload(argc, argv);
        else if (strcmp(argv[0], "on") == 0) cmd_on(argc, argv);
        else if (strcmp(argv[0], "off") == 0) cmd_off(argc, argv);
        else printf("Unknown command %s, try 'help'\n", argv[0]);
    }
}

static void cammon_task(void *p)
{
    char ch;
    char cmd[81];
    int i = 0;
    printf("\n\n\nWelcome to camera mon. Type 'help<enter>' for, well, help\n");
    printf("%% ");
    while(1) {
        if (read(0, (void*)&ch, 1)) { // 0 is stdin
            printf("%c", ch);
            if (ch == '\n' || ch == '\r') {
                cmd[i] = 0;
                i = 0;
                printf("\n");
                handle_command((char*) cmd);
                printf("%% ");
            } else {
                if (i < sizeof(cmd)) cmd[i++] = ch;
            }
        }
    }
}

void cli_init()
{
    xTaskCreate(&cammon_task, (signed char *) "cammon_task", 256, NULL, 2, NULL);
}

