# mech_console

[![CI regression](https://github.com/liquidhiter/Embedded_Applications/actions/workflows/mech-console.yml/badge.svg?branch=main)](https://github.com/liquidhiter/Embedded_Applications/actions/workflows/mech-console.yml)

ESP32-C3-LCDkit monitoring and control application, currently implementing the
UART command pipeline. See the [roadmap](docs/roadmap.md) for progress.

## Structure

```text
mech_console/
|-- components/        Application modules and frame codec
|-- main/main.c       Application initialization
|-- tests/            Test firmware and pytest cases
|-- tools/            Formatting, QEMU runner and UART utilities
|-- docs/             Roadmap, protocol and hardware notes
|-- CMakeLists.txt    Application build definition
|-- sdkconfig.defaults
|-- pyproject.toml    Python dependencies managed by uv
`-- uv.lock           Locked Python dependencies
```

## Build and flash

Use ESP-IDF v6.1. Run from this directory in an ESP-IDF terminal.
COM8 is the current board's native USB port.

```powershell
idf.py -DIDF_TARGET=esp32c3 build
idf.py -p COM8 flash monitor
```

## Documentation

- [Published API reference](https://liquidhiter.github.io/Embedded_Applications/ESP32/mech_console/)
- [Testing and formatting](TESTING.md)
- [Roadmap](docs/roadmap.md)
- [UART protocol](docs/protocol.md)
- [Hardware and wiring](docs/hardware.md)
