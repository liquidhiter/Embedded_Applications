# Embedded Applications

Embedded application development projects, organized by platform.

## Repository layout

```text
Embedded_Applications/
└── ESP32/
    └── mech_console/
        ├── components/
        │   ├── app_health/
        │   ├── app_uart/
        │   └── frame_codec/
        ├── main/
        ├── tests/frame_codec_app/
        ├── tools/
        ├── pyproject.toml
        └── uv.lock
```

## ESP32: mech_console

A staged embedded application project for the ESP32-C3-LCDkit. The current
baseline includes application health monitoring, UART initialization, COBS
encoding and decoding, and target-side Unity tests driven by pytest.

Display, IMU integration, wireless communication, and higher-level framing and
control protocols are planned work, not implemented features.

### Development environment

- ESP32-C3-LCDkit with 4 MB flash.
- ESP-IDF v6.1 and its configured toolchain.
- uv and Python 3.12 or newer for host-side tools.
- Native USB Serial/JTAG for the ESP32 console and flashing.
- Optional CH340 adapter for UART data testing, using 3.3 V signal levels.

Open `ESP32/mech_console` as the ESP-IDF project in VS Code. Checked-in editor
settings contain local paths and port names; adjust them for your machine.
ESP-IDF uses its own Python environment; uv manages host tools in `.venv`.

All commands below start in the firmware project directory:

```powershell
cd ESP32/mech_console
```

### Build and flash the application

Run in an ESP-IDF terminal:

```powershell
idf.py -DIDF_TARGET=esp32c3 build
idf.py -p COM8 flash monitor
```

The project defaults select 4 MB flash and the USB Serial/JTAG console. Existing
`sdkconfig` values take precedence over defaults; check them when reusing a build.
COM8 is the current development board's port, not a universal device assignment.

### Run the codec tests on the ESP32

The tests execute the actual C implementation on the target. There are 11
independently selectable pytest items covering known vectors, invalid arguments,
zero bytes, truncated blocks, 254-byte boundaries, capacity guards, and round trips.
All 11 passed on the development board on 2026-10-19.

Build the isolated test firmware in an ESP-IDF terminal after changing C code:

```powershell
idf.py -C tests/frame_codec_app -DIDF_TARGET=esp32c3 build
```

Install Python test dependencies, close any monitor using COM8, and run:

```powershell
uv sync --locked --group test
uv run --group test pytest tests/frame_codec_app/pytest_frame_codec.py --embedded-services esp,idf --target esp32c3 --port COM8 --app-path tests/frame_codec_app --build-dir build -v
```

This command flashes the test firmware, replacing the current application, and
runs Unity cases through the USB console. It does not compile the firmware.
Append `-k decode` to select decoder cases or `-k decode-zero-data` for the
embedded-zero regression. Use `--collect-only` to list cases without flashing.
Restore the normal application with `idf.py -p COM8 flash` from the project directory.

### UART echo tool

The temporary echo task is currently disabled in `app_uart.c`. Enable its
`APP_UART_ECHO_TEST` switch and rebuild/flash before running the echo script.

Connect CH340 TX to GPIO20, CH340 RX to GPIO21, and share ground. Power the board
through its USB port. COM10 is the current CH340 data port; COM8 remains the console.

```powershell
uv sync --locked
uv run python tools/uart_echo_test.py --port COM10
```

The script checks binary echoes and reports round-trip latency, not one-way UART
throughput. Close other applications using COM10 first.

### Source and generated files

Track `pyproject.toml`, `uv.lock`, source files, and `sdkconfig.defaults`.
Local environments, build outputs, `sdkconfig`, test reports, and `.python-version`
are ignored. The inherited `pytest_hello_world.py` is excluded from default test
discovery; `tests/frame_codec_app/pytest_frame_codec.py` is the active test entry.

Source files retain their individual license notices, including inherited
Espressif example code. Refer to those notices for each file's licensing terms.
