# userspace-utils
Set of utilities for interacting with x86 controllers from userspace

## PCI Config
A tool to read/write PCI config space

~~~~
PCI Config Space read/write utility
Usage:
    ./pci_config <b> <d> <f> <reg> <width> [val]

Where:
    b   - The PCI bus the device is on
    d   - The PCI device number
    f   - The PCI function to read from
    reg - The offset to read/write (0-0xff)
    width - The width of the access (8, 16 or 32)
    val - The value to write (if writing, or empty if reading)
~~~~

Note, this is not intended to provide access to BARs or anything like that. See https://github.com/billfarrow/pcimem for a tool that will accomplish this.

## IO
A tool to read/write IO space

~~~~
IO read/write utility
Usage:
    ./io <reg> [val]

Where:
    reg - The IO register to read/write (0-0xffff)
    val - The value to write (if writing, or empty if reading)
~~~~

