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

#include <arducam.h>
#include <unistd.h>
#include <string.h>
#include <http_upload.h>
#include "timeutils.h"
#include "camdriver.h"


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define BUF_SIZE (200)
static char buffer[BUF_SIZE];

bool arducam_setup(void)
{
    uint8_t vid, pid, temp;

    arducam(smOV2640);
    // Check if the ArduCAM SPI bus is OK
    arducam_write_reg(ARDUCHIP_TEST1, 0x55);
    temp = arducam_read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55) {
        printf("SPI interface error (got 0x%02x)!\n", temp);
        return false;
    }

    // Change MCU mode
//    arducam_write_reg(ARDUCHIP_MODE, 0x00);

    // Check if the camera module type is OV2640
    arducam_i2c_read(OV2640_CHIPID_HIGH, &vid);
    arducam_i2c_read(OV2640_CHIPID_LOW, &pid);
    if((vid != 0x26) || (pid != 0x42)) {
        printf("Error: cannot find OV2640 module (got 0x%02x, 0x%02x)\n", vid, pid);
        return false;
    } else {
        printf("OV2640 detected\n");
    }
    printf("Setting JPEG\n");
    arducam_set_format(fmtJPEG);
    printf("Init\n");
    arducam_init(); // Note to self. Must call set_format before init.
    printf("Setting size\n");
    arducam_set_jpeg_size(sz640x480);
    // Allow for auto exposure loop to adapt to after resolution change
    printf("Autoexposure working...\n");
    delay_ms(1000);
    printf("Done...\n");
    return true;
}

bool arducam_capture(void)
{
    uint8_t temp;
    uint32_t start_time = systime_ms();

//    arducam_flush_fifo(); // TODO: These are the same
    arducam_clear_fifo_flag(); // TODO: These are the same
    printf("Start capture\n");
    arducam_start_capture();
    temp = arducam_read_reg(ARDUCHIP_TRIG);
    if (!temp) {
        printf("Failed to start capture!\n");
        return false;
    } else {
        while (!(arducam_read_reg(ARDUCHIP_TRIG) & CAP_DONE_MASK)) {
            delay_ms(1);
        }
        printf("Capture done after %ums\n", systime_ms()-start_time);
    }
    return true;
}

void arudcam_fifo_to_socket(int client_sock)
{
    uint8_t temp, temp_last = 0, buf_idx = 0;
    uint32_t fifo_size, bytes_read, start_time = systime_ms();

    fifo_size = (arducam_read_reg(0x44) << 16) | (arducam_read_reg(0x43) << 8) | (arducam_read_reg(0x42));
    printf("%u bytes in fifo, according to camera\n", fifo_size); 

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
            printf("Fifo error\n");
            break;
        }
    }
    if (client_sock && buf_idx > 0) {
        int res =  write(client_sock, buffer, buf_idx);
        (void) res;
    }
    printf("Done, read %u bytes in %ums\n", bytes_read, systime_ms()-start_time);
}

void arudcam_fifo_to_devnull()
{
    uint8_t temp, temp_last = 0, buf_idx = 0;
    uint32_t fifo_size, bytes_read, start_time = systime_ms();

    fifo_size = (arducam_read_reg(0x44) << 16) | (arducam_read_reg(0x43) << 8) | (arducam_read_reg(0x42));
    printf("%u bytes in fifo, according to camera\n", fifo_size); 

    bytes_read = 1;
    temp = arducam_read_fifo();
//    printf("%02x ", temp);
    buffer[buf_idx++] = temp;
    // Read JPEG data from FIFO
    while((temp != 0xd9) | (temp_last != 0xff)) {
        temp_last = temp;
        temp = arducam_read_fifo();
        bytes_read++;
        if (bytes_read > fifo_size) {
            printf("Fifo error\n");
            break;
        }
    }
    printf("Done, read %u bytes in %ums\n", bytes_read, systime_ms()-start_time);
}

void arudcam_upload_fifo(char *host, uint16_t port)
{
    uint8_t temp, temp_last = 0, buf_idx = 0;
    uint32_t http_res, fifo_size, bytes_read, start_time = systime_ms();

    fifo_size = (arducam_read_reg(0x44) << 16) | (arducam_read_reg(0x43) << 8) | (arducam_read_reg(0x42));
    printf("%u bytes in fifo, according to camera\n", fifo_size); 

    do {
        printf("Connecting to server...\n");
        if (!upload_connect(host, port)) {
            printf("Failed to connect to %s:%d\n", host, port);
            break;
        }

        printf(" Connected\n");
        if (!upload_begin("/", "image.jpg", fifo_size)) {
            printf("Failed to upload\n");
            break;
        }
        printf("Uploading...\n");

        bytes_read = 1;
        temp = arducam_read_fifo();
        buffer[buf_idx++] = temp;
        // Read JPEG data from FIFO
        while((temp != 0xd9) | (temp_last != 0xff)) {
            temp_last = temp;
            temp = arducam_read_fifo();
            buffer[buf_idx++] = temp;
            if (buf_idx == BUF_SIZE) {
                if (!upload_data(buffer, buf_idx)) {
                    printf("Data upload failed\n");
                    break;
                }
                buf_idx = 0;
            }
            bytes_read++;
            if (bytes_read > fifo_size) {
                printf("Fifo error\n");
                break;
            }
        }
        if (buf_idx > 0) {
            (void) upload_data(buffer, buf_idx);
        }
        if (bytes_read < fifo_size) {
            printf("Padding upload data with %d bytes\n", fifo_size - bytes_read);
            memset(buffer, 0, BUF_SIZE);
            while (bytes_read < fifo_size) {
                uint32_t diff = fifo_size - bytes_read;
                (void) upload_data(buffer, MIN(diff, BUF_SIZE));
                bytes_read += MIN(diff, BUF_SIZE);
            }
        }
        printf("Finishing upload\n");
        http_res = upload_finish();
        printf("Closing\n");
        upload_close();
        printf("Done, read %u bytes in %ums, server responded with %d\n", bytes_read, systime_ms()-start_time, http_res);
    } while(0);
}
