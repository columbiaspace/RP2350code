// Pico firmware: FreeRTOS + UART protocol + analog IR on GP28.
//
// Two tasks:
//   protocol (priority 3) — owns UART0 on GP0/GP1
//   sensor   (priority 2) — owns the ADC and produces DATA lines
//
// They never share UART or ADC. They pass messages on two queues.

#include "IrAdcSensor.hpp"
#include "PicoProtocol.hpp"

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <cstdio>
#include <cstring>

namespace {

// UART0 default pins on a Pico 2: GP0 = TX, GP1 = RX.
constexpr uint UART_TX_PIN = 0;
constexpr uint UART_RX_PIN = 1;
constexpr std::uint32_t UART_BAUD = 115200;
constexpr std::uint32_t MIN_STREAM_PERIOD_MS = 10;
constexpr std::uint32_t MAX_STREAM_PERIOD_MS = 60000;

// Higher number = more important. Protocol must stay responsive.
constexpr UBaseType_t PROTOCOL_TASK_PRIORITY = 3;
constexpr UBaseType_t SENSOR_TASK_PRIORITY = 2;

// Stack sizes are in 32-bit words, not bytes.
constexpr configSTACK_DEPTH_TYPE PROTOCOL_TASK_STACK_WORDS = 1024;
constexpr configSTACK_DEPTH_TYPE SENSOR_TASK_STACK_WORDS = 768;
constexpr UBaseType_t COMMAND_QUEUE_LENGTH = 8;
constexpr UBaseType_t TX_QUEUE_LENGTH = 8;

// One outbound UART line, already formatted, including the trailing '\n'.
struct TxFrame {
    char data[PicoProtocol::MAX_LINE_LENGTH + 2];
};

// Work for the sensor task. PING/ECHO never get here.
struct SensorCommand {
    enum class Type : std::uint8_t { READ, STREAM };
    Type type;
    std::uint16_t id;
    std::uint32_t periodMs;  // STREAM only; 0 means stop
};

PicoProtocol::LineParser g_parser;
IrAdcSensor g_sensor;  // swap this type to change sensors (see Sensor.hpp)
std::uint32_t g_sequence = 0;

// Queues + their backing storage. Static = no malloc.
QueueHandle_t g_commandQueue = nullptr;
QueueHandle_t g_txQueue = nullptr;
StaticQueue_t g_commandQueueControl;
StaticQueue_t g_txQueueControl;
alignas(portBYTE_ALIGNMENT) std::uint8_t
    g_commandQueueStorage[COMMAND_QUEUE_LENGTH * sizeof(SensorCommand)];
alignas(portBYTE_ALIGNMENT) std::uint8_t
    g_txQueueStorage[TX_QUEUE_LENGTH * sizeof(TxFrame)];

StaticTask_t g_protocolTaskControl;
StaticTask_t g_sensorTaskControl;
StackType_t g_protocolTaskStack[PROTOCOL_TASK_STACK_WORDS];
StackType_t g_sensorTaskStack[SENSOR_TASK_STACK_WORDS];

bool enqueueFrame(const TxFrame& frame, TickType_t waitTicks) {
    return xQueueSend(g_txQueue, &frame, waitTicks) == pdTRUE;
}

void enqueueError(std::uint16_t id, const char* code) {
    TxFrame frame{};
    if (PicoProtocol::formatError(frame.data, sizeof(frame.data), id, code)) {
        (void)enqueueFrame(frame, pdMS_TO_TICKS(50));
    }
}

// Take one ADC sample and queue a DATA line. id=0 means unsolicited stream data.
bool enqueueSample(std::uint16_t id) {
    const std::uint64_t nowUs = time_us_64();
    const SensorSample sample = g_sensor.read(nowUs);
    ++g_sequence;
    if (!sample.valid && (id != 0)) {
        enqueueError(id, sample.status != nullptr ? sample.status : "SENSOR");
        return false;
    }
    TxFrame frame{};
    if (!PicoProtocol::formatData(frame.data, sizeof(frame.data), id, g_sequence,
                                  nowUs, sample.valid, sample.valueMilli,
                                  sample.status)) {
        if (id != 0) {
            enqueueError(id, "INTERNAL");
        }
        return false;
    }
    // Stream samples (id 0) must not block if the TX queue is full.
    return enqueueFrame(frame, id == 0 ? 0 : pdMS_TO_TICKS(50)) && sample.valid;
}

// Called once per complete '\n'-terminated line from the UART.
void handleLine(void*, const char* line) {
    PicoProtocol::Request request{};
    if (PicoProtocol::parseRequest(line, request) != PicoProtocol::ParseResult::OK) {
        std::printf("Dropped malformed protocol line\n");
        return;
    }

    TxFrame frame{};
    switch (request.type) {
        case PicoProtocol::RequestType::PING:
            if (PicoProtocol::formatAck(frame.data, sizeof(frame.data), request.id, "PONG")) {
                (void)enqueueFrame(frame, pdMS_TO_TICKS(50));
            }
            break;
        case PicoProtocol::RequestType::ECHO:
            if (PicoProtocol::formatAck(frame.data, sizeof(frame.data), request.id,
                                        request.echo)) {
                (void)enqueueFrame(frame, pdMS_TO_TICKS(50));
            } else {
                enqueueError(request.id, "BAD_ARG");
            }
            break;
        case PicoProtocol::RequestType::READ: {
            const SensorCommand command{SensorCommand::Type::READ, request.id, 0};
            if (xQueueSend(g_commandQueue, &command, pdMS_TO_TICKS(20)) != pdTRUE) {
                enqueueError(request.id, "BUSY");
            }
            break;
        }
        case PicoProtocol::RequestType::STREAM:
            if ((request.periodMs != 0) &&
                ((request.periodMs < MIN_STREAM_PERIOD_MS) ||
                 (request.periodMs > MAX_STREAM_PERIOD_MS))) {
                enqueueError(request.id, "BAD_ARG");
                break;
            }
            {
                const SensorCommand command{SensorCommand::Type::STREAM, request.id,
                                            request.periodMs};
                if (xQueueSend(g_commandQueue, &command, pdMS_TO_TICKS(20)) != pdTRUE) {
                    enqueueError(request.id, "BUSY");
                }
            }
            break;
    }
}

bool tickReached(TickType_t now, TickType_t deadline) {
    // Signed subtract handles FreeRTOS tick wrap.
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

void protocolTask(void*) {
    while (true) {
        // Drain every queued reply onto UART0.
        TxFrame outgoing{};
        while (xQueueReceive(g_txQueue, &outgoing, 0) == pdTRUE) {
            uart_write_blocking(uart0,
                                reinterpret_cast<const std::uint8_t*>(outgoing.data),
                                std::strlen(outgoing.data));
        }

        // Pull whatever bytes are waiting and feed the line parser.
        std::uint8_t input[64];
        std::size_t count = 0;
        while (uart_is_readable(uart0) && (count < sizeof(input))) {
            input[count++] = static_cast<std::uint8_t>(uart_getc(uart0));
        }
        if (count > 0) {
            g_parser.consume(input, count, handleLine, nullptr);
        }
        vTaskDelay(pdMS_TO_TICKS(1));  // yield so the sensor task can run
    }
}

void sensorTask(void*) {
    const bool sensorReady = g_sensor.initialize();
    std::printf("Sensor task ready=%u\n", sensorReady ? 1U : 0U);

    TickType_t streamPeriodTicks = 0;
    TickType_t nextSampleTick = 0;

    while (true) {
        const TickType_t now = xTaskGetTickCount();
        TickType_t waitTicks = portMAX_DELAY;  // sleep until a command arrives
        if (streamPeriodTicks != 0) {
            waitTicks = tickReached(now, nextSampleTick) ? 0 : nextSampleTick - now;
        }

        SensorCommand command{};
        if (xQueueReceive(g_commandQueue, &command, waitTicks) == pdTRUE) {
            if (command.type == SensorCommand::Type::READ) {
                (void)enqueueSample(command.id);
            } else {
                streamPeriodTicks =
                    command.periodMs == 0 ? 0 : pdMS_TO_TICKS(command.periodMs);
                nextSampleTick = xTaskGetTickCount() + streamPeriodTicks;
                TxFrame acknowledgement{};
                if (PicoProtocol::formatAck(acknowledgement.data,
                                            sizeof(acknowledgement.data), command.id)) {
                    (void)enqueueFrame(acknowledgement, pdMS_TO_TICKS(50));
                }
            }
            continue;
        }

        if (streamPeriodTicks != 0) {
            (void)enqueueSample(0);  // unsolicited stream sample
            nextSampleTick = xTaskGetTickCount() + streamPeriodTicks;
        }
    }
}

}  // namespace

extern "C" void vApplicationStackOverflowHook(TaskHandle_t task, char* taskName) {
    (void)task;
    panic("FreeRTOS stack overflow in %s",
          taskName == nullptr ? "unknown" : taskName);
}

int main() {
    stdio_init_all();  // USB CDC for printf logs

    uart_init(uart0, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);  // 8N1
    uart_set_fifo_enabled(uart0, true);
    uart_set_hw_flow(uart0, false, false);

    std::printf("PicoLink starting: UART0 GP0/GP1 %lu 8N1\n",
                static_cast<unsigned long>(UART_BAUD));

    g_commandQueue =
        xQueueCreateStatic(COMMAND_QUEUE_LENGTH, sizeof(SensorCommand),
                           g_commandQueueStorage, &g_commandQueueControl);
    g_txQueue = xQueueCreateStatic(TX_QUEUE_LENGTH, sizeof(TxFrame),
                                   g_txQueueStorage, &g_txQueueControl);
    configASSERT(g_commandQueue != nullptr);
    configASSERT(g_txQueue != nullptr);

    const TaskHandle_t protocolTaskHandle = xTaskCreateStatic(
        protocolTask, "protocol", PROTOCOL_TASK_STACK_WORDS, nullptr,
        PROTOCOL_TASK_PRIORITY, g_protocolTaskStack, &g_protocolTaskControl);
    const TaskHandle_t sensorTaskHandle = xTaskCreateStatic(
        sensorTask, "sensor", SENSOR_TASK_STACK_WORDS, nullptr,
        SENSOR_TASK_PRIORITY, g_sensorTaskStack, &g_sensorTaskControl);
    configASSERT(protocolTaskHandle != nullptr);
    configASSERT(sensorTaskHandle != nullptr);
    (void)protocolTaskHandle;
    (void)sensorTaskHandle;

    vTaskStartScheduler();  // does not return if the scheduler starts
    panic("FreeRTOS scheduler exited");
}
