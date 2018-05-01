/**
 * A utility to read and write IO space from User space on Linux
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

#define REG_INDEX       1
#define VAL_INDEX       2

void print_usage(void)
{
    printf("IO read/write utility\n");
    printf("Usage:\n");
    printf("    ./io <reg> [val]\n");
    printf("\n");
    printf("Where:\n");
    printf("    reg - The IO register to read/write (0-0xffff)\n");
    printf("    val - The value to write (if writing, or empty if reading)\n");
}

int main(int argc, char* argv[])
{
    unsigned long reg = 0;
    unsigned long val = 0;
    int write = 0;
    int ret = 0;
    char* end = NULL;

    if(argc > REG_INDEX)
    {
        reg = strtoul(argv[REG_INDEX], &end, 0);
        if(argv[REG_INDEX] == end)
        {
            printf("Invaid register\n");
            print_usage();
            return -1;
        }
        else if(reg > 0xffff)
        {
            printf("Register must be in the range of 0-0xffff\n");
            return -1;
        }
        
        if(argc > VAL_INDEX)
        {
            val = strtoul(argv[VAL_INDEX], &end, 0);
            if(argv[VAL_INDEX] == end)
            {
                printf("Invalid value\n");
                print_usage();
                return -1;
            }
            else if(val > 0xff)
            {
                printf("Value must be in the range of 0-0xff\n");
                return -1;
            }
            write = 1;
        }
    }
    else
    {
        print_usage();
        return -1;
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

    /* If we're writing, write the value to the register */
    if(write)
    {
        outb(val, reg);
    }

    /* Read and display the register value */
    printf("Reg 0x%04lx: 0x%02x\n", reg, inb(reg));

    return 0;
}

