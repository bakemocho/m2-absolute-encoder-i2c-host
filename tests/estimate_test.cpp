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
    bool corrupt=false;
    uint8_t nack=0;
    uint32_t latency=0;
    void beginTransmission(uint8_t a) { assert(a==M2_ADDRESS); tx.clear(); }
    size_t write(uint8_t v) { tx.push_back(v); return 1; }
    size_t write(const uint8_t* p,size_t n) { tx.assign(p,p+n); return n; }
    uint8_t endTransmission(bool) { return nack; }
    size_t requestFrom(uint8_t,uint8_t n,uint8_t) {
        assert(n==32); frame[31]=em_crc(frame.data(),31); if(corrupt) frame[31]^=1;
        pos=0; now+=latency; return 32;
    }
    int available() { return int(32-pos); }
    int read() { return available() ? frame[pos++] : -1; }
    /* an estimate frame as the sensor builds it */
    void estimate(uint16_t cell,uint8_t level,uint16_t detail,uint16_t confirmed,uint32_t age) {
        frame.fill(0); frame[0]=0xec; frame[1]=M2_PROTOCOL; frame[2]=1; frame[3]=2;
        em_p16(frame.data()+4,cell); frame[6]=level; frame[7]=33; em_p16(frame.data()+8,detail);
        em_p16(frame.data()+10,confirmed); em_p32(frame.data()+12,77); em_p32(frame.data()+16,age);
        em_p32(frame.data()+20,confirmed==0xffff?0:1000); frame[24]=M2_CORE_VERSION; frame[26]=Absolute;
    }
};
int main() {
    Bus bus; Client<Bus> c(bus,clockNow); Estimate e; float deg=0;
    /* a good estimate, not confirmed */
    bus.estimate(500,Good,0x0200,0xffff,20000);
    assert(c.readEstimate(e)==Ok && bus.tx.size()==1 && bus.tx[0]==M2_ESTIMATE);
    assert(e.valid && e.cell==500 && e.level==Good && e.margin==33 && e.core==2 && e.sequence==77);
    assert(e.confirmed_cell==0xffff && e.cautions==Settling && e.diagnostic==0x0200);
    assert(e.atLeast(Good,now,100000) && e.atLeast(Doubtful,now,100000) && !e.atLeast(Confirmed,now,100000));
    assert(e.angleDegrees(Good,now,100000,deg) && deg>99.9f && deg<100.1f);
    /* freshness: the age reported by the sensor plus the time since the read */
    assert(!e.atLeast(Good,now,20000) && e.atLeast(Good,now+79999,100000) && !e.atLeast(Good,now+80000,100000));
    assert(!e.atLeast(NoEstimate,now,100000));
    bus.latency=90000; assert(c.readEstimate(e)==Ok && !e.atLeast(Good,now,100000)); bus.latency=0;
    /* confirmed: both cells equal */
    bus.estimate(42,Confirmed,0,42,5000);
    assert(c.readEstimate(e)==Ok && e.level==Confirmed && e.confirmed_cell==42 && !e.cautions && e.valid_for_us==1000);
    assert(e.atLeast(Confirmed,now,100000));
    /* the four caution groups */
    struct { uint16_t detail; uint8_t caution; } groups[]={{0x0001,ShaftFast},{0x0040,ShaftFast},{0x0002,SensorDoubt},
        {0x0004,SensorDoubt},{0x0080,SensorDoubt},{0x0100,SensorDoubt},{0x0400,SensorDoubt},{0x0008,InstallationCheck},
        {0x0010,InstallationCheck},{0x0020,Settling},{0x0200,Settling}};
    for(auto g : groups) { bus.estimate(7,Doubtful,g.detail,0xffff,1000); assert(c.readEstimate(e)==Ok && e.cautions==g.caution); }
    bus.estimate(7,Doubtful,0x07ff,0xffff,1000);
    assert(c.readEstimate(e)==Ok && e.cautions==(ShaftFast|SensorDoubt|InstallationCheck|Settling));
    assert((M2_EST_FAST|M2_EST_SENSOR|M2_EST_INSTALL|M2_EST_SETTLING)==0x07ff);
    assert(!(M2_EST_FAST&M2_EST_SENSOR) && !(M2_EST_FAST&M2_EST_INSTALL) && !(M2_EST_FAST&M2_EST_SETTLING));
    assert(!(M2_EST_SENSOR&M2_EST_INSTALL) && !(M2_EST_SENSOR&M2_EST_SETTLING) && !(M2_EST_INSTALL&M2_EST_SETTLING));
    /* no estimate, a guess, stale sensor reads */
    bus.estimate(0xffff,NoEstimate,0,0xffff,UINT32_MAX);
    assert(c.readEstimate(e)==Ok && e.valid && e.level==NoEstimate && !e.atLeast(Guess,now,100000));
    bus.estimate(900,Guess,0x0400,0xffff,UINT32_MAX);
    assert(c.readEstimate(e)==Ok && e.level==Guess && e.cautions==SensorDoubt && !e.atLeast(Guess,now,100000));
    /* frames that break the contract are refused whole */
    bus.estimate(500,5,0,0xffff,1000); assert(c.readEstimate(e)==CorruptResponse && !e.valid);
    bus.estimate(1800,Good,0,0xffff,1000); assert(c.readEstimate(e)==CorruptResponse && !e.valid);
    bus.estimate(0xffff,Good,0,0xffff,1000); assert(c.readEstimate(e)==CorruptResponse);
    bus.estimate(500,NoEstimate,0,0xffff,1000); assert(c.readEstimate(e)==CorruptResponse);
    bus.estimate(500,Confirmed,0,0xffff,1000); assert(c.readEstimate(e)==CorruptResponse);
    bus.estimate(500,Confirmed,0,501,1000); assert(c.readEstimate(e)==CorruptResponse);
    bus.estimate(500,Good,0,500,1000); assert(c.readEstimate(e)==CorruptResponse);
    bus.estimate(500,Good,0,0xffff,1000); bus.frame[2]=2; assert(c.readEstimate(e)==UnsupportedProtocol && !e.valid);
    bus.estimate(500,Good,0,0xffff,1000); bus.frame[1]=2; assert(c.readEstimate(e)==UnsupportedProtocol);
    bus.estimate(500,Good,0,0xffff,1000); bus.corrupt=true; assert(c.readEstimate(e)==CorruptResponse && !e.valid); bus.corrupt=false;
    /* a sensor without the frame: the selector is refused and whatever comes back is not an estimate */
    bus.frame.fill(0xff); assert(c.readEstimate(e)!=Ok && !e.valid);
    bus.frame.fill(0); bus.frame[0]=0xe1; bus.frame[1]=M2_PROTOCOL; assert(c.readEstimate(e)==CorruptResponse && !e.valid);
    bus.nack=2; assert(c.readEstimate(e)==BusError && !e.valid); bus.nack=0;
    /* reading the estimate needs no begin() and leaves the command state alone */
    assert(!c.ready() && !c.pendingId());
    std::puts("estimate_test ok");
    return 0;
}
