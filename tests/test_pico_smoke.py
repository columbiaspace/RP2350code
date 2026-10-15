import importlib.util
import pathlib
import sys
import unittest


SCRIPT = pathlib.Path(__file__).parents[1] / "tools" / "pico_smoke.py"
SPEC = importlib.util.spec_from_file_location("pico_smoke", SCRIPT)
assert SPEC and SPEC.loader
pico_smoke = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = pico_smoke
SPEC.loader.exec_module(pico_smoke)


class FakeSerial:
    def __init__(self) -> None:
        self.chunks: list[bytes] = []
        self.closed = False

    def write(self, data: bytes) -> int:
        line = data.decode("ascii")
        responses = {
            "CMD,1,PING\n": b"ACK,1,OK,PONG\n",
            "CMD,2,ECHO,smoke-test\n": b"ACK,2,OK,smoke-test\n",
            "CMD,3,READ\n": b"DATA,3,1,1000,1,MOCK,1650,0,OK\n",
            "CMD,4,STREAM,100\n": (
                b"ACK,4,OK\nDATA,0,2,2000,1,MOCK,1651,0,OK\n"
            ),
            "CMD,5,STREAM,0\n": b"ACK,5,OK\n",
        }
        response = responses[line]
        midpoint = max(1, len(response) // 2)
        self.chunks.extend((response[:midpoint], response[midpoint:]))
        return len(data)

    def read(self, size: int = 1) -> bytes:
        del size
        return self.chunks.pop(0) if self.chunks else b""

    def close(self) -> None:
        self.closed = True


class PicoSmokeTest(unittest.TestCase):
    def test_fragmented_and_coalesced_exercise(self) -> None:
        serial = FakeSerial()
        pico_smoke.exercise(pico_smoke.Client(serial, 0.1), 100)

    def test_overflow_recovery(self) -> None:
        reader = pico_smoke.LineReader()
        lines = reader.feed(b"x" * 257 + b"\nACK,1,OK,PONG\r\n")
        self.assertEqual(lines, ["ACK,1,OK,PONG"])

    def test_numeric_bounds_and_malformed_frames(self) -> None:
        with self.assertRaises(ValueError):
            pico_smoke.parse_frame("ACK,65536,OK")
        with self.assertRaises(ValueError):
            pico_smoke.parse_frame("DATA,0,1,2,1,FAKE,3,4,OK")
        with self.assertRaises(ValueError):
            pico_smoke.parse_frame("ACK,0,OK")

    def test_error_reply(self) -> None:
        client = pico_smoke.Client(FakeSerial(), 0.1)
        frame = pico_smoke.Frame("ERR", 4, ("BUSY",))
        with self.assertRaisesRegex(RuntimeError, "BUSY"):
            client._validate(frame, "ACK")


if __name__ == "__main__":
    unittest.main()
