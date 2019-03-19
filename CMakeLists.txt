project(userspace_utils)

add_executable(i2c i2c.c)
add_executable(io io.c)
add_executable(pci_config pci_config.c)

install(
    TARGETS i2c
    DESTINATION bin
)

install(
    TARGETS io
    DESTINATION bin
)

install(
    TARGETS pci_config
    DESTINATION bin
)

