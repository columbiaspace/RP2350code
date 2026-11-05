#ifndef PICO_LINK_PROTOCOL_HPP
#define PICO_LINK_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>

// ASCII UART protocol shared by Pico firmware and F Prime on the Pi.
// One command or reply per line, comma-separated, ended by '\n'.
// Full grammar: docs/protocol.md
namespace PicoProtocol {

constexpr std::size_t MAX_LINE_LENGTH = 256;
constexpr std::size_t MAX_ECHO_LENGTH = 180;

enum class RequestType { PING, ECHO, READ, STREAM };
enum class ReplyType { ACK, ERROR, DATA };

struct Request {
    RequestType type;
    std::uint16_t id;       // 1..65535; 0 is reserved for unsolicited DATA
    std::uint32_t periodMs; // STREAM only
    char echo[MAX_ECHO_LENGTH + 1];
};

struct Reply {
    ReplyType type;
    std::uint16_t id;
    char detail[MAX_ECHO_LENGTH + 1];  // ACK payload or ERR code
    std::uint32_t sampleSequence;
    std::uint64_t uptimeUs;
    bool valid;
    std::int32_t valueMilli;  // GP28 millivolts
    char status[32];
};

enum class ParseResult { OK, MALFORMED, OUT_OF_RANGE, UNKNOWN_TYPE };

using LineCallback = void (*)(void* context, const char* line);

// Collects UART bytes until a newline, then calls the callback.
class LineParser {
  public:
    LineParser();
    void reset();
    void consume(const std::uint8_t* data,
                 std::size_t size,
                 LineCallback callback,
                 void* context);
    std::uint32_t overflowCount() const;

  private:
    char m_line[MAX_LINE_LENGTH + 1];
    std::size_t m_length;
    bool m_discarding;
    std::uint32_t m_overflowCount;
};

ParseResult parseRequest(const char* line, Request& request);
ParseResult parseReply(const char* line, Reply& reply);

bool formatPing(char* output, std::size_t capacity, std::uint16_t id);
bool formatEcho(char* output, std::size_t capacity, std::uint16_t id, const char* text);
bool formatRead(char* output, std::size_t capacity, std::uint16_t id);
bool formatStream(char* output, std::size_t capacity, std::uint16_t id, std::uint32_t periodMs);
bool formatAck(char* output, std::size_t capacity, std::uint16_t id, const char* payload = nullptr);
bool formatError(char* output, std::size_t capacity, std::uint16_t id, const char* code);
bool formatData(char* output,
                std::size_t capacity,
                std::uint16_t id,
                std::uint32_t sequence,
                std::uint64_t uptimeUs,
                bool valid,
                std::int32_t valueMilli,
                const char* status);

}  // namespace PicoProtocol

#endif
