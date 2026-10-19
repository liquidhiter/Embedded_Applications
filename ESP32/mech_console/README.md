# mech_console

[![CI regression](https://github.com/liquidhiter/Embedded_Applications/actions/workflows/mech-console.yml/badge.svg?branch=main)](https://github.com/liquidhiter/Embedded_Applications/actions/workflows/mech-console.yml)

## Application structure

```text
mech_console/
|-- components/
|   |-- app_health/                 Health task and lifecycle Unity test
|   |-- app_uart/                   UART driver setup and shared configuration
|   `-- frame_codec/
|       |-- frame_codec.c          COBS encoder and decoder
|       |-- include/               Public codec interface
|       `-- test/                  COBS, stream-framer, and CRC32 Unity cases
|-- main/main.c                    Application entry point
|-- tests/frame_codec_app/
|   |-- CMakeLists.txt             Isolated test firmware project
|   |-- main/test_main.c           Unity test menu
|   |-- pytest_frame_codec.py      One pytest item per Unity case
|   |-- pytest_frame_stream.py     Stream-framer pytest items
|   |-- pytest_frame_crc.py        CRC32 pytest items
|   |-- pytest_app_health.py       Health-task lifecycle pytest item
|   |-- sdkconfig.defaults         Physical-board USB console configuration
|   `-- sdkconfig.defaults.qemu    QEMU UART console configuration
|-- tests/uart_echo_app/
|   |-- main/test_uart_echo.c      Dedicated UART echo firmware
|   |-- conftest.py                Physical data-port option and opt-in guard
|   `-- pytest_uart_echo.py        Six binary echo pytest cases
|-- tools/uart_echo_test.py         Serial echo validation
|-- tools/run_qemu_tests.py         Discover, build, and run project QEMU tests
|-- CMakeLists.txt                  Application build definition
|-- sdkconfig.defaults             Application configuration defaults
|-- pyproject.toml                 Python dependencies managed by uv
|-- uv.lock                        Locked Python dependencies
`-- pytest.ini                     Test discovery and reporting settings
```

The project CI workflow is `../../.github/workflows/mech-console.yml`.
`build/`, `build-qemu/`, `.venv/`, and test reports are generated and ignored.
The application entry point contains initialization only. Duplicate-start and
timer-based stop checks live in `components/app_health/test/test_app_health.c`.
They verify stop-request acceptance, not completion of idle-task memory cleanup.

## Build the application

Use ESP-IDF v6.1. Run commands from this project directory in an ESP-IDF terminal.
Examples use PowerShell; COM8 is the current board's native USB port.

```powershell
idf.py -DIDF_TARGET=esp32c3 build
idf.py -p COM8 flash monitor
```

## Install test dependencies

```powershell
uv sync --locked --group test
uv run --group test pytest --collect-only -q
```

## Run tests on the board

Build after changing C code. Close the COM8 monitor before running pytest.
The pytest command flashes test firmware, replacing the current application.

```powershell
idf.py -C tests/frame_codec_app -DIDF_TARGET=esp32c3 build
uv run --group test pytest tests/frame_codec_app --embedded-services esp,idf --target esp32c3 --port COM8 --app-path tests/frame_codec_app --build-dir build -v
```

Append `-k decode` to select decoder cases, or `-k decode-zero-data` for one case.
Append `--junitxml=test-results/codec.xml` to save a report.

Restore the application after testing:

```powershell
idf.py -p COM8 flash
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
