# UART protocol

115200 baud, 8 data bits, no parity, 1 stop bit, no flow control. Each message is one ASCII line ended by `\n`. Max 256 bytes before the newline.

No CRC. This is a bench protocol.

## Commands (Pi → Pico)

```text
CMD,<id>,PING
CMD,<id>,ECHO,<text>
CMD,<id>,READ
CMD,<id>,STREAM,<period_ms>
```

`id` is 1..65535. `period_ms=0` stops streaming. Echo text cannot contain comma, CR, or LF.

The Pico waits for commands instead of sampling periodically by default. Send
`CMD,<id>,READ` to request one sensor sample; the Pico replies with one `DATA`
line carrying the same ID. `STREAM` enables periodic samples only after it is
explicitly requested.

## Replies (Pico → Pi)

```text
ACK,<id>,OK
ACK,<id>,OK,<payload>
ERR,<id>,<code>
DATA,<id>,<seq>,<uptime_us>,<valid>,<ir_millivolts>,<status>
```

`DATA` with `id=0` is an unsolicited stream sample. `valid` is `0` or `1`. `ir_millivolts` is GP28 voltage vs 3.3 V: `counts * 3300 / 4095`.

Example request and response:

```text
CMD,3,READ
DATA,3,1,1234567,1,1650,OK
```
