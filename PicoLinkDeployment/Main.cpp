// Linux entry point for F Prime flight software (runs on the Pi 5).
//
//   -a / -p  GDS TCP (browser ground station)
//   -d / -b  Pico UART device and baud (must be 115200)

#include <PicoLinkDeployment/Top/PicoLinkDeploymentTopology.hpp>
#include <Os/Os.hpp>
#include <Fw/Logger/Logger.hpp>

#include <cstdlib>
#include <getopt.h>
#include <signal.h>

void print_usage(const char* app) {
    Fw::Logger::log(
        "Usage: %s -a <GDS bind address> -p <GDS port> -d <UART device> [-b 115200]\n"
        "  -a  GDS TCP bind address (not the Pico UART)\n"
        "  -p  GDS TCP port\n"
        "  -d  Pico UART, e.g. /dev/ttyAMA0 or /dev/serial/by-id/...\n"
        "  -b  baud; must be 115200\n",
        app);
}

static void signalHandler(int) {
    PicoLinkDeployment::stopRateGroups();
}

int main(int argc, char* argv[]) {
    CHAR* hostname = nullptr;
    U16 port_number = 0;
    CHAR* uart_device = nullptr;
    U32 uart_baud = 115200;

    Os::init();

    I32 option = 0;
    while ((option = getopt(argc, argv, "hp:a:d:b:")) != -1) {
        switch (option) {
            case 'a':
                hostname = optarg;
                break;
            case 'p':
                port_number = static_cast<U16>(atoi(optarg));
                break;
            case 'd':
                uart_device = optarg;
                break;
            case 'b':
                uart_baud = static_cast<U32>(strtoul(optarg, nullptr, 10));
                break;
            case 'h':
            case '?':
            default:
                print_usage(argv[0]);
                return (option == 'h') ? 0 : 1;
        }
    }

    PicoLinkDeployment::TopologyState inputs;
    inputs.hostname = hostname;
    inputs.port = port_number;
    inputs.uartDevice = uart_device;
    inputs.uartBaud = uart_baud;
    if (uart_baud != 115200) {
        Fw::Logger::log("Unsupported Pico UART baud %lu; use 115200\n",
                        static_cast<unsigned long>(uart_baud));
        return 1;
    }
    if (uart_device == nullptr) {
        Fw::Logger::log("No Pico UART device supplied\n");
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    Fw::Logger::log("Hit Ctrl-C to quit\n");

    PicoLinkDeployment::setupTopology(inputs);
    PicoLinkDeployment::startRateGroups();
    PicoLinkDeployment::teardownTopology(inputs);
    Fw::Logger::log("Exiting...\n");
    return 0;
}
