# Pico 2 UART sensor test

This project turns one Pico 2 into a UART polling tester for the PicoLink sensor
firmware running on a second Pico 2. The tester sends one `READ` request per
second and prints each response to its USB serial port.

Both boards use the RP2350. This guide assumes Windows, PowerShell, and the
Raspberry Pi Pico SDK 2.3.1 installed by the Raspberry Pi Pico VS Code extension.

## What you need

- Two Raspberry Pi Pico 2 boards.
- An analog-output IR sensor connected to the sensor Pico, with output no higher
  than 3.3 V.
- USB data cables for both boards.
- Raspberry Pi Pico SDK 2.3.1, CMake, Ninja, and the ARM GNU toolchain installed
  by the Pico VS Code extension.
- Visual Studio C++ Build Tools for the initial CMake configure if the Pico SDK
  needs to build Picotool from source.

The sensor firmware also needs the RP2350 FreeRTOS port. From the repository
root, install the pinned kernel once if `third-party\FreeRTOS-Kernel` is not
already present:

```powershell
git clone https://github.com/raspberrypi/FreeRTOS-Kernel.git third-party\FreeRTOS-Kernel
git -C third-party\FreeRTOS-Kernel checkout 4f7299d6ea746b27a9dd19e87af568e34bd65b15
```

## Build both UF2 files

Open the repository root in VS Code. In a PowerShell terminal, set up the SDK
paths for this machine:

```powershell
$env:PICO_SDK_PATH = "$env:USERPROFILE\.pico-sdk\sdk\2.3.1"
$env:Path = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1\bin;$env:USERPROFILE\.pico-sdk\ninja\v1.13.2;$env:Path"
$cmake = "$env:USERPROFILE\.pico-sdk\cmake\v4.3.4\bin\cmake.exe"
```

If configuring/building for the first time, run these from the repository root.
For the first configure, use a Visual Studio Developer PowerShell (or another
terminal where the Visual Studio C++ compiler is available), in case the SDK
needs to build Picotool:

```powershell
& $cmake -S firmware -B firmware\build -G Ninja -DPICO_BOARD=pico2
& $cmake --build firmware\build --parallel

& $cmake -S uart_testing -B uart_testing\build -G Ninja -DPICO_BOARD=pico2
& $cmake --build uart_testing\build --parallel
```

For later rebuilds, configure again only if needed; build from each project
folder with `cmake --build build --parallel`:

```powershell
cd firmware
& $cmake --build build --parallel

cd ..\uart_testing
& $cmake --build build --parallel
```

The resulting files are different firmware and must go on the matching boards:

| Board | UF2 |
|---|---|
| Sensor Pico 2 | `firmware\build\pico_link_firmware.uf2` |
| Tester Pico 2 | `uart_testing\build\uart_testing.uf2` |

## Flash the boards

Flash each board separately:

1. Disconnect the board from USB.
2. Hold **BOOTSEL** while plugging it into the computer with a USB data cable.
3. Release BOOTSEL when the `RP2350` or `RPI-RP2` drive appears.
4. Copy that board's UF2 from the table above onto the drive.
5. Wait for the copy to finish. The board reboots and the drive disappears.

## Wire the boards and sensor

Power both Pico boards over USB. Cross the UART data lines and connect their
grounds:

| Tester Pico 2 | Sensor Pico 2 |
|---|---|
| GP4 / UART1 TX | GP1 / UART0 RX |
| GP5 / UART1 RX | GP0 / UART0 TX |
| GND | GND |

Connect the analog IR sensor to the sensor Pico:

| Sensor lead | Sensor Pico 2 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Analog OUT | GP28 / ADC2 |

Use 3.3 V logic only. Do not connect a 5 V sensor output to a Pico GPIO. The
firmware reports the GP28 input voltage in millivolts; it does not convert the
reading into distance.

## Read the sensor output

Connect the tester Pico to USB and open its USB serial/COM port in a serial
monitor at 115200 baud, 8 data bits, no parity, 1 stop bit. Select the tester's
port, not the sensor board's port. The tester writes results over USB serial;
the inter-board protocol uses UART pins.

Once running, the tester requests a sample every second and prints lines like:

```text
Sensor reading: 1650 mV, valid=1, sequence=12, uptime_us=12000000, status=OK
```

If a request receives an error or no response, the tester prints an error or
timeout message and continues polling. Check that both boards are powered,
TX/RX are crossed as shown, the grounds are connected, and the sensor Pico has
the PicoLink UF2 installed.

## Files used by this workflow

### Sensor Pico firmware

- [`../firmware/CMakeLists.txt`](../firmware/CMakeLists.txt) selects the Pico 2
  board, SDK, FreeRTOS RP2350 port, and firmware sources.
- [`../firmware/src/main.cpp`](../firmware/src/main.cpp) initializes UART0 on
  GP0/GP1, waits for commands, and schedules sensor reads.
- [`../firmware/src/IrAdcSensor.cpp`](../firmware/src/IrAdcSensor.cpp) reads
  the analog input on GP28 and converts ADC counts to millivolts.
- [`../firmware/src/IrAdcSensor.hpp`](../firmware/src/IrAdcSensor.hpp) declares
  the analog sensor implementation.
- [`../firmware/src/Sensor.hpp`](../firmware/src/Sensor.hpp) defines the sensor
  interface and sample data structure.
- [`../firmware/src/FreeRTOSConfig.h`](../firmware/src/FreeRTOSConfig.h)
  configures the FreeRTOS kernel for the firmware.
- [`../common/protocol/PicoProtocol.cpp`](../common/protocol/PicoProtocol.cpp)
  and [`../common/protocol/PicoProtocol.hpp`](../common/protocol/PicoProtocol.hpp)
  parse UART commands and format replies.

### Tester Pico firmware

- [`CMakeLists.txt`](CMakeLists.txt) configures the tester project for `pico2`
  and builds `uart_testing.cpp`.
- [`uart_testing.cpp`](uart_testing.cpp) sends `READ` requests over UART1 on
  GP4/GP5, checks replies, and prints sensor readings over USB serial.
- [`pico_sdk_import.cmake`](pico_sdk_import.cmake) connects this project to the
  installed Pico SDK.

The shared message format is documented in
[`../docs/protocol.md`](../docs/protocol.md). `ethan.cpp` is an alternate/older
tester source and is not compiled by this project's CMake configuration.
