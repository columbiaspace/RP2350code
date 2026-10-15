module PicoLink {

  active component PicoLink {
    import Drv.PassiveByteStreamDriverClient

    sync input port tick: Svc.Sched

    async command PING opcode 0x00
    async command ECHO(value: string size 180) opcode 0x01
    async command READ_SENSOR opcode 0x02
    async command START_STREAM(period_ms: U32) opcode 0x03
    async command STOP_STREAM opcode 0x04

    event DriverReady severity activity high id 0x00 \
      format "Pico UART driver ready"
    event RequestSent(requestId: U16, requestName: string size 16) severity activity low id 0x01 \
      format "Pico request {} sent: {}"
    event ReplyReceived(requestId: U16, replyName: string size 32) severity activity low id 0x02 \
      format "Pico reply {} received: {}"
    event RequestBusy severity warning low id 0x03 \
      format "PicoLink already has a pending request"
    event RequestTimeout(requestId: U16) severity warning high id 0x04 \
      format "Pico request {} timed out"
    event ProtocolError(detail: string size 48) severity warning low id 0x05 \
      format "Pico protocol error: {}"
    event DriverError severity warning high id 0x06 \
      format "Pico UART driver reported an error"
    event SensorInvalid(sequence: U32, status: string size 31) severity warning low id 0x07 \
      format "Pico sample {} invalid: {}"

    @ 0=down, 1=driver ready, 2=request/reply healthy
    telemetry LinkStatus: U8 id 0x00 update on change
    @ IR analog output on Pico GP28, millivolts vs 3.3 V ADC reference
    telemetry IrMilliVolts: I32 id 0x01
    telemetry SampleSequence: U32 id 0x02
    @ Pico monotonic timestamp in microseconds
    telemetry PicoUptimeUs: U64 id 0x03
    telemetry SampleValid: U8 id 0x04 update on change
    @ Age reported by Pico in milliseconds
    telemetry SampleAgeMs: U32 id 0x05
    @ 0=unknown, 1=mock, 2=real
    telemetry SampleSource: U8 id 0x06 update on change
    telemetry TxBytes: U32 id 0x07
    telemetry RxBytes: U32 id 0x08
    telemetry ParseErrors: U32 id 0x09
    telemetry Timeouts: U32 id 0x0A
    telemetry SensorErrors: U32 id 0x0B

    time get port timeCaller
    command reg port cmdRegOut
    command recv port cmdIn
    command resp port cmdResponseOut
    event port logOut
    text event port logTextOut
    telemetry port tlmOut
  }
}
