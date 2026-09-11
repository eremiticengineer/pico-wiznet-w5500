cmake_minimum_required(VERSION 3.13)

set(LIBRARY pico_wiznet_w5500)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PICO_WIZNET_W5500_STANDALONE TRUE)
else()
    set(PICO_WIZNET_W5500_STANDALONE FALSE)
endif()

set(WIZNET_PORT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/lib/WIZnet-PICO-FREERTOS-C/port"
)

set(WIZNET_IOLIB_DIR
    "${CMAKE_CURRENT_LIST_DIR}/lib/ioLibrary_Driver"
)

if(PICO_WIZNET_W5500_STANDALONE)
    set(PROJECT pico_wiznet_w5500_example)

    set(PICO_BOARD pico CACHE STRING "Board type" FORCE)

    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

    set(FREERTOS_KERNEL_PATH
        "${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel"
    )

    set(FREERTOS_CONFIG_FILE_DIRECTORY
        "${CMAKE_CURRENT_LIST_DIR}/examples/include"
    )

    include(
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake
    )

    set(PICO_MBEDTLS_CONFIG_FILE
        "${CMAKE_CURRENT_LIST_DIR}/examples/include/mbedtls_config.h"
    )

    project(${PROJECT} C CXX ASM)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 23)

    pico_sdk_init()
endif()

if(NOT EXISTS "${WIZNET_IOLIB_DIR}/Ethernet/wizchip_conf.c")
    message(FATAL_ERROR "ioLibrary_Driver not found at ${WIZNET_IOLIB_DIR}")
endif()

if(NOT EXISTS "${WIZNET_PORT_DIR}/ioLibrary_Driver/src/wizchip_spi.c")
    message(FATAL_ERROR "WIZnet Pico SPI port not found at ${WIZNET_PORT_DIR}")
endif()

add_library(pico_wiznet_w5500_driver STATIC
    ${WIZNET_IOLIB_DIR}/Ethernet/socket.c
    ${WIZNET_IOLIB_DIR}/Ethernet/wizchip_conf.c
    ${WIZNET_IOLIB_DIR}/Ethernet/W5500/w5500.c
    ${WIZNET_IOLIB_DIR}/Internet/DHCP/dhcp.c
    ${WIZNET_IOLIB_DIR}/Internet/DNS/dns.c
    ${WIZNET_PORT_DIR}/ioLibrary_Driver/src/wizchip_spi.c
)

target_compile_definitions(pico_wiznet_w5500_driver PUBLIC
    _WIZCHIP_=W5500
    DEVICE_BOARD_NAME=W5500_EVB_PICO
    _WIZCHIP_SPI_SCLK_SPEED=40
)

target_include_directories(pico_wiznet_w5500_driver PUBLIC
    ${WIZNET_IOLIB_DIR}/Ethernet
    ${WIZNET_IOLIB_DIR}/Ethernet/W5500
    ${WIZNET_IOLIB_DIR}/Internet/DHCP
    ${WIZNET_IOLIB_DIR}/Internet/DNS
    ${WIZNET_PORT_DIR}
    ${WIZNET_PORT_DIR}/ioLibrary_Driver/inc
)

target_link_libraries(pico_wiznet_w5500_driver PUBLIC
    pico_stdlib
    hardware_clocks
    hardware_dma
    hardware_spi
    FreeRTOS-Kernel
)

add_library(${LIBRARY}
    src/WiznetW5500.cpp
)

target_include_directories(${LIBRARY} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

if(PICO_WIZNET_W5500_STANDALONE)
    target_include_directories(${LIBRARY} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/examples/include
        ${CMAKE_CURRENT_LIST_DIR}/examples/config
    )
endif()

target_link_libraries(${LIBRARY} PUBLIC
    pico_stdlib
    pico_mbedtls
    pico_wiznet_w5500_driver
    FreeRTOS-Kernel
)

if(PICO_WIZNET_W5500_STANDALONE)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/examples/config/secrets.hpp")
        message(FATAL_ERROR
            "Copy examples/config/secrets.example.hpp to examples/config/secrets.hpp and edit it."
        )
    endif()

    add_executable(${PROJECT}
        examples/main.cpp
    )

    target_include_directories(${PROJECT} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/examples/include
        ${CMAKE_CURRENT_LIST_DIR}/examples/config
    )

    target_link_libraries(${PROJECT} PRIVATE
        ${LIBRARY}
        pico_stdlib
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
    )

    pico_enable_stdio_usb(${PROJECT} 1)
    pico_enable_stdio_uart(${PROJECT} 1)
    pico_add_extra_outputs(${PROJECT})
endif()
