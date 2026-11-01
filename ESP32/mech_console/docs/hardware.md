# Hardware

## Current setup

| Item | Recorded information |
| --- | --- |
| Board | ESP32-C3-LCDkit; PCB revision still to be recorded |
| Chip | ESP32-C3, silicon revision v0.4 |
| Flash | 4 MiB detected; application configured for 4 MiB |
| SDK | ESP-IDF v6.1; record the exact SDK commit/toolchain baseline |
| USB console/flash | Native USB Serial/JTAG, currently COM8 |
| Data adapter | CH340, currently COM10; exact variant and driver version not recorded |
| LCD | Not initialized; confirm fitted daughterboard/controller |
| IMU | Available; model, interface and supply requirements not recorded |
| Motor | Available; motor type, driver, ratings and feedback not recorded |
| IR jumper | Position not recorded |

COM port numbers are host assignments and may change.

## UART wiring and ownership

| Connection | Board pin |
| --- | --- |
| CH340 TX to UART1 RX | GPIO20 |
| CH340 RX to UART1 TX | GPIO21 |
| CH340 GND | GND |
| Native USB D- / D+ | GPIO18 / GPIO19 |

Use 3.3 V UART logic; the adapter's power-output selector alone does not prove
its TX logic level. Do not connect 5 V UART signals to the board GPIOs.

UART1 is configured for 115200 baud, 8N1, without RTS/CTS.
RX and TX driver rings are each 2048 bytes. USB carries logs; UART1 carries packets.

## Before adding peripherals

Record the board revision, screen variant and IR jumper position. Build a complete
GPIO/controller ownership table before enabling shared resources. GPIO20/21 are
already assigned to the UART data link; alternative sensor profiles must resolve
that ownership explicitly.

Do not fix IMU or motor wiring until their actual modules are identified.
