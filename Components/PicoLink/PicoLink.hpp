#ifndef COMPONENTS_PICOLINK_HPP
#define COMPONENTS_PICOLINK_HPP

#include <Components/PicoLink/PicoLinkComponentAc.hpp>
#include <Os/Mutex.hpp>

#include <common/protocol/PicoProtocol.hpp>

namespace PicoLink {

class PicoLink final : public PicoLinkComponentBase {
  public:
    explicit PicoLink(const char* componentName);
    ~PicoLink() override = default;

  private:
    enum class ExpectedReply { NONE, ACK, DATA };

    void byteStreamDriverReady_handler(FwIndexType portNum) override;
    void fromByteStreamDriver_handler(FwIndexType portNum,
                                      Fw::Buffer& buffer,
                                      const Drv::ByteStreamStatus& status) override;
    void tick_handler(FwIndexType portNum, U32 context) override;

    void PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void ECHO_cmdHandler(FwOpcodeType opCode,
                         U32 cmdSeq,
                         const Fw::CmdStringArg& value) override;
    void READ_SENSOR_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void START_STREAM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 periodMs) override;
    void STOP_STREAM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    bool startRequest(FwOpcodeType opCode,
                      U32 cmdSeq,
                      ExpectedReply expected,
                      const char* requestName,
                      const char* expectedDetail,
                      const char* frame);
    void handleLine(const char* line);
    void handleData(const PicoProtocol::Reply& reply);
    void writeCounters();
    static void lineCallback(void* context, const char* line);

    Os::Mutex m_mutex;
    PicoProtocol::LineParser m_parser;
    U16 m_nextRequestId;
    bool m_pending;
    U16 m_pendingId;
    FwOpcodeType m_pendingOpCode;
    U32 m_pendingCmdSeq;
    ExpectedReply m_expectedReply;
    char m_expectedDetail[PicoProtocol::MAX_ECHO_LENGTH + 1];
    U8 m_timeoutTicks;
    U32 m_txBytes;
    U32 m_rxBytes;
    U32 m_parseErrors;
    U32 m_timeouts;
    U32 m_sensorErrors;
};

}  // namespace PicoLink

#endif
