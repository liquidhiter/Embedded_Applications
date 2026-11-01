# Roadmap

Updated: 2026-11-01. Baseline: design specification v0.1, 2026-10-15.
The full specification remains outside this repository.

| Stage | Status | Remaining work |
| --- | --- | --- |
| P0 Observable startup | Partial | Hardware inventory, board resource profiles, self-test, 20-reset check, dependency baseline and OTA partition/rollback preparation |
| P1 UART pipeline | In progress | Handshake, session recovery, fixed packet pools, credit, TX arbitration, END reconciliation and 921600-baud validation |
| P2 Data and rules | Not started | Sample units/timestamps, snapshots, statistics, missing-data handling and alarms |
| P3 Panel and feedback | RGB bring-up only | LCD, encoder, backlight, RGB state patterns, audio and separate IR RX/TX profiles |
| P4 Wireless | Not started | MQTT, BLE provisioning/diagnostics and coexistence measurements |
| P5 Persistence and OTA | Not started | Maintenance barrier, transactional configuration and tested rollback |
| P6 Real IMU | Not started | Identify hardware, integrate acquisition and validate units/timing |
| P7 Extensions | Not started | Select and validate 2 Mbps, UHCI, ADC, FFT or motor-node experiments separately |

## Working today

- USB console, chip/version information and periodic heap/uptime logging.
- UART1 packet receive/send, COBS, streaming framing, CRC32 and packet validation.
- Four-entry command queue and worker, with GET_STATUS end-to-end.
- RGB initialization, synchronous color setting and dim-green startup indication;
  shared board constants use the `APP_BOARD_RGB_*` prefix.
- Component tests on QEMU and the board; physical UART tests and CI.

Latest local regression (2026-11-01): 66 component cases passed on QEMU, with
the physical RGB case skipped; 67 passed on the board, including real RMT
transmission; and 17 production-firmware command/status cases passed.
RGB color order still needs visual confirmation; driver success does not measure light output.
The earlier 115200-baud stress results used dedicated link-test firmware;
they do not establish mixed-load or 921600-baud performance.

## Next

1. Add handshake, session validation/recovery and error counters.
2. Add packet pools, credit and a dedicated TX scheduler.
3. Add END reconciliation and run the planned throughput matrix.

Independent peripheral checks may run alongside P1, but do not complete it.

## Decisions and deviations

- Use ESP-IDF v6.1 instead of the original v5.5.1 reference; check APIs/resources when adding drivers.
- Run C tests on the target and QEMU, without a separate host C test project.
- COMMAND/RESPONSE envelopes now match the original message table. Session and deduplication behavior remains pending; see [protocol.md](protocol.md).
- Current RX ring is 2 KiB, rather than the proposed 8 KiB. Size it from measured service gaps when increasing baud/load.
- Update this file when a milestone or acceptance gap changes. Keep build/test commands in the project README.
