# PicoLink: F Prime on Pi 5 ↔ Pico 2 UART

F Prime flight software runs on a Raspberry Pi 5. Standalone FreeRTOS firmware
runs on a Raspberry Pi Pico 2. They talk over a dedicated 115200 8N1 UART.
F Prime GDS uses a separate TCP connection.

The Pico samples an analog **IR sensor on GP28** (ADC2) and reports millivolts
against the 3.3 V ADC reference. `IrMilliVolts` is voltage, not distance. A
mock millivolt source remains a CMake option (`-DPICO_LINK_MOCK_SENSOR=ON`).

```text
GDS (browser)
    TCP :50000
Raspberry Pi 5  —  PicoLinkDeployment (F Prime)
    UART 115200 8N1
Raspberry Pi Pico 2  —  pico_link_firmware (FreeRTOS)
```

## What each file is for

| Path | Role |
|---|---|
| `common/protocol/` | Shared C++ line parser and formatter used by F Prime and firmware |
| `docs/protocol.md` | Exact UART frame grammar, fields, units, and error codes |
| `Components/PicoLink/` | F Prime component: commands, events, telemetry, timeouts |
| `PicoLinkDeployment/` | Linux F Prime app and topology (GDS TCP + Pico UART) |
| `firmware/` | Pico 2 FreeRTOS firmware |
| `tools/pico_smoke.py` | pySerial hardware or PTY smoke test |
| `tools/mock_pico_peer.py` | Host-only fake Pico over a pseudo-terminal |
| `tests/` | Unit tests for the smoke tool |
| `requirements.txt` | Python pins: F Prime tools 4.4.0 and pyserial |
| `settings.ini` | F Prime project settings |
| `third-party/` | Local clones of F Prime, Pico SDK, and FreeRTOS (not committed) |

Firmware internals (`firmware/`):

- `src/main.cpp` starts UART0 on GP0/GP1, then two FreeRTOS tasks.
- `protocol` (priority 3) owns UART RX/TX and answers PING/ECHO.
- `sensor` (priority 2) handles READ/STREAM and GP28 ADC samples.
- Two static queues pass commands and outgoing frames between the tasks.
- `src/IrAdcSensor.*` reads ADC2; `src/MockSensor.*` is the optional mock.

F Prime internals (`Components/PicoLink/`):

- Commands: `PING`, `ECHO`, `READ_SENSOR`, `START_STREAM`, `STOP_STREAM`.
- One request may be outstanding. A successful UART write does **not** complete
  the command. Completion waits for a matching Pico ACK or DATA.
- A second command returns `BUSY`. A missing reply times out after 3 seconds.

Pinned versions: F Prime **v4.4.0**, Pico SDK **2.3.1**, Raspberry Pi
FreeRTOS-Kernel commit `4f7299d6ea746b27a9dd19e87af568e34bd65b15`.

## One-time setup

On the Mac (firmware) or Pi 5 (F Prime):

```bash
cd /path/to/RP2350code

git clone --branch v4.4.0 --depth 1 https://github.com/nasa/fprime.git third-party/fprime
git -C third-party/fprime submodule update --init --recursive
git clone --branch 2.3.1 --depth 1 --recurse-submodules \
  https://github.com/raspberrypi/pico-sdk.git third-party/pico-sdk
git clone https://github.com/raspberrypi/FreeRTOS-Kernel.git third-party/FreeRTOS-Kernel
git -C third-party/FreeRTOS-Kernel checkout 4f7299d6ea746b27a9dd19e87af568e34bd65b15

python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

Firmware also needs `arm-none-eabi-gcc` and Ninja. The official Pico VS Code
extension can install them on macOS.

## Wire the UART

3.3 V logic only. Cross TX to RX. Share ground. Power the Pico from USB.

| Pico 2 | Pi 5 header |
|---|---|
| GP0 TX / physical pin 1 | GPIO15 RX / physical pin 10 |
| GP1 RX / physical pin 2 | GPIO14 TX / physical pin 8 |
| GND / physical pin 3 | GND / physical pin 6 |

A 3.3 V USB-to-UART adapter is easier on Yocto: adapter TX → Pico GP1, adapter
RX → Pico GP0, grounds together. Discover the device; do not assume
`/dev/serial0`.

## Wire the IR sensor

3.3 V analog output only. Do not feed a 5 V signal into GP28.

| IR module | Pico 2 |
|---|---|
| VCC | 3V3 / physical pin 36 |
| GND | GND / physical pin 33 |
| analog OUT | GP28 / ADC2 / physical pin 34 |

Firmware converts 12-bit ADC counts with `millivolts = counts * 3300 / 4095`.
Treat `IrMilliVolts` as voltage. Do not interpret it as range unless you add a
calibration for a specific module.

```bash
ls -l /dev/serial/by-id/ /dev/ttyAMA* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

## How to test (do this in order)

### 1. Host tests (no hardware)

These run on the Mac and check framing, recovery, and the smoke tool.

```bash
cd /path/to/RP2350code
. .venv/bin/activate

cmake -S common/protocol -B build/protocol
cmake --build build/protocol --parallel
ctest --test-dir build/protocol --output-on-failure

python3 -m unittest discover -s tests -v
```

Expected: `PicoProtocol tests passed` and four Python tests `ok`.

### 2. Host-only UART exercise (no Pico)

Terminal 1:

```bash
. .venv/bin/activate
python tools/mock_pico_peer.py
```

It prints a path such as `/dev/ttys012`. Terminal 2:

```bash
. .venv/bin/activate
python tools/pico_smoke.py --port /dev/ttys012 --baud 115200
```

Expected:

```text
PASS PING: PONG
PASS ECHO: smoke-test
PASS READ: ... source=MOCK ...
PASS STREAM sample: ... source=MOCK ...
PASS STOP_STREAM
All PicoLink smoke checks passed
```

This proves the host protocol, not Pico hardware.

### 3. Flash and smoke-test the Pico

```bash
export PICO_SDK_PATH="$PWD/third-party/pico-sdk"
export FREERTOS_KERNEL_PATH="$PWD/third-party/FreeRTOS-Kernel"
cmake -S firmware -B firmware/build -G Ninja -DPICO_BOARD=pico2
cmake --build firmware/build --parallel
```

Add `-DPICO_LINK_MOCK_SENSOR=ON` only if you want a sawtooth 0..3300 mV source
instead of GP28.

Flash `firmware/build/pico_link_firmware.uf2`: hold BOOTSEL, plug in a USB
**data** cable, copy the UF2, wait for the drive to disappear.

USB logs should show:

```text
PicoLink FreeRTOS starting: UART0 GP0/GP1 115200 8N1
Sensor task: source=REAL ready=1
```

Then, with only the smoke tool using the UART:

```bash
python tools/pico_smoke.py --port /dev/serial/by-id/YOUR_ADAPTER --baud 115200
```

Close the smoke tool before starting F Prime.

### 4. Build and run F Prime on the Pi 5

```bash
cd /path/to/RP2350code
. .venv/bin/activate
fprime-util generate
fprime-util build -j4
```

Outputs:

```text
build-artifacts/Linux/PicoLinkDeployment/bin/PicoLinkDeployment
build-artifacts/Linux/PicoLinkDeployment/dict/PicoLinkDeploymentTopologyDictionary.json
```

Start flight software:

```bash
build-artifacts/Linux/PicoLinkDeployment/bin/PicoLinkDeployment \
  -a 0.0.0.0 -p 50000 \
  -d /dev/serial/by-id/YOUR_ADAPTER \
  -b 115200
```

In another terminal:

```bash
. .venv/bin/activate
fprime-gds --no-app \
  --dictionary build-artifacts/Linux/PicoLinkDeployment/dict/PicoLinkDeploymentTopologyDictionary.json \
  --ip-address 127.0.0.1 --ip-port 50000 --ip-client
```

Open the GDS URL (usually `http://127.0.0.1:5000`) and send:

- `PicoLinkDeployment.picoLink.PING`
- `PicoLinkDeployment.picoLink.ECHO` (`value` has no comma, CR, or LF)
- `PicoLinkDeployment.picoLink.READ_SENSOR`
- `PicoLinkDeployment.picoLink.START_STREAM` (`period_ms` 10..60000)
- `PicoLinkDeployment.picoLink.STOP_STREAM`

A command is OK only after a matching Pico reply. Watch
`DriverReady`, `RequestSent`, `ReplyReceived`. Telemetry lives under
`PicoLinkDeployment.picoLink`. Use a sample only when `SampleValid=1`.
`SampleSource=2` is the real GP28 ADC. `IrMilliVolts` is analog voltage.

## Protocol (short)

```text
CMD,<id>,PING
CMD,<id>,ECHO,<text>
CMD,<id>,READ
CMD,<id>,STREAM,<period_ms>     # 0 = stop

ACK,<id>,OK[,payload]
ERR,<id>,<code>
DATA,<id>,<seq>,<uptime_us>,<valid>,<MOCK|REAL>,<ir_millivolts>,<age_ms>,<status>
```

IDs 1–65535 identify requests. ID 0 is unsolicited stream data. There is no
CRC; this is a bench protocol. Full grammar: `docs/protocol.md`.

## Troubleshooting

- Build Pico 2 with `-DPICO_BOARD=pico2`, not `pico`.
- Use a USB data cable for BOOTSEL flashing.
- Cross TX/RX, share ground, stay at 3.3 V and 115200 8N1.
- Only one process may own the UART.
- Pico USB CDC is logs only; protocol is GP0/GP1.
- `/dev/serial0` on Pi 5 is often the debug connector, not header pins 8/10.
- Invalid or zero readings are not healthy unless `valid=1` and source/age match.
- Keep the IR analog pin at or below 3.3 V. Zero millivolts can be a real dark
  reading; check `SampleValid` and wiring before treating it as a fault.
