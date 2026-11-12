// Second-Pico smoke tester: the same checks as tools/pico_smoke.py, in plain C.
// No FreeRTOS. Results go to USB serial; the protocol goes out UART0 GP0/GP1.
//
// Every 3 seconds it does this:
//   1. Loopback check: send "LOOPBACK,..." and see if the same line comes back.
//      With a jumper from GP0 to GP1, it does -> "PASS LOOPBACK", then stop.
//   2. Otherwise run PING / ECHO / READ / STREAM against the PicoLink firmware.
//      (The firmware ignores the LOOPBACK line, so step 1 is harmless.)
//
// Onboard LED: on = last run passed, off = last run failed.

#include <stdio.h>
#include <string.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"

#define UART_ID uart1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define UART_BAUD 115200

#define REPLY_TIMEOUT_US 2000000u    // 2 s per reply line, like pico_smoke.py
#define LOOPBACK_TIMEOUT_US 100000u  // 100 ms; a jumper echoes instantly
#define MAX_LINE_LENGTH 256
#define MAX_FIELDS 8

static const char LOOPBACK_LINE[] = "LOOPBACK,pico-tester-123";

static char g_line[MAX_LINE_LENGTH + 1];
static char* g_fields[MAX_FIELDS];
static int g_field_count;

// Read one '\n'-terminated line into g_line, CR stripped. Overlong lines are dropped.
static bool read_line(uint32_t timeout_us) {
    size_t length = 0;
    const absolute_time_t deadline = make_timeout_time_us(timeout_us);

    while (!time_reached(deadline)) {
        if (!uart_is_readable(UART_ID)) {
            continue;
        }

        const char c = (char)uart_getc(UART_ID);

        if (c == '\n') {
            if ((length > 0) && (g_line[length - 1] == '\r')) {
                --length;
            }

            g_line[length] = '\0';
            return true;
        }

        if (length < MAX_LINE_LENGTH) {
            g_line[length++] = c;
        } else {
            length = 0;  // Too long: discard and keep listening.
        }
    }

    return false;
}

static void split_line(void) {
    g_field_count = 0;
    g_fields[g_field_count++] = g_line;

    for (char* p = g_line; *p != '\0'; ++p) {
        if ((*p == ',') && (g_field_count < MAX_FIELDS)) {
            *p = '\0';
            g_fields[g_field_count++] = p + 1;
        }
    }
}

static void print_fields(void) {
    for (int i = 0; i < g_field_count; ++i) {
        printf(i == 0 ? "%s" : ",%s", g_fields[i]);
    }

    printf("\n");
}

static void drain_rx(void) {
    while (uart_is_readable(UART_ID)) {
        (void)uart_getc(UART_ID);
    }
}

static void send_line(const char* text) {
    uart_puts(UART_ID, text);
    uart_tx_wait_blocking(UART_ID);
}

// Wait for a reply of kind ("ACK" or "DATA") with this id.
// id 0 with "DATA" means any unsolicited stream sample.
// Mirrors expect() in pico_smoke.py.
static bool expect(unsigned id, const char* kind) {
    char id_text[8];
    snprintf(id_text, sizeof(id_text), "%u", id);

    const bool want_data = (strcmp(kind, "DATA") == 0);

    while (true) {
        if (!read_line(REPLY_TIMEOUT_US)) {
            printf("FAIL: timed out waiting for a UART line\n");
            return false;
        }

        split_line();

        if (g_field_count < 3) {
            printf("FAIL: malformed line: ");
            print_fields();
            return false;
        }

        if ((strcmp(g_fields[0], "DATA") == 0) &&
            (strcmp(g_fields[1], "0") == 0) && !want_data) {
            continue;  // Leftover stream sample.
        }

        if ((strcmp(g_fields[1], id_text) != 0) &&
            !(want_data && (id == 0))) {
            continue;  // Reply to some other request.
        }

        if (strcmp(g_fields[0], "ERR") == 0) {
            printf("FAIL: Pico ERR: %s\n", g_fields[2]);
            return false;
        }

        if (strcmp(g_fields[0], kind) != 0) {
            printf("FAIL: expected %s, got %s\n", kind, g_fields[0]);
            return false;
        }

        if (want_data && (g_field_count != 7)) {
            printf("FAIL: bad DATA: ");
            print_fields();
            return false;
        }

        return true;
    }
}

// True if the last reply was ACK,<id>,OK,<payload>.
static bool ack_payload_is(const char* payload) {
    return (g_field_count == 4) &&
           (strcmp(g_fields[2], "OK") == 0) &&
           (strcmp(g_fields[3], payload) == 0);
}

static bool run_smoke(void) {
    send_line("CMD,1,PING\n");

    if (!expect(1, "ACK")) return false;

    if (!ack_payload_is("PONG")) {
        printf("FAIL: bad PING: ");
        print_fields();
        return false;
    }

    printf("PASS PING\n");

    send_line("CMD,2,ECHO,smoke-test\n");

    if (!expect(2, "ACK")) return false;

    if (!ack_payload_is("smoke-test")) {
        printf("FAIL: bad ECHO: ");
        print_fields();
        return false;
    }

    printf("PASS ECHO\n");

    send_line("CMD,3,READ\n");

    if (!expect(3, "DATA")) return false;

    printf("PASS READ: seq=%s uptime_us=%s valid=%s ir_millivolts=%s status=%s\n",
           g_fields[2], g_fields[3], g_fields[4], g_fields[5], g_fields[6]);

    send_line("CMD,4,STREAM,100\n");

    if (!expect(4, "ACK")) return false;
    if (!expect(0, "DATA")) return false;

    printf("PASS STREAM: ir_millivolts=%s\n", g_fields[5]);

    send_line("CMD,5,STREAM,0\n");

    if (!expect(5, "ACK")) return false;

    printf("PASS STOP_STREAM\n");
    printf("All PicoLink smoke checks passed\n");

    return true;
}

// Returns true if what we send comes straight back (GP0 jumpered to GP1).
static bool loopback_detected(void) {
    drain_rx();
    send_line(LOOPBACK_LINE);
    send_line("\n");

    return read_line(LOOPBACK_TIMEOUT_US) &&
           (strcmp(g_line, LOOPBACK_LINE) == 0);
}

static void set_led(bool on) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#else
    (void)on;  // e.g. Pico W: LED is on the wireless chip, not a GPIO.
#endif
}

int main(void) {
    stdio_init_all();  // USB serial for results.

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(UART_RX_PIN);  // Idle-high when nothing is wired, so no junk bytes.

    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_fifo_enabled(UART_ID, true);

    sleep_ms(2000);  // Give the PC time to open the USB serial port.

    unsigned run = 0;

    while (true) {
        printf("\n--- run %u: UART0 TX=GP0 RX=GP1 %u 8N1 ---\n",
               ++run, UART_BAUD);

        bool passed;

        if (loopback_detected()) {
            printf("PASS LOOPBACK: GP0 -> GP1 jumper echoed the line\n");
            printf("Now wire to the PicoLink Pico: TX GP0->its GP1, RX GP1<-its GP0, GND-GND\n");
            passed = true;
        } else {
            drain_rx();
            passed = run_smoke();

            if (!passed) {
                printf("Hints: loopback = jumper GP0 to GP1. Pico link = cross TX/RX, "
                       "share GND, firmware Pico powered and flashed.\n");
            }
        }

        set_led(passed);
        sleep_ms(3000);
    }
}
