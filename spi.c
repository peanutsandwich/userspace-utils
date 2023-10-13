/**
 * A utility to read and write I2C from User space on Linux
 *
 * Copyright 2019 Mark Walton
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPI_BUFFER_SIZE             256

static void print_usage(
    void);

int main(int argc, char* argv[])
{
    const char* device = NULL;
    uint32_t bytes = 0;
    uint8_t writeBuffer[SPI_BUFFER_SIZE] = {0};
    uint8_t readBuffer[SPI_BUFFER_SIZE] = {0};
    int ret = 0;
    int fd = 0;
    uint8_t mode = SPI_MODE_3;

    if(argc < 3) {
        printf("Not enough arguments\n");
        print_usage();
        return 1;
    }

    device = argv[1];
    bytes = (uint32_t)argc - 2;

    for(int i = 2; i < argc; ++i) {
        writeBuffer[i - 2] = (unsigned char)strtoul(argv[i], NULL, 0);
    }

    fd = open(device, O_RDWR);
    if(fd < 0) {
        printf("Unable to open spidev, fd: %d (errno: %d)\n", fd, errno);
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if(ret == -1) {
        printf("Unable to set SPI mode, fd: %d (errno: %d)\n", fd, errno);
        return 1;
    }

    struct spi_ioc_transfer transferData = {
        .tx_buf = (long long unsigned int)writeBuffer,
        .rx_buf = (long long unsigned int)readBuffer,
        .len = bytes,
        .speed_hz = 1000000,
        .delay_usecs = 1,
        .bits_per_word = 8,
        .cs_change = 1,
        .tx_nbits = 0,
        .rx_nbits = 0
    };

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &transferData);
    if(ret < 1) {
        printf("Unable to transfer SPI data. Ret: %d, errno: %d\n", ret, errno);
        return 1;
    }

    printf("Sent:\n");
    for(uint32_t b = 0; b < bytes; ++b) {
        printf("%02x ", writeBuffer[b]);
    }

    printf("\n\n");

    printf("Received:\n");
    for(uint32_t b = 0; b < bytes; ++b) {
        printf("%02x ", readBuffer[b]);
    }
    printf("\n");

    close(fd);

    return 0;
}

static void print_usage(
    void)
{
    printf("I2C read/write utility\n");
    printf("Usage:\n");
    printf("    ./i2c <op> <bus> <addr> [args...]\n");
    printf("\n");
    printf("Where:\n");
    printf("    op      - The Operation to perform. One of:\n");
    printf("                * r     - Plain read from the device\n");
    printf("                    Arguments: <count>\n");
    printf("                        - count     - The number of bytes to read\n");
    printf("                * w     - Plain write to the device\n");
    printf("                    Arguments: <bytes...>\n");
    printf("                        - bytes - The bytes to write\n");
    printf("                * r8    - Read from an 8 bit offset\n");
    printf("                    Arguments: <offset> <count>\n");
    printf("                        - offset    - the offset to read from\n");
    printf("                        - count     - The number of bytes to read\n");
    printf("                * w8    - Write to an 8 bit offset\n");
    printf("                    Arguments: <offset> <bytes...>\n");
    printf("                        - offset - the offset to write to\n");
    printf("                        - bytes - The bytes to write\n");
    printf("                * r16   - Read from a 16 bit offset\n");
    printf("                    Arguments: <offset> <count>\n");
    printf("                        - offset - the offset to read from\n");
    printf("                        - count     - The number of bytes to read\n");
    printf("                * w16   - Write to a 16 bit offset\n");
    printf("                    Arguments: <offset> <bytes...>\n");
    printf("                        - offset - the offset to write to\n");
    printf("                        - bytes - The bytes to write\n");
    printf("    bus     - The I2C bus to perform the operation on\n");
    printf("    addr    - The I2C address of the device to access (7-bit)\n");
    printf("    val...  - Optional arguments for the operation (see above)\n");
}
