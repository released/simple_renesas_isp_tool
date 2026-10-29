// Actual MFC frame/event pump with synthetic traffic only. No serial port opens.
#include <afxcmn.h>
#include <afxwin.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include "../src/main_frame.h"

CWinApp test_app;
int TestLogView();
namespace {
unsigned paints=0U;
ULONGLONG paint_requested=0U,maximum_paint_ms=0U;
unsigned heartbeats=0U;
ULONGLONG last_heartbeat=0U,maximum_heartbeat_ms=0U;
void CALLBACK Heartbeat(HWND,UINT,UINT_PTR,DWORD) {
    const auto now=GetTickCount64();
    if(last_heartbeat) maximum_heartbeat_ms=(std::max)(maximum_heartbeat_ms,now-last_heartbeat);
    last_heartbeat=now;
    ++heartbeats;
}
LRESULT CALLBACK ObservePaint(HWND hwnd,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
    if(msg==WM_PAINT) {
        ++paints;
        if(paint_requested) {
            maximum_paint_ms=(std::max)(maximum_paint_ms,GetTickCount64()-paint_requested);
            paint_requested=0U;
        }
    }
    return DefSubclassProc(hwnd,msg,w,l);
}
void DispatchPending() {
    MSG message={};
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
}

struct WorkerResponsivenessFixture {
static int Run(int argc,wchar_t** argv) {
    using namespace firmware_upgrade::ui;
    int failures=0;
    const auto check=[&](bool ok,const char* reason) {
        if(!ok) {++failures;std::cerr<<"FAIL: "<<reason<<'\n';}
    };
    if(!AfxWinInit(GetModuleHandleW(nullptr),nullptr,GetCommandLineW(),SW_HIDE) || !AfxInitRichEdit2()) return 2;
    check(TestLogView()==0,"actual RichEdit follow/pause/selection/batch behavior");
    WorkerEventQueue<WorkerEvent> queue;
    unsigned wakes=0U;
    for(int i=0;i<=100;++i) {
        WorkerEvent e;e.kind=WorkerEventKind::Progress;e.progress=i;e.text=L"Transfer";
        if(queue.Push(std::move(e))) ++wakes;
    }
    const auto latest=queue.Take(64U);
    check(wakes==1U && latest.size()==1U && latest[0].progress==100 && queue.TakeDropped()==0U,
        "duplicate percentage events coalesce into one wake/latest value without log-drop warning");
    WorkerEvent activity;activity.kind=WorkerEventKind::Progress;activity.text=L"Confirm App";activity.indeterminate=true;
    check(queue.Push(activity),"draining empty queue rearms notification");
    WorkerEvent state;state.kind=WorkerEventKind::State;
    queue.Push(state);
    WorkerEvent done;done.kind=WorkerEventKind::OperationComplete;done.text=L"canceled";
    queue.Push(done);
    check(queue.Take(1U).size()==1U && queue.Pending(),"bounded drain retains unprocessed events");
    const auto terminal=queue.Take(64U);
    check(terminal.size()==2U && terminal[1].kind==WorkerEventKind::OperationComplete,"state/completion retain order");
    for(int i=0;i<1000;++i) {WorkerEvent e;e.text=L"synthetic trace";queue.Push(std::move(e));}
    queue.Push(done);
    const auto drops=queue.TakeDropped();
    unsigned drained=0U;bool found_done=false;
    while(queue.Pending()) {
        const auto batch=queue.Take(64U);
        check(batch.size()<=64U,"overflow drain is bounded");
        drained+=static_cast<unsigned>(batch.size());
        for(const auto& e:batch) found_done=found_done || e.kind==WorkerEventKind::OperationComplete;
    }
    check(drops>0U && drained<=256U && found_done,"trace flood retains completion and exposes bounded overflow");

    auto* frame=new CMainFrame();
    if(!frame->Create(nullptr,L"Offline ISP UI stress - no board connection",WS_OVERLAPPEDWINDOW,CRect(0,0,1366,768))) return 3;
    test_app.m_pMainWnd=frame;
    frame->worker_session_.StopAndJoin();
    while(frame->worker_session_.HasPendingEvents()) frame->worker_session_.DrainEvents();
    frame->worker_session_.notify_window_=frame->GetSafeHwnd();
    frame->worker_session_.notify_message_=WM_APP+1U;
    frame->ShowWindow(SW_SHOWNOACTIVATE);
    frame->UpdateWindow();
    state.state.busy=true;
    frame->ApplyWorkerEvent(state);
    frame->ApplyWorkerEvent(activity);
    CString caption;frame->progress_label_.GetWindowTextW(caption);
    check(frame->progress_indeterminate_ && (frame->progress_bar_.GetStyle()&PBS_MARQUEE) && caption.Find(L"waiting...")>=0 && caption.Find(L"%")<0,
        "unknown App restart is animated and has no invented percentage");
    WorkerEvent progress;progress.kind=WorkerEventKind::Progress;progress.progress=73;progress.text=L"3/5 Transfer";
    frame->ApplyWorkerEvent(progress);
    check(!frame->progress_indeterminate_ && !(frame->progress_bar_.GetStyle()&PBS_MARQUEE) && frame->progress_bar_.GetPos()==73,
        "real transfer percentage stops marquee and restores determinate bar");
    frame->ApplyWorkerEvent(activity);
    frame->ApplyWorkerEvent(done);
    check(!frame->progress_indeterminate_ && frame->progress_bar_.GetPos()!=100 && frame->last_update_result_==firmware_upgrade::feedback::Result::None,
        "cancellation stops animation without manufacturing completion");
    frame->last_operation_error_.clear();
    frame->ApplyWorkerEvent(state);
    std::wstring history;
    for(int i=0;i<70000;++i) history+=L"offline retained history line 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n";
    frame->AppendLog(history);
    frame->StartLogRecording();
    const auto recording=frame->log_record_path_;
    SetWindowSubclass(frame->GetSafeHwnd(),ObservePaint,1U,0U);
    SetTimer(frame->GetSafeHwnd(),80U,20U,Heartbeat);
    const auto started=GetTickCount64();
    std::atomic<bool> producing{true};
    std::thread producer([&]() {
        for(int i=0;i<120;++i) {
            frame->worker_session_.PostProgress(i*100/119,L"3/5 Transfer");
            for(int j=0;j<10;++j) frame->worker_session_.PostLog(L"OFFLINE_RECORD_"+std::to_wstring(i*10+j)+L" synthetic trace payload 0123456789ABCDEF");
            Sleep(20U);
        }
        frame->worker_session_.PostActivity(L"5/5 App restart");
        producing=false;
    });
    unsigned requests=0U;
    while(producing || frame->worker_session_.HasPendingEvents()) {
        DispatchPending();
        if(GetTickCount64()-started>static_cast<ULONGLONG>(requests+1U)*600U && requests<3U) {
            paint_requested=GetTickCount64();
            frame->Invalidate(TRUE);
            ++requests;
        }
        if(GetTickCount64()-started>20000U) {check(false,"synthetic trace drains within deadline");break;}
        Sleep(1U);
    }
    producer.join();
    DispatchPending();
    frame->StopLogRecording(false);
    KillTimer(frame->GetSafeHwnd(),80U);
    RemoveWindowSubclass(frame->GetSafeHwnd(),ObservePaint,1U);
    std::ifstream input(recording,std::ios::binary);
    const std::string recorded((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    for(int i=0;i<1200;++i) check(recorded.find("OFFLINE_RECORD_"+std::to_string(i)+" synthetic")!=std::string::npos,"all accepted trace lines recorded");
    check(recorded.find("UI event queue overflow")==std::string::npos,"sustained synthetic trace has no log loss");
    check(heartbeats>20U && maximum_heartbeat_ms<500U && paints>=3U && maximum_paint_ms<500U,
        "paint/input timers run during trace with long RichEdit history");
    check(frame->progress_indeterminate_,"restart animation remains active after transfer");
    std::cout<<"UI stress: history_chars="<<history.size()<<", recorded_lines=1200, heartbeats="<<heartbeats
        <<", max_heartbeat_ms="<<maximum_heartbeat_ms<<", paints="<<paints<<", max_paint_ms="<<maximum_paint_ms<<'\n';
    if(argc>1 && failures==0) {
        const unsigned seconds=static_cast<unsigned>(_wtoi(argv[1]));
        const auto until=GetTickCount64()+static_cast<ULONGLONG>(seconds)*1000U;
        frame->SetWindowTextW(L"Offline ISP UI preview - synthetic traffic, no COM");
        unsigned tick=0U;
        while(GetTickCount64()<until && IsWindow(frame->GetSafeHwnd())) {
            frame->worker_session_.PostLog(L"Offline preview heartbeat "+std::to_wstring(tick++));
            frame->worker_session_.PostActivity(L"5/5 Check APP_READY");
            for(int i=0;i<20;++i) {DispatchPending();Sleep(10U);}
        }
    }
    frame->StopProgressActivity();
    frame->SendMessageW(WM_CLOSE);
    std::cout<<"Worker queue / real frame / waiting progress tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures?1:0;
}
};
int wmain(int argc,wchar_t** argv) {return WorkerResponsivenessFixture::Run(argc,argv);}
