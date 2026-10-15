#!/usr/bin/env python3
"""Create a pseudo-terminal that behaves like Pico firmware with mock millivolts."""

from __future__ import annotations

import argparse
import os
import pty
import select
import signal
import sys
import termios
import time


def sample(request_id: int, sequence: int, start: float) -> str:
    uptime_us = int((time.monotonic() - start) * 1_000_000)
    millivolts = sequence % 3301
    return f"DATA,{request_id},{sequence},{uptime_us},1,MOCK,{millivolts},0,OK\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--max-seconds", type=float, default=0, help="exit after this duration; 0 is forever")
    args = parser.parse_args()

    master, slave = pty.openpty()
    attributes = termios.tcgetattr(slave)
    attributes[3] &= ~(termios.ECHO | termios.ICANON)
    termios.tcsetattr(slave, termios.TCSANOW, attributes)
    print(os.ttyname(slave), flush=True)
    print("Keep this process running; pass the path above to pico_smoke.py.", file=sys.stderr)

    running = True

    def stop(_signum: int, _frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    start = time.monotonic()
    line = bytearray()
    discarding = False
    sequence = 0
    stream_period = 0.0
    next_sample = float("inf")

    while running and (args.max_seconds <= 0 or time.monotonic() - start < args.max_seconds):
        timeout = max(0.0, min(0.05, next_sample - time.monotonic()))
        readable, _, _ = select.select([master], [], [], timeout)
        if readable:
            data = os.read(master, 256)
            for byte in data:
                if byte == ord("\n"):
                    if discarding:
                        line.clear()
                        discarding = False
                        continue
                    command = line.rstrip(b"\r").decode("ascii", errors="replace")
                    line.clear()
                    fields = command.split(",")
                    response = ""
                    try:
                        if len(fields) < 3 or fields[0] != "CMD":
                            raise ValueError
                        request_id = int(fields[1])
                        if not 1 <= request_id <= 65535:
                            raise ValueError
                        if fields[2:] == ["PING"]:
                            response = f"ACK,{request_id},OK,PONG\n"
                        elif len(fields) == 4 and fields[2] == "ECHO" and "," not in fields[3]:
                            response = f"ACK,{request_id},OK,{fields[3]}\n"
                        elif fields[2:] == ["READ"]:
                            sequence += 1
                            response = sample(request_id, sequence, start)
                        elif len(fields) == 4 and fields[2] == "STREAM":
                            period_ms = int(fields[3])
                            if period_ms != 0 and not 10 <= period_ms <= 60_000:
                                response = f"ERR,{request_id},BAD_ARG\n"
                            else:
                                stream_period = period_ms / 1000
                                next_sample = (
                                    time.monotonic() + stream_period
                                    if stream_period
                                    else float("inf")
                                )
                                response = f"ACK,{request_id},OK\n"
                        else:
                            response = f"ERR,{request_id},UNKNOWN_CMD\n"
                    except (ValueError, IndexError):
                        response = "ERR,1,MALFORMED\n"
                    os.write(master, response.encode("ascii"))
                elif len(line) < 256:
                    if not discarding:
                        line.append(byte)
                else:
                    line.clear()
                    discarding = True

        if stream_period and time.monotonic() >= next_sample:
            sequence += 1
            os.write(master, sample(0, sequence, start).encode("ascii"))
            next_sample += stream_period

    os.close(slave)
    os.close(master)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
