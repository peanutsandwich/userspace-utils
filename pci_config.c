/**
 * A utility to read and write PCI config space from User space on Linux
 *
 * Copyright 2018 Mark Walton
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
#include <sys/io.h>
#include <string.h>
#include <errno.h>

/** Offsets of the command line arguments */
#define BUS_INDEX       1
#define DEVICE_INDEX    2
#define FUNC_INDEX      3
#define REG_INDEX       4
#define WIDTH_INDEX     5
#define VAL_INDEX       6

/** The IO space register to write to for PCI operation control */
#define PCI_OPERATION_REG   0xcf8
/** The IO space register to read/write for the data for a PCI operation */
#define PCI_DATA_REG        0xcfc

/** Macro to turn a bus, device and function into a value to write in the pci
 *  operation register for the BDF */
#define MAKE_BDF(__bus, __device, __function)       \
    (((__bus) << 16) | ((__device) << 11) | ((__function) << 8))

void print_usage(void)
{
    printf("PCI Config Space read/write utility\n");
    printf("Usage:\n");
    printf("    ./pci_config <b> <d> <f> <reg> <width> [val]\n");
    printf("\n");
    printf("Where:\n");
    printf("    b   - The PCI bus the device is on\n");
    printf("    d   - The PCI device number\n");
    printf("    f   - The PCI function to read from\n");
    printf("    reg - The offset to read/write (0-0xff)\n");
    printf("    width - The width of the access (8, 16 or 32)\n");
    printf("    val - The value to write (if writing, or empty if reading)\n");
}

/**
 * @brief Helper function to retrieve an integer value from the command line
 *
 * @param arg - The command line argument to convert
 * @param val - Pointer to store the converted value in
 * @param max - Maximum value for the integer (or 0 if no max)
 * @param field_name - A name to use when printing errors about the field
 *
 * @return 0 - Failed to convert the integer value
 * @return 1 - Successfully converted the value
 */
int get_int(char* arg, unsigned long* val, unsigned long max, char* field_name)
{
    char* end = NULL;
    unsigned long parsed_val = 0;

    parsed_val = strtoul(arg, &end, 0);
    if(arg == end)
    {
        printf("Invalid value provided for %s\n", field_name);
        return 0;
    }
    else if((max > 0) && (parsed_val > max))
    {
        printf("%s must be in the range of 0-0x%lx",
               field_name,
               max);
        return 0;
    }

    *val = parsed_val;

    return 1;
}

unsigned long read_config_8(unsigned long bdf, unsigned long offset)
{
    outl(0x80000000 | bdf | offset, PCI_OPERATION_REG);
    return inb(PCI_DATA_REG + (offset & 3));
}

unsigned long read_config_16(unsigned long bdf, unsigned long offset)
{
    outl(0x80000000 | bdf | offset, PCI_OPERATION_REG);
    return inw(PCI_DATA_REG + (offset&3));
}

unsigned long read_config_32(unsigned long bdf, unsigned long offset)
{
    outl(0x80000000 | bdf | offset, PCI_OPERATION_REG);
    return inl(PCI_DATA_REG);
}

void write_config_8(unsigned long bdf, unsigned long offset, unsigned long val)
{
    outl(0x80000000 | bdf | offset, PCI_OPERATION_REG);
    outb(val, PCI_DATA_REG + (offset & 3));
}

void write_config_16(unsigned long bdf, unsigned long offset, unsigned long val)
{

    outl(0x80000000 | bdf | offset, PCI_OPERATION_REG);
    outw(val, PCI_DATA_REG + (offset & 2));
}

void write_config_32(unsigned long bdf, unsigned long offset, unsigned long val)
{
    outl(0x80000000 | bdf | offset, PCI_OPERATION_REG);
    outl(val, PCI_DATA_REG);
}

int main(int argc, char* argv[])
{
    unsigned long bus = 0;
    unsigned long dev = 0;
    unsigned long func = 0;
    unsigned long width = 0;
    unsigned long reg = 0;
    unsigned long val = 0;
    unsigned long bdf = 0;
    unsigned long read_val = 0;
    int write = 0;
    int ret = 0;

    /* Process the command line parameters */
    if(argc > WIDTH_INDEX)
    {
        if(!get_int(argv[BUS_INDEX], &bus, 0xff, "Bus"))
        {
            print_usage();
            return 1;
        }

        if(!get_int(argv[DEVICE_INDEX], &dev, 0x1f, "Device"))
        {
            print_usage();
            return 1;
        }

        if(!get_int(argv[FUNC_INDEX], &func, 7, "Function"))
        {
            print_usage();
            return 1;
        }

        if(!get_int(argv[REG_INDEX], &reg, 0xff, "Register"))
        {
            print_usage();
            return 1;
        }

        if(!get_int(argv[WIDTH_INDEX], &width, 32, "Width"))
        {
            print_usage();
            return 1;
        }

        if(width != 8 && width != 16 && width != 32)
        {
            print_usage();
            return 1;
        }

        if(argc > VAL_INDEX)
        {
            if(!get_int(argv[VAL_INDEX], &val, 0xffffffff, "Value"))
            {
                print_usage();
                return 1;
            }
            write = 1;
        }
    }
    else
    {
        print_usage();
        return 1;
    }

    /* Request privileges. ioperm only grants us access from 0-0x3ff, but this
     * tool needs access to the full range of io registers, so use iopl */
    ret = iopl(3);
    if(ret < 0)
    {
        printf("Failed to request io privileges. Errno: %d (%s)\n"
               "Try running as root, or with \"sudo\"\n",
               errno, strerror(errno));
        return -1;
    }

    /* Convert the bus device and function to a bdf value suitable for use with
     * the read and write functions */
    bdf = MAKE_BDF(bus, dev, func);

    /* If we're writing, write the value to the register */
    if(write)
    {
        switch(width)
        {
            case 8:
                write_config_8(bdf, reg, val);
                break;
            case 16:
                write_config_16(bdf, reg, val);
                break;
            case 32:
                write_config_32(bdf, reg, val);
                break;
        }
    }

    /* Read the register value */
    switch(width)
    {
        case 8:
            read_val = read_config_8(bdf, reg);
            break;
        case 16:
            read_val = read_config_16(bdf, reg);
            break;
        case 32:
            read_val = read_config_32(bdf, reg);
            break;
    }
    
    /* Display the register value */
    printf("Config Register 0x%02lx for %02lx:%02lx:%ld -> ", reg, bus, dev, func);
    switch(width)
    {
        case 8:
            printf("0x%02lx", read_val);
            break;
        case 16:
            printf("0x%04lx", read_val);
            break;
        case 32:
            printf("0x%08lx", read_val);
            break;
    }
    printf("\n");

    return 0;
}

