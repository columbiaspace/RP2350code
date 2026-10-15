# PicoLink bench UART protocol

This repository uses 115200 baud, 8 data bits, no parity, one stop bit, and no
hardware or software flow control. Frames are printable ASCII lines terminated
by `\n`; receivers also accept `\r\n`. A line is limited to 256 bytes excluding
the terminator. On overflow, receivers discard through the next newline.

This is a bench protocol. It has no CRC and cannot reliably detect arbitrary
serial corruption.

## Requests

```text
CMD,<id>,PING
CMD,<id>,ECHO,<text>
CMD,<id>,READ
CMD,<id>,STREAM,<period_ms>
```

`id` is decimal 1..65535 and cannot be reused while pending. `period_ms=0`
stops streaming; nonzero periods are 10..60000 ms. Echo text is at most 180
ASCII bytes and cannot contain comma, CR, or LF.

## Replies

```text
ACK,<id>,OK
ACK,<id>,OK,<payload>
ERR,<id>,<code>
DATA,<id>,<sample_seq>,<uptime_us>,<valid>,<source>,<ir_millivolts>,<age_ms>,<status>
```

Field order after `DATA`:

1. `id` — request ID; `0` for unsolicited stream samples
2. `sample_seq` — wrapping unsigned 32-bit acquisition counter
3. `uptime_us` — Pico monotonic timestamp at acquisition, microseconds
4. `valid` — `0` or `1`
5. `source` — `MOCK` or `REAL`
6. `ir_millivolts` — signed 32-bit analog IR voltage in millivolts
7. `age_ms` — sample age when transmitted, milliseconds
8. `status` — `OK` or an error token with no commas

- PING returns `ACK,<id>,OK,PONG`.
- ECHO returns the text as the ACK payload.
- STREAM ACKs only after applying the period.
- READ returns matching-ID DATA or `ERR,<id>,SENSOR`.
- Integers are base-10.
- `ir_millivolts` is GP28 ADC2 voltage vs the 3.3 V reference:
  `millivolts = adc_counts * 3300 / 4095`. Range is 0..3300 for a real
  conversion. This is analog output voltage, not distance.
- F Prime updates `IrMilliVolts` only when `valid=1`. Also check `source`,
  `status`, and `age_ms` before treating a sample as healthy.

Error codes: `BAD_ARG`, `BUSY`, `SENSOR`, `ADC`, `INTERNAL`. Malformed input
without a trustworthy request ID is discarded without a reply.
