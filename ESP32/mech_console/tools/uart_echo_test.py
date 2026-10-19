#!/usr/bin/env python3
"""Validate binary UART echoes using CH340 COM10; reserve ESP32 COM8 for logs.

Send one payload at a time and validate its complete echo before sending another.
Measure host-observed round-trip time, including transmission, USB/driver queues,
firmware processing, and reception. This is not a one-way throughput measurement.
Set up the environment: uv sync
Run: uv run python tools/uart_echo_test.py --port COM10
"""

import argparse
import math
import random
import statistics
import sys
import time

try:
    import serial
except ImportError:
    raise SystemExit("Missing pyserial. Run uv sync, then uv run python tools/uart_echo_test.py from the project root.")


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return number


def positive_float(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be a finite positive number")
    return number


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM10", help="data port (default: COM10)")
    parser.add_argument("--baud", type=positive_int, default=115200)
    parser.add_argument("--lengths", nargs="+", type=positive_int, default=[1, 255, 256, 257, 1024, 4096])
    parser.add_argument("--repeat", type=positive_int, default=10)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument(
        "--timeout",
        type=positive_float,
        default=5.0,
        help="combined send/receive timeout per round in seconds (default: 5)",
    )
    parser.add_argument(
        "--settle",
        type=positive_float,
        default=0.3,
        help="settling time before clearing stale input after opening the port (seconds)",
    )
    parser.add_argument(
        "--quiet",
        type=positive_float,
        default=0.02,
        help="extra-byte detection window after each round, excluded from RTT (seconds)",
    )
    return parser.parse_args(argv)


class EchoFailure(Exception):
    def __init__(self, reason, expected, received, written, elapsed):
        super().__init__(reason)
        self.expected = expected
        self.received = bytes(received)
        self.written = written
        self.elapsed = elapsed


def exchange(ser, payload, timeout, quiet):
    """Handle partial I/O with a monotonic deadline and return the full echo RTT."""
    received = bytearray()
    written = 0
    started = time.perf_counter()
    deadline = started + timeout

    def failure(reason):
        return EchoFailure(reason, payload, received, written, time.perf_counter() - started)

    try:
        while written < len(payload):
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise failure("Send timed out")
            ser.write_timeout = remaining
            count = ser.write(payload[written:])
            if count <= 0:
                raise failure("Serial write made no progress")
            written += count

        # A complete echo proves transmission; skip flush() to avoid an unbounded wait.
        while len(received) < len(payload):
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise failure("Receive did not complete within the round deadline")
            ser.timeout = min(0.1, remaining)
            received.extend(ser.read(len(payload) - len(received)))

        elapsed = time.perf_counter() - started
        if elapsed > timeout:
            raise failure("Complete echo arrived after the round deadline")
        if received != payload:
            raise failure("Echo content mismatch")

        # Reject extra data; clearing input between rounds could hide duplicate or late bytes.
        ser.timeout = quiet
        extra = ser.read(1)
        if extra:
            received.extend(extra)
            raise failure("Received extra data after the complete echo (at least 1 extra byte)")
        return elapsed
    except serial.SerialException as exc:
        raise failure(f"Serial I/O failed: {exc}") from exc


def report_failure(exc):
    expected, received = exc.expected, exc.received
    position = next((i for i, (a, b) in enumerate(zip(expected, received)) if a != b), None)
    if position is None and len(expected) != len(received):
        position = min(len(expected), len(received))
    print(f"Failed: {exc}", file=sys.stderr)
    print(
        f"  expected TX={len(expected)} B, confirmed written={exc.written} B, "
        f"received={len(received)} B, elapsed={exc.elapsed:.3f} s",
        file=sys.stderr,
    )
    if position is not None:
        want = f"0x{expected[position]:02X}" if position < len(expected) else "<end>"
        got = f"0x{received[position]:02X}" if position < len(received) else "<missing>"
        print(f"  first mismatch offset={position} (zero-based), expected={want}, actual={got}", file=sys.stderr)


def main(argv=None):
    args = parse_args(argv)
    rng = random.Random(args.seed)
    passed = 0
    context = "Opening serial port"
    print(f"Data port={args.port}, {args.baud} baud, 8N1, no flow control, seed={args.seed}")
    print("Close other applications using this data port and ensure the firmware echo task is running.")
    print("Measuring echo round-trip time, not one-way UART throughput.", flush=True)
    try:
        with serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1,
            write_timeout=args.timeout,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        ) as ser:
            time.sleep(args.settle)
            # Clear stale input only once before testing, such as ROM output from a reset.
            stale = ser.in_waiting
            ser.reset_input_buffer()
            if stale:
                print(f"Cleared approximately {stale} B of stale input before testing.")

            for length in args.lengths:
                durations = []
                for repeat in range(1, args.repeat + 1):
                    context = f"Payload {length} B, round {repeat}/{args.repeat}"
                    payload = bytearray(rng.getrandbits(8) for _ in range(length))
                    if length >= 2:
                        payload[0], payload[-1] = 0x00, 0xFF
                    durations.append(exchange(ser, bytes(payload), args.timeout, args.quiet))
                    passed += 1
                print(
                    f"Passed {length:5d} B x {args.repeat} rounds: "
                    f"RTT min/mean/max = {min(durations) * 1000:.2f} / "
                    f"{statistics.mean(durations) * 1000:.2f} / "
                    f"{max(durations) * 1000:.2f} ms",
                    flush=True,
                )
    except EchoFailure as exc:
        print(f"\n{context}; {passed} rounds passed before failure.", file=sys.stderr)
        report_failure(exc)
        return 1
    except serial.SerialException as exc:
        print(f"{context} failed: {exc}", file=sys.stderr)
        print(
            "Check the port name, USB connection, and whether another application is using the port.", file=sys.stderr
        )
        return 1
    except KeyboardInterrupt:
        print(f"\nInterrupted by user; {passed} rounds passed.", file=sys.stderr)
        return 130

    print(f"All passed: {passed} rounds, {sum(args.lengths) * args.repeat} B echoed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
