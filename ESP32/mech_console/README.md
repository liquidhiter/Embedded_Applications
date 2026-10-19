# mech_console

## Application structure

```text
mech_console/
|-- components/
|   |-- app_health/                 Application health task
|   |-- app_uart/                   UART setup and optional echo task
|   `-- frame_codec/
|       |-- frame_codec.c          COBS encoder and decoder
|       |-- include/               Public codec interface
|       `-- test/test_frame_cobs.c Unity test cases
|-- main/main.c                    Application entry point
|-- tests/frame_codec_app/
|   |-- CMakeLists.txt             Isolated test firmware project
|   |-- main/test_main.c           Unity test menu
|   |-- pytest_frame_codec.py      One pytest item per Unity case
|   |-- sdkconfig.defaults         Physical-board USB console configuration
|   `-- sdkconfig.defaults.qemu    QEMU UART console configuration
|-- tools/uart_echo_test.py         Serial echo validation
|-- CMakeLists.txt                  Application build definition
|-- sdkconfig.defaults             Application configuration defaults
|-- pyproject.toml                 Python dependencies managed by uv
|-- uv.lock                        Locked Python dependencies
`-- pytest.ini                     Test discovery and reporting settings
```

The repository CI workflow is `../../.github/workflows/esp32-codec.yml`.
`build/`, `build-qemu/`, `.venv/`, and test reports are generated and ignored.

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
uv run --group test pytest tests/frame_codec_app/pytest_frame_codec.py --embedded-services esp,idf --target esp32c3 --port COM8 --app-path tests/frame_codec_app --build-dir build -v
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

From this project directory in the new ESP-IDF terminal:

```powershell
idf.py -C tests/frame_codec_app -B "$PWD/tests/frame_codec_app/build-qemu" -DIDF_TARGET=esp32c3 "-DSDKCONFIG=$PWD/tests/frame_codec_app/build-qemu/sdkconfig" "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.qemu" build
Push-Location tests/frame_codec_app/build-qemu
uv run --group test python -m esptool --chip esp32c3 merge-bin --output flash_image.bin --pad-to-size 4MB '@flash_args'
Pop-Location
uv run --group test pytest tests/frame_codec_app/pytest_frame_codec.py --embedded-services idf,qemu --target esp32c3 --app-path tests/frame_codec_app --build-dir build-qemu --skip-regenerate-image yes -v
```

Regenerate `flash_image.bin` after each firmware rebuild. QEMU validates codec
behavior; real UART timing and peripheral behavior require the board.

## Run the UART echo test

Enable `APP_UART_ECHO_TEST` in `components/app_uart/app_uart.c`, rebuild, and flash.
Use CH340 TX to GPIO20, RX to GPIO21, and common ground with 3.3 V signal levels.
Close other applications using COM10.

```powershell
uv sync --locked
uv run python tools/uart_echo_test.py --port COM10
```
