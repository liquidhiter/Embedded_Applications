# mech_console API

ESP32-C3-LCDkit monitoring and control application, currently implementing the
UART command pipeline. This reference covers project-owned public headers;
ESP-IDF, managed dependencies, test code and private implementations are excluded.

## Modules

| Module | Public interface |
| --- | --- |
| UART driver | app_uart.h |
| Packet transport | app_link.h |
| Command worker | app_command.h |
| Command dispatch | app_protocol.h |
| Session handshake | app_session.h, app_session_t |
| RGB indicator | app_rgb.h |
| Health monitoring | app_health.h |
| Frame encoding | frame_codec.h, frame_stream.h, frame_packet.h, frame_crc.h |
| Shared configuration | app_board_config.h, app_task_config.h, app_uart_config.h |
| Utilities | app_utils.h, app_sys_info.h |

## Design notes

- [UART protocol](protocol.md): packet layout and handshake contract.
- [Hardware](hardware.md): wiring and board configuration.
- [Roadmap](roadmap.md): implemented functionality and remaining work.
- [Build, test and maintenance commands](https://github.com/liquidhiter/Embedded_Applications/blob/main/ESP32/mech_console/TESTING.md).

HELLO/ACK and CONFIRM/READY helpers have component tests but are not yet connected
to production UART routing. Timeouts and credit remain pending. See the roadmap before
treating a declared API or planned protocol feature as available on the wire.
