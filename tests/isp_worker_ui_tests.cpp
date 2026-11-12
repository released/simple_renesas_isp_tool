// Actual MFC frame and worker event path. Synthetic state only; no COM opens.
#include <afxwin.h>
#include <afxcmn.h>
#include <atlimage.h>
#include <iostream>
#include <algorithm>
#include <iterator>
#include "../src/main_frame.h"

CWinApp test_app;
struct WorkerResponsivenessFixture {
    static int Run() {
        using namespace firmware_upgrade;
        if(!AfxWinInit(GetModuleHandleW(nullptr),nullptr,GetCommandLineW(),SW_HIDE)||!AfxInitRichEdit2())return 2;
        int failures=0;
        const auto check=[&](bool okay,const char* reason){if(!okay){++failures;std::cerr<<"FAIL "<<reason<<'\n';}};
        auto* frame=new CMainFrame();
        if(!frame->Create(nullptr,L"Offline ISP outcome verification - no COM",WS_OVERLAPPEDWINDOW,CRect(0,0,1366,768)))return 3;
        test_app.m_pMainWnd=frame;
        frame->worker_session_.StopAndJoin();
        while(frame->worker_session_.HasPendingEvents())frame->worker_session_.DrainEvents();
        auto& worker=frame->worker_session_;
        worker.stopping_=false;worker.cancel_requested_=false;
        unsigned infos=0U;bool fail_final=true,fail_echo=false;
        worker.state_.isp_contract=2U;worker.state_.uart_configuration.baud=100000U;
        worker.isp_client_.SetBusConfiguration(2U,worker.state_.uart_configuration);
        worker.isp_client_.SetExchange([&](const std::vector<std::uint8_t>& bytes,std::uint32_t,
            std::vector<std::uint8_t>* reply,std::wstring* error) {
            IspPacket request={},response={};
            if(!IspDecode(bytes.data(),static_cast<unsigned short>(bytes.size()),&request))return false;
            response=request;response.response=1U;
            if(request.command==ISP_CMD_INFO) {
                if(++infos==2U&&fail_final){*error=L"Injected final INFO failure.";return false;}
                response.length=88U;std::fill(std::begin(response.data),std::end(response.data),std::uint8_t{0U});
                response.data[0]=2U;response.data[1]=ISP_MODE_TRANSFER;response.data[2]=IP_I2C;response.data[3]=64U;
                response.data[4]=0x4BU;response.data[5]=0xF2U;response.data[54]=0x10U;
                IspWrite32(response.data+36U,100000U);
                const char part[]="R7F124FPJ5xFB",family[]="RL78/F24";
                std::copy(std::begin(part),std::end(part),response.data+56U);
                std::copy(std::begin(family),std::end(family),response.data+72U);
            } else if(request.command==ISP_CMD_STATS) {
                response.length=16U;std::fill_n(response.data,16U,std::uint8_t{0U});
            } else if(request.command==ISP_CMD_ECHO&&fail_echo)response.data[0]^=1U;
            unsigned char wire[ISP_FRAME_MAX]={};const auto count=IspEncode(&response,wire);
            reply->assign(wire,wire+count);return true;
        });
        std::wstring transfer_error;
        check(!worker.ProcessIspTransferTest(2U,&transfer_error)&&infos==2U,"actual worker fails when final INFO fails after data checks");
        std::wstring stages;
        for(const auto& event:worker.DrainEvents())stages+=event.text+L"\n";
        check(stages.find(L"PING/ECHO/counters passed")!=std::wstring::npos&&
            stages.find(L"final live INFO refresh failed")!=std::wstring::npos&&
            stages.find(L"final live INFO refresh passed")==std::wstring::npos,"worker reports accurate separate sub-results");
        infos=0U;fail_final=false;
        check(worker.ProcessIspTransferTest(2U,&transfer_error)&&infos==2U,"actual worker confirms final INFO on success");
        worker.DrainEvents();infos=0U;fail_echo=true;
        check(!worker.ProcessIspTransferTest(2U,&transfer_error)&&infos==1U&&
            transfer_error.find(L"I2C device Transfer")!=std::wstring::npos,"data failure stops before refresh and preserves I2C identity");
        worker.DrainEvents();
        frame->bus_tabs_.SetCurSel(1);
        frame->i2c_workflow_combo_.SetCurSel(1);
        frame->session_state_={};frame->session_state_.busy=true;frame->worker_command_pending_=true;
        frame->UpdateUi();
        CRect client;frame->GetClientRect(&client);frame->LayoutControls(client.Width(),client.Height());
        CString status,hint;
        frame->operation_status_card_.GetWindowTextW(status);
        frame->firmware_prerequisites_label_.GetWindowTextW(hint);
        check(status.Find(L"I2C device operation")>=0&&status.Find(L"UART device operation")<0,"I2C busy card uses selected transport");
        check(hint.Find(L"I2C connection")>=0&&hint.Find(L"UART1")<0,"I2C busy hint preserves the I2C connection");
        check(ui::WorkerSession::CommandName(ui::WorkerSession::CommandType::UartDiscover,2U)==L"I2C device discovery","worker error context uses I2C discovery");
        frame->ShowWindow(SW_SHOWNOACTIVATE);frame->UpdateWindow();
        CImage picture;picture.Create(client.Width(),client.Height(),32);
        HDC dc=picture.GetDC();frame->PrintWindow(CDC::FromHandle(dc),PW_CLIENTONLY);picture.ReleaseDC();
        check(SUCCEEDED(picture.Save(L"i2c_working.png")),"capture actual I2C working frame");
        frame->ShowWindow(SW_HIDE);
        ui::WorkerEvent terminal;terminal.kind=ui::WorkerEventKind::OperationComplete;
        terminal.text=L"ISP response framing/CRC/sequence/command mismatch. Device command was not repeated.";
        terminal.advice=guidance::Explain(terminal.text,guidance::Transport::I2c);
        terminal.guidance=guidance::Format(terminal.text,L"I2C device discovery",terminal.advice);
        frame->log_edit_.SetWindowTextW(L"");frame->closing_=true;
        frame->session_state_.busy=false;frame->ApplyWorkerEvent(terminal);
        CString log;frame->log_edit_.GetWindowTextW(log);
        check(log.Find(L"Suggested checks [Host / I2C device]")>=0&&log.Find(L"UART1")<0,"UI uses worker-owned advice without reclassification");
        check(frame->last_error_guidance_==terminal.guidance,"dialog/details retain identical worker guidance");
        frame->closing_=false;frame->SendMessageW(WM_CLOSE);
        std::cout<<"PASS actual MFC I2C busy state, worker context and terminal guidance\n";
        return failures?1:0;
    }
};
int wmain(){return WorkerResponsivenessFixture::Run();}
