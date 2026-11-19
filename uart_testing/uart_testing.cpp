#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hardware/uart.h"
#include "pico/stdlib.h"

namespace {

uart_inst_t* const UART_ID = uart1;
constexpr uint UART_TX_PIN = 4;
constexpr uint UART_RX_PIN = 5;
constexpr uint UART_BAUD = 115200;
constexpr uint32_t POLL_INTERVAL_MS = 1000;
constexpr uint32_t REPLY_TIMEOUT_US = 2000000;
constexpr std::size_t MAX_LINE_LENGTH = 256;
constexpr std::size_t MAX_FIELDS = 8;

char g_line[MAX_LINE_LENGTH + 1];
char* g_fields[MAX_FIELDS];
std::size_t g_fieldCount;

bool readLine(absolute_time_t deadline) {
    std::size_t length = 0;
    bool discarding = false;

    while (!time_reached(deadline)) {
        if (!uart_is_readable(UART_ID)) {
            sleep_ms(1);
            continue;
        }

        const char byte = static_cast<char>(uart_getc(UART_ID));
        if (byte == '\n') {
            if (discarding) {
                return false;
            }
            if ((length > 0) && (g_line[length - 1] == '\r')) {
                --length;
            }
            g_line[length] = '\0';
            return true;
        }

        if (length < MAX_LINE_LENGTH) {
            g_line[length++] = byte;
        } else {
            discarding = true;
        }
    }

    return false;
}

bool splitLine() {
    g_fieldCount = 0;
    g_fields[g_fieldCount++] = g_line;

    for (char* cursor = g_line; *cursor != '\0'; ++cursor) {
        if (*cursor == ',') {
            if (g_fieldCount == MAX_FIELDS) {
                return false;
            }
            *cursor = '\0';
            g_fields[g_fieldCount++] = cursor + 1;
        }
    }
    return true;
}

bool requestSample(unsigned requestId) {
    char command[32];
    std::snprintf(command, sizeof(command), "CMD,%u,READ\n", requestId);
    uart_puts(UART_ID, command);

    char idText[8];
    std::snprintf(idText, sizeof(idText), "%u", requestId);
    const absolute_time_t deadline = make_timeout_time_us(REPLY_TIMEOUT_US);

    while (!time_reached(deadline)) {
        if (!readLine(deadline)) {
            break;
        }
        if (!splitLine() || (g_fieldCount < 3)) {
            std::printf("Ignoring malformed UART reply\n");
            continue;
        }
        if (std::strcmp(g_fields[1], idText) != 0) {
            continue;
        }
        if (std::strcmp(g_fields[0], "ERR") == 0) {
            std::printf("Sensor request %u failed: %s\n", requestId, g_fields[2]);
            return false;
        }
        if ((std::strcmp(g_fields[0], "DATA") != 0) || (g_fieldCount != 7) ||
            ((std::strcmp(g_fields[4], "0") != 0) &&
             (std::strcmp(g_fields[4], "1") != 0))) {
            std::printf("Unexpected reply for request %u: %s\n", requestId, g_line);
            return false;
        }

        std::printf("Sensor reading: %s mV, valid=%s, sequence=%s, "
                    "uptime_us=%s, status=%s\n",
                    g_fields[5], g_fields[4], g_fields[2], g_fields[3], g_fields[6]);
        return true;
    }

    std::printf("Timed out waiting for sensor reply to request %u\n", requestId);
    return false;
}

}  // namespace

int main() {
    stdio_init_all();  // Results go to the USB serial console.

    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(UART_RX_PIN);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_fifo_enabled(UART_ID, true);

    sleep_ms(2000);  // Allow time for the USB serial console to connect.
    std::printf("Polling PicoLink on UART1 GP4/GP5 at %u baud\n", UART_BAUD);

    unsigned requestId = 1;
    while (true) {
        (void)requestSample(requestId);
        requestId = (requestId == 65535) ? 1 : requestId + 1;
        sleep_ms(POLL_INTERVAL_MS);
    }
}
