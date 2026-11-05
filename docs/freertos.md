# FreeRTOS basics

FreeRTOS is a tiny real-time kernel for microcontrollers. It is not Linux. No files, no users, no shell. It is a scheduler plus queues.

A **task** is the MCU version of a thread: its own stack and loop, same address space as every other task. This firmware is one UF2 with two application tasks.

## What the scheduler does

About every 1 ms (`configTICK_RATE_HZ = 1000`) FreeRTOS looks at which task is ready. The highest-priority ready task runs. If a task waits (`vTaskDelay`, or `xQueueReceive` with a timeout), it **blocks** and the CPU goes to someone else.

`vTaskStartScheduler()` in `main()` starts this. `main()` does not run again.

## This firmware

| Task | Priority | Job |
|---|---|---|
| `protocol` | 3 | UART0 on GP0/GP1 |
| `sensor` | 2 | GP28 ADC |

Higher number wins, so a UART byte can preempt an ADC read.

They talk with **queues**, not by calling each other:

- command queue: protocol → sensor (`READ` / `STREAM`)
- TX queue: either task → protocol (a finished UART line)

Everything is **static**: stacks and queues are global arrays. There is no `malloc` (`configSUPPORT_DYNAMIC_ALLOCATION 0`).

## Why not one loop?

A single `while (true)` that samples the sensor and services UART will drop serial bytes when the ADC work is slow. Two tasks keep the link responsive.

See `firmware/src/main.cpp` and `firmware/src/FreeRTOSConfig.h`.
