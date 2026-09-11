# Pico WIZnet W5500-EVB-Pico2

Small Pico SDK C++ wrapper/example for WIZnet W5500-EVB-Pico using FreeRTOS,
WIZnet ioLibrary_Driver, the WIZnet Pico SPI port, DHCP/DNS, and Mbed TLS.

## Cloning the project

```bash
git clone https://github.com/eremiticengineer/pico-wiznet-w5500

cd pico-wiznet-w5500

git submodule update --init lib/FreeRTOS-Kernel lib/ioLibrary_Driver lib/WIZnet-PICO-FREERTOS-C

cp examples/config/secrets.example.hpp examples/config/secrets.hpp
```

## Hardware

W5500-EVB-Pico2 uses the board-wired hardware SPI interface:

- GPIO16 MISO
- GPIO17 CSn
- GPIO18 SCK
- GPIO19 MOSI
- GPIO20 RESET
- GPIO21 INT

Those pins are unavailable to other peripherals while Ethernet is active.

## Secrets

Edit `examples/config/secrets.hpp` and replace the placeholder certificate in `examples/config/WebServerCertificate.hpp`.

## Build

```bash
./build_project
```

## Flash

```bash
./deploy_usb
```

## Notes

A conceptual overview of the data journey from Pico, through the W5500
to the server and back is provided in [PROTOCOL.md](PROTOCOL.md)

## Creating from new

```bash
git submodule add https://github.com/raspberrypi/FreeRTOS-Kernel.git lib/FreeRTOS-Kernel

git submodule add https://github.com/Wiznet/ioLibrary_Driver.git lib/ioLibrary_Driver

git submodule add https://github.com/WIZnet-ioNIC/WIZnet-PICO-FREERTOS-C.git lib/WIZnet-PICO-FREERTOS-C
```

## Acknowledgments

chatgpt was consulted with extensively and was used to create the code rather than use
an AI Agent such as codex. Using chatgpt allows understanding the codebase and the decisions
made on the architecture which lets the developer guide the implementation rather than
issuing commands and flashing the compiled binary without fully understanding the architecture
and decisions that led to it.

[PROTOCOL.md](PROTOCOL.md) was created after lengthy discussions and learning using chatgpt.

## References

- [W5500-EVB-Pico](https://docs.wiznet.io/Product/Chip/Ethernet/W5500/w5500-evb-pico)
- [W5500 datasheet](https://docs.wiznet.io/Product/Chip/Ethernet/W5500)
- [Getting Started with Ethernet Examples](https://github.com/Wiznet/RP2040-HAT-C/blob/main/getting_started.md)
- [mbedTLS](https://sourcevu.sysprogs.com/stm32/Libraries/mbedTLS/)
- [Embedded C Coding Standard](https://barrgroup.com/embedded-c-coding-standard)
