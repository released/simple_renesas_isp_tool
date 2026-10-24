#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
extern "C" {
#include "isp_i2c_mailbox.h"
#include "isp_xmodem.h"
}
namespace {
std::vector<unsigned char> written, controls;
unsigned writes=0, finishes=0, aborts=0;
bool finish_ok=true;
unsigned char Write(unsigned long at,const unsigned char* p,unsigned short n) {
    assert(at==written.size()); written.insert(written.end(),p,p+n);++writes;return 1;
}
unsigned char Finish(){++finishes;return finish_ok?1U:0U;}
void Abort(){++aborts;}
void Send(unsigned char b){controls.push_back(b);}
const IspXmodemOps ops={Write,Finish,Abort,Send};
void Reset(){written.clear();controls.clear();writes=finishes=aborts=0;finish_ok=true;}
std::vector<unsigned char> Block(unsigned char number,unsigned char seed=0U) {
    std::vector<unsigned char> p{1U,number,static_cast<unsigned char>(~number)};
    for(unsigned i=0;i<128U;++i)p.push_back(static_cast<unsigned char>(i+seed));
    auto crc=IspXmodemCrc(p.data()+3,128U);p.push_back(static_cast<unsigned char>(crc>>8));p.push_back(static_cast<unsigned char>(crc));return p;
}
void Feed(IspXmodem& x,const std::vector<unsigned char>& p,unsigned short now=10U){for(auto b:p)IspXmodemFeed(&x,b,now,&ops);}
void RamPayloads(){
    unsigned char source[128],copy[130];
    for(unsigned i=0U;i<128U;++i)source[i]=static_cast<unsigned char>(i);
    for(unsigned n=0U;n<=128U;++n){
        std::memset(copy,0xA5,sizeof(copy));IspBytesCopy(copy+1U,source,static_cast<unsigned char>(n));
        assert(copy[0]==0xA5&&copy[n+1U]==0xA5&&IspBytesEqual(copy+1U,source,static_cast<unsigned char>(n)));
        if(n){copy[n]^=1U;assert(!IspBytesEqual(copy+1U,source,static_cast<unsigned char>(n)));}
    }
}
void Xmodem() {
    assert(IspXmodemCrc(reinterpret_cast<const unsigned char*>("123456789"),9U)==0x31C3U);
    Reset();
#if defined(ISP_TEST_XMODEM_CONTEXT)
    IspXmodem& x=g_isp_input.x;
#else
    IspXmodem x{};
#endif
    IspXmodemInit(&x,256U,0U);
    IspXmodemPoll(&x,999U,&ops);assert(controls.empty());IspXmodemPoll(&x,1000U,&ops);assert(controls.back()=='C');
    auto p=Block(1U);Feed(x,p);assert(writes==1&&controls.back()==6U);
    Feed(x,p);assert(writes==1&&controls.back()==6U);
    p=Block(2U);p.back()^=1U;Feed(x,p);assert(writes==1&&controls.back()==0x15U);
    p=Block(2U);Feed(x,p);assert(writes==2);Feed(x,{4U});assert(finishes==1&&!x.active&&controls.back()==6U);
    Feed(x,{4U});assert(finishes==1); // lost EOT ACK never repeats finalization
    Reset();IspXmodemInit(&x,128U,0U);Feed(x,Block(1));Feed(x,Block(1,1));assert(aborts==1&&writes==1&&!x.active);
    for(unsigned at=0U;at<128U;++at){
        Reset();IspXmodemInit(&x,128U,0U);p=Block(1U);Feed(x,p);p[at+3U]^=1U;
        auto changed_crc=IspXmodemCrc(p.data()+3U,128U);p[131]=static_cast<unsigned char>(changed_crc>>8);p[132]=static_cast<unsigned char>(changed_crc);
        Feed(x,p);assert(aborts==1&&writes==1&&!x.active);
    }
    Reset();IspXmodemInit(&x,4U,0U);p=Block(1U);Feed(x,p);assert(writes==0&&aborts==1);
    Reset();IspXmodemInit(&x,4U,0U);p=Block(1U);std::fill(p.begin()+7,p.begin()+131,static_cast<unsigned char>(0x1AU));
    auto crc=IspXmodemCrc(p.data()+3,128);p[131]=static_cast<unsigned char>(crc>>8);p[132]=static_cast<unsigned char>(crc);
    Feed(x,p);assert(written.size()==4);finish_ok=false;Feed(x,{4U});assert(finishes==1&&aborts==1&&controls.back()==0x18);
    Reset();IspXmodemInit(&x,128U,65530U);Feed(x,{1U,1U,0xFEU},65530U);IspXmodemPoll(&x,194U,&ops);assert(x.count==0&&controls.back()==0x15);
    Feed(x,{0x18,0x18},195U);assert(!x.active&&aborts==1);
    Reset();IspXmodemInit(&x,128U*257U,0U);for(unsigned i=0;i<257U;++i)Feed(x,Block(static_cast<unsigned char>(i+1U)));
    Feed(x,{4U});assert(writes==257&&finishes==1&&aborts==0);
    Reset();IspXmodemInit(&x,128U,0U);IspXmodemPoll(&x,10000U,&ops);assert(!x.active&&aborts==1);
}
void Request(IspI2cMailbox& b){assert(IspMailboxStart(&b,0U));for(unsigned i=0;i<14U;++i)assert(IspMailboxReceive(&b,static_cast<unsigned char>(i)));IspMailboxStop(&b);}
void Drain(IspI2cMailbox& b){unsigned char v=0;for(unsigned i=0;i<14U;++i){assert(IspMailboxGet(&b,&v));assert(v==i);}assert(!IspMailboxGet(&b,&v));}
void Mailbox(){
    IspI2cMailbox b{};unsigned char p[14];std::memset(p,0xCC,sizeof(p));
    IspMailboxInit(&b);Request(b);assert(b.ready&&b.busy);assert(!IspMailboxStart(&b,0U));IspMailboxStop(&b);Drain(b);
    assert(IspMailboxStart(&b,1U));assert(IspMailboxNext(&b)==0U);assert(IspMailboxPublish(&b,p,14U));
    for(unsigned i=1;i<111;++i)assert(IspMailboxNext(&b)==0xFFU);IspMailboxNack(&b);IspMailboxStop(&b);assert(!b.reply_done&&b.busy);
    assert(IspMailboxStart(&b,1U));assert(IspMailboxNext(&b)==0xA5U);IspMailboxNack(&b);IspMailboxStop(&b);assert(b.busy&&!b.reply_done);
    assert(IspMailboxStart(&b,1U));assert(IspMailboxNext(&b)==0xA5U);
    for(unsigned i=1;i<111;++i)assert(IspMailboxNext(&b)==(i<=14?0xCC:0xFF));
    IspMailboxStop(&b);assert(b.busy); // no final NACK, no commit
    assert(IspMailboxStart(&b,1U));for(unsigned i=0;i<111;++i)(void)IspMailboxNext(&b);
    IspMailboxNack(&b);IspMailboxStop(&b);assert(!b.busy&&b.reply_done);
    Request(b);Drain(b);IspMailboxRelease(&b);assert(!b.busy); // malformed frame releases without action
    assert(IspMailboxStart(&b,0U));for(unsigned i=0;i<110U;++i)assert(IspMailboxReceive(&b,0U));
    assert(!IspMailboxReceive(&b,0));IspMailboxStop(&b);assert(!b.busy&&!b.ready);
}
}
extern "C" {
unsigned char IspXmodemWrite(unsigned long at,const unsigned char* p,unsigned short n){return Write(at,p,n);}
unsigned char IspXmodemFinish(){return Finish();}
void IspXmodemAbort(){Abort();}
void IspXmodemSend(unsigned char b){Send(b);}
}
int main(){RamPayloads();Xmodem();Mailbox();std::cout<<"PASS actual C RAM payloads/XMODEM-CRC128/replay/wrap/padding/cancel/EOT and I2C mailbox ownership\n";}
