#include "../PicoProtocol.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

void collect(void* context, const char* line) {
    static_cast<std::vector<std::string>*>(context)->emplace_back(line);
}

void testLineFraming() {
    PicoProtocol::LineParser parser;
    std::vector<std::string> lines;
    const char* first = "CMD,1,PI";
    const char* second = "NG\r\nCMD,2,READ\n";
    parser.consume(reinterpret_cast<const std::uint8_t*>(first), std::strlen(first), collect, &lines);
    assert(lines.empty());
    parser.consume(reinterpret_cast<const std::uint8_t*>(second), std::strlen(second), collect, &lines);
    assert((lines == std::vector<std::string>{"CMD,1,PING", "CMD,2,READ"}));
}

void testOverflowRecovery() {
    PicoProtocol::LineParser parser;
    std::vector<std::string> lines;
    std::string input(PicoProtocol::MAX_LINE_LENGTH + 1, 'x');
    input += "\nCMD,7,PING\n";
    parser.consume(reinterpret_cast<const std::uint8_t*>(input.data()), input.size(), collect, &lines);
    assert(parser.overflowCount() == 1);
    assert((lines == std::vector<std::string>{"CMD,7,PING"}));
}

void testRequests() {
    PicoProtocol::Request request{};
    assert(PicoProtocol::parseRequest("CMD,1,PING", request) == PicoProtocol::ParseResult::OK);
    assert(request.type == PicoProtocol::RequestType::PING);
    assert(PicoProtocol::parseRequest("CMD,65535,ECHO,hello", request) ==
           PicoProtocol::ParseResult::OK);
    assert(std::strcmp(request.echo, "hello") == 0);
    assert(PicoProtocol::parseRequest("CMD,0,READ", request) ==
           PicoProtocol::ParseResult::OUT_OF_RANGE);
    assert(PicoProtocol::parseRequest("CMD,65536,READ", request) ==
           PicoProtocol::ParseResult::OUT_OF_RANGE);
    assert(PicoProtocol::parseRequest("CMD,2,STREAM,-1", request) ==
           PicoProtocol::ParseResult::OUT_OF_RANGE);
    assert(PicoProtocol::parseRequest("CMD,2,ECHO,a,b", request) !=
           PicoProtocol::ParseResult::OK);
}

void testReplies() {
    PicoProtocol::Reply reply{};
    assert(PicoProtocol::parseReply("ACK,3,OK,PONG", reply) == PicoProtocol::ParseResult::OK);
    assert(reply.type == PicoProtocol::ReplyType::ACK);
    assert(std::strcmp(reply.detail, "PONG") == 0);
    assert(PicoProtocol::parseReply("DATA,0,42,123456,1,MOCK,-250,3,OK", reply) ==
           PicoProtocol::ParseResult::OK);
    assert(reply.type == PicoProtocol::ReplyType::DATA);
    assert(reply.id == 0);
    assert(reply.sampleSequence == 42);
    assert(reply.uptimeUs == 123456);
    assert(reply.valid);
    assert(reply.source == PicoProtocol::Source::MOCK);
    assert(reply.valueMilli == -250);
    assert(reply.ageMs == 3);
    assert(PicoProtocol::parseReply("DATA,1,1,1,2,MOCK,1,0,OK", reply) ==
           PicoProtocol::ParseResult::OUT_OF_RANGE);
    assert(PicoProtocol::parseReply("ACK,0,OK", reply) == PicoProtocol::ParseResult::UNKNOWN_TYPE);
}

void testFormatting() {
    char output[PicoProtocol::MAX_LINE_LENGTH + 2];
    assert(PicoProtocol::formatEcho(output, sizeof(output), 9, "roundtrip"));
    assert(std::strcmp(output, "CMD,9,ECHO,roundtrip\n") == 0);
    assert(!PicoProtocol::formatEcho(output, sizeof(output), 9, "not,allowed"));
    assert(PicoProtocol::formatData(output,
                                    sizeof(output),
                                    4,
                                    5,
                                    6,
                                    true,
                                    PicoProtocol::Source::MOCK,
                                    7,
                                    8,
                                    "OK"));
    assert(std::strcmp(output, "DATA,4,5,6,1,MOCK,7,8,OK\n") == 0);
}

}  // namespace

int main() {
    testLineFraming();
    testOverflowRecovery();
    testRequests();
    testReplies();
    testFormatting();
    std::cout << "PicoProtocol tests passed\n";
    return 0;
}
