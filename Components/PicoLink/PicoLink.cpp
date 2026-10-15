#include "PicoLink.hpp"

#include <Fw/Log/LogString.hpp>

#include <cstring>

namespace PicoLink {

namespace {
constexpr U8 REQUEST_TIMEOUT_TICKS = 3;
}

PicoLink::PicoLink(const char* componentName)
    : PicoLinkComponentBase(componentName),
      m_nextRequestId(1),
      m_pending(false),
      m_pendingId(0),
      m_pendingOpCode(0),
      m_pendingCmdSeq(0),
      m_expectedReply(ExpectedReply::NONE),
      m_timeoutTicks(0),
      m_txBytes(0),
      m_rxBytes(0),
      m_parseErrors(0),
      m_timeouts(0),
      m_sensorErrors(0) {
    this->m_expectedDetail[0] = '\0';
    this->tlmWrite_LinkStatus(0);
    this->tlmWrite_SampleValid(0);
    this->tlmWrite_SampleSource(0);
}

void PicoLink::byteStreamDriverReady_handler(FwIndexType portNum) {
    (void)portNum;
    this->tlmWrite_LinkStatus(1);
    this->log_ACTIVITY_HI_DriverReady();
}

void PicoLink::fromByteStreamDriver_handler(FwIndexType portNum,
                                            Fw::Buffer& buffer,
                                            const Drv::ByteStreamStatus& status) {
    if ((status == Drv::ByteStreamStatus::OP_OK) && buffer.isValid()) {
        this->m_mutex.lock();
        this->m_rxBytes += static_cast<U32>(buffer.getSize());
        this->m_mutex.unlock();
        const U32 overflowsBefore = this->m_parser.overflowCount();
        this->m_parser.consume(buffer.getData(),
                               static_cast<std::size_t>(buffer.getSize()),
                               &PicoLink::lineCallback,
                               this);
        const U32 overflowDelta = this->m_parser.overflowCount() - overflowsBefore;
        if (overflowDelta > 0) {
            this->m_mutex.lock();
            this->m_parseErrors += overflowDelta;
            this->m_mutex.unlock();
            Fw::LogStringArg detail("overlong line discarded");
            this->log_WARNING_LO_ProtocolError(detail);
        }
    } else {
        this->log_WARNING_HI_DriverError();
    }

    // PosixUartDriver allocated this buffer. Never retain it after this call.
    this->fromByteStreamDriverReturn_out(portNum, buffer);
}

void PicoLink::tick_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;
    bool timedOut = false;
    U16 requestId = 0;
    FwOpcodeType opCode = 0;
    U32 cmdSeq = 0;

    this->m_mutex.lock();
    if (this->m_pending && (this->m_timeoutTicks > 0)) {
        --this->m_timeoutTicks;
        if (this->m_timeoutTicks == 0) {
            timedOut = true;
            requestId = this->m_pendingId;
            opCode = this->m_pendingOpCode;
            cmdSeq = this->m_pendingCmdSeq;
            this->m_pending = false;
            this->m_expectedReply = ExpectedReply::NONE;
            ++this->m_timeouts;
        }
    }
    this->m_mutex.unlock();

    if (timedOut) {
        this->log_WARNING_HI_RequestTimeout(requestId);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
    this->writeCounters();
}

void PicoLink::PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    char frame[PicoProtocol::MAX_LINE_LENGTH + 2];
    if (!PicoProtocol::formatPing(frame, sizeof(frame), this->m_nextRequestId) ||
        !this->startRequest(opCode, cmdSeq, ExpectedReply::ACK, "PING", "PONG", frame)) {
        return;
    }
}

void PicoLink::ECHO_cmdHandler(FwOpcodeType opCode,
                               U32 cmdSeq,
                               const Fw::CmdStringArg& value) {
    char frame[PicoProtocol::MAX_LINE_LENGTH + 2];
    if (!PicoProtocol::formatEcho(frame, sizeof(frame), this->m_nextRequestId, value.toChar())) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    (void)this->startRequest(opCode, cmdSeq, ExpectedReply::ACK, "ECHO", value.toChar(), frame);
}

void PicoLink::READ_SENSOR_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    char frame[PicoProtocol::MAX_LINE_LENGTH + 2];
    if (!PicoProtocol::formatRead(frame, sizeof(frame), this->m_nextRequestId) ||
        !this->startRequest(opCode, cmdSeq, ExpectedReply::DATA, "READ", "", frame)) {
        return;
    }
}

void PicoLink::START_STREAM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 periodMs) {
    if ((periodMs < 10) || (periodMs > 60000)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    char frame[PicoProtocol::MAX_LINE_LENGTH + 2];
    if (!PicoProtocol::formatStream(frame, sizeof(frame), this->m_nextRequestId, periodMs) ||
        !this->startRequest(opCode, cmdSeq, ExpectedReply::ACK, "STREAM", "", frame)) {
        return;
    }
}

void PicoLink::STOP_STREAM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    char frame[PicoProtocol::MAX_LINE_LENGTH + 2];
    if (!PicoProtocol::formatStream(frame, sizeof(frame), this->m_nextRequestId, 0) ||
        !this->startRequest(opCode, cmdSeq, ExpectedReply::ACK, "STOP_STREAM", "", frame)) {
        return;
    }
}

bool PicoLink::startRequest(FwOpcodeType opCode,
                            U32 cmdSeq,
                            ExpectedReply expected,
                            const char* requestName,
                            const char* expectedDetail,
                            const char* frame) {
    this->m_mutex.lock();
    if (this->m_pending) {
        this->m_mutex.unlock();
        this->log_WARNING_LO_RequestBusy();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return false;
    }
    const U16 requestId = this->m_nextRequestId;
    this->m_pending = true;
    this->m_pendingId = requestId;
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->m_expectedReply = expected;
    std::strncpy(this->m_expectedDetail, expectedDetail, sizeof(this->m_expectedDetail) - 1);
    this->m_expectedDetail[sizeof(this->m_expectedDetail) - 1] = '\0';
    this->m_timeoutTicks = REQUEST_TIMEOUT_TICKS;
    ++this->m_nextRequestId;
    if (this->m_nextRequestId == 0) {
        this->m_nextRequestId = 1;
    }
    this->m_mutex.unlock();

    Fw::Buffer sendBuffer(reinterpret_cast<U8*>(const_cast<char*>(frame)),
                          static_cast<FwSizeType>(std::strlen(frame)));
    const Drv::ByteStreamStatus sendStatus = this->toByteStreamDriver_out(0, sendBuffer);
    if (sendStatus != Drv::ByteStreamStatus::OP_OK) {
        this->m_mutex.lock();
        this->m_pending = false;
        this->m_expectedReply = ExpectedReply::NONE;
        this->m_mutex.unlock();
        this->log_WARNING_HI_DriverError();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return false;
    }

    this->m_mutex.lock();
    this->m_txBytes += static_cast<U32>(sendBuffer.getSize());
    this->m_mutex.unlock();
    Fw::LogStringArg requestArg(requestName);
    this->log_ACTIVITY_LO_RequestSent(requestId, requestArg);
    return true;
}

void PicoLink::lineCallback(void* context, const char* line) {
    static_cast<PicoLink*>(context)->handleLine(line);
}

void PicoLink::handleLine(const char* line) {
    PicoProtocol::Reply reply{};
    if (PicoProtocol::parseReply(line, reply) != PicoProtocol::ParseResult::OK) {
        this->m_mutex.lock();
        ++this->m_parseErrors;
        this->m_mutex.unlock();
        Fw::LogStringArg detail("malformed or out-of-range reply");
        this->log_WARNING_LO_ProtocolError(detail);
        return;
    }

    if ((reply.type == PicoProtocol::ReplyType::DATA) && (reply.id == 0)) {
        this->handleData(reply);
        return;
    }

    FwOpcodeType opCode = 0;
    U32 cmdSeq = 0;
    ExpectedReply expected = ExpectedReply::NONE;
    char expectedDetail[PicoProtocol::MAX_ECHO_LENGTH + 1] = {};
    bool matches = false;
    this->m_mutex.lock();
    if (this->m_pending && (reply.id == this->m_pendingId)) {
        matches = true;
        opCode = this->m_pendingOpCode;
        cmdSeq = this->m_pendingCmdSeq;
        expected = this->m_expectedReply;
        std::strcpy(expectedDetail, this->m_expectedDetail);
        this->m_pending = false;
        this->m_expectedReply = ExpectedReply::NONE;
    }
    this->m_mutex.unlock();

    if (!matches) {
        this->m_mutex.lock();
        ++this->m_parseErrors;
        this->m_mutex.unlock();
        Fw::LogStringArg detail("unexpected request ID");
        this->log_WARNING_LO_ProtocolError(detail);
        return;
    }

    bool successful = false;
    const char* replyName = "ERROR";
    if ((reply.type == PicoProtocol::ReplyType::ACK) && (expected == ExpectedReply::ACK) &&
        (std::strcmp(reply.detail, expectedDetail) == 0)) {
        successful = true;
        replyName = "ACK";
    } else if ((reply.type == PicoProtocol::ReplyType::DATA) &&
               (expected == ExpectedReply::DATA)) {
        this->handleData(reply);
        successful = reply.valid;
        replyName = "DATA";
    } else if (reply.type != PicoProtocol::ReplyType::ERROR) {
        this->m_mutex.lock();
        ++this->m_parseErrors;
        this->m_mutex.unlock();
        Fw::LogStringArg detail("reply type does not match request");
        this->log_WARNING_LO_ProtocolError(detail);
    }

    if (successful) {
        this->tlmWrite_LinkStatus(2);
    }
    Fw::LogStringArg replyArg(replyName);
    this->log_ACTIVITY_LO_ReplyReceived(reply.id, replyArg);
    this->cmdResponse_out(opCode,
                          cmdSeq,
                          successful ? Fw::CmdResponse::OK
                                     : Fw::CmdResponse::EXECUTION_ERROR);
}

void PicoLink::handleData(const PicoProtocol::Reply& reply) {
    this->tlmWrite_SampleSequence(reply.sampleSequence);
    this->tlmWrite_PicoUptimeUs(reply.uptimeUs);
    this->tlmWrite_SampleValid(reply.valid ? 1 : 0);
    this->tlmWrite_SampleAgeMs(reply.ageMs);
    this->tlmWrite_SampleSource(reply.source == PicoProtocol::Source::MOCK
                                   ? 1
                                   : (reply.source == PicoProtocol::Source::REAL ? 2 : 0));
    if (reply.valid) {
        this->tlmWrite_IrMilliVolts(reply.valueMilli);
    } else {
        this->m_mutex.lock();
        ++this->m_sensorErrors;
        this->m_mutex.unlock();
        Fw::LogStringArg status(reply.status);
        this->log_WARNING_LO_SensorInvalid(reply.sampleSequence, status);
    }
}

void PicoLink::writeCounters() {
    this->m_mutex.lock();
    const U32 txBytes = this->m_txBytes;
    const U32 rxBytes = this->m_rxBytes;
    const U32 parseErrors = this->m_parseErrors;
    const U32 timeouts = this->m_timeouts;
    const U32 sensorErrors = this->m_sensorErrors;
    this->m_mutex.unlock();
    this->tlmWrite_TxBytes(txBytes);
    this->tlmWrite_RxBytes(rxBytes);
    this->tlmWrite_ParseErrors(parseErrors);
    this->tlmWrite_Timeouts(timeouts);
    this->tlmWrite_SensorErrors(sensorErrors);
}

}  // namespace PicoLink
