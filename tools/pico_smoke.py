#!/usr/bin/env python3
"""Send PING/ECHO/READ/STREAM over UART. Run this *instead of* F Prime, not with it."""

from __future__ import annotations

import argparse
import sys
import time

MAX_LINE_LENGTH = 256


def read_line(port, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(1)
        if not chunk:
            continue
        if chunk == b"\n":
            if buf.endswith(b"\r"):
                buf.pop()
            return buf.decode("ascii")
        buf.extend(chunk)
        if len(buf) > MAX_LINE_LENGTH:
            buf.clear()
    raise TimeoutError("timed out waiting for a UART line")


def expect(port, request_id: int, kind: str, timeout: float) -> list[str]:
    while True:
        fields = read_line(port, timeout).split(",")
        if len(fields) < 3:
            raise RuntimeError(f"malformed line: {fields}")
        if fields[0] == "DATA" and fields[1] == "0" and kind != "DATA":
            continue  # leftover stream sample
        if fields[1] != str(request_id) and not (kind == "DATA" and request_id == 0):
            continue
        if fields[0] == "ERR":
            raise RuntimeError(f"Pico ERR: {fields[2]}")
        if fields[0] != kind:
            raise RuntimeError(f"expected {kind}, got {fields[0]}")
        return fields


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="e.g. /dev/ttyAMA0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=2.0)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("ERROR: pip install pyserial", file=sys.stderr)
        return 2

    port = serial.Serial(
        args.port,
        args.baud,
        timeout=0.05,
        write_timeout=args.timeout,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    )
    try:
        port.write(b"CMD,1,PING\n")
        ping = expect(port, 1, "ACK", args.timeout)
        if ping[2:] != ["OK", "PONG"]:
            raise RuntimeError(f"bad PING: {ping}")
        print("PASS PING")

        port.write(b"CMD,2,ECHO,smoke-test\n")
        echo = expect(port, 2, "ACK", args.timeout)
        if echo[2:] != ["OK", "smoke-test"]:
            raise RuntimeError(f"bad ECHO: {echo}")
        print("PASS ECHO")

        port.write(b"CMD,3,READ\n")
        data = expect(port, 3, "DATA", args.timeout)
        print(
            f"PASS READ: seq={data[2]} uptime_us={data[3]} valid={data[4]} "
            f"ir_millivolts={data[5]} status={data[6]}"
        )

        port.write(b"CMD,4,STREAM,100\n")
        expect(port, 4, "ACK", args.timeout)
        stream = expect(port, 0, "DATA", args.timeout)
        print(f"PASS STREAM: ir_millivolts={stream[5]}")

        port.write(b"CMD,5,STREAM,0\n")
        expect(port, 5, "ACK", args.timeout)
        print("PASS STOP_STREAM")
        print("All PicoLink smoke checks passed")
        return 0
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    finally:
        port.close()


if __name__ == "__main__":
    raise SystemExit(main())
