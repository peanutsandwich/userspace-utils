cmake_minimum_required(VERSION 3.5)
project(userspace_utils)

option(I2C      "Tools for interacting with I2C devices from userspace"     ON)
option(IO       "Tools for interacting with x86 IO space"                   ON)
option(PCIE     "Tools for interacting with PCIe devices from userspace"    ON)

if(I2C)
    add_executable(i2c i2c.c)
    install(
        TARGETS i2c
        DESTINATION bin)
endif()

if(IO)
    add_executable(io io.c)
    install(
        TARGETS io
        DESTINATION bin)
endif()

if(PCIE)
    add_executable(pci_config pci_config.c)
    install(
        TARGETS pci_config
        DESTINATION bin)
endif()
