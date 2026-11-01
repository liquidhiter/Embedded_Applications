# Testing

Run commands from `ESP32/mech_console/` in an ESP-IDF v6.1 terminal.
Examples use PowerShell, COM8 for the board USB console/flash, and COM10 for
CH340 data. Port numbers may differ; see [hardware notes](docs/hardware.md).
Close serial monitors before running hardware tests.

## Test layout

| Location | Purpose |
| --- | --- |
| `components/*/test/` | Component Unity cases |
| `tests/frame_codec_app/` | Component-test firmware and pytest selectors, on board or QEMU |
| `tests/uart_echo_app/` | Dedicated firmware and physical UART echo tests |
| `tests/uart_link_app/` | Dedicated firmware, physical packet tests and optional stress tests |
| `tests/protocol_e2e/` | CH340 tests against the normal application firmware |
| `tests/conftest.py` | Hardware/stress opt-in options |
| `tools/run_qemu_tests.py` | QEMU regression entry point used by CI |

Dedicated test firmware replaces the application. Restore it with
`idf.py -p COM8 flash` after testing. Production end-to-end tests leave the
normal application installed. QEMU does not validate physical wiring or timing.
Generated builds, virtual environments and reports are ignored by Git.
Pytest also excludes generated build directories and `managed_components/`, so
third-party example tests are not included in the project regression.

## Install test dependencies

```powershell
uv sync --locked --group test
uv run --group test pytest --collect-only -q
```

## Run tests in QEMU

Install Espressif's emulator, then reopen an ESP-IDF terminal so its executable
is on PATH. QEMU runs the ESP32-C3 firmware without using COM8 or COM10.

```powershell
python "$env:IDF_PATH/tools/idf_tools.py" install qemu-riscv32
```

From this project directory in the new ESP-IDF terminal, run the same regression
entry point used by CI. It builds firmware, regenerates images, and runs all
pytest files in every QEMU-capable test app:

```powershell
python tools/run_qemu_tests.py
```

A test app opts in by providing `CMakeLists.txt` and `sdkconfig.defaults.qemu`
under `tests/`. The current test firmware includes every application component
with a `test/CMakeLists.txt`. Each new Unity case also needs a pytest item so its
result remains individually selectable. No workflow change is needed when adding
test cases or QEMU-capable test apps. Hardware-only tests require a physical-board
runner and are not executed by QEMU. Reports are saved under `test-results/`.

## Run tests on the board

Build after changing C code. Close the COM8 monitor before running pytest.
The pytest command flashes test firmware, replacing the current application.

```powershell
idf.py -C tests/frame_codec_app -DIDF_TARGET=esp32c3 build
uv run --group test pytest tests/frame_codec_app --embedded-services esp,idf --target esp32c3 --port COM8 --app-path tests/frame_codec_app --build-dir build -v
```

Append `-k decode` to select decoder cases, or `-k decode-zero-data` for one case.
Append `-k app_protocol` to run the eight command-dispatch cases. These execute on
the target using deterministic status values and intercepted response submission.
Append `-k app_command` for the six worker-queue cases. These use real FreeRTOS
queues/tasks with gated handlers and injected startup allocation failures.
Append `--junitxml=test-results/codec.xml` to save a report.

Append `-k app_rgb` for RGB lifecycle/configuration and error-injection cases.
These wrap the LED driver and run on both the board and QEMU. The additional
`rgb-hardware` case uses the real RMT driver, sends dim red/green/blue, then turns
the LED off; pytest skips it under QEMU. It needs the LCDkit but no CH340.
Driver success verifies transmission, not emitted color; confirm colors visually.
Each Unity case starts after a fresh boot, since RGB has no deinitialization API.

Append `-k app_session` for the nine HELLO/ACK cases: initialization, invalid
arguments/headers, negotiation limits, ACK byte layout, duplicate/conflicting
requests, state rejection, ID-generation failure and independent instances.
They use deterministic ID callbacks on target/QEMU. The session helper is not
yet connected to production UART routing; these are component tests, not a
completed on-wire handshake test.

Restore the application after testing:

```powershell
idf.py -p COM8 flash
```

## Run the UART echo test

Use CH340 TX to GPIO20, RX to GPIO21, and common ground with 3.3 V signal levels.
Close applications using COM8 or COM10. Build the dedicated echo firmware, then
let pytest flash it through COM8 and validate data through COM10:

```powershell
idf.py -C tests/uart_echo_app -DIDF_TARGET=esp32c3 build
uv sync --locked --group test
uv run --group test pytest tests/uart_echo_app --embedded-services esp,idf --target esp32c3 --port COM8 --uart-data-port COM10 --app-path tests/uart_echo_app --build-dir build -v
```

Six independent cases cover 1, 255, 256, 257, 1024, and 4096 bytes, each repeated
ten times. The echo task exists only in this test app. Its `UART_ECHO_READY` log
signals readiness before data transfer. Tests require `--uart-data-port` to opt
in; no port is opened by hardware tests when this option is omitted. This app
has no QEMU configuration and is excluded from the cloud QEMU regression.
Testing changes the firmware on the board; restore the application afterward:

```powershell
idf.py -p COM8 flash
```

The standalone echo tool remains available after flashing the echo test app:

```powershell
uv run python tools/uart_echo_test.py --port COM10
```

## Run the physical UART link test

Use the same CH340 wiring and close applications using COM8 or COM10. This app
uses the real UART driver and app_link receive task, without the QEMU UART read
wrapper. Python builds frames independently using struct, zlib, and COBS. The
test-only callback returns valid packets through the production app_link_send API.

```powershell
idf.py -C tests/uart_link_app -DIDF_TARGET=esp32c3 build
uv sync --locked --group test
uv run --group test pytest tests/uart_link_app --embedded-services esp,idf --target esp32c3 --port COM8 --uart-data-port COM10 --app-path tests/uart_link_app --build-dir build -v
```

Cases cover empty, binary and maximum payloads, fragmented transmission, multiple
frames, invalid CRC, malformed COBS and recovery after oversized input. Host write
boundaries do not guarantee the same boundaries in UART reads. This app requires
`--uart-data-port` and has no QEMU configuration. Restore the main firmware afterward:

```powershell
idf.py -p COM8 flash
```

## Run sustained UART stress tests

After building the UART link test app, enable the two optional stress profiles:

```powershell
uv run --group test pytest tests/uart_link_app -m stress --uart-stress --uart-stress-frames 1000 --embedded-services esp,idf --target esp32c3 --port COM8 --uart-data-port COM10 --app-path tests/uart_link_app --build-dir build -v -s
```

Python writes and reads concurrently. The full-duplex profile sends maximum
payloads continuously; the recovery profile interleaves valid frames, bad CRCs
and oversized frames. Exact reply comparison checks corruption, missing,
duplicate and reordered packets. Metrics are host-observed payload and wire
bytes per second, excluding flashing and startup. At 115200 baud with 8N1, each
direction has a theoretical wire ceiling of 11520 bytes/s. These tests do not
measure concurrent ESP32 send callers or prove indefinite operation. Restore
the main firmware with `idf.py -p COM8 flash` afterward.

## Run production GET_STATUS end-to-end tests

Build the normal application, then run the CH340 tests against its build directory:

```powershell
idf.py -DIDF_TARGET=esp32c3 build
uv run --group test pytest tests/protocol_e2e --embedded-services esp,idf --target esp32c3 --port COM8 --uart-data-port COM10 --app-path . --build-dir build -v
```

These seventeen cases validate real status values, session/request-ID echoing,
uptime progress, command error responses and recovery after malformed input. No MCU-side
wrappers are used. The tests require the same CH340 wiring as the link tests and
leave the normal production firmware installed. They do not run in QEMU CI.

## Format source files

The C/H layout follows ESP-IDF conventions using clang-format 19.1.7: four-space
indentation, a 120-column limit, and function braces on a separate line. Includes
are grouped as standard C/POSIX headers, common ESP-IDF headers, other dependencies,
and project headers (`app_*` and `frame_*`). ESP-IDF itself uses Astyle; this project
expresses the documented layout in `.clang-format`.

Ruff formats Python and sorts its imports. Both tools are locked in the uv `dev`
group. Generated files and virtual environments are excluded through Git's file list.

```powershell
uv sync --locked --group dev
uv run --group dev python tools/format_sources.py
uv run --group dev python tools/format_sources.py --check
```

The project CI runs the same formatting check before QEMU regression tests.

## CI

The [project workflow](../../.github/workflows/mech-console.yml) runs formatting
checks and QEMU regression. It does not run the physical UART tests.
Health lifecycle tests verify stop-request acceptance, not completion of
idle-task memory cleanup.
