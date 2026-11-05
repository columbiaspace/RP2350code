# PicoLink

An analog IR sensor on a Raspberry Pi Pico 2 talks to F Prime on a Raspberry Pi 5 over UART. You operate it from a browser on your laptop.

```text
Laptop (GDS, browser)  --TCP :50000-->  Pi 5 (F Prime flight software)
                                        |
                                        | UART 115200 8N1
                                        v
                               Pico 2 (FreeRTOS firmware)
                                        |
                                        | analog voltage
                                        v
                               IR sensor on GP28
```

Three programs, three machines:

| Name | Where it runs | Source |
|---|---|---|
| Firmware | Pico 2 | `firmware/` |
| Flight software (FSW) | Pi 5 | `Components/`, `PicoLinkDeployment/` |
| Ground software (GDS) | your laptop | `fprime-gds` + a dictionary from the Pi build |

UART is the Pico–Pi wire. TCP is the Pi–laptop link. They are not the same connection.

## Hardware

**3.3 V only.** Do not put 5 V on Pico GPIO.

### UART (Pico ↔ Pi 5 header)

Cross TX and RX. Share ground. Power the Pico from USB.

| Pico 2 | Pi 5 40-pin header |
|---|---|
| GP0 TX (pin 1) | pin 10 (GPIO15 RX) |
| GP1 RX (pin 2) | pin 8 (GPIO14 TX) |
| GND (pin 3) | pin 6 GND |

Do **not** use Pi pin 1 (that is 3.3 V, not UART). Do **not** use `/dev/ttyAMA10` for these pins — that is the tiny 3-pin debug connector between the HDMI ports.

On a Pi 5, enable the header UART once, then reboot:

```bash
sudo raspi-config
# Interface Options → Serial Port → login shell No, serial hardware Yes
```

If `/dev/ttyAMA0` is still missing, add `dtoverlay=uart0-pi5` to `/boot/firmware/config.txt` and reboot.

```bash
ls -l /dev/ttyAMA* /dev/serial/by-id/
```

Use `/dev/ttyAMA0` (or whatever new `ttyAMA*` appeared besides `ttyAMA10`). Put your user in `dialout`: `sudo usermod -aG dialout $USER` then log out and back in.

### IR sensor (analog, 3 wires)

Any analog IR / distance module whose **output stays at or below 3.3 V**.

| Sensor | Pico 2 |
|---|---|
| VCC | 3V3 (physical pin 36) |
| GND | GND (physical pin 33) |
| analog OUT | GP28 / ADC2 (physical pin 34) |

Firmware reports millivolts: `counts * 3300 / 4095`. That is voltage, not distance.

## Switch the sensor

The rest of the firmware talks to a `Sensor`, not to GP28 directly.

1. Copy `firmware/src/IrAdcSensor.cpp` / `.hpp` as a starting point.
2. Implement `initialize()` and `read()` on a class that inherits `Sensor` (`firmware/src/Sensor.hpp`).
3. In `firmware/src/main.cpp`, change `IrAdcSensor g_sensor;` to your class.
4. In `firmware/CMakeLists.txt`, compile your `.cpp` instead of `IrAdcSensor.cpp`.
5. Rebuild and flash the UF2.

If the new sensor is digital (I2C, another GPIO), `read()` still returns millivolts-or-counts in `valueMilli`. Keep `status` a short token with no commas.

## One-time setup

### Laptop (firmware + GDS)

Need: `git`, `cmake`, `ninja`, `arm-none-eabi-gcc` (full toolchain with newlib — Homebrew’s formula is missing libc; xPack GCC works), Python 3.

```bash
cd /path/to/RP2350code

git clone --branch 2.3.1 --depth 1 --recurse-submodules \
  https://github.com/raspberrypi/pico-sdk.git third-party/pico-sdk
git clone https://github.com/raspberrypi/FreeRTOS-Kernel.git third-party/FreeRTOS-Kernel
git -C third-party/FreeRTOS-Kernel checkout 4f7299d6ea746b27a9dd19e87af568e34bd65b15

python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

### Pi 5 (flight software)

Need: `git`, `cmake` 3.16+, `g++`, Python 3. You do **not** need the Pico SDK on the Pi.

```bash
cd ~/RP2350code   # copy this repo onto the Pi (rsync/scp)

git clone --branch v4.4.0 --depth 1 https://github.com/nasa/fprime.git third-party/fprime
git -C third-party/fprime submodule update --init --recursive

python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
```

## Run

Do these in order. Flash the Pico first. Only one process may own the UART.

### 1. Build and flash firmware (laptop)

```bash
export PICO_SDK_PATH="$PWD/third-party/pico-sdk"
export FREERTOS_KERNEL_PATH="$PWD/third-party/FreeRTOS-Kernel"
# If arm-none-eabi-gcc is not on PATH:
# export PATH="$HOME/.local/xpack-arm-none-eabi-gcc-14.2.1-1.1/bin:$PATH"
cmake -S firmware -B firmware/build -G Ninja -DPICO_BOARD=pico2
cmake --build firmware/build --parallel
```

Hold **BOOTSEL**, plug in a USB **data** cable, copy `firmware/build/pico_link_firmware.uf2` onto the `RP2350` / `RPI-RP2` drive. The Pico reboots.

USB serial logs (not the protocol UART):

```bash
screen /dev/cu.usbmodem* 115200    # macOS; Ctrl-A then K to quit
```

You should see:

```text
PicoLink starting: UART0 GP0/GP1 115200 8N1
Sensor task ready=1
```

### 2. Test UART without F Prime (Pi)

Stop FSW if it is running. Then:

```bash
cd ~/RP2350code
. .venv/bin/activate
python tools/pico_smoke.py --port /dev/ttyAMA0 --baud 115200
```

Expect `PASS PING`, `PASS ECHO`, `PASS READ` with an `ir_millivolts` value, then `PASS STREAM`. If it times out: swap TX/RX, check GND, confirm firmware logs, stop `serial-getty@ttyAMA0`.

### 3. Build and run F Prime (Pi)

```bash
cd ~/RP2350code
. .venv/bin/activate
fprime-util generate
fprime-util build -j4
```

If CMake complains the cache is stale: `fprime-util purge` then generate again.

```bash
build-artifacts/Linux/PicoLinkDeployment/bin/PicoLinkDeployment \
  -a 0.0.0.0 -p 50000 \
  -d /dev/ttyAMA0 \
  -b 115200
```

Leave this terminal running. `DriverReady` means the Pi opened the UART, not that the Pico answered.

### 4. Ground station (laptop)

Copy the dictionary from the Pi (it must match that build):

```bash
mkdir -p build-artifacts/Linux/PicoLinkDeployment/dict
scp USER@PI_IP:~/RP2350code/build-artifacts/Linux/PicoLinkDeployment/dict/PicoLinkDeploymentTopologyDictionary.json \
  build-artifacts/Linux/PicoLinkDeployment/dict/
```

```bash
. .venv/bin/activate
fprime-gds --no-app \
  --dictionary build-artifacts/Linux/PicoLinkDeployment/dict/PicoLinkDeploymentTopologyDictionary.json \
  --ip-address PI_IP --ip-port 50000 --ip-client
```

Open `http://127.0.0.1:5000`. Send `PicoLinkDeployment.picoLink.PING`, then `READ_SENSOR`. Use `IrMilliVolts` only when `SampleValid=1`. A command is OK only after a Pico reply; a 3 second silence is a timeout (wiring / firmware / UART device).

## Layout

| Path | Role |
|---|---|
| `firmware/` | Pico FreeRTOS app |
| `common/protocol/` | Shared line parser used by firmware and FSW |
| `Components/PicoLink/` | F Prime commands, events, telemetry |
| `PicoLinkDeployment/` | Linux F Prime app (GDS TCP + UART) |
| `tools/pico_smoke.py` | UART test without F Prime |
| `docs/` | Protocol plus teaching notes |

Teaching notes: [FreeRTOS](docs/freertos.md), [UART](docs/uart.md), [Pico GPIO](docs/pico-gpio.md), [C++](docs/cpp.md), [protocol](docs/protocol.md).

Pinned versions: F Prime v4.4.0, Pico SDK 2.3.1, FreeRTOS-Kernel `4f7299d6ea746b27a9dd19e87af568e34bd65b15`.
