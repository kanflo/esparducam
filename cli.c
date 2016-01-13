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
#include "camdriver.h"


#define MAX_ARGC (10)

static void cmd_capture(uint32_t argc, char *argv[])
{
    if (arducam_capture()) {
        arudcam_fifo_to_devnull();
    } else {
        printf("Capture failed\n");
    }
}

static void cmd_capture_upload(uint32_t argc, char *argv[])
{
    if (argc == 2) {
        if (arducam_capture()) {
            arudcam_upload_fifo(argv[1], 8000);
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
    printf("upload:<hostname>                     Capture and upload image to host\n");
    printf("capture                               Capture image to '/dev/null'\n");
    printf("on:<gpio number>[:<gpio number>]+     Set gpio to 1\n");
    printf("off:<gpio number>[:<gpio number>]+    Set gpio to 0\n");
    printf("\nExample:\n");
    printf("  upload:172.16.3.107<enter> captures and uploads image to host running server.py\n");
    printf("  on:0<enter> switches on gpio 0\n");
    printf("  on:0:2:4<enter> switches on gpios 0, 2 and 4\n");
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

