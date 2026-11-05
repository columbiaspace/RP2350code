# UART basics

UART (Universal Asynchronous Receiver/Transmitter) sends bytes as timed bits on a wire. No clock line. Both ends agree on baud rate in advance.

This project uses **115200 8N1**: 115200 bits per second, 8 data bits, no parity, 1 stop bit, no flow control.

## Pins

Each UART has:

- **TX** — this chip transmits
- **RX** — this chip receives
- **GND** — common ground (required)

Always **cross** TX and RX:

```text
Pico GP0 TX  ----->  Pi pin 10 RX
Pico GP1 RX  <-----  Pi pin 8  TX
Pico GND     ------  Pi pin 6  GND
```

If both TXs sit on the same wire, nothing answers and PING times out. Swap the two data wires once.

UART is **3.3 V** here. 5 V into a Pico pin can damage it.

## Driver vs protocol

A **UART driver** moves raw bytes (`uart_write_blocking` on the Pico, Linux `/dev/ttyAMA0` on the Pi). It does not know what `PING` means.

The **protocol** (`common/protocol/`) is the grammar of those bytes: `CMD,1,PING\n`.

USB serial on the Pico is **logs only** (`printf`). The protocol does not go over USB.

## Two UARTs on a Pi 5

| Device | Physical connector |
|---|---|
| `/dev/ttyAMA0` (after enabling uart0) | 40-pin header pins 8 and 10 |
| `/dev/ttyAMA10` | 3-pin debug header between HDMI |

This project uses the 40-pin header. F Prime `-d` must match the wires you actually connected.

Only one process may open the device. Stop FSW before `pico_smoke.py`. Disable `serial-getty@ttyAMA0` if a login console is sitting on that port.
