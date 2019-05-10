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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define OP_INDEX        1
#define BUS_INDEX       2
#define ADDR_INDEX      3
#define ARGS_START      4
#define OP_RD           0
#define OP_WR           1

static void print_usage(
    void);

static int do_smbus_transfer(
    int             bus,
    unsigned long   addr,
    int             op,
    unsigned long   offset_len,
    unsigned char*  wr_data,
    unsigned long   wr_count,
    unsigned char*  rd_data,
    unsigned long   rd_count);

int main(int argc, char* argv[])
{
    const char* op = NULL;
    unsigned long bus_no = 0;
    unsigned long addr = 0;
    unsigned long rd_count = 0;
    unsigned long wr_count = 0;
    unsigned long offset = 0;
    unsigned char* rd_data = NULL;
    unsigned char* wr_data = NULL;
    unsigned long funcs;
    unsigned long offset_len = 0;
    int operation = OP_RD;
    int bus = 0;
    char bus_dev[32] = {0};
    char* end = NULL;
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data ioctl_data = {0};
    int msg_idx = 0;
    int ret = 0;
    int i = 0;
    int data_idx = 0;

    if(argc < ARGS_START) {
        printf("Not enough arguments\n");
        print_usage();
        return 1;
    }

    op = argv[OP_INDEX];
    bus_no = strtoul(argv[BUS_INDEX], &end, 0);
    addr = strtoul(argv[ADDR_INDEX], &end, 0);

    if(strcmp(op, "r") == 0) {
        /* Plain read */
        if(argc < (ARGS_START + 1)) {
            printf("Please provide a number of bytes to read\n");
            return 1;
        }

        rd_count = strtoul(argv[ARGS_START], &end, 0);
        if(!rd_count) {
            printf("Please provide a non-zero number of bytes to read\n");
            return 1;
        }

        rd_data = calloc(1, rd_count);
    } else if(strcmp(op, "r8") == 0) {
        offset_len = 1;

        if(argc < (ARGS_START + 2)) {
            printf("Please provide an offset and a number of bytes to read\n");
            return 1;
        }

        /* Read 8 bit offset */
        wr_count = 1;
        wr_data = calloc(1, 1);
        offset = strtoul(argv[ARGS_START], &end, 0);
        wr_data[0] = (unsigned char)offset;
        rd_count = strtoul(argv[ARGS_START + 1], &end, 0);
        if(!rd_count) {
            printf("Please provide a non-zero number of bytes to read\n");
            return 1;
        }
        rd_data = calloc(1, rd_count);
    } else if(strcmp(op, "r16") == 0) {
        offset_len = 2;

        /* Read 16 bit offset */
        if(argc < (ARGS_START + 2)) {
            printf("Please provide an offset and a number of bytes to read\n");
            return 1;
        }

        /* Read 8 bit offset */
        wr_count = 2;
        wr_data = calloc(1, 2);
        offset = strtoul(argv[ARGS_START], &end, 0);
        wr_data[0] = (unsigned char)offset >> 8;
        wr_data[1] = (unsigned char)offset & 0xff;
        rd_count = strtoul(argv[ARGS_START + 1], &end, 0);
        if(!rd_count) {
            printf("Please provide a non-zero number of bytes to read\n");
            return 1;
        }
        rd_data = calloc(1, rd_count);
    } else if(strcmp(op, "w") == 0) {
        operation = OP_WR;

        /* Plain write */
        if(argc < (ARGS_START + 1)) {
            printf("Please provide some data to write\n");
            return 1;
        }

        wr_count = argc - ARGS_START;
        wr_data = calloc(1, wr_count);

        for(i = ARGS_START; i < argc; ++i) {
            wr_data[i - ARGS_START] = (unsigned char)strtoul(argv[i], &end, 0);
        }
    } else if(strcmp(op, "w8") == 0) {
        operation = OP_WR;
        offset_len = 1;

        /* Write 8 bit offset */
        if(argc < (ARGS_START + 2)) {
            printf("Please provide an offset and some data to write\n");
            return 1;
        }

        wr_count = argc - (ARGS_START + 1);

        /* Add bytes for the offset */
        wr_count += 1;
        wr_data = calloc(1, wr_count);

        offset = strtoul(argv[ARGS_START], &end, 0);
        wr_data[0] = (unsigned char)offset;
        data_idx = 1;

        for(i = ARGS_START+1; i < argc; ++i) {
            wr_data[data_idx] = (unsigned char)strtoul(argv[i], &end, 0);
            ++data_idx;
        }
    } else if(strcmp(op, "w16") == 0) {
        operation = OP_WR;
        offset_len = 2;

        /* Write 16 bit offset */
        if(argc < (ARGS_START + 2)) {
            printf("Please provide an offset and some data to write\n");
            return 1;
        }

        wr_count = argc - (ARGS_START + 1);

        /* Add bytes for the offset */
        wr_count += 2;
        wr_data = calloc(1, wr_count);

        offset = strtoul(argv[ARGS_START], &end, 0);
        wr_data[0] = (unsigned char)offset >> 8;
        wr_data[1] = (unsigned char)offset & 0xff;
        data_idx = 2;

        for(i = ARGS_START+1; i < argc; ++i) {
            wr_data[data_idx] = (unsigned char)strtoul(argv[i], &end, 0);
            ++data_idx;
        }
    }

    snprintf(bus_dev, 32, "/dev/i2c-%d", bus_no);

    bus = open(bus_dev, O_RDWR);
    if(bus < 0) {
        printf("Unable to open bus %d (errno: %d)\n", bus_no, errno);
        return 1;
    }

    if (ioctl(bus, I2C_FUNCS, &funcs) < 0) {
        printf("Unable to retrieve I2C function support flags\n");
        return 1;
    }

    if(!(funcs & I2C_FUNC_I2C)) {
        /* Device doesn't support normal transfers, try and perform the
         * operation using smbus transfers instead. Note: this is more dangerous
         * as there will be a stop between the write and read of offset based
         * reads, so if we are on a multi master bus this could cause problems.
         * 
         * Also note that this limits the size of an individual transfer due to
         * the max block length of smbus */
        ret = do_smbus_transfer(bus,
                                addr,
                                operation,
                                offset_len,
                                wr_data, wr_count,
                                rd_data, rd_count);
        if(ret < 0) {
            printf("Error performing smbus emulated transfer\n");
            return 1;
        }
    } else {
        /* If we have a write - add that first */
        if(wr_count > 0 && wr_data) {
            msgs[msg_idx].addr = addr;
            msgs[msg_idx].len = wr_count;
            msgs[msg_idx].buf = wr_data;
            msgs[msg_idx].flags = 0;
            ++msg_idx;
        }

        /* Add a read if we have one */
        if(rd_count > 0 && rd_data) {
            msgs[msg_idx].addr = addr;
            msgs[msg_idx].len = rd_count;
            msgs[msg_idx].buf = rd_data;
            msgs[msg_idx].flags = I2C_M_RD;
            ++msg_idx;
        }

        ioctl_data.msgs = msgs;
        ioctl_data.nmsgs = msg_idx;

        ret = ioctl(bus, I2C_RDWR, &ioctl_data);
        if(ret < 0) {
            printf("Error performing I2C operation (errno: %d)\n", errno);
            return 1;
        }
    }

    if(wr_count) {
        printf("Written %d bytes\n", wr_count);
    }

    if(rd_count && rd_data) {
        printf("Read %d byte\n", rd_count);
        for(i = 0; i < rd_count; ++i) {
            printf("%02x ", rd_data[i]);
        }
        printf("\n");
    }

    if(wr_data) {
        free(wr_data);
    }

    if(rd_data) {
        free(rd_data);
    }
    
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

static int do_smbus_transfer(
    int             bus,
    unsigned long   addr,
    int             op,
    unsigned long   offset_len,
    unsigned char*  wr_data,
    unsigned long   wr_count,
    unsigned char*  rd_data,
    unsigned long   rd_count)
{
    int ret = 0;
    unsigned long offset = 0;

    ret = ioctl(bus, I2C_SLAVE_FORCE, addr);
    if(ret < 0) {
        printf("Unable to set slave address\n");
        return -1;
    }

    /* Process the command based on the operation and offset length */
    if(op == OP_RD) {
        if(offset_len == 0) {
            /* 0 byte offset - we can just read byte-by-byte */
            while(offset < rd_count) {
                struct i2c_smbus_ioctl_data smb;

                smb.read_write = I2C_SMBUS_READ;
                smb.command = 0;
                smb.size = I2C_SMBUS_BYTE;
                smb.data = &rd_data[offset];

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus byte read\n");
                    return -1;
                }

                ++offset;
            }
        } else if(offset_len == 1) {
            /* 1 byte offset - we use read byte commands, which send the offset
             * and then read a byte, so we increment the offset for each byte
             * we read */
            unsigned char dev_offset = wr_data[0];

            while(offset < rd_count) {
                struct i2c_smbus_ioctl_data smb;

                smb.read_write = I2C_SMBUS_READ;
                smb.command = dev_offset;
                smb.size = I2C_SMBUS_BYTE_DATA;
                smb.data = &rd_data[offset];

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus byte read\n");
                    return -1;
                }

                ++offset;
                ++dev_offset;
            }
        } else if(offset_len == 2) {
            /* 2 byte offset - we use write byte followed by read commands. This
             * sends the offset, a STOP, and then reads a signle data byte. We
             * need to update and resend the offset for each subsequent byte */
            unsigned short dev_offset = wr_data[0] << 8 | wr_data[1];

            while(offset < rd_count) {
                struct i2c_smbus_ioctl_data smb;
                unsigned char lsb = dev_offset & 0xff;

                smb.read_write = I2C_SMBUS_WRITE;
                smb.command = dev_offset >> 8;
                smb.size = I2C_SMBUS_BYTE_DATA;
                smb.data = &lsb;

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus byte read\n");
                    return -1;
                }

                smb.read_write = I2C_SMBUS_READ;
                smb.command = 0;
                smb.size = I2C_SMBUS_BYTE;
                smb.data = &rd_data[offset];

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus byte read\n");
                    return -1;
                }

                ++offset;
                ++dev_offset;
            }
        } else {
            printf("Unsupported I2C operation for smbus emulation\n");
            return -1;
        }
    } else {
        if(offset_len == 0) {
            /* No offset - we can just write data byte-by-byte */
            while(offset < wr_count) {
                struct i2c_smbus_ioctl_data smb;

                smb.read_write = I2C_SMBUS_READ;
                smb.command = 0;
                smb.size = I2C_SMBUS_BYTE;
                smb.data = &rd_data[offset];

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus byte read\n");
                    return -1;
                }

                ++offset;

                /* Wait between bytes - we could be talking to an eeprom and we
                 * get a NACK if it is busy committing data to the device */
                usleep(6000);
            }
        } else if(offset_len == 1) {
            /* 1 byte offset - we use write byte commands, which send the offset
             * and then a byte of data, so we increment the offset for each byte
             * we write */
            unsigned char dev_offset = wr_data[0];

            while(offset < (wr_count - offset_len)) {
                struct i2c_smbus_ioctl_data smb;

                smb.read_write = I2C_SMBUS_WRITE;
                smb.command = dev_offset;
                smb.size = I2C_SMBUS_BYTE_DATA;
                smb.data = &wr_data[offset_len + offset];

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus byte read\n");
                    return -1;
                }

                ++offset;
                ++dev_offset;

                /* Wait between bytes - we could be talking to an eeprom and we
                 * get a NACK if it is busy committing data to the device */
                usleep(6000);
            }
        } else if(offset_len == 2) {
            /* 2 byte offset - we use write word commands, which send the msb of
             * the offset and then 2 bytes of data which is made up of the lsb
             * of the offset, followed by the byte we're writing. We increment
             * the offset for each byte we write */
            unsigned short dev_offset = wr_data[0] << 8 | wr_data[1];

            while(offset < (wr_count - offset_len)) {
                struct i2c_smbus_ioctl_data smb;
                unsigned short data = (wr_data[offset_len + offset] << 8) | dev_offset & 0xff;

                smb.read_write = I2C_SMBUS_WRITE;
                smb.command = dev_offset >> 8;
                smb.size = I2C_SMBUS_WORD_DATA;
                smb.data = &data;

                ret = ioctl(bus, I2C_SMBUS, &smb);
                if(ret < 0) {
                    printf("Failed to perform smbus word write (errno: %d)\n", errno);
                    return -1;
                }

                ++offset;
                ++dev_offset;

                /* Wait between bytes - we could be talking to an eeprom and we
                 * get a NACK if it is busy committing data to the device */
                usleep(6000);
            }
        } else {
            printf("Unsupported I2C operation for smbus emulation\n");
            return -1;
        }
    }

    return 0;
}
