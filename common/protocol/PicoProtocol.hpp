#ifndef PICO_LINK_PROTOCOL_HPP
#define PICO_LINK_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>

namespace PicoProtocol {

constexpr std::size_t MAX_LINE_LENGTH = 256;
constexpr std::size_t MAX_ECHO_LENGTH = 180;

enum class RequestType { PING, ECHO, READ, STREAM };
enum class ReplyType { ACK, ERROR, DATA };
enum class Source { MOCK, REAL, UNKNOWN };

struct Request {
    RequestType type;
    std::uint16_t id;
    std::uint32_t periodMs;
    char echo[MAX_ECHO_LENGTH + 1];
};

struct Reply {
    ReplyType type;
    std::uint16_t id;
    char detail[MAX_ECHO_LENGTH + 1];
    std::uint32_t sampleSequence;
    std::uint64_t uptimeUs;
    bool valid;
    Source source;
    // GP28 analog IR voltage in millivolts vs 3.3 V (0..3300 for a real ADC).
    std::int32_t valueMilli;
    std::uint32_t ageMs;
    char status[32];
};

enum class ParseResult {
    OK,
    MALFORMED,
    OUT_OF_RANGE,
    UNKNOWN_TYPE,
};

using LineCallback = void (*)(void* context, const char* line);

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
bool formatEcho(char* output,
                std::size_t capacity,
                std::uint16_t id,
                const char* text);
bool formatRead(char* output, std::size_t capacity, std::uint16_t id);
bool formatStream(char* output,
                  std::size_t capacity,
                  std::uint16_t id,
                  std::uint32_t periodMs);
bool formatAck(char* output,
               std::size_t capacity,
               std::uint16_t id,
               const char* payload = nullptr);
bool formatError(char* output,
                 std::size_t capacity,
                 std::uint16_t id,
                 const char* code);
bool formatData(char* output,
                std::size_t capacity,
                std::uint16_t id,
                std::uint32_t sequence,
                std::uint64_t uptimeUs,
                bool valid,
                Source source,
                std::int32_t valueMilli,
                std::uint32_t ageMs,
                const char* status);

const char* sourceName(Source source);

}  // namespace PicoProtocol

#endif
