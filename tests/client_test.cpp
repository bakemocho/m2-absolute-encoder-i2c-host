#include "M2Client.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace m2enc;
static uint32_t now;
static uint32_t clockNow() { return now; }
struct Bus {
    std::array<uint8_t,32> frame{};
    std::vector<uint8_t> tx;
    size_t pos=0;
    uint8_t nack=0;
    bool corrupt=false, short_read=false, empty=false, short_write=false;
    uint32_t latency=0;
    unsigned transactions=0;
    void beginTransmission(uint8_t a) { assert(a==M2_ADDRESS); tx.clear(); ++transactions; }
    size_t write(uint8_t v) { tx.push_back(v); return short_write ? 0 : 1; }
    size_t write(const uint8_t* p,size_t n) { tx.assign(p,p+n); return short_write ? n-1 : n; }
    uint8_t endTransmission(bool stop) { assert(stop==(tx.size()!=1)); return nack; }
    size_t requestFrom(uint8_t a,uint8_t n,uint8_t stop) {
        assert(a==M2_ADDRESS && n==32 && stop);
        frame[31]=em_crc(frame.data(),31); if(corrupt) frame[31]^=1;
        pos=0; now+=latency; return short_read ? 7 : 32;
    }
    int available() { return empty ? 0 : int(32-pos); }
    int read() { return available() ? frame[pos++] : -1; }
    void page(uint8_t magic) { frame.fill(0); frame[0]=magic; frame[1]=M2_PROTOCOL; }
    void status(uint16_t receipt=0) {
        page(0xe1); em_p16(frame.data()+2,receipt); em_p16(frame.data()+22,receipt);
        frame[5]=M2_FIXED; frame[6]=2; frame[7]=Absolute; em_p16(frame.data()+8,42);
        frame[24]=M2_CORE_VERSION; frame[25]=3; em_p32(frame.data()+26,1000);
    }
};
int main() {
    Bus bus; bus.status(); Client<Bus> c(bus,clockNow); Reading r;
    assert(c.start()==NotBegun && c.begin()==Ok && c.ready());
    Timing timing={100,20,5,false};
    assert(c.configureAuto(2000,20000,timing)==Submitted);
    const auto command=bus.tx; const uint16_t id=c.pendingId();
    assert(id==1 && command.size()==25 && command[0]==M2_COMMAND && command[4]==M2_AUTO);
    assert(em_u32(command.data()+5)==2000 && em_u32(command.data()+9)==20000);
    assert(em_u32(command.data()+13)==100 && em_u32(command.data()+17)==20);
    assert(command[21]==5 && command[24]==em_crc(command.data()+1,23));
    assert(c.start()==Busy && c.begin()==Busy);
    assert(c.poll(r)==Ok && c.pendingId()==id);
    bus.nack=4; assert(c.retry()==BusError && c.pendingId()==id);
    bus.nack=0; assert(c.retry()==Submitted && bus.tx==command);
    bus.status(id); bus.frame[4]=M2_RANGE;
    assert(c.poll(r)==Ok && r.command_result==M2_RANGE && !c.pendingId());
    assert(c.start()==Submitted); const auto previous=c.pendingId();
    assert(c.stop()==Submitted && c.pendingId()!=previous && bus.tx[4]==M2_STOP);
    bus.status(c.pendingId()); assert(c.poll(r)==Ok && !c.pendingId());
    assert(r.usable(now) && !r.usable(now+1000));
    float angle=0; assert(r.angleDegrees(now,angle) && angle>8.39f && angle<8.41f);
    bus.latency=1000; assert(c.poll(r)==Ok && !r.valid); bus.latency=0;
    for(uint8_t flag : {uint8_t(NeedMotion),uint8_t(ConfigurationError),uint8_t(Probation)}) {
        bus.frame[7]=Absolute|flag; assert(c.poll(r)==Ok && !r.valid);
    }
    bus.frame[7]=Absolute|Degraded|BitFault; assert(c.poll(r)==Ok && r.valid);
    bus.corrupt=true; assert(c.poll(r)==CorruptResponse && !r.valid); bus.corrupt=false;
    bus.short_read=true; assert(c.poll(r)==ShortRead && !r.valid); bus.short_read=false;
    bus.empty=true; assert(c.poll(r)==ShortRead && !r.valid); bus.empty=false;
    bus.short_write=true; assert(c.poll(r)==BusError); bus.short_write=false;
    bus.nack=2; assert(c.poll(r)==BusError && !r.valid); bus.nack=0;
    bus.frame[1]=2; assert(c.poll(r)==UnsupportedProtocol && !r.valid);
    bus.frame[1]=1; bus.frame[24]=7; assert(c.poll(r)==UnsupportedProtocol && !c.ready());
    assert(c.start()==NotBegun);
    bus.status(65535); assert(c.begin()==Ok && c.start()==Submitted && c.pendingId()==1);
    bus.status(1); assert(c.poll(r)==Ok); bus.status();
    assert(c.poll(r)==DeviceRestarted && !r.valid && c.start()==NotBegun);
    assert(c.begin()==Ok);
    now=UINT32_MAX-100; bus.status(); assert(c.poll(r)==Ok && r.usable(500) && !r.usable(1000));
    bus.page(0xe8); bus.frame[2]=3; em_p16(bus.frame.data()+4,42);
    em_p32(bus.frame.data()+6,8300); em_p32(bus.frame.data()+10,100); em_p32(bus.frame.data()+18,1000);
    Position p; assert(c.readPosition(p)==Ok && p.angleDegrees(now,angle) && angle>8.29f && angle<8.31f);
    em_p32(bus.frame.data()+18,0x80000000UL); assert(c.readPosition(p)==Ok && !p.valid);
    bus.page(0xe2); bus.frame[4]=1; bus.frame[5]=M2_AUTO;
    em_p32(bus.frame.data()+6,2000); em_p32(bus.frame.data()+10,20000);
    em_p32(bus.frame.data()+14,100); em_p32(bus.frame.data()+18,20); bus.frame[22]=5;
    Configuration config; assert(c.readConfiguration(config)==Ok && config.valid && config.max_period_us==20000);
    assert(config.timing.settle_us==100 && config.timing.stable_reads==5);
    bus.frame[23]=2; assert(c.readConfiguration(config)==CorruptResponse && !config.valid);
    Candidates set; bus.page(0xe4); bus.frame[2]=3; em_p32(bus.frame.data()+4,11);
    em_p16(bus.frame.data()+8,1); em_p16(bus.frame.data()+16,42); em_p16(bus.frame.data()+18,1);
    em_p32(bus.frame.data()+20,100000);
    assert(c.readQuality(set.quality)==Ok && set.quality.usable(now));
    assert(set.quality.resolutionDegrees(angle) && angle==0.2f);
    for(uint8_t page=0;page<12;++page) {
        bus.page(0xe3); bus.frame[2]=page; bus.frame[3]=page==11 ? 16 : 19;
        em_p32(bus.frame.data()+4,11); em_p32(bus.frame.data()+8,100000);
        if(page==0) bus.frame[12+42/8]=1u<<(42%8);
        assert(c.readCandidatePage(page,set)==Ok);
    }
    assert(set.complete() && set.usable(now) && set.contains(42) && !set.contains(43));
    assert(set.countWithin(42,0)==1);
    CandidateOrigin origin; origin.valid=true; origin.capture_id=11; origin.offset_mdeg=100;
    origin.remaining_us=100000; origin.received_since_us=now;
    uint32_t adjusted=0; assert(set.angleMilliDegrees(origin,42,now,adjusted) && adjusted==8300);
    ++origin.capture_id; assert(!set.angleMilliDegrees(origin,42,now,adjusted));
    assert(c.readCandidatePage(12,set)==InvalidArgument && !set.complete());
    bus.frame[2]=0; em_p32(bus.frame.data()+4,12);
    assert(c.readCandidatePage(0,set)==SnapshotChanged && !set.complete());
    bus.page(0xe6); bus.frame[2]=1; bus.frame[23]=1; bus.frame[24]=WatchdogReset; bus.frame[25]=BootAttached;
    BoardDiagnostic diag; assert(c.readBoardDiagnostic(diag)==Ok && diag.valid && diag.reset_cause==WatchdogReset);
    bus.frame[25]=BootTimedOut; assert(c.readBoardDiagnostic(diag)==CorruptResponse && !diag.valid);
    bus.frame[23]=0; assert(c.readBoardDiagnostic(diag)==UnsupportedProtocol && !diag.valid);
    bus.page(0xe7); bus.frame[2]=3; Origin saved;
    assert(c.readOrigin(saved)==Ok && saved.valid && !saved.durable);
    bus.page(0xe5); bus.frame[2]=9; em_p16(bus.frame.data()+6,3); em_p16(bus.frame.data()+8,3);
    FaultMemory faults; assert(c.readFaultMemory(faults)==Ok && faults.durable && faults.confirmed==3);
    bus.corrupt=true; assert(c.readFaultMemory(faults)==CorruptResponse && !faults.durable); bus.corrupt=false;
    assert(c.setOriginMilliDegrees(360000)==InvalidArgument);
    assert(c.testSensors(0)==InvalidArgument && c.testSensors(0x400)==InvalidArgument);
    assert(c.testSensors(1,1)==InvalidArgument && c.acknowledgeRepairedSensor(10,1)==InvalidArgument);
    assert(c.acknowledgeRepairedSensor(0,0)==InvalidArgument);
    SensorTest sensor; assert(c.readSensorTest(10,sensor)==InvalidArgument && !sensor.valid);
    bus.page(0xeb); bus.frame[2]=1; assert(c.readSensorTest(0,sensor)==CorruptResponse);
    bus.page(0xea); bus.frame[2]=2; Maintenance maintenance;
    assert(c.readMaintenance(maintenance)==UnsupportedProtocol && !maintenance.valid);
    const unsigned before=bus.transactions;
    Client<Bus> no_clock(bus,nullptr); Quality quality; CandidateOrigin candidate_origin;
    assert(no_clock.begin()==InvalidArgument && no_clock.poll(r)==InvalidArgument);
    assert(no_clock.readPosition(p)==InvalidArgument && no_clock.readQuality(quality)==InvalidArgument);
    assert(no_clock.readCandidateOrigin(candidate_origin)==InvalidArgument);
    assert(no_clock.readMaintenance(maintenance)==InvalidArgument);
    Client<Bus> bad_address(bus,clockNow,0x80); assert(bad_address.begin()==InvalidArgument);
    assert(bus.transactions==before);
    std::puts("PASS client: receipts/retry/STOP/restart/CRC/short-read/TTL/wrap/versions/candidates/origin/diagnostics/arguments");
}
