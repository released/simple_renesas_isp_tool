#include "operation_guidance.h"
#include "isp_bus_settings.h"
#include "isp_program_evidence.h"
#include "update_report_view.h"
#include "../src/ui/update_feedback.h"
#include <iostream>
#include <stdexcept>

int main() {
    using namespace firmware_upgrade;
    const auto check=[](bool okay,const char* text) {if(!okay)throw std::runtime_error(text);};
    try {
        const std::wstring error=L"ISP response framing/CRC/sequence/command mismatch. Device command was not repeated.";
        check(guidance::Explain(error,guidance::Transport::I2c).layer==L"Host / I2C device","I2C frame failure has I2C advice");
        check(guidance::Explain(error,guidance::Transport::Uart).layer==L"Bridge / UART device","UART frame failure has UART advice");
        check(guidance::Explain(error,guidance::Transport::Xmodem).layer==L"Bridge / Xmodem device","Xmodem frame failure has Xmodem advice");
        check(guidance::Explain(L"Host returned I2C_NACK for command 0x41.").layer==L"Host / I2C device","existing returned bus status preserved");
        check(guidance::Explain(L"Host returned TIMEOUT for command 0x21.").layer==L"Host / device bus","CAN transaction timeout preserved");
        check(guidance::Explain(L"Host Link response timeout after retries.",guidance::Transport::I2c).layer==L"PC / Host Link","I2C operation can still fail at PC link");
        check(guidance::Explain(L"Image metadata mismatch.",guidance::Transport::I2c).layer==L"Image / device contract","image failures retain image guidance");
        check(guidance::Explain(L"END CRC succeeded; APP_READY unconfirmed.",guidance::Transport::I2c).layer==L"App confirmation","unknown App outcome keeps no-rewrite guidance");
        for(const auto contract:{1U,2U,3U}) {
            const auto name=isp::OperationName(isp::Operation::Discover,static_cast<std::uint8_t>(contract));
            check(name.find(isp::ProtocolName(static_cast<std::uint8_t>(contract)))==0U,"operation name uses selected contract");
        }
        isp::ProgramState state;
        check(state.MayAbortDevice(),"unsent END permits existing pre-END cleanup");
        state.last_confirmed=isp::ProgramStage::Write;state.end=isp::RequestOutcome::Unconfirmed;state.accepted_bytes=983040U;
        check(!state.MayAbortDevice(),"unknown END blocks device ABORT");
        const auto json=isp::ProgramEvidenceJson(state);
        check(json.find(L"\"end\":\"unconfirmed\"")!=std::wstring::npos&&json.find(L"983040")!=std::wstring::npos,"structured unknown evidence");
        evidence::ReportView view;view.result=L"FAIL";view.fields={{L"Device program state",isp::ProgramEvidenceText(state)}};
        check(evidence::RenderReport(view).find(L"END/EOT=unconfirmed")!=std::wstring::npos,"HTML preserves the same unknown evidence");
        const auto advice=guidance::Explain(error,guidance::Transport::I2c);
        const auto formatted=guidance::Format(error,L"I2C device discovery",advice);
        check(formatted.find(L"UART1")==std::wstring::npos&&formatted.find(error)!=std::wstring::npos,"shared advice keeps exact error without UART wiring hints");
        using feedback::Result;
        const auto confirmed=feedback::Describe(Result::DeviceAppConfirmed);
        check(confirmed.status.find(L"startup confirmed")!=std::wstring::npos &&
            confirmed.status.find(L"CAN stopped")!=std::wstring::npos &&
            confirmed.compact.find(L"Next update: Discover Target")!=std::wstring::npos,
            "CAN result and next action remain distinct");
        const auto serial=feedback::Describe(Result::UartAppConfirmed);
        check(serial.status.find(L"device bus released")!=std::wstring::npos &&
            serial.compact.find(L"CAN")==std::wstring::npos,"serial completion keeps its own bus result");
        for(const auto warning:{Result::UartAppConfirmedRxWarning,Result::DeviceAppConfirmedBusWarning,Result::DeviceAppConfirmedCanActive}) {
            const auto warning_view=feedback::Describe(warning);
            check(warning_view.warning && warning_view.status.find(L"startup confirmed")!=std::wstring::npos &&
                warning_view.next.find(L"Do not repeat")!=std::wstring::npos &&
                feedback::ReportResult(true,false,warning)==L"PASS_WITH_WARNINGS",
                "confirmed App warnings retain caution without replay instructions");
        }
        check(feedback::Describe(Result::DeviceAppConfirmedCanActive).compact.find(L"Close COM before changing CAN settings")!=std::wstring::npos,
            "unconfirmed CAN stop retains visible recovery action");
        const auto written=feedback::Describe(Result::HostAppWritten);
        check(written.status.find(L"still in Boot")!=std::wstring::npos &&
            written.compact.find(L"Run Validated Bridge App")!=std::wstring::npos &&
            written.compact.find(L"App has not started")!=std::wstring::npos,
            "Host write never implies App execution");
        check(feedback::Describe(Result::HostAppRunning).status.find(L"image identity confirmed")!=std::wstring::npos,
            "Host RUN confirmation retains image identity");
        check(feedback::Describe(Result::None).status.empty() &&
            feedback::ReportResult(false,false,Result::DeviceAppConfirmed)==L"FAIL" &&
            feedback::ReportResult(false,true,Result::DeviceAppConfirmed)==L"CANCELED",
            "unknown failed canceled results never inherit successful report tone");
        check(guidance::Explain(L"END CRC succeeded; APP_READY unconfirmed.").next.find(L"Do not repeat erase or RUN")!=std::wstring::npos,
            "unknown App guidance preserves no-repeat condition");
        std::cout<<"PASS UI completion/warning/unknown-result copy and typed report distinctions\n";
        std::cout<<"PASS transport-aware guidance, names and structured update evidence\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
