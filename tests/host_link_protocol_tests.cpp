#include "host_link_protocol.h"
#include "host_link_client.h"
#include "host_link_baud.h"
#include "can_host_client.h"
#include "f13_boot_profile.h"
#include "f13_boot_update.h"
#include "f13_transfer_profile.h"
#include "firmware_update_profile.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

using firmware_upgrade::host_link::Command;
using firmware_upgrade::host_link::DecodeResult;
using firmware_upgrade::host_link::Frame;
using firmware_upgrade::host_link::FrameStreamParser;
using firmware_upgrade::host_link::FrameType;
using firmware_upgrade::host_link::Status;
using firmware_upgrade::host_link::Transport;

int g_failures = 0;

void Check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        ++g_failures;
    }
}

void WriteLe16(std::uint8_t* data, std::uint16_t value) {
    data[0] = static_cast<std::uint8_t>(value & 0xFFU);
    data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

Frame MakePing(std::uint16_t sequence,
               const std::vector<std::uint8_t>& payload) {
    Frame frame;
    frame.type = FrameType::Request;
    frame.sequence = sequence;
    frame.command = static_cast<std::uint8_t>(Command::Ping);
    frame.status = Status::Ok;
    frame.payload = payload;
    return frame;
}

std::vector<std::uint8_t> Encode(const Frame& frame) {
    std::vector<std::uint8_t> encoded;
    Check(firmware_upgrade::host_link::EncodeFrame(frame, &encoded), "encode frame");
    return encoded;
}

void PutLe16(std::vector<std::uint8_t>* output, std::uint16_t value) {
    output->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
}

void PutLe32(std::vector<std::uint8_t>* output, std::uint32_t value) {
    output->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
}

std::uint32_t AsciiWord(const char* text) {
    std::uint32_t value = 0U;
    for (unsigned index = 0U; index < 4U && text[index] != '\0'; ++index) {
        value |= static_cast<std::uint32_t>(
                     static_cast<unsigned char>(text[index])) <<
                 (index * 8U);
    }
    return value;
}

class FakeTransport final : public Transport {
public:
    bool IsOpen() const override {
        return open;
    }

    bool WriteAll(const std::uint8_t* data,
                  std::size_t length,
                  std::uint32_t,
                  std::wstring*) override {
        writes.emplace_back(data, data + length);
        if (on_write) {
            on_write(writes.back(), writes.size());
        }
        return true;
    }

    bool ReadSome(std::vector<std::uint8_t>* output,
                  std::size_t,
                  std::uint32_t,
                  std::wstring* error) override {
        if (reads.empty()) {
            if (error != nullptr) {
                *error = L"Read timeout.";
            }
            return false;
        }
        *output = std::move(reads.front());
        reads.pop_front();
        return true;
    }

    void Queue(const Frame& frame) {
        reads.push_back(Encode(frame));
    }

    bool open = true;
    std::vector<std::vector<std::uint8_t>> writes;
    std::deque<std::vector<std::uint8_t>> reads;
    std::function<void(const std::vector<std::uint8_t>&, std::size_t)> on_write;
};

Frame DecodeRequest(const std::vector<std::uint8_t>& encoded) {
    Frame request;
    Check(firmware_upgrade::host_link::DecodeFrame(
              encoded.data(), encoded.size(), &request) == DecodeResult::Ok,
          "fake transport decodes request");
    return request;
}

Frame MakeResponse(const Frame& request,
                   Status status,
                   const std::vector<std::uint8_t>& payload) {
    Frame response;
    response.type = FrameType::Response;
    response.sequence = request.sequence;
    response.command = request.command;
    response.status = status;
    response.payload = payload;
    return response;
}

void RefreshHeaderCrc(std::vector<std::uint8_t>* encoded) {
    const std::uint16_t crc = firmware_upgrade::host_link::Crc16CcittFalse(
        &(*encoded)[2], 8U);
    WriteLe16(&(*encoded)[10], crc);
}

void TestLinkBaudNegotiation() {
    using namespace firmware_upgrade::host_link;
    for (const auto desired:{460800U,921600U}) for (unsigned fault=0;fault<=6;++fault) {
        FakeTransport transport; Client client; client.SetTransport(&transport);
        std::uint32_t pc=115200U, peer=115200U;
        unsigned sets=0, confirms=0; bool changed=false;
        transport.on_write=[&](const std::vector<std::uint8_t>& bytes,std::size_t) {
            const auto q=DecodeRequest(bytes);
            Check(q.command>=5U && q.command<=7U,"baud flow never emits bus/flash commands");
            if (q.command==HL_BAUD_INFO) {
                std::vector<std::uint8_t> p={1U,1U,0U,0U};
                PutLe32(&p,115200U); PutLe32(&p,peer); PutLe32(&p,7U);
                PutLe32(&p,peer==115200U?114942U:peer==460800U?454545U:909090U);
                if (fault==5U) p[0]=99U;
                transport.Queue(MakeResponse(q,Status::Ok,p));
            } else if (q.command==HL_BAUD_SET) {
                ++sets; Check(pc==115200U && peer==115200U,"SET ACK at original speed");
                if (fault==1U) transport.Queue(MakeResponse(q,Status::Busy,{}));
                else if (fault!=3U) {
                    auto echo=q.payload; if (fault==2U) echo[0]^=1U;
                    transport.Queue(MakeResponse(q,Status::Ok,echo)); peer=desired;
                }
            } else if (q.command==HL_BAUD_CONFIRM) {
                ++confirms; Check(changed && pc==desired && peer==desired,"confirm only at new speed");
                if (fault!=4U) transport.Queue(MakeResponse(q,Status::Ok,q.payload));
            }
        };
        LinkBaudInfo info; std::wstring error;
        const bool ok=SwitchLinkBaud(client,desired,[&](std::uint32_t n,std::wstring*) {
            if (fault==6U) return false;
            changed=true; pc=n; return true;
        },&info,&error);
        Check(ok==(fault==0U),"baud switch success and failure matrix");
        Check(sets<=1U && confirms<=1U,"side-effecting switch/confirm never retried at wrong baud");
        if (!fault) Check(info.current==desired && changed,"verified nominal baud");
        if (fault==1U || fault==2U || fault==3U || fault==5U) Check(!changed,"failed SET never changes PC baud");
    }
}

void TestReconnectSequenceCollision() {
    using namespace firmware_upgrade::host_link;
    for (const unsigned rejects:{1U,2U}) {
        FakeTransport transport; Client client; client.SetTransport(&transport);
        unsigned calls=0;
        transport.on_write=[&](const std::vector<std::uint8_t>& bytes,std::size_t) {
            const auto q=DecodeRequest(bytes); ++calls;
            Check(q.command==static_cast<std::uint8_t>(Command::GetHostInfo),"collision recovery is identity-only");
            if (calls<=rejects) transport.Queue(MakeResponse(q,Status::SequenceError,{}));
            else {
                std::vector<std::uint8_t> p={1U,1U,1U,4U,0U,2U,2U,0U};
                PutLe32(&p,20261015U); PutLe32(&p,HL_BAUD_FEATURE);
                transport.Queue(MakeResponse(q,Status::Ok,p));
            }
        };
        HostInfo info; std::wstring error;
        Check(client.GetHostInfo(&info,&error)==(rejects==1U),"lost CONFIRM ACK reconnect sequence recovery");
        Check(calls==2U && DecodeRequest(transport.writes[0]).sequence!=DecodeRequest(transport.writes[1]).sequence,
            "fresh identity sequence recovery bounded to one attempt");
    }
}

void TestCrcKnownVector() {
    const std::string input = "123456789";
    const auto crc = firmware_upgrade::host_link::Crc16CcittFalse(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    Check(crc == 0x29B1U, "CRC-16/CCITT-FALSE known vector");
    Check(firmware_upgrade::host_link::Crc16CcittFalse(nullptr, 0U) == 0xFFFFU,
          "empty payload CRC");
}

void TestRoundTripAndLimits() {
    Frame decoded;
    const auto empty_encoded = Encode(MakePing(0x0001U, {}));
    Check(empty_encoded.size() == 14U, "empty frame size");
    Check(empty_encoded[12] == 0xFFU && empty_encoded[13] == 0xFFU,
          "empty payload CRC bytes");
    Check(firmware_upgrade::host_link::DecodeFrame(
              empty_encoded.data(), empty_encoded.size(), &decoded) ==
              DecodeResult::Ok && decoded.payload.empty(),
          "empty payload round trip");

    std::vector<std::uint8_t> payload(128U);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<std::uint8_t>(index);
    }

    const Frame original = MakePing(0x1234U, payload);
    const auto encoded = Encode(original);
    Check(encoded.size() == firmware_upgrade::host_link::kMaximumFrameSize,
          "maximum encoded size");
    Check(firmware_upgrade::host_link::DecodeFrame(
              encoded.data(), encoded.size(), &decoded) == DecodeResult::Ok,
          "maximum frame decodes");
    Check(decoded.sequence == original.sequence &&
              decoded.command == original.command &&
              decoded.payload == original.payload,
          "round-trip fields");
    for (std::size_t split=0; split<=encoded.size(); ++split) {
        FrameStreamParser split_parser; Frame reconstructed;
        split_parser.Push(encoded.data(),split);
        split_parser.Push(encoded.data()+split,encoded.size()-split);
        Check(split_parser.TryPop(&reconstructed) && reconstructed.payload==payload,
              "128-byte payload reassembles at every UART frame split boundary");
    }

    Frame too_large = MakePing(1U, std::vector<std::uint8_t>(129U, 0xA5U));
    std::vector<std::uint8_t> rejected{0xFFU};
    Check(!firmware_upgrade::host_link::EncodeFrame(too_large, &rejected),
          "129-byte encode rejected");
    Check(rejected.empty(), "failed encode clears destination");
}

void TestDecodeFailures() {
    Frame decoded;
    auto encoded = Encode(MakePing(7U, {0x10U, 0x20U, 0x30U}));

    auto wrong_version = encoded;
    wrong_version[2] = 0x01U;
    RefreshHeaderCrc(&wrong_version);
    Check(firmware_upgrade::host_link::DecodeFrame(
              wrong_version.data(), wrong_version.size(), &decoded) ==
              DecodeResult::UnsupportedVersion,
          "wrong version rejected");

    auto wrong_type = encoded;
    wrong_type[3] = 0x40U;
    RefreshHeaderCrc(&wrong_type);
    Check(firmware_upgrade::host_link::DecodeFrame(
              wrong_type.data(), wrong_type.size(), &decoded) ==
              DecodeResult::InvalidFrameType,
          "wrong type rejected");

    auto wrong_header_crc = encoded;
    wrong_header_crc[10] ^= 0x01U;
    Check(firmware_upgrade::host_link::DecodeFrame(
              wrong_header_crc.data(), wrong_header_crc.size(), &decoded) ==
              DecodeResult::HeaderCrcError,
          "header CRC rejected");

    auto wrong_payload_crc = encoded;
    wrong_payload_crc.back() ^= 0x01U;
    Check(firmware_upgrade::host_link::DecodeFrame(
              wrong_payload_crc.data(), wrong_payload_crc.size(), &decoded) ==
              DecodeResult::PayloadCrcError,
          "payload CRC rejected");

    auto wrong_length = encoded;
    WriteLe16(&wrong_length[8], 129U);
    RefreshHeaderCrc(&wrong_length);
    Check(firmware_upgrade::host_link::DecodeFrame(
              wrong_length.data(), wrong_length.size(), &decoded) ==
              DecodeResult::InvalidLength,
          "wire length129 rejected after valid header CRC");

    Frame invalid_status = MakePing(1U, {});
    invalid_status.status = Status::Busy;
    std::vector<std::uint8_t> output;
    Check(!firmware_upgrade::host_link::EncodeFrame(invalid_status, &output),
          "request status must be zero");
}

void TestSplitAndConcatenatedFrames() {
    const auto first = Encode(MakePing(1U, {0x01U}));
    const auto second = Encode(MakePing(2U, {0x02U, 0x03U}));
    FrameStreamParser parser;
    Frame decoded;

    for (std::size_t index = 0; index + 1U < first.size(); ++index) {
        parser.Push(&first[index], 1U);
        Check(!parser.TryPop(&decoded), "split frame waits for completion");
    }
    parser.Push(&first.back(), 1U);
    Check(parser.TryPop(&decoded) && decoded.sequence == 1U,
          "split frame completes");

    std::vector<std::uint8_t> joined = first;
    joined.insert(joined.end(), second.begin(), second.end());
    parser.Push(joined.data(), joined.size());
    Check(parser.TryPop(&decoded) && decoded.sequence == 1U,
          "first concatenated frame");
    Check(parser.TryPop(&decoded) && decoded.sequence == 2U,
          "second concatenated frame");
    Check(!parser.TryPop(&decoded), "concatenated buffer drained");
}

void TestNoiseOverlapAndRecovery() {
    const auto valid = Encode(MakePing(0x55AAU, {0x55U, 0xAAU}));
    auto bad = valid;
    bad.back() ^= 0x80U;

    std::vector<std::uint8_t> stream{0x00U, 0x55U, 0x55U};
    stream.insert(stream.end(), bad.begin() + 1, bad.end());
    stream.push_back(0x7EU);
    stream.insert(stream.end(), valid.begin(), valid.end());

    FrameStreamParser parser;
    Frame decoded;
    parser.Push(stream.data(), stream.size());
    Check(parser.TryPop(&decoded), "parser recovers after noise and bad CRC");
    Check(decoded.sequence == 0x55AAU, "recovered frame is valid frame");
    Check(parser.Stats().discarded_bytes != 0U,
          "noise discard is counted");
    Check(parser.Stats().invalid_payloads == 1U,
          "bad payload CRC is counted");
}

void TestBoundedInputBuffer() {
    FrameStreamParser parser;
    Frame decoded;
    std::vector<std::uint8_t> noise(
        firmware_upgrade::host_link::kStreamBufferCapacity + 37U, 0x11U);

    parser.Push(noise.data(), noise.size());
    Check(parser.BufferedSize() ==
              firmware_upgrade::host_link::kStreamBufferCapacity,
          "stream input buffer is bounded");
    Check(parser.Stats().input_overflow_bytes == 37U,
          "stream input overflow is counted");
    Check(!parser.TryPop(&decoded), "noise produces no frame");
}

void TestClientProbeAndPing() {
    firmware_upgrade::host_link::Client client;
    firmware_upgrade::host_link::HostInfo info;
    firmware_upgrade::host_link::Capabilities capabilities;
    firmware_upgrade::host_link::HostStats stats;
    FakeTransport transport;
    std::wstring error;

    std::uint16_t advertised_payload=128U;
    bool telemetry=false;
    transport.on_write = [&transport,&advertised_payload,&telemetry](const std::vector<std::uint8_t>& raw,
                                       std::size_t) {
        const Frame request = DecodeRequest(raw);
        std::vector<std::uint8_t> payload;

        if (request.command == static_cast<std::uint8_t>(Command::GetHostInfo)) {
            PutLe16(&payload, 0x0101U);
            payload.push_back(1U);
            payload.push_back(2U);
            payload.push_back(3U);
            payload.push_back(2U);
            payload.push_back(2U);
            payload.push_back(0U);
            PutLe32(&payload, 42U);
            PutLe32(&payload, 0x0000000FU);
        } else if (request.command ==
                   static_cast<std::uint8_t>(Command::GetCapabilities)) {
            PutLe32(&payload, 0x0000000FU);
            PutLe16(&payload, advertised_payload);
            payload.push_back(64U);
            payload.push_back(1U);
            payload.push_back(1U);
            payload.push_back(4U);
            payload.push_back(1U);
            payload.push_back(0U);
            PutLe32(&payload, 0x00000004U);
            PutLe32(&payload, 0x00000002U);
            PutLe32(&payload, 0U);
        } else if (request.command ==
                   static_cast<std::uint8_t>(Command::GetStats)) {
            for (std::uint32_t value = 1U; value <= (telemetry?21U:14U); ++value) {
                PutLe32(&payload, value);
            }
        } else {
            payload = request.payload;
        }
        transport.Queue(MakeResponse(request, Status::Ok, payload));
    };

    client.SetTransport(&transport);
    Check(client.Probe(0x0101U, &info, &capabilities, &error),
          "client host probe");
    Check(client.IsHostValidated(), "client records validated host");
    Check(info.board_id == 0x0101U && info.firmware_build == 42U,
          "client parses host info");
    Check(capabilities.maximum_host_link_payload == 128U &&
              capabilities.maximum_can_data_bytes == 64U &&
              capabilities.filter_slots == 1U,
          "client parses capabilities");
    Check(client.Ping({0x10U, 0x20U, 0x30U}, &error),
          "client ping validates echo");
    Check(client.GetStats(&stats, &error) &&
              stats.uart_rx_bytes == 1U && stats.can_rx_frames == 6U &&
              stats.can_bus_off_entries == 12U &&
              stats.host_command_errors == 14U,
          "client parses all F24 statistics counters");

    Check(transport.writes.size() == 4U,
          "probe, ping, and statistics request count");
    if (transport.writes.size() == 4U) {
        Check(DecodeRequest(transport.writes[0]).sequence == 1U &&
                  DecodeRequest(transport.writes[1]).sequence == 2U &&
                  DecodeRequest(transport.writes[2]).sequence == 3U &&
                  DecodeRequest(transport.writes[3]).sequence == 4U,
              "client sequences start non-zero and advance");
    }
    Check(client.Ping(std::vector<std::uint8_t>(128U,0xA5),&error),"128-byte client PING round trip");
    telemetry=true;
    Check(client.GetStats(&stats,&error) && stats.telemetry_available && stats.uart_rx_high_water==15U &&
        stats.process_max_ms==21U,"84-byte host telemetry extension parsed without changing counter order");
    telemetry=false;
    Check(client.GetStats(&stats,&error) && !stats.telemetry_available,"fixed Boot statistics never inherit App peaks");
    advertised_payload=96U; client.BeginSession();
    Check(!client.Probe(0x0101U,&info,&capabilities,&error) && !client.IsHostValidated(),
          "old96-byte host explicitly rejected");
}

void TestClientRetryMatchingAndEvents() {
    firmware_upgrade::host_link::Client client;
    firmware_upgrade::host_link::ClientOptions options;
    firmware_upgrade::host_link::Response response;
    FakeTransport transport;
    std::vector<firmware_upgrade::host_link::TraceRecord> trace;
    std::wstring error;

    options.response_timeout_ms = 1U;
    options.maximum_attempts = 2U;
    client.SetOptions(options);
    client.SetTraceCallback(
        [&trace](const firmware_upgrade::host_link::TraceRecord& record) {
            trace.push_back(record);
        });
    transport.on_write = [&transport](const std::vector<std::uint8_t>& raw,
                                       std::size_t write_count) {
        if (write_count == 2U) {
            const Frame request = DecodeRequest(raw);
            Frame event;
            Frame wrong = MakeResponse(request, Status::Ok, {0xEEU});

            event.type = FrameType::Event;
            event.sequence = 0U;
            event.command = static_cast<std::uint8_t>(Command::CanStateEvent);
            event.status = Status::Ok;
            event.payload = {0x01U};
            wrong.sequence = static_cast<std::uint16_t>(request.sequence + 1U);
            transport.Queue(event);
            transport.Queue(wrong);
            transport.Queue(MakeResponse(request, Status::Ok, {0xA5U}));
        }
    };

    client.SetTransport(&transport);
    Check(client.Request(Command::GetStats, {}, &response, &error),
          "client retries after timeout");
    Check(response.payload == std::vector<std::uint8_t>{0xA5U},
          "client accepts matching response only");
    Check(transport.writes.size() == 2U &&
              transport.writes[0] == transport.writes[1],
          "retry is byte-identical");
    Check(client.Stats().retries == 1U &&
              client.Stats().response_timeouts == 1U &&
              client.Stats().unmatched_frames == 1U,
          "client retry and matching counters");
    const auto trace_count = [&trace](firmware_upgrade::host_link::TraceKind kind) {
        return static_cast<unsigned>(std::count_if(
            trace.begin(), trace.end(),
            [kind](const firmware_upgrade::host_link::TraceRecord& record) {
                return record.kind == kind;
            }));
    };
    Check(trace_count(firmware_upgrade::host_link::TraceKind::Request) == 2U &&
              trace_count(firmware_upgrade::host_link::TraceKind::Retry) == 1U &&
              trace_count(firmware_upgrade::host_link::TraceKind::Timeout) == 1U &&
              trace_count(firmware_upgrade::host_link::TraceKind::Response) == 1U &&
              trace_count(firmware_upgrade::host_link::TraceKind::Event) == 1U &&
              trace_count(firmware_upgrade::host_link::TraceKind::Unmatched) == 1U,
          "client diagnostic trace records directions and retry outcomes");

    Frame event;
    Check(client.TryPopEvent(&event) &&
              event.command == static_cast<std::uint8_t>(Command::CanStateEvent),
          "client queues asynchronous event");
}

void TestClientNackIsNotRetried() {
    firmware_upgrade::host_link::Client client;
    firmware_upgrade::host_link::ClientOptions options;
    firmware_upgrade::host_link::Response response;
    FakeTransport transport;
    std::wstring error;

    options.maximum_attempts = 3U;
    client.SetOptions(options);
    transport.on_write = [&transport](const std::vector<std::uint8_t>& raw,
                                       std::size_t) {
        const Frame request = DecodeRequest(raw);
        transport.Queue(MakeResponse(request, Status::BadState, {}));
    };
    client.SetTransport(&transport);

    Check(!client.Request(Command::CanStart, {}, &response, &error),
          "client reports host NACK");
    Check(response.status == Status::BadState,
          "client preserves NACK status");
    Check(transport.writes.size() == 1U,
          "host NACK is not retried");
}

void TestClientCancellation() {
    firmware_upgrade::host_link::Client client;
    firmware_upgrade::host_link::Response response;
    FakeTransport transport;
    bool cancel_requested = false;
    std::wstring error;

    client.SetTransport(&transport);
    client.SetCancelCheck(
        [&cancel_requested]() { return cancel_requested; });
    transport.on_write = [&cancel_requested](
                             const std::vector<std::uint8_t>&,
                             std::size_t) {
        cancel_requested = true;
    };

    Check(!client.Request(Command::Ping, {}, &response, &error),
          "client cancellation interrupts response wait");
    Check(transport.writes.size() == 1U,
          "client cancellation does not retry");
    Check(client.Stats().response_timeouts == 0U,
          "client cancellation is not counted as a timeout");
    Check(error == L"Host Link request canceled.",
          "client cancellation returns a specific error");
}

void TestClientResetAndBackgroundEvents() {
    firmware_upgrade::host_link::Client client;
    firmware_upgrade::host_link::ClientOptions options;
    FakeTransport transport;
    std::wstring error;

    options.maximum_pending_events = 1U;
    client.SetOptions(options);
    transport.on_write = [&transport](const std::vector<std::uint8_t>& raw,
                                      std::size_t) {
        const Frame request = DecodeRequest(raw);
        transport.Queue(MakeResponse(request, Status::Ok, {}));
    };
    client.SetTransport(&transport);
    Check(client.ResetHost(&error), "client RESET_HOST ACK");
    Check(transport.writes.size() == 1U &&
              DecodeRequest(transport.writes.front()).command ==
                  static_cast<std::uint8_t>(Command::ResetHost),
          "RESET_HOST is sent exactly once");

    Frame event;
    event.type = FrameType::Event;
    event.sequence = 0U;
    event.command = static_cast<std::uint8_t>(Command::HostErrorEvent);
    event.status = Status::Ok;
    event.payload = {
        0x80U, 0x00U, 0x00U, 0x00U,
        0x34U, 0x12U, 0x00U, 0x00U,
        0x03U, 0x00U, 0x00U, 0x00U,
    };
    transport.Queue(event);
    event.command = static_cast<std::uint8_t>(Command::CanStateEvent);
    transport.Queue(event);

    Check(client.PollEvents(1U, &error),
          "background Host Link event poll");
    Check(client.PollEvents(1U, &error),
          "second background Host Link event poll");
    Check(client.Stats().received_events == 2U &&
              client.Stats().dropped_events == 1U,
          "background event queue is bounded and counted");
    Check(client.TryPopEvent(&event) &&
              event.command ==
                  static_cast<std::uint8_t>(Command::HostErrorEvent) &&
              event.payload.size() == 12U,
          "background diagnostic event is available to worker");
    Check(client.PollEvents(1U, &error) && error.empty(),
          "background read timeout is an idle condition");
}

void TestCanHostClientCommandsAndValidation() {
    firmware_upgrade::host_link::Client host_link_client;
    firmware_upgrade::can_host::Client can_client;
    firmware_upgrade::can_host::Configuration configuration;
    firmware_upgrade::can_host::Filter filter;
    firmware_upgrade::can_host::Frame tx_frame;
    firmware_upgrade::can_host::Frame rx_frame;
    firmware_upgrade::can_host::ReceiveMatch match;
    firmware_upgrade::can_host::TransmitResult tx_result;
    FakeTransport transport;
    std::wstring error;
    std::uint32_t controller_state = 0U;
    bool saw_config = false;
    bool saw_filter = false;
    bool saw_tx = false;
    bool saw_wait_rx = false;
    std::vector<firmware_upgrade::can_host::TraceDirection> can_trace;

    transport.on_write = [&transport, &saw_config, &saw_filter,
                          &saw_tx, &saw_wait_rx](
                             const std::vector<std::uint8_t>& raw,
                             std::size_t) {
        const Frame request = DecodeRequest(raw);
        std::vector<std::uint8_t> payload;

        if (request.command ==
            static_cast<std::uint8_t>(Command::SetCanConfig)) {
            saw_config = request.payload ==
                std::vector<std::uint8_t>({0U, 2U, 0xFFU, 0U});
            payload = request.payload;
        } else if (request.command ==
                   static_cast<std::uint8_t>(Command::SetCanFilter)) {
            saw_filter = request.payload.size() == 12U &&
                         request.payload[0] == 0U &&
                         request.payload[1] == 1U &&
                         request.payload[4] == 0x01U &&
                         request.payload[5] == 0x07U &&
                         request.payload[8] == 0xFFU &&
                         request.payload[9] == 0x07U;
            payload = request.payload;
        } else if (request.command ==
                   static_cast<std::uint8_t>(Command::CanStart) ||
                   request.command ==
                   static_cast<std::uint8_t>(Command::CanStop)) {
            PutLe32(&payload, 0x00000080U);
        } else if (request.command ==
                   static_cast<std::uint8_t>(Command::CanTx)) {
            saw_tx = request.payload.size() == 14U &&
                     request.payload[0] == 0U &&
                     request.payload[2] == 2U &&
                     request.payload[3] == 2U &&
                     request.payload[4] == 0x00U &&
                     request.payload[5] == 0x07U &&
                     request.payload[12] == 0x11U &&
                     request.payload[13] == 0x22U;
            PutLe32(&payload, 0x00001234U);
            PutLe32(&payload, 2U);
            PutLe16(&payload, 0x0700U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0020U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x2211U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0700U);
            PutLe16(&payload, 0x0000U);
            payload.push_back(0x20U);
            payload.push_back(0x00U);
            PutLe16(&payload, 0x2211U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0000U);
            PutLe16(&payload, 0x0000U);
        } else if (request.command ==
                   static_cast<std::uint8_t>(Command::CanTxWaitRx)) {
            saw_wait_rx = request.payload.size() == 30U &&
                          request.payload[0] == 0xF4U &&
                          request.payload[1] == 0x01U &&
                          request.payload[2] == 1U &&
                          request.payload[3] == 2U &&
                          request.payload[4] == 0x01U &&
                          request.payload[5] == 0x07U &&
                          request.payload[8] == 0xFFU &&
                          request.payload[9] == 0x07U;
            payload.push_back(0U);
            payload.push_back(0U);
            payload.push_back(2U);
            payload.push_back(2U);
            PutLe32(&payload, 0x701U);
            PutLe32(&payload, 0x1234U);
            payload.push_back(0xAAU);
            payload.push_back(0x55U);
        }
        transport.Queue(MakeResponse(request, Status::Ok, payload));
    };

    host_link_client.SetTransport(&transport);
    can_client.SetHostLinkClient(&host_link_client);
    can_client.SetTraceCallback(
        [&can_trace](firmware_upgrade::can_host::TraceDirection direction,
                     const firmware_upgrade::can_host::Frame&) {
            can_trace.push_back(direction);
        });

    Check(can_client.SetConfiguration(configuration, &error),
          "CAN client configuration");
    Check(can_client.SetFilter(0U, filter, &error),
          "CAN client exact filter");
    Check(can_client.Start(&controller_state, &error) &&
              controller_state == 0x80U,
          "CAN client start state");

    tx_frame.id = 0x700U;
    tx_frame.dlc = 2U;
    tx_frame.data = {0x11U, 0x22U};
    Check(can_client.Transmit(tx_frame, &tx_result, &error) &&
              tx_result.completion_timestamp == 0x1234U &&
              tx_result.controller_result == 2U &&
              tx_result.diagnostic_available &&
              tx_result.input_head[0] == 0x0700U &&
              tx_result.input_data[0] == 0x2211U &&
              tx_result.ram_id_low == 0x0700U &&
              tx_result.ram_dlc_register == 0x20U &&
              tx_result.ram_data[0] == 0x2211U,
          "CAN client transmit result");

    match.timeout_ms = 500U;
    match.retry_count = 1U;
    match.expected_dlc = 2U;
    match.id = 0x701U;
    match.id_mask = 0x7FFU;
    match.flags_value = 0U;
    match.flags_mask = 0x01U;
    Check(can_client.TransmitWaitReceive(tx_frame, match, &rx_frame, &error),
          "CAN client TX-wait-RX");
    Check(rx_frame.id == 0x701U && rx_frame.timestamp == 0x1234U &&
              rx_frame.data == std::vector<std::uint8_t>({0xAAU, 0x55U}),
          "CAN client decodes received record");
    Check(can_client.Stop(&controller_state, &error) &&
              controller_state == 0x80U,
          "CAN client stop state");
    Check(saw_config && saw_filter && saw_tx && saw_wait_rx,
          "CAN client wire payloads");
    Check(can_trace.size() == 3U &&
              can_trace[0] == firmware_upgrade::can_host::TraceDirection::Transmit &&
              can_trace[1] == firmware_upgrade::can_host::TraceDirection::Transmit &&
              can_trace[2] == firmware_upgrade::can_host::TraceDirection::Receive,
          "CAN client diagnostic trace reports TX and RX frames");

    const std::size_t writes_before_invalid = transport.writes.size();
    filter.mask = 0U;
    Check(!can_client.SetFilter(0U, filter, &error),
          "CAN client rejects zero-mask filter");
    tx_frame.bit_rate_switch = true;
    Check(!can_client.Transmit(tx_frame, &tx_result, &error),
          "CAN client rejects BRS without FD");
    Check(transport.writes.size() == writes_before_invalid,
          "invalid CAN input is rejected before UART write");
}

void TestCanBitratePresets() {
    using namespace firmware_upgrade::can_host;
    firmware_upgrade::host_link::Client link;
    Client can;
    FakeTransport transport;
    std::wstring error;
    transport.on_write = [&transport](const std::vector<std::uint8_t>& raw, std::size_t) {
        const auto request = DecodeRequest(raw);
        Check(request.command == static_cast<std::uint8_t>(Command::SetCanConfig),
              "bitrate switch uses UART SET_CAN_CONFIG, not CAN_TX");
        transport.Queue(MakeResponse(request, Status::Ok, request.payload));
    };
    link.SetTransport(&transport);
    can.SetHostLinkClient(&link);
    firmware_upgrade::host_link::Capabilities caps;
    caps.feature_flags = 3U;
    caps.nominal_bitrate_mask = kSupportedNominalMask;
    caps.data_bitrate_mask = 2U;
    for (const auto preset : kNominalPresets) {
        std::uint8_t parsed = 0U;
        Check(NominalPresetFromText(NominalPresetName(preset), &parsed) && parsed == preset,
              "nominal name/preset mapping including 1M=4");
        for (const auto mode : {Mode::Classical, Mode::CanFd}) {
            Configuration config;
            config.mode = mode;
            config.nominal_bitrate_preset = preset;
            config.data_bitrate_preset = mode == Mode::Classical ? 0xFFU : 1U;
            Check(IsConfigurationSupported(config, caps, &error) && can.SetConfiguration(config, &error),
                  "250K/500K/1M supported in Classical/FD by UART");
            Check(DecodeRequest(transport.writes.back()).payload == std::vector<std::uint8_t>(
                      {static_cast<std::uint8_t>(mode), preset, config.data_bitrate_preset, 0U}),
                  "exact bitrate preset UART payload");
        }
    }
    const auto writes_before = transport.writes.size();
    for (const auto preset : {0U, 3U, 5U, 255U}) {
        Configuration invalid;
        invalid.nominal_bitrate_preset = static_cast<std::uint8_t>(preset);
        Check(!can.SetConfiguration(invalid, &error), "unsupported bitrate rejected before UART");
    }
    Configuration config;
    caps.nominal_bitrate_mask = 4U;
    config.nominal_bitrate_preset = 4U;
    Check(!IsConfigurationSupported(config, caps, &error), "old 500K-only host rejects 1M locally");
    config.nominal_bitrate_preset = 2U;
    Check(IsConfigurationSupported(config, caps, &error), "old host keeps 500K support");
    config.mode = Mode::CanFd;
    config.data_bitrate_preset = 1U;
    caps.data_bitrate_mask = 0U;
    Check(!IsConfigurationSupported(config, caps, &error), "missing FD data capability rejected");
    Check(!NominalPresetFromText(L"800K", &config.nominal_bitrate_preset), "unknown UI preset is not 500K fallback");
    Check(transport.writes.size() == writes_before, "invalid configuration has zero UART side effects");
}

void TestBridgeClockErrorAndLegacyTxResponse() {
    firmware_upgrade::host_link::Client link_client;
    firmware_upgrade::can_host::Client can_client;
    firmware_upgrade::can_host::Configuration configuration;
    firmware_upgrade::can_host::Frame tx_frame;
    firmware_upgrade::can_host::TransmitResult result;
    FakeTransport transport;
    std::wstring error;

    transport.on_write = [&transport](const std::vector<std::uint8_t>& raw,
                                     std::size_t) {
        const Frame request = DecodeRequest(raw);
        if (request.command == static_cast<std::uint8_t>(Command::SetCanConfig)) {
            transport.Queue(MakeResponse(request, Status::CanError,
                {0xC1U, 2U, 0U, 0U, 0U, 0U, 0x40U, 0U}));
        } else {
            std::vector<std::uint8_t> payload;
            PutLe32(&payload, 123U);
            PutLe32(&payload, 2U);
            transport.Queue(MakeResponse(request, Status::Ok, payload));
        }
    };
    link_client.SetTransport(&transport);
    can_client.SetHostLinkClient(&link_client);
    Check(!can_client.SetConfiguration(configuration, &error) &&
              error.find(L"X1 stabilization") != std::wstring::npos &&
              transport.writes.size() == 1U,
          "bridge clock error identifies oscillator stage without retries");
    tx_frame.id = 0x123U;
    tx_frame.dlc = 0U;
    result.diagnostic_available = true;
    Check(can_client.Transmit(tx_frame, &result, &error) &&
              result.completion_timestamp == 123U &&
              result.controller_result == 2U && !result.diagnostic_available,
          "legacy 8-byte CAN_TX response clears previous diagnostics");
}

firmware_upgrade::can_host::Frame MakeF13TransferResponse(
    std::uint8_t sequence,
    firmware_upgrade::f13_transfer::Command command,
    const std::array<std::uint8_t, 3>& payload) {
    firmware_upgrade::can_host::Frame frame;
    frame.id = firmware_upgrade::f13_transfer::kResponseId;
    frame.dlc = 8U;
    frame.data = {
        0x5AU,
        firmware_upgrade::f13_transfer::kProtocolMajor,
        sequence,
        static_cast<std::uint8_t>(command),
        payload[0], payload[1], payload[2], 0U,
    };
    frame.data[7] = firmware_upgrade::f13_transfer::Crc8Atm(
        frame.data.data(), 7U);
    return frame;
}

void TestF13TransferProfile() {
    using firmware_upgrade::f13_transfer::Command;
    using firmware_upgrade::f13_transfer::DeviceInfoAssembler;
    using firmware_upgrade::f13_transfer::ErrorStatus;
    using firmware_upgrade::f13_transfer::Response;

    const std::string crc_input = "123456789";
    Check(firmware_upgrade::f13_transfer::Crc8Atm(
              reinterpret_cast<const std::uint8_t*>(crc_input.data()),
              crc_input.size()) == 0xF4U,
          "F13 Transfer CRC-8/ATM known vector");

    firmware_upgrade::can_host::Frame request;
    std::wstring error;
    Check(firmware_upgrade::f13_transfer::BuildRequest(
              1U, Command::Echo, {0x12U, 0x34U, 0x56U}, &request, &error),
          "F13 Transfer request encode");
    Check(request.id == 0x710U && request.dlc == 8U &&
              request.data.size() == 8U && request.data[0] == 0xA5U &&
              request.data[1] == 1U && request.data[2] == 1U &&
              request.data[3] == 3U && request.data[4] == 0x12U &&
              request.data[5] == 0x34U && request.data[6] == 0x56U &&
              request.data[7] == firmware_upgrade::f13_transfer::Crc8Atm(
                                     request.data.data(), 7U),
          "F13 Transfer request fields");
    Check(!firmware_upgrade::f13_transfer::BuildRequest(
              0U, Command::Ping, {}, &request, &error),
          "F13 Transfer rejects sequence zero");
    Check(!firmware_upgrade::f13_transfer::BuildRequest(
              1U, Command::ErrorResponse, {}, &request, &error),
          "F13 Transfer rejects response command in request");

    Response parsed;
    auto response = MakeF13TransferResponse(
        7U, Command::Echo, {0xAAU, 0x55U, 0x11U});
    Check(firmware_upgrade::f13_transfer::ParseResponse(
              7U, Command::Echo, response, &parsed, &error) &&
              parsed.payload == std::array<std::uint8_t, 3>{
                                    0xAAU, 0x55U, 0x11U} &&
              parsed.error_status == ErrorStatus::None,
          "F13 Transfer response decode");

    auto invalid = response;
    invalid.id = 0x701U;
    Check(!firmware_upgrade::f13_transfer::ParseResponse(
              7U, Command::Echo, invalid, &parsed, &error),
          "F13 Transfer rejects wrong response ID");
    invalid = response;
    invalid.data[7] ^= 1U;
    Check(!firmware_upgrade::f13_transfer::ParseResponse(
              7U, Command::Echo, invalid, &parsed, &error),
          "F13 Transfer rejects bad response CRC");
    Check(!firmware_upgrade::f13_transfer::ParseResponse(
              8U, Command::Echo, response, &parsed, &error),
          "F13 Transfer rejects wrong response sequence");

    auto error_response = MakeF13TransferResponse(
        9U, Command::ErrorResponse,
        {static_cast<std::uint8_t>(Command::PatternTest),
         static_cast<std::uint8_t>(ErrorStatus::BadArgument), 0x22U});
    Check(firmware_upgrade::f13_transfer::ParseResponse(
              9U, Command::PatternTest, error_response, &parsed, &error) &&
              parsed.error_status == ErrorStatus::BadArgument &&
              parsed.error_detail == 0x22U,
          "F13 Transfer error response decode");

    DeviceInfoAssembler info;
    const std::array<std::array<std::uint8_t, 3>, 7> pages{{
        {{0U, 1U, 0U}},
        {{1U, 0x34U, 0x12U}},
        {{2U, 'R', 'A'}},
        {{3U, '6', 'M'}},
        {{4U, '3', 'C'}},
        {{5U, 'A', 'N'}},
        {{6U, 0x3FU, 0U}},
    }};
    for (std::size_t index = 0U; index < pages.size(); ++index) {
        const auto page_frame = MakeF13TransferResponse(
            static_cast<std::uint8_t>(index + 1U), Command::DeviceInfo,
            pages[index]);
        Check(firmware_upgrade::f13_transfer::ParseResponse(
                  static_cast<std::uint8_t>(index + 1U), Command::DeviceInfo,
                  page_frame, &parsed, &error) &&
                  info.AddPage(parsed, &error),
              "F13 Transfer DEVICE_INFO page");
    }
    Check(info.IsComplete() && info.Value().protocol_major == 1U &&
              info.Value().protocol_minor == 0U &&
              info.Value().target_id == 0x1234U &&
              info.Value().part_number == "RA6M3CAN" &&
              info.Value().capabilities == 0x003FU,
          "Custom Transfer accepts a contract-compatible MCU identity");

    DeviceInfoAssembler invalid_info;
    Response invalid_identity;
    const auto invalid_identity_frame = MakeF13TransferResponse(
        1U, Command::DeviceInfo, {1U, 0U, 0U});
    Check(firmware_upgrade::f13_transfer::ParseResponse(
              1U, Command::DeviceInfo, invalid_identity_frame,
              &invalid_identity, &error) &&
              !invalid_info.AddPage(invalid_identity, &error),
          "Custom Transfer rejects a reserved target identity");

    DeviceInfoAssembler invalid_label_info;
    Response invalid_label;
    const auto invalid_label_frame = MakeF13TransferResponse(
        2U, Command::DeviceInfo, {2U, 0x01U, 'A'});
    Check(firmware_upgrade::f13_transfer::ParseResponse(
              2U, Command::DeviceInfo, invalid_label_frame,
              &invalid_label, &error) &&
              !invalid_label_info.AddPage(invalid_label, &error),
          "Custom Transfer rejects a non-ASCII device label");
}

std::vector<std::uint8_t> EncodeCanRecord(
    const firmware_upgrade::can_host::Frame& frame) {
    std::vector<std::uint8_t> encoded;
    encoded.push_back(0U);
    encoded.push_back(0U);
    encoded.push_back(frame.dlc);
    encoded.push_back(static_cast<std::uint8_t>(frame.data.size()));
    PutLe32(&encoded, frame.id);
    PutLe32(&encoded, frame.timestamp);
    encoded.insert(encoded.end(), frame.data.begin(), frame.data.end());
    return encoded;
}

void WriteLe16At(std::vector<std::uint8_t>* data,
                 std::size_t offset,
                 std::uint16_t value) {
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    (*data)[offset + 1U] =
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void WriteLe32At(std::vector<std::uint8_t>* data,
                 std::size_t offset,
                 std::uint32_t value) {
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    (*data)[offset + 1U] =
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    (*data)[offset + 2U] =
        static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    (*data)[offset + 3U] =
        static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void WriteAsciiAt(std::vector<std::uint8_t>* data,
                  std::size_t offset,
                  std::size_t length,
                  const std::string& value) {
    for (std::size_t index = 0U; index < length; ++index) {
        (*data)[offset + index] = index < value.size() ?
            static_cast<std::uint8_t>(value[index]) : 0U;
    }
}

std::vector<std::uint8_t> MakeValidF13BootImage(
    std::size_t image_size = 32U * 1024U,
    std::uint32_t application_start = 0x2000U,
    std::uint32_t application_capacity = 0x1E000U,
    std::uint32_t code_flash_size = 0x20000U,
    std::uint32_t boot_reserved_size = 0x2000U) {
    using namespace firmware_upgrade::f13_boot;

    std::vector<std::uint8_t> image(image_size, 0xFFU);
    WriteLe32At(&image, 0U, application_start);
    WriteLe32At(&image, kManifestOffset, kManifestMagic);
    image[kManifestOffset + 4U] = kImageFormatVersion;
    image[kManifestOffset + 5U] = kManifestSize;
    WriteLe16At(&image, kManifestOffset + 6U, 0x0003U);
    WriteLe16At(&image, kManifestOffset + 8U,
                firmware_upgrade::firmware_update::kProtocolId);
    image[kManifestOffset + 10U] = kProtocolVersion;
    image[kManifestOffset + 11U] =
        firmware_upgrade::firmware_update::kChecksumCrc32;
    WriteLe16At(&image, kManifestOffset + 12U, kTargetId);
    image[kManifestOffset + 14U] = 0x01U;
    image[kManifestOffset + 15U] = 0x00U;
    WriteLe32At(&image, kManifestOffset + 16U, 0x00010100U);
    WriteLe32At(&image, kManifestOffset + 20U, 20261014U);
    WriteLe32At(&image, kManifestOffset + 24U,
                static_cast<std::uint32_t>(image.size()));
    WriteLe32At(&image, kManifestOffset + 28U, application_start);
    WriteLe32At(&image, kManifestOffset + 32U, application_capacity);
    WriteLe32At(&image, kManifestOffset + 36U, code_flash_size);
    WriteLe32At(&image, kManifestOffset + 40U, boot_reserved_size);
    WriteLe32At(&image, kManifestOffset + 44U, 0x400U);
    WriteLe32At(&image, kManifestOffset + 48U, 0x00000004U);
    WriteLe32At(&image, kManifestOffset + 52U, 0U);
    WriteLe32At(&image, kManifestOffset + 56U, kRequestId);
    WriteLe32At(&image, kManifestOffset + 60U, kResponseId);
    WriteAsciiAt(&image, kManifestOffset + 64U, 16U, "R5F10BMG");
    WriteAsciiAt(&image, kManifestOffset + 80U, 12U, "RL78/F13");
    WriteLe32At(&image, kManifestOffset + 92U, kMinimumUpdateImageSize);
    std::fill_n(image.begin()+kManifestOffset+96U,32U,static_cast<std::uint8_t>(0U));
    WriteLe32At(&image,kManifestOffset+96U,IP_MAGIC);
    image[kManifestOffset+100U]=IP_DEVICE_VERSION; image[kManifestOffset+101U]=IP_SIZE;
    image[kManifestOffset+102U]=IP_ROLE_DEVICE_APP; image[kManifestOffset+103U]=IP_CAN;
    WriteLe32At(&image,kManifestOffset+104U,IP_F13_IRQ_ABI);
    const std::size_t stored_crc_offset = image.size() - 4U;
    WriteLe32At(&image, stored_crc_offset,
                Crc32(image.data(), stored_crc_offset));
    return image;
}

firmware_upgrade::can_host::Frame MakeF13BootResponse(
    const std::vector<std::uint8_t>& data) {
    firmware_upgrade::can_host::Frame frame;
    frame.id = firmware_upgrade::f13_boot::kResponseId;
    frame.dlc = static_cast<std::uint8_t>(data.size());
    frame.data = data;
    return frame;
}

std::vector<std::uint8_t> MakeF13BootAck(
    firmware_upgrade::f13_boot::Command command,
    firmware_upgrade::f13_boot::Status status,
    std::uint16_t sequence,
    std::uint32_t value) {
    std::vector<std::uint8_t> data;
    data.push_back(static_cast<std::uint8_t>(command));
    data.push_back(static_cast<std::uint8_t>(status));
    PutLe16(&data, sequence);
    PutLe32(&data, value);
    return data;
}

void TestFirmwareUpdateProfileDispatch() {
    const std::vector<std::uint8_t> image = MakeValidF13BootImage();
    firmware_upgrade::firmware_update::ImageInfo info;
    std::wstring error;

    const firmware_upgrade::firmware_update::Profile profile =
        firmware_upgrade::firmware_update::FromDisplayName(L"CAN Upgrade");
    Check(profile == firmware_upgrade::firmware_update::Profile::CanUpgrade,
          "Firmware profile display-name mapping");
    Check(firmware_upgrade::firmware_update::ValidateImage(
              profile, image, &info, &error) &&
              info.profile_name == L"CAN Upgrade" &&
              info.mcu_family == L"RL78/F13" &&
              info.mcu_part == L"R5F10BMG" &&
              info.image_size == image.size() &&
              info.application_start ==
                  firmware_upgrade::f13_boot::kApplicationStart &&
              info.application_end ==
                  firmware_upgrade::f13_boot::kApplicationStart +
                      image.size() - 1U,
          "Firmware profile owns F13 image boundaries");

    Check(!firmware_upgrade::firmware_update::ValidateImage(
              firmware_upgrade::firmware_update::Profile::None,
              image, &info, &error) && !error.empty(),
          "Unsupported firmware profile rejects image explicitly");

    const firmware_upgrade::firmware_update::ImageInspection inspection =
        firmware_upgrade::firmware_update::InspectImage(image);
    Check(inspection.image_size == image.size() &&
              inspection.checksum_available &&
              inspection.checksum_matches &&
              inspection.metadata_available &&
              inspection.metadata_valid &&
              inspection.metadata.application_capacity == 0x1E000U &&
              inspection.calculated_crc32 == inspection.stored_crc32,
          "Firmware inspection exposes BIN size and CRC32 trailer match");
    std::vector<std::uint8_t> mismatched = image;
    mismatched.back() ^= 1U;
    Check(!firmware_upgrade::firmware_update::InspectImage(mismatched)
               .checksum_matches,
          "Firmware inspection exposes CRC32 trailer mismatch");
    auto reserved=image; reserved[0x160U]=0U;
    WriteLe32At(&reserved,reserved.size()-4U,firmware_upgrade::firmware_update::Crc32(reserved.data(),reserved.size()-4U));
    Check(!firmware_upgrade::firmware_update::InspectImage(reserved).metadata_valid,
          "CAN reserved metadata extension rejected even with correct CRC");
    reserved=image; reserved[0x104U]=3U;
    Check(!firmware_upgrade::firmware_update::InspectImage(reserved).metadata_valid,"old CAN image format3 rejected");
    reserved=image; reserved[0x104U]=4U;
    WriteLe32At(&reserved,reserved.size()-4U,firmware_upgrade::firmware_update::Crc32(reserved.data(),reserved.size()-4U));
    Check(!firmware_upgrade::firmware_update::InspectImage(reserved).metadata_valid,"old callback-IRQ format4 rejected despite valid CRC");
}

void TestF13BootImageAndFraming() {
    using namespace firmware_upgrade::f13_boot;

    const std::string crc_input = "123456789";
    Check(Crc32(reinterpret_cast<const std::uint8_t*>(crc_input.data()),
                crc_input.size()) == 0xCBF43926U,
          "F13 Boot CRC32 known vector");

    std::vector<std::uint8_t> image = MakeValidF13BootImage();
    ImageInfo image_info;
    std::wstring error;
    Check(ParseUpdateImage(image, &image_info, &error) &&
              image_info.format == kImageFormatVersion &&
              image_info.target_id == kTargetId &&
              image_info.version == 0x00010100U &&
              image_info.image_size == image.size() &&
              image_info.stored_crc32 == image_info.calculated_crc32,
          "F13 Boot valid variable-length update image");

    const std::array<std::size_t, 4> supported_sizes{{
        32U * 1024U,
        60U * 1024U,
        80U * 1024U,
        kMaximumUpdateImageSize,
    }};
    for (const std::size_t supported_size : supported_sizes) {
        const std::vector<std::uint8_t> supported_image =
            MakeValidF13BootImage(supported_size);
        Check(ParseUpdateImage(supported_image, &image_info, &error) &&
                  image_info.image_size == supported_size,
              "F13 Boot accepts a supported variable image size");
    }
    const std::uint32_t alternate_start = 0x4000U;
    const std::vector<std::uint8_t> alternate_image =
        MakeValidF13BootImage(
            80U * 1024U, alternate_start, 0x1C000U, 0x20000U,
            alternate_start);
    Check(ParseUpdateImage(alternate_image, &image_info, &error) &&
              image_info.application_start == alternate_start &&
              image_info.application_capacity == 0x1C000U,
          "CAN Upgrade accepts a metadata-defined application layout");

    const std::vector<std::uint8_t> large_image =
        MakeValidF13BootImage(
            300U * 1024U, 0x4000U, 0x7C000U, 0x80000U, 0x4000U);
    Check(ParseUpdateImage(large_image, &image_info, &error) &&
              image_info.image_size == 300U * 1024U,
          "CAN Upgrade accepts an image larger than 256 KiB");

    std::vector<std::uint8_t> invalid = image;
    invalid.resize(invalid.size() - 1U);
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects an unaligned image size");
    invalid.assign(kMinimumUpdateImageSize - 4U, 0xFFU);
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects an undersized image");
    invalid = image;
    WriteLe32At(&invalid, kManifestOffset + 32U,
                static_cast<std::uint32_t>(image.size() - 4U));
    WriteLe32At(&invalid, invalid.size() - 4U,
                Crc32(invalid.data(), invalid.size() - 4U));
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "CAN Upgrade rejects an image larger than metadata capacity");
    invalid = image;
    invalid[0] = invalid[1] = invalid[2] = invalid[3] = 0xFFU;
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects missing jump table");
    invalid = image;
    invalid[kManifestOffset] ^= 1U;
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects manifest magic");
    invalid = image;
    invalid[kManifestOffset + 4U] = 1U;
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects manifest format");
    invalid = image;
    invalid[kManifestOffset + 4U] = 4U;
    WriteLe32At(&invalid, invalid.size()-4U, Crc32(invalid.data(), invalid.size()-4U));
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects legacy callback ABI even with valid CRC");
    invalid = image;
    WriteLe16At(&invalid, kManifestOffset + 12U, 0x1234U);
    WriteAsciiAt(&invalid, kManifestOffset + 64U, 16U, "R7FA6M5BH");
    WriteAsciiAt(&invalid, kManifestOffset + 80U, 12U, "RA6M5");
    WriteLe32At(&invalid, invalid.size() - 4U,
                Crc32(invalid.data(), invalid.size() - 4U));
    Check(ParseUpdateImage(invalid, &image_info, &error) &&
              image_info.target_id == 0x1234U,
          "CAN Upgrade accepts a non-F13 metadata identity");
    invalid = image;
    WriteLe16At(&invalid, kManifestOffset + 12U, 0U);
    WriteLe32At(&invalid, invalid.size() - 4U,
                Crc32(invalid.data(), invalid.size() - 4U));
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "CAN Upgrade rejects an uninitialized target identity");
    invalid = image;
    invalid[kManifestOffset + 14U] = 0U;
    WriteLe32At(&invalid, invalid.size() - 4U,
                Crc32(invalid.data(), invalid.size() - 4U));
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "CAN Upgrade rejects an empty CAN mode mask");
    invalid = image;
    invalid[kManifestOffset + 14U] = 0x02U;
    WriteLe32At(&invalid, kManifestOffset + 52U, 0U);
    WriteLe32At(&invalid, invalid.size() - 4U,
                Crc32(invalid.data(), invalid.size() - 4U));
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "CAN Upgrade rejects CAN FD metadata without a data rate");
    invalid = image;
    WriteLe32At(&invalid, kManifestOffset + 16U, 0xFFFFFFFFU);
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects uninitialized version");
    invalid = image;
    WriteLe32At(&invalid, kManifestOffset + 24U,
                static_cast<std::uint32_t>(image.size() - 4U));
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects manifest image size");
    invalid = image;
    invalid[0x200U] ^= 1U;
    Check(!ParseUpdateImage(invalid, &image_info, &error),
          "F13 Boot rejects image CRC32");

    Check(ParseUpdateImage(image, &image_info, &error),
          "F13 Boot image reparses for frame tests");
    firmware_upgrade::can_host::Frame frame;
    Check(BuildHelloRequest(&frame, &error) && frame.id == kRequestId &&
              frame.dlc == 1U &&
              frame.data == std::vector<std::uint8_t>({0x01U}),
          "F13 Boot HELLO request framing");
    Check(BuildEnterRequest(&frame, &error) && frame.dlc == 1U &&
              frame.data == std::vector<std::uint8_t>({0x06U}),
          "F13 Boot ENTER request framing");
    Check(BuildInfoRequest(InfoPage::Core, &frame, &error) &&
              frame.dlc == 2U &&
              frame.data == std::vector<std::uint8_t>({0x07U, 0x00U}),
          "F13 identity request framing");
    Check(BuildInfoRequest(InfoPage::ApplicationStart, &frame, &error) &&
              frame.data == std::vector<std::uint8_t>({0x07U, 0x05U}),
          "F13 application-start identity request framing");
    Check(BuildStartRequest(image_info, &frame, &error) &&
              frame.dlc == 8U &&
              frame.data == std::vector<std::uint8_t>({
                  0x02U, 0x05U, 0x3BU, 0xF1U,
                  0x00U, 0x80U, 0x00U, 0x00U}),
          "F13 Boot START request framing");
    Check(BuildBlockRequest(firmware_upgrade::f13_boot::Command::BlockBegin, 0x1240U, 0x44332211U,
                           &frame, &error) &&
              frame.data == std::vector<std::uint8_t>({
                  0x0BU, 0x40U, 0x12U, 0x00U,
                  0x11U, 0x22U, 0x33U, 0x44U}),
          "F13 Boot DATA request framing");
    Check(BuildBlockRequest(firmware_upgrade::f13_boot::Command::BlockCommit,
              0x00040000U, 0U, &frame, &error) &&
              frame.data[1] == 0x00U && frame.data[2] == 0x00U &&
              frame.data[3] == 0x04U,
          "CAN Upgrade DATA framing crosses the old 256-KiB limit");
    Check(!BuildBlockRequest(firmware_upgrade::f13_boot::Command::BlockCommit,
              kMaximumDataOffset + kDataBytes, 0U, &frame, &error),
          "CAN Upgrade rejects out-of-range DATA offset");

    HelloInfo hello;
    const firmware_upgrade::can_host::Frame hello_response =
        MakeF13BootResponse({0x01U, 0x00U, 0x05U, 0x06U,
                             0x3BU, 0xF1U, 0x78U, 0x00U});
    Check(ParseHelloResponse(hello_response, &hello, &error) &&
              hello.status == firmware_upgrade::f13_boot::Status::Ok &&
              hello.protocol_version == kProtocolVersion &&
              hello.image_format == kImageFormatVersion &&
              hello.target_id == kTargetId &&
              hello.maximum_update_size_kib == 120U,
          "F13 Boot HELLO response identity");
    firmware_upgrade::can_host::Frame invalid_response = hello_response;
    invalid_response.id = 0x702U;
    Check(!ParseHelloResponse(invalid_response, &hello, &error),
          "F13 Boot rejects wrong response ID");
    invalid_response = hello_response;
    invalid_response.data[1] = 0xFFU;
    Check(!ParseHelloResponse(invalid_response, &hello, &error),
          "F13 Boot rejects unknown status");
    invalid_response = hello_response;
    invalid_response.data[2] = 0x02U;
    Check(!ParseHelloResponse(invalid_response, &hello, &error),
          "F13 Boot rejects incompatible HELLO protocol");

    DeviceState identity_state;
    std::uint32_t identity_value = 0U;
    const firmware_upgrade::can_host::Frame info_response =
        MakeF13BootResponse({0x07U, 0x00U, 0x00U, 0x01U,
                             0x3BU, 0xF1U, 0x03U, 0x03U});
    Check(ParseInfoResponse(InfoPage::Core, info_response, &identity_state,
                            &identity_value, &error) &&
              identity_state == DeviceState::Application &&
              identity_value == 0x0303F13BU,
          "F13 application identity response");
    invalid_response = info_response;
    invalid_response.data[2] = 1U;
    Check(!ParseInfoResponse(InfoPage::Core, invalid_response,
                             &identity_state, &identity_value, &error),
          "F13 identity rejects wrong page");
    invalid_response = info_response;
    invalid_response.data[3] = 0xFFU;
    Check(!ParseInfoResponse(InfoPage::Core, invalid_response,
                             &identity_state, &identity_value, &error),
          "F13 identity rejects unknown execution state");
}

void TestF13BootClientTransactions() {
    using namespace firmware_upgrade::f13_boot;
    using BootCommand = firmware_upgrade::f13_boot::Command;
    using BootStatus = firmware_upgrade::f13_boot::Status;

    firmware_upgrade::host_link::Client host_link_client;
    firmware_upgrade::can_host::Client can_client;
    firmware_upgrade::f13_boot::Client boot_client;
    FakeTransport transport;
    std::vector<std::uint16_t> timeouts;
    std::vector<std::uint8_t> commands;
    bool corrupt_data_ack = false;
    std::wstring error;
    const std::vector<std::uint8_t> image = MakeValidF13BootImage();
    ImageInfo image_info;
    Check(ParseUpdateImage(image, &image_info, &error),
          "F13 Boot client test image");

    transport.on_write = [&transport, &timeouts, &commands, &image_info, &image,
                          &corrupt_data_ack](
                             const std::vector<std::uint8_t>& raw,
                             std::size_t) {
        const Frame host_request = DecodeRequest(raw);
        if (host_request.command == static_cast<std::uint8_t>(firmware_upgrade::host_link::Command::CanBurstWaitRx)) {
            const auto& p = host_request.payload;
            Check(p.size() <= 128U && p[0] == 1U && p[1] >= 3U && p[1] <= 10U,
                  "F13 block fits one128-byte Host Link request");
            timeouts.push_back(static_cast<std::uint16_t>(p[2] | (p[3] << 8U)));
            commands.push_back(0x0CU);
            std::size_t at = 12U;
            std::uint32_t offset = 0U;
            std::size_t bytes = 0U;
            for (unsigned i = 0; i < p[1]; ++i) {
                const auto dlc = p[at+2];
                const auto id = static_cast<unsigned>(p[at] | (p[at+1] << 8U));
                if (i == 0U || i + 1U == p[1]) {
                    Check(id == kRequestId && dlc == 8U && p[at+3] == (i == 0U ? 0x0BU : 0x0CU),
                          "block BEGIN / COMMIT control frames");
                    offset = p[at+4] | (static_cast<std::uint32_t>(p[at+5]) << 8U) |
                        (static_cast<std::uint32_t>(p[at+6]) << 16U);
                } else {
                    Check(id == 0x720U + i - 1U && (dlc == 8U || dlc == 4U),
                          "indexed raw data frames use all8bytes except4byte tail");
                    bytes += dlc;
                }
                at += 3U + dlc;
            }
            Check(at == p.size(), "burst records consume exact payload");
            const auto reply = MakeF13BootResponse(MakeF13BootAck(BootCommand::BlockCommit,
                BootStatus::Ok, static_cast<std::uint16_t>(offset/64U),
                kApplicationStart + offset + static_cast<std::uint32_t>(bytes) + (corrupt_data_ack ? 4U : 0U)));
            transport.Queue(MakeResponse(host_request, Status::Ok, EncodeCanRecord(reply)));
            return;
        }
        Check(host_request.command == static_cast<std::uint8_t>(
                  firmware_upgrade::host_link::Command::CanTxWaitRx),
              "F13 Boot client uses CAN_TX_WAIT_RX");
        Check(host_request.payload.size() >= 29U &&
                  host_request.payload[2] == 1U &&
                  host_request.payload[4] == 0x01U &&
                  host_request.payload[5] == 0x07U &&
                  host_request.payload[8] == 0xFFU &&
                  host_request.payload[9] == 0x07U &&
                  host_request.payload[13] == 0x07U &&
                  host_request.payload[16] == 0U &&
                  host_request.payload[17] == 0U &&
                  host_request.payload[20] == 0x00U &&
                  host_request.payload[21] == 0x07U,
              "F13 Boot client exact standard CAN match");

        const std::uint16_t timeout = static_cast<std::uint16_t>(
            host_request.payload[0] |
            (static_cast<std::uint16_t>(host_request.payload[1]) << 8U));
        const std::size_t target_offset = 28U;
        const auto command = static_cast<BootCommand>(
            host_request.payload[target_offset]);
        timeouts.push_back(timeout);
        commands.push_back(host_request.payload[target_offset]);

        firmware_upgrade::can_host::Frame target_response;
        if (command == BootCommand::Info) {
            const std::uint8_t page =
                host_request.payload[target_offset + 1U];
            std::uint32_t value = 0U;
            if (page == static_cast<std::uint8_t>(InfoPage::Core)) {
                value = 0x0605F13BU;
            } else if (page ==
                       static_cast<std::uint8_t>(InfoPage::BootVersion)) {
                value = 0x00010000U;
            } else if (page ==
                       static_cast<std::uint8_t>(InfoPage::BootBuild)) {
                value = 20261014U;
            } else if (page ==
                       static_cast<std::uint8_t>(InfoPage::AppVersion)) {
                value = 0x00010100U;
            } else if (page ==
                       static_cast<std::uint8_t>(InfoPage::AppBuild)) {
                value = 20261014U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::ApplicationStart)) {
                value = kApplicationStart;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::ApplicationCapacity)) {
                value = kMaximumUpdateImageSize;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::CodeFlashSize)) {
                value = 0x20000U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::BootReservedSize)) {
                value = 0x2000U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::EraseBlockSize)) {
                value = 0x400U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::CanCapabilities)) {
                value = 0x01010001U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::NominalBitrateMask)) {
                value = 0x00000004U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::DataBitrateMask)) {
                value = 0U;
            } else if (page ==
                       static_cast<std::uint8_t>(InfoPage::RequestId)) {
                value = kRequestId;
            } else if (page ==
                       static_cast<std::uint8_t>(InfoPage::ResponseId)) {
                value = kResponseId;
            } else if (page == static_cast<std::uint8_t>(InfoPage::Part0)) {
                value = AsciiWord("R5F1");
            } else if (page == static_cast<std::uint8_t>(InfoPage::Part1)) {
                value = AsciiWord("0BMG");
            } else if (page == static_cast<std::uint8_t>(InfoPage::Family0)) {
                value = AsciiWord("RL78");
            } else if (page == static_cast<std::uint8_t>(InfoPage::Family1)) {
                value = AsciiWord("/F13");
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::InstalledAppSize)) {
                value = image_info.image_size;
            } else if (page == static_cast<std::uint8_t>(InfoPage::UpdatePolicy)) {
                value=0x01012003U;
            } else if (page == static_cast<std::uint8_t>(InfoPage::IrqAbi)) {
                value=IP_F13_IRQ_ABI;
            } else if (page == static_cast<std::uint8_t>(InfoPage::Block)) {
                value=0x07200040U;
            } else if (page == static_cast<std::uint8_t>(InfoPage::VersionPolicy)) {
                value=1U;
            } else if (page == static_cast<std::uint8_t>(
                                   InfoPage::MinimumImageSize)) {
                value = kMinimumUpdateImageSize;
            } else {
                value = 0U;
            }
            std::vector<std::uint8_t> data{
                0x07U, 0x00U, page, 0x02U,
            };
            PutLe32(&data, value);
            target_response = MakeF13BootResponse(data);
        } else if (command == BootCommand::Hello) {
            target_response = MakeF13BootResponse(
                {0x01U, 0x00U, 0x05U, 0x06U,
                 0x3BU, 0xF1U, 0x78U, 0x00U});
        } else if (command == BootCommand::Enter) {
            target_response = MakeF13BootResponse({0x06U, 0x00U});
        } else if (command == BootCommand::MetadataReset) {
            target_response=MakeF13BootResponse(MakeF13BootAck(command,BootStatus::Ok,0U,256U));
        } else if (command == BootCommand::Metadata || command == BootCommand::Seal) {
            std::uint16_t next=256U;
            if (command==BootCommand::Metadata) {
                const auto offset=static_cast<std::uint16_t>(host_request.payload[target_offset+1U] |
                    (static_cast<std::uint16_t>(host_request.payload[target_offset+2U])<<8U));
                Check(offset<=252U && std::equal(host_request.payload.begin()+target_offset+4U,
                    host_request.payload.begin()+target_offset+8U,image.begin()+kManifestOffset+offset),
                    "CAN metadata chunks preserve exact image bytes");
                next=offset+4U;
            } else {
                Check(firmware_upgrade::image_policy::Read32(host_request.payload.data()+target_offset+4U)==Crc32(image.data()+kManifestOffset,256U),
                    "CAN metadata SEAL carries full region CRC");
            }
            target_response=MakeF13BootResponse(MakeF13BootAck(command,BootStatus::Ok,next,256U));
        } else if (command == BootCommand::Start) {
            target_response = MakeF13BootResponse(MakeF13BootAck(
                command, BootStatus::Ok, 0U, kApplicationStart));
        } else if (command == BootCommand::End) {
            target_response = MakeF13BootResponse(MakeF13BootAck(
                command, BootStatus::Ok,
                static_cast<std::uint16_t>(image_info.image_size /
                                           kDataBytes),
                image_info.calculated_crc32));
        } else {
            target_response = MakeF13BootResponse(MakeF13BootAck(
                command, BootStatus::Ok, 2U, kApplicationStart + 8U));
        }
        transport.Queue(MakeResponse(
            host_request, Status::Ok, EncodeCanRecord(target_response)));
    };

    host_link_client.SetTransport(&transport);
    can_client.SetHostLinkClient(&host_link_client);
    boot_client.SetCanClient(&can_client);

    HelloInfo hello;
    TargetInfo target_info;
    Ack ack;
    Check(boot_client.GetTargetInfo(&target_info, &error) &&
              target_info.state == DeviceState::Bootloader &&
              target_info.target_id == kTargetId &&
              target_info.protocol_version == kProtocolVersion &&
              target_info.image_format == kImageFormatVersion &&
               target_info.boot_version == 0x00010000U &&
               target_info.boot_build == 20261014U &&
               target_info.app_version == 0x00010100U &&
               target_info.application_start == kApplicationStart &&
               target_info.application_capacity == kMaximumUpdateImageSize &&
               target_info.installed_app_size == image_info.image_size &&
               target_info.minimum_image_size == kMinimumUpdateImageSize &&
               target_info.mcu_family == L"RL78/F13" &&
               target_info.mcu_part == L"R5F10BMG",
          "F13 Boot paged identity transaction");
    Check(boot_client.Hello(&hello, &error),
          "F13 Boot client HELLO transaction");
    Check(boot_client.EnterBootloader(&error),
          "F13 Boot client ENTER transaction");
    Check(boot_client.StageMetadata(image,{},&error),"F13 client64 metadata chunks and SEAL before START");
    Check(boot_client.Start(image_info, &ack, &error),
          "F13 Boot client START transaction");
    Check(boot_client.SendBlock(0U, std::vector<std::uint8_t>(64U, 0x11U),
                               &ack, &error),
          "F13 Boot client erase-boundary DATA transaction");
    Check(boot_client.SendBlock(64U, {0x55U, 0x66U, 0x77U, 0x88U},
                               &ack, &error),
          "F13 Boot client normal DATA transaction");
    Check(boot_client.End(
              image_info.calculated_crc32,
              image_info.image_size,
              &ack, &error),
          "F13 Boot client END transaction");
    Check(boot_client.Abort(&ack, &error),
          "F13 Boot client ABORT transaction");
    Check(commands.size() == 101U &&
              std::count(commands.begin(), commands.end(), 0x07U) == 28 &&
              commands[28] == 0x01U && commands[29] == 0x06U &&
              commands[30]==0x0AU && std::count(commands.begin(),commands.end(),0x08U)==64 && commands[95]==0x09U &&
              commands[96] == 0x02U && commands[97] == 0x0CU &&
              commands[98] == 0x0CU && commands[99] == 0x04U &&
              commands[100] == 0x05U,
          "F13 Boot client command order");
    Check(timeouts.size() == commands.size() &&
              timeouts[97] == 3000U && timeouts[98] == 500U &&
              timeouts[99] == 5000U && timeouts[100] == 500U,
          "F13 Boot client erase and END timeout policy");

    corrupt_data_ack = true;
    Check(!boot_client.SendBlock(128U, {0U, 0U, 0U, 0U}, &ack, &error),
          "F13 Boot rejects DATA ACK with wrong next address");
    corrupt_data_ack = false;

    const std::size_t writes_before_invalid = transport.writes.size();
    std::vector<std::uint8_t> invalid_image = image;
    invalid_image[0x200U] ^= 1U;
    ImageInfo unused_info;
    Check(!ParseUpdateImage(invalid_image, &unused_info, &error),
          "F13 Boot validates image before START");
    Check(transport.writes.size() == writes_before_invalid,
          "invalid F13 Boot image produces no CAN traffic");
}

class MockF13UpdateClient final :
    public firmware_upgrade::f13_boot::UpdateClient {
public:
    bool StageMetadata(const std::vector<std::uint8_t>&,
        const std::function<bool()>& cancelled,std::wstring* error) override {
        ++metadata_calls;
        if (fault==Fault::MetadataTimeout || fault==Fault::MetadataDisconnect || fault==Fault::MetadataAck) {
            *error=L"Metadata timeout/disconnect/invalid ACK before erase.";
            return false;
        }
        return !cancelled || !cancelled();
    }
    enum class Fault {
        None,
        MetadataTimeout,
        MetadataDisconnect,
        MetadataAck,
        StartTimeout,
        DataTimeout,
        FlashError,
        CorruptAck,
        OutOfOrderAck,
        Disconnect,
        EndCrc,
        AbortTimeout,
        DuplicateRecovered,
    };

    bool Start(const firmware_upgrade::f13_boot::ImageInfo&,
               firmware_upgrade::f13_boot::Ack*,
               std::wstring* error) override {
        ++start_calls;
        if (fault == Fault::StartTimeout) {
            *error = L"START timeout.";
            return false;
        }
        return true;
    }

    bool SendBlock(
        std::uint32_t image_offset,
        const std::vector<std::uint8_t>&,
        firmware_upgrade::f13_boot::Ack*,
        std::wstring* error) override {
        ++data_calls;
        last_data_offset = image_offset;
        if (image_offset == fault_offset) {
            switch (fault) {
                case Fault::DataTimeout:
                    *error = L"DATA timeout.";
                    return false;
                case Fault::FlashError:
                    *error = L"Target FLASH_ERROR (simulated FSL failure).";
                    return false;
                case Fault::CorruptAck:
                    *error = L"DATA ACK CRC is corrupt.";
                    return false;
                case Fault::OutOfOrderAck:
                    *error = L"DATA ACK sequence is out of order.";
                    return false;
                case Fault::Disconnect:
                    *error = L"Host Link disconnected.";
                    return false;
                case Fault::DuplicateRecovered:
                    ++duplicate_replays;
                    break;
                default:
                    break;
            }
        }
        return true;
    }

    bool End(std::uint32_t,
             std::uint32_t expected_byte_count,
             firmware_upgrade::f13_boot::Ack*,
             std::wstring* error) override {
        ++end_calls;
        last_end_byte_count = expected_byte_count;
        if (fault == Fault::EndCrc) {
            *error = L"END CRC mismatch.";
            return false;
        }
        return true;
    }

    bool Abort(firmware_upgrade::f13_boot::Ack*,
               std::wstring* error) override {
        ++abort_calls;
        if (fault == Fault::AbortTimeout) {
            *error = L"ABORT timeout.";
            return false;
        }
        return true;
    }

    Fault fault = Fault::None;
    std::uint32_t metadata_calls=0U;
    std::uint32_t fault_offset = 128U;
    std::uint32_t start_calls = 0U;
    std::uint32_t data_calls = 0U;
    std::uint32_t end_calls = 0U;
    std::uint32_t abort_calls = 0U;
    std::uint32_t duplicate_replays = 0U;
    std::uint32_t last_data_offset = 0U;
    std::uint32_t last_end_byte_count = 0U;
};

void TestF13BootUpdateFaultInjection() {
    using namespace firmware_upgrade::f13_boot;

    const std::vector<std::uint8_t> image = MakeValidF13BootImage();
    std::wstring error;
    UpdateCallbacks callbacks;
    UpdateReport report;

    MockF13UpdateClient normal;
    for (const auto fault:{MockF13UpdateClient::Fault::MetadataTimeout,
        MockF13UpdateClient::Fault::MetadataDisconnect,MockF13UpdateClient::Fault::MetadataAck}) {
        MockF13UpdateClient before_erase; before_erase.fault=fault;
        Check(!RunUpdate(&before_erase,image,callbacks,&report,&error) &&
            before_erase.metadata_calls==1U && before_erase.start_calls==0U && before_erase.data_calls==0U &&
            report.abort_attempted,"metadata failure/disconnect cannot issue START or DATA");
    }
    Check(RunUpdate(&normal, image, callbacks, &report, &error) &&
              report.result == UpdateResult::Success &&
              report.start_acknowledged &&
              report.blocks_acked == (image.size() + kBlockBytes - 1U) / kBlockBytes &&
              normal.last_end_byte_count == image.size() &&
              normal.end_calls == 1U && normal.abort_calls == 0U,
          "F13 update runner normal flow");

    MockF13UpdateClient duplicate;
    duplicate.fault = MockF13UpdateClient::Fault::DuplicateRecovered;
    Check(RunUpdate(&duplicate, image, callbacks, &report, &error) &&
              duplicate.duplicate_replays == 1U &&
              report.result == UpdateResult::Success,
          "F13 update runner tolerates lower-layer duplicate replay");

    const MockF13UpdateClient::Fault data_faults[] = {
        MockF13UpdateClient::Fault::DataTimeout,
        MockF13UpdateClient::Fault::FlashError,
        MockF13UpdateClient::Fault::CorruptAck,
        MockF13UpdateClient::Fault::OutOfOrderAck,
        MockF13UpdateClient::Fault::Disconnect,
    };
    for (const auto fault : data_faults) {
        MockF13UpdateClient mock;
        mock.fault = fault;
        Check(!RunUpdate(&mock, image, callbacks, &report, &error) &&
                  report.result == UpdateResult::DataFailed &&
                  report.abort_attempted && report.abort_acknowledged &&
                  mock.abort_calls == 1U && mock.end_calls == 0U,
              "F13 update DATA fault triggers one ABORT");
    }

    MockF13UpdateClient start_timeout;
    start_timeout.fault = MockF13UpdateClient::Fault::StartTimeout;
    Check(!RunUpdate(&start_timeout, image, callbacks, &report, &error) &&
              report.result == UpdateResult::StartFailed &&
              report.abort_attempted && report.abort_acknowledged &&
              start_timeout.abort_calls == 1U,
          "F13 update START timeout performs ambiguous-session ABORT");

    MockF13UpdateClient end_crc;
    end_crc.fault = MockF13UpdateClient::Fault::EndCrc;
    Check(!RunUpdate(&end_crc, image, callbacks, &report, &error) &&
              report.result == UpdateResult::EndFailed &&
              report.abort_attempted && report.abort_acknowledged,
          "F13 update END CRC fault triggers ABORT");

    MockF13UpdateClient canceled;
    callbacks.is_canceled = [&canceled]() {
        return canceled.data_calls >= 3U;
    };
    Check(!RunUpdate(&canceled, image, callbacks, &report, &error) &&
              report.result == UpdateResult::Canceled &&
              report.blocks_acked == 3U &&
              report.abort_attempted && report.abort_acknowledged,
          "F13 update cancellation after START triggers ABORT");
    callbacks.is_canceled = {};

    MockF13UpdateClient abort_timeout;
    abort_timeout.fault = MockF13UpdateClient::Fault::AbortTimeout;
    abort_timeout.fault_offset = 8U;
    callbacks.is_canceled = [&abort_timeout]() {
        return abort_timeout.data_calls >= 2U;
    };
    Check(!RunUpdate(&abort_timeout, image, callbacks, &report, &error) &&
              report.abort_attempted && !report.abort_acknowledged &&
              error.find(L"Cleanup ABORT failed") != std::wstring::npos,
          "F13 update reports ABORT cleanup timeout");
    callbacks.is_canceled = {};

    std::vector<std::uint8_t> invalid = image;
    invalid[0x300U] ^= 1U;
    MockF13UpdateClient invalid_client;
    Check(!RunUpdate(&invalid_client, invalid, callbacks, &report, &error) &&
              report.result == UpdateResult::InvalidImage &&
              invalid_client.start_calls == 0U,
          "F13 update invalid image sends no target command");
}

void TestF13TransferClientWorkflow() {
    using firmware_upgrade::f13_transfer::Command;

    firmware_upgrade::host_link::Client host_link_client;
    firmware_upgrade::can_host::Client can_client;
    firmware_upgrade::f13_transfer::Client transfer_client;
    firmware_upgrade::f13_transfer::DeviceInfo info;
    FakeTransport transport;
    std::vector<std::uint8_t> target_sequences;
    std::wstring error;

    transport.on_write = [&transport, &target_sequences](
                             const std::vector<std::uint8_t>& raw,
                             std::size_t) {
        const Frame host_request = DecodeRequest(raw);
        Check(host_request.command ==
                  static_cast<std::uint8_t>(
                      firmware_upgrade::host_link::Command::CanTxWaitRx),
              "F13 Transfer client uses CAN_TX_WAIT_RX");
        Check(host_request.payload.size() == 36U &&
                  host_request.payload[0] == 0xF4U &&
                  host_request.payload[1] == 0x01U &&
                  host_request.payload[2] == 1U &&
                  host_request.payload[3] == 8U &&
                  host_request.payload[4] == 0x11U &&
                  host_request.payload[5] == 0x07U &&
                  host_request.payload[8] == 0xFFU &&
                  host_request.payload[9] == 0x07U &&
                  host_request.payload[13] == 0x07U &&
                  host_request.payload[18] == 8U &&
                  host_request.payload[19] == 8U &&
                  host_request.payload[20] == 0x10U &&
                  host_request.payload[21] == 0x07U,
              "F13 Transfer client exact CAN match and TX record");

        const std::size_t target_offset = 28U;
        const std::uint8_t sequence = host_request.payload[target_offset + 2U];
        const auto target_command = static_cast<Command>(
            host_request.payload[target_offset + 3U]);
        const std::array<std::uint8_t, 3> target_payload{{
            host_request.payload[target_offset + 4U],
            host_request.payload[target_offset + 5U],
            host_request.payload[target_offset + 6U],
        }};
        target_sequences.push_back(sequence);
        Check(host_request.payload[target_offset] == 0xA5U &&
                  host_request.payload[target_offset + 1U] == 1U &&
                  host_request.payload[target_offset + 7U] ==
                      firmware_upgrade::f13_transfer::Crc8Atm(
                          &host_request.payload[target_offset], 7U),
              "F13 Transfer client target request framing");

        std::array<std::uint8_t, 3> response_payload = target_payload;
        switch (target_command) {
            case Command::DeviceInfo: {
                static constexpr std::array<std::array<std::uint8_t, 3>, 7>
                    kPages{{
                        {{0U, 1U, 0U}},
                        {{1U, 0x3BU, 0xF1U}},
                        {{2U, 'R', '5'}},
                        {{3U, 'F', '1'}},
                        {{4U, '0', 'B'}},
                        {{5U, 'M', 'G'}},
                        {{6U, 0x3FU, 0U}},
                    }};
                response_payload = kPages[target_payload[0]];
                break;
            }
            case Command::PatternTest:
                response_payload = {
                    static_cast<std::uint8_t>(target_payload[0] ^ 0xA5U),
                    static_cast<std::uint8_t>(target_payload[1] + 0x3DU),
                    static_cast<std::uint8_t>((target_payload[2] << 1U) |
                                              (target_payload[2] >> 7U)),
                };
                break;
            case Command::GetStats:
                response_payload = {target_payload[0], 0x34U, 0x12U};
                break;
            default:
                break;
        }

        const auto target_response = MakeF13TransferResponse(
            sequence, target_command, response_payload);
        transport.Queue(MakeResponse(
            host_request, Status::Ok, EncodeCanRecord(target_response)));
    };

    host_link_client.SetTransport(&transport);
    can_client.SetHostLinkClient(&host_link_client);
    transfer_client.SetCanClient(&can_client);

    Check(transfer_client.Discover(&info, &error) &&
              info.target_id == 0xF13BU && info.part_number == "R5F10BMG",
          "F13 Transfer client discovery workflow");
    Check(transfer_client.Ping({0x10U, 0x20U, 0x30U}, &error),
          "F13 Transfer client PING workflow");

    std::array<std::uint8_t, 3> output{};
    Check(transfer_client.Echo({0xAAU, 0x55U, 0x7EU}, &output, &error) &&
              output == std::array<std::uint8_t, 3>{0xAAU, 0x55U, 0x7EU},
          "F13 Transfer client ECHO workflow");
    Check(transfer_client.PatternTest(
              {0x12U, 0x34U, 0x81U}, &output, &error) &&
              output == std::array<std::uint8_t, 3>{0xB7U, 0x71U, 0x03U},
          "F13 Transfer client PATTERN_TEST workflow");

    std::uint16_t stat_value = 0U;
    Check(transfer_client.GetStat(4U, &stat_value, &error) &&
              stat_value == 0x1234U,
          "F13 Transfer client GET_STATS workflow");
    Check(transfer_client.ClearStats(&error),
          "F13 Transfer client CLEAR_STATS workflow");
    Check(target_sequences.size() == 12U && target_sequences.front() == 1U &&
              target_sequences.back() == 12U,
          "F13 Transfer client non-zero advancing sequences");
}

} // namespace

extern "C" int TestF24CanClock(void);
extern "C" int TestF24Uart(void);
extern "C" int TestF13BoardSupport(void);
int TestSerialPortCatalog();
int TestSerialPortIo();

extern "C" int TestBridgeI2c(void);
extern "C" int TestF24I2cBackend(void);
int TestMultiBus();
int TestOverviewModel();
int TestConsoleBridge();
int TestBridgeEngine();
int TestImagePolicy();
int TestCanBlock(const char* path);
int TestF24CanProcess();
int TestPackagedBridge(const char* path);
int TestPackagedCan(const char* path);
int TestF24AppReadback(const char* path);
int TestToolExperience();
int TestLogView();
int main(int argc, char** argv) {
    Check(TestLogView()==0,"Real RichEdit follow / preserve viewport and selection");
    Check(TestSerialPortIo() == 0, "Real SerialPort low-latency reads and bounded deadlines");
    Check(TestToolExperience()==0,"Tool HTML report, guidance and preflight models");
    TestLinkBaudNegotiation();
    TestReconnectSequenceCollision();
    Check(TestF24Uart()==0,"Real F24 UART high-speed state machine");
    Check(TestImagePolicy()==0,"Common metadata policy and F13 staging fault cases");
    Check(TestCanBlock(argc>2?argv[2]:nullptr)==0,"CAN v5 burst / block update interoperability");
    Check(TestF24CanProcess()==0,"Actual F24 nonblocking burst progression");
    if (argc>1) Check(TestPackagedBridge(argv[1]) == 0,"Actual packaged bridge image validation and simulated programming");
    if (argc>1) Check(TestF24AppReadback(argv[1]) == 0,"Actual App handler metadata readback after reconnect");
    if (argc>2) Check(TestPackagedCan(argv[2]) == 0,"Actual packaged CAN image metadata/CRC validation");
    Check(TestBridgeEngine() == 0, "Bridge app-only flash safety and wire interoperability");
    Check(TestConsoleBridge() == 0, "Shared console geometry and bridge image contract");
    Check(TestOverviewModel() == 0, "UI live/offline provenance and stale state");
    if (!TestBridgeI2c()) return 1;
    Check(TestF24I2cBackend() == 0, "F24 real IICA0 backend register/ISR mocks");
    Check(TestMultiBus() == 0, "Multi-bus PC transport and G16 reference contract");
    Check(TestSerialPortCatalog() == 0, "COM catalog and read-only device enumeration");
    Check(TestF13BoardSupport() == 0, "F13 benchmark clock and boot/app CAN handoff");
    Check(TestF24CanClock() == 0, "F24 CAN clock fault/fallback tests");
    TestCrcKnownVector();
    TestRoundTripAndLimits();
    TestDecodeFailures();
    TestSplitAndConcatenatedFrames();
    TestNoiseOverlapAndRecovery();
    TestBoundedInputBuffer();
    TestClientProbeAndPing();
    TestClientRetryMatchingAndEvents();
    TestClientNackIsNotRetried();
    TestClientCancellation();
    TestClientResetAndBackgroundEvents();
    TestCanHostClientCommandsAndValidation();
    TestCanBitratePresets();
    TestBridgeClockErrorAndLegacyTxResponse();
    TestFirmwareUpdateProfileDispatch();
    TestF13BootImageAndFraming();
    TestF13BootClientTransactions();
    TestF13BootUpdateFaultInjection();
    TestF13TransferProfile();
    TestF13TransferClientWorkflow();

    if (g_failures != 0) {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "Host Link v2, CAN client, F13 Boot, and Custom Transfer tests passed\n";
    return 0;
}
