#!/usr/bin/env python3
"""Exercise the PicoLink UART protocol without starting F Prime."""

from __future__ import annotations

import argparse
import dataclasses
import sys
import time
from typing import Protocol

MAX_LINE_LENGTH = 256


class SerialLike(Protocol):
    def write(self, data: bytes) -> int: ...
    def read(self, size: int = 1) -> bytes: ...
    def close(self) -> None: ...


@dataclasses.dataclass(frozen=True)
class Frame:
    kind: str
    request_id: int
    fields: tuple[str, ...]


class LineReader:
    def __init__(self) -> None:
        self._line = bytearray()
        self._discarding = False

    def feed(self, data: bytes) -> list[str]:
        lines: list[str] = []
        for byte in data:
            if byte == ord("\n"):
                if not self._discarding:
                    if self._line.endswith(b"\r"):
                        self._line.pop()
                    lines.append(self._line.decode("ascii", errors="strict"))
                self._line.clear()
                self._discarding = False
            elif not self._discarding:
                if len(self._line) < MAX_LINE_LENGTH:
                    self._line.append(byte)
                elif byte != ord("\r"):
                    self._discarding = True
                    self._line.clear()
        return lines


def parse_frame(line: str) -> Frame:
    fields = line.split(",")
    if len(fields) < 3 or fields[0] not in {"ACK", "ERR", "DATA"}:
        raise ValueError(f"malformed frame: {line!r}")
    try:
        request_id = int(fields[1], 10)
    except ValueError as error:
        raise ValueError(f"invalid request ID: {line!r}") from error
    if not 0 <= request_id <= 65535:
        raise ValueError(f"request ID out of range: {line!r}")
    if fields[0] == "ACK":
        if request_id == 0 or len(fields) not in (3, 4) or fields[2] != "OK":
            raise ValueError(f"malformed ACK: {line!r}")
    elif fields[0] == "ERR":
        if request_id == 0 or len(fields) != 3 or not fields[2]:
            raise ValueError(f"malformed ERR: {line!r}")
    else:
        if len(fields) != 9 or fields[4] not in {"0", "1"} or fields[5] not in {"MOCK", "REAL"}:
            raise ValueError(f"malformed DATA: {line!r}")
        for index in (2, 3, 6, 7):
            int(fields[index], 10)
    return Frame(fields[0], request_id, tuple(fields[2:]))


class Client:
    def __init__(self, serial_port: SerialLike, timeout: float) -> None:
        self.serial = serial_port
        self.timeout = timeout
        self.reader = LineReader()
        self.pending_frames: list[Frame] = []

    def send(self, request_id: int, command: str) -> None:
        line = f"CMD,{request_id},{command}\n"
        encoded = line.encode("ascii")
        if len(encoded) - 1 > MAX_LINE_LENGTH:
            raise ValueError("request exceeds protocol line limit")
        written = self.serial.write(encoded)
        if written != len(encoded):
            raise RuntimeError(f"short UART write: {written}/{len(encoded)} bytes")

    def wait_for(self, request_id: int, expected_kind: str) -> Frame:
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            for index, frame in enumerate(self.pending_frames):
                if frame.request_id == request_id:
                    self.pending_frames.pop(index)
                    return self._validate(frame, expected_kind)
            data = self.serial.read(64)
            if not data:
                continue
            for line in self.reader.feed(data):
                frame = parse_frame(line)
                if frame.request_id == request_id:
                    return self._validate(frame, expected_kind)
                self.pending_frames.append(frame)
        raise TimeoutError(f"timed out waiting for request ID {request_id}")

    @staticmethod
    def _validate(frame: Frame, expected_kind: str) -> Frame:
        if frame.kind == "ERR":
            raise RuntimeError(f"Pico returned ERR for ID {frame.request_id}: {frame.fields[0]}")
        if frame.kind != expected_kind:
            raise RuntimeError(
                f"ID {frame.request_id}: expected {expected_kind}, received {frame.kind}"
            )
        return frame


def exercise(client: Client, stream_period_ms: int) -> None:
    client.send(1, "PING")
    ping = client.wait_for(1, "ACK")
    if ping.fields != ("OK", "PONG"):
        raise RuntimeError(f"unexpected PING reply: {ping}")
    print("PASS PING: PONG")

    client.send(2, "ECHO,smoke-test")
    echo = client.wait_for(2, "ACK")
    if echo.fields != ("OK", "smoke-test"):
        raise RuntimeError(f"unexpected ECHO reply: {echo}")
    print("PASS ECHO: smoke-test")

    client.send(3, "READ")
    reading = client.wait_for(3, "DATA")
    print_data("PASS READ", reading)

    client.send(4, f"STREAM,{stream_period_ms}")
    client.wait_for(4, "ACK")
    stream = client.wait_for(0, "DATA")
    print_data("PASS STREAM sample", stream)

    client.send(5, "STREAM,0")
    client.wait_for(5, "ACK")
    print("PASS STOP_STREAM")


def print_data(label: str, frame: Frame) -> None:
    sequence, uptime_us, valid, source, value_milli, age_ms, status = frame.fields
    print(
        f"{label}: seq={sequence} uptime_us={uptime_us} valid={valid} "
        f"source={source} ir_millivolts={value_milli} age_ms={age_ms} status={status}"
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="serial device, e.g. /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--stream-period-ms", type=int, default=100)
    args = parser.parse_args(argv)
    if args.baud <= 0 or args.timeout <= 0 or not 10 <= args.stream_period_ms <= 60_000:
        parser.error("baud/timeout must be positive; stream period must be 10..60000 ms")

    try:
        import serial
    except ImportError:
        print("ERROR: pyserial is required: python3 -m pip install pyserial", file=sys.stderr)
        return 2

    port = None
    try:
        port = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.05,
            write_timeout=args.timeout,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        )
        exercise(Client(port, args.timeout), args.stream_period_ms)
        print("All PicoLink smoke checks passed")
        return 0
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    finally:
        if port is not None:
            port.close()


if __name__ == "__main__":
    raise SystemExit(main())
