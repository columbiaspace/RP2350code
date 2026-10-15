#include "PicoProtocol.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace PicoProtocol {
namespace {

constexpr std::size_t MAX_FIELDS = 10;

std::size_t split(char* text, char* fields[MAX_FIELDS]) {
    std::size_t count = 0;
    if (*text == '\0') {
        return 0;
    }
    fields[count++] = text;
    for (char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == ',') {
            *cursor = '\0';
            if (count == MAX_FIELDS) {
                return MAX_FIELDS + 1;
            }
            fields[count++] = cursor + 1;
        }
    }
    return count;
}

bool parseUnsigned(const char* text, std::uint64_t maximum, std::uint64_t& value) {
    if ((text == nullptr) || (*text == '\0') || (*text == '-')) {
        return false;
    }
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if ((*cursor < '0') || (*cursor > '9')) {
            return false;
        }
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if ((errno == ERANGE) || (*end != '\0') || (parsed > maximum)) {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parseSigned32(const char* text, std::int32_t& value) {
    if ((text == nullptr) || (*text == '\0')) {
        return false;
    }
    const char* digits = (*text == '-') ? text + 1 : text;
    if (*digits == '\0') {
        return false;
    }
    for (const char* cursor = digits; *cursor != '\0'; ++cursor) {
        if ((*cursor < '0') || (*cursor > '9')) {
            return false;
        }
    }
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(text, &end, 10);
    if ((errno == ERANGE) || (*end != '\0') ||
        (parsed < std::numeric_limits<std::int32_t>::min()) ||
        (parsed > std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    value = static_cast<std::int32_t>(parsed);
    return true;
}

bool copyField(char* destination, std::size_t capacity, const char* source) {
    const std::size_t length = std::strlen(source);
    if ((length >= capacity) || (std::strchr(source, '\r') != nullptr) ||
        (std::strchr(source, '\n') != nullptr)) {
        return false;
    }
    std::memcpy(destination, source, length + 1);
    return true;
}

template <typename... Args>
bool format(char* output, std::size_t capacity, const char* pattern, Args... args) {
    if ((output == nullptr) || (capacity == 0)) {
        return false;
    }
    const int written = std::snprintf(output, capacity, pattern, args...);
    return (written > 0) && (static_cast<std::size_t>(written) < capacity) &&
           (written <= static_cast<int>(MAX_LINE_LENGTH + 1));
}

}  // namespace

LineParser::LineParser() {
    this->reset();
    this->m_overflowCount = 0;
}

void LineParser::reset() {
    this->m_length = 0;
    this->m_discarding = false;
}

void LineParser::consume(const std::uint8_t* data,
                         std::size_t size,
                         LineCallback callback,
                         void* context) {
    if ((data == nullptr) || (callback == nullptr)) {
        return;
    }
    for (std::size_t i = 0; i < size; ++i) {
        const char byte = static_cast<char>(data[i]);
        if (byte == '\n') {
            if (!this->m_discarding) {
                if ((this->m_length > 0) && (this->m_line[this->m_length - 1] == '\r')) {
                    --this->m_length;
                }
                this->m_line[this->m_length] = '\0';
                callback(context, this->m_line);
            }
            this->m_length = 0;
            this->m_discarding = false;
        } else if (!this->m_discarding) {
            if (this->m_length < MAX_LINE_LENGTH) {
                this->m_line[this->m_length++] = byte;
            } else if (byte != '\r') {
                this->m_discarding = true;
                ++this->m_overflowCount;
            }
        }
    }
}

std::uint32_t LineParser::overflowCount() const {
    return this->m_overflowCount;
}

ParseResult parseRequest(const char* line, Request& request) {
    if ((line == nullptr) || (std::strlen(line) > MAX_LINE_LENGTH)) {
        return ParseResult::MALFORMED;
    }
    char copy[MAX_LINE_LENGTH + 1];
    std::strcpy(copy, line);
    char* fields[MAX_FIELDS] = {};
    const std::size_t count = split(copy, fields);
    if ((count < 3) || (count > 4) || (std::strcmp(fields[0], "CMD") != 0)) {
        return ParseResult::MALFORMED;
    }

    std::uint64_t id = 0;
    if (!parseUnsigned(fields[1], UINT16_MAX, id) || (id == 0)) {
        return ParseResult::OUT_OF_RANGE;
    }

    request = {};
    request.id = static_cast<std::uint16_t>(id);
    if ((std::strcmp(fields[2], "PING") == 0) && (count == 3)) {
        request.type = RequestType::PING;
    } else if ((std::strcmp(fields[2], "READ") == 0) && (count == 3)) {
        request.type = RequestType::READ;
    } else if ((std::strcmp(fields[2], "ECHO") == 0) && (count == 4)) {
        if (!copyField(request.echo, sizeof(request.echo), fields[3])) {
            return ParseResult::OUT_OF_RANGE;
        }
        request.type = RequestType::ECHO;
    } else if ((std::strcmp(fields[2], "STREAM") == 0) && (count == 4)) {
        std::uint64_t period = 0;
        if (!parseUnsigned(fields[3], UINT32_MAX, period)) {
            return ParseResult::OUT_OF_RANGE;
        }
        request.type = RequestType::STREAM;
        request.periodMs = static_cast<std::uint32_t>(period);
    } else {
        return ParseResult::UNKNOWN_TYPE;
    }
    return ParseResult::OK;
}

ParseResult parseReply(const char* line, Reply& reply) {
    if ((line == nullptr) || (std::strlen(line) > MAX_LINE_LENGTH)) {
        return ParseResult::MALFORMED;
    }
    char copy[MAX_LINE_LENGTH + 1];
    std::strcpy(copy, line);
    char* fields[MAX_FIELDS] = {};
    const std::size_t count = split(copy, fields);
    if (count < 3) {
        return ParseResult::MALFORMED;
    }

    std::uint64_t id = 0;
    if (!parseUnsigned(fields[1], UINT16_MAX, id)) {
        return ParseResult::OUT_OF_RANGE;
    }
    reply = {};
    reply.id = static_cast<std::uint16_t>(id);

    if ((std::strcmp(fields[0], "ACK") == 0) && ((count == 3) || (count == 4)) &&
        (std::strcmp(fields[2], "OK") == 0) && (id != 0)) {
        reply.type = ReplyType::ACK;
        if ((count == 4) && !copyField(reply.detail, sizeof(reply.detail), fields[3])) {
            return ParseResult::OUT_OF_RANGE;
        }
        return ParseResult::OK;
    }
    if ((std::strcmp(fields[0], "ERR") == 0) && (count == 3) && (id != 0)) {
        reply.type = ReplyType::ERROR;
        return copyField(reply.detail, sizeof(reply.detail), fields[2])
                   ? ParseResult::OK
                   : ParseResult::OUT_OF_RANGE;
    }
    if ((std::strcmp(fields[0], "DATA") == 0) && (count == 9)) {
        std::uint64_t sequence = 0;
        std::uint64_t uptime = 0;
        std::uint64_t valid = 0;
        std::uint64_t age = 0;
        if (!parseUnsigned(fields[2], UINT32_MAX, sequence) ||
            !parseUnsigned(fields[3], UINT64_MAX, uptime) ||
            !parseUnsigned(fields[4], 1, valid) ||
            !parseSigned32(fields[6], reply.valueMilli) ||
            !parseUnsigned(fields[7], UINT32_MAX, age)) {
            return ParseResult::OUT_OF_RANGE;
        }
        if (std::strcmp(fields[5], "MOCK") == 0) {
            reply.source = Source::MOCK;
        } else if (std::strcmp(fields[5], "REAL") == 0) {
            reply.source = Source::REAL;
        } else {
            return ParseResult::OUT_OF_RANGE;
        }
        if (!copyField(reply.status, sizeof(reply.status), fields[8])) {
            return ParseResult::OUT_OF_RANGE;
        }
        reply.type = ReplyType::DATA;
        reply.sampleSequence = static_cast<std::uint32_t>(sequence);
        reply.uptimeUs = uptime;
        reply.valid = (valid != 0);
        reply.ageMs = static_cast<std::uint32_t>(age);
        return ParseResult::OK;
    }
    return ParseResult::UNKNOWN_TYPE;
}

bool formatPing(char* output, std::size_t capacity, std::uint16_t id) {
    return (id != 0) && format(output, capacity, "CMD,%u,PING\n", id);
}

bool formatEcho(char* output, std::size_t capacity, std::uint16_t id, const char* text) {
    if ((id == 0) || (text == nullptr) || (std::strlen(text) > MAX_ECHO_LENGTH) ||
        (std::strpbrk(text, ",\r\n") != nullptr)) {
        return false;
    }
    return format(output, capacity, "CMD,%u,ECHO,%s\n", id, text);
}

bool formatRead(char* output, std::size_t capacity, std::uint16_t id) {
    return (id != 0) && format(output, capacity, "CMD,%u,READ\n", id);
}

bool formatStream(char* output,
                  std::size_t capacity,
                  std::uint16_t id,
                  std::uint32_t periodMs) {
    return (id != 0) && format(output, capacity, "CMD,%u,STREAM,%" PRIu32 "\n", id, periodMs);
}

bool formatAck(char* output,
               std::size_t capacity,
               std::uint16_t id,
               const char* payload) {
    if ((id == 0) || ((payload != nullptr) && (std::strpbrk(payload, ",\r\n") != nullptr))) {
        return false;
    }
    return (payload == nullptr) || (*payload == '\0')
               ? format(output, capacity, "ACK,%u,OK\n", id)
               : format(output, capacity, "ACK,%u,OK,%s\n", id, payload);
}

bool formatError(char* output,
                 std::size_t capacity,
                 std::uint16_t id,
                 const char* code) {
    if ((id == 0) || (code == nullptr) || (*code == '\0') ||
        (std::strpbrk(code, ",\r\n") != nullptr)) {
        return false;
    }
    return format(output, capacity, "ERR,%u,%s\n", id, code);
}

bool formatData(char* output,
                std::size_t capacity,
                std::uint16_t id,
                std::uint32_t sequence,
                std::uint64_t uptimeUs,
                bool valid,
                Source source,
                std::int32_t valueMilli,
                std::uint32_t ageMs,
                const char* status) {
    if ((status == nullptr) || (*status == '\0') ||
        (std::strpbrk(status, ",\r\n") != nullptr) || (source == Source::UNKNOWN)) {
        return false;
    }
    return format(output,
                  capacity,
                  "DATA,%u,%" PRIu32 ",%" PRIu64 ",%u,%s,%" PRId32 ",%" PRIu32 ",%s\n",
                  id,
                  sequence,
                  uptimeUs,
                  valid ? 1U : 0U,
                  sourceName(source),
                  valueMilli,
                  ageMs,
                  status);
}

const char* sourceName(Source source) {
    switch (source) {
        case Source::MOCK:
            return "MOCK";
        case Source::REAL:
            return "REAL";
        default:
            return "UNKNOWN";
    }
}

}  // namespace PicoProtocol
