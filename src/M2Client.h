#ifndef M2_ENCODER_CLIENT_H
#define M2_ENCODER_CLIENT_H
#include <stddef.h>
#include <string.h>
#include "M2Protocol.h"
namespace m2enc {
enum Result { Ok=0, Submitted, Busy, NotBegun, BusError, ShortRead,
              CorruptResponse, UnsupportedProtocol, DeviceRestarted, SnapshotChanged, InvalidArgument };
struct Timing {
    uint32_t settle_us,blank_us;
    uint8_t stable_reads;
    bool inverted_input;
};
struct Configuration {
    bool valid=false,configured=false;
    uint8_t mode=0;
    uint16_t last_command_id=0,gaps=0;
    uint32_t min_period_us=0,max_period_us=0,overruns=0;
    Timing timing={0,0,0,false};
};
struct Reading {
    uint16_t command_id=0,last_command_id=0,cell=0xffff;
    uint8_t command_result=0,mode=0,state=M2_STOPPED,core_status=0,core_version=0,reason=0;
    uint32_t age_us=UINT32_MAX,period_us=0,sequence=0,valid_for_us=0,received_since_us=0;
    bool valid=false,configured=false,overrun=false;
    bool usable(uint32_t now_us) const {
        return valid && cell<1800 && valid_for_us>0 && valid_for_us<0x80000000UL && (uint32_t)(now_us-received_since_us)<valid_for_us;
    }
    /* Raw coordinate. Use Position for the configured origin. */
    bool angleDegrees(uint32_t now_us,float &degrees) const {
        if(!usable(now_us)) return false;
        degrees=cell*0.2f;return true;
    }
};
struct Origin {
    bool valid=false,supported=false,coordinate_ready=false,record_valid=false,busy=false,durable=false;
    uint8_t boot_status=0,error=0;
    uint32_t offset_mdeg=UINT32_MAX,pending_mdeg=UINT32_MAX,generation=0,completed_writes=0;
};
struct Position {
    bool valid=false,origin_ready=false,origin_busy=false;
    uint16_t raw_cell=0xffff;
    uint32_t angle_mdeg=UINT32_MAX,origin_mdeg=UINT32_MAX,origin_generation=0;
    uint32_t valid_for_us=0,received_since_us=0,sequence=0;
    bool usable(uint32_t now) const {
        return valid && origin_ready && raw_cell<1800 && angle_mdeg<360000UL &&
            valid_for_us>0 && valid_for_us<0x80000000UL &&
            (uint32_t)(now-received_since_us)<valid_for_us;
    }
    bool angleDegrees(uint32_t now,float &out) const {
        if(!usable(now))return false;out=angle_mdeg/1000.0f;return true;
    }
};
struct CandidateOrigin {
    bool valid=false;
    uint32_t capture_id=0,offset_mdeg=UINT32_MAX,generation=0,remaining_us=0,received_since_us=0;
    bool usable(uint32_t now) const {
        return valid && offset_mdeg<360000UL && remaining_us>0 && remaining_us<0x80000000UL && (uint32_t)(now-received_since_us)<remaining_us;
    }
};
struct Quality {
    uint32_t capture_id=0,remaining_us=0,age_us=0,received_since_us=0;
    uint16_t candidate_cells=0,dead_mask=0,suspect_mask=0,live_mask=0;
    uint16_t interval_start=0xffff,interval_cells=0xffff;
    bool complete=false,fresh=false,fault_persistence_supported=false,building=false;
    bool usable(uint32_t now_us) const {
        return complete && fresh && remaining_us>0 && remaining_us<0x80000000UL && (uint32_t)(now_us-received_since_us)<remaining_us;
    }
    bool resolutionDegrees(float &width) const {
        if(!complete || interval_cells==0xffff || !interval_cells || interval_cells>1800)return false;
        width=interval_cells*0.2f;return true; // optical region width, not absolute accuracy
    }
};
struct FaultMemory {
    uint8_t boot_status=0,error=0;
    uint16_t confirmed=0,durable_mask=0xffff,suspect=0;
    uint32_t generation=0,table_tag=0,map_tag=0,completed_writes=0;
    bool supported=false,record_valid=false,busy=false,durable=false,clear_pending=false;
};
enum BootState { BootWaiting=0, BootAttached=1, BootTimedOut=2, BootBackendMissing=3 };
enum ResetCauseFlags {
    PowerOnReset=0x01, ExternalReset=0x02, BrownOutReset=0x04,
    WatchdogReset=0x08, JtagReset=0x10
};
struct BoardDiagnostic {
    bool valid=false,boot_ready=false;
    bool boot_diagnostics_supported=false;
    uint8_t boot_revision=0,reset_cause=0,boot_state=0,boot_wait_mask=0;
    uint32_t uptime_ms=0;
    uint8_t last_fault=0,map_version=0;
    uint16_t fault_count=0;
    uint32_t map_tag=0,service_limit_us=0,bus_limit_us=0,led_slack_us=0;
};
struct Maintenance {
    bool valid=false,durable=false,running=false,configured=false;
    uint8_t revision=0,state=M2_MT_NONE,sensor=0,cycles=0,cycle=0,phase=0,restore_sensor=0,error=0;
    uint16_t test_id=0,requested=0,completed=0,response=0,confirmed=0,suspect=0,durable_mask=0xffff;
    uint32_t remaining_us=0,received_since_us=0;
};
struct SensorTest {
    bool valid=false,restore_eligible=false,confirmed=false,suspect=false;
    uint16_t test_id=0;
    uint8_t sensor=0,result=M2_SR_UNTESTED,responses=0,unchanged=0,inconsistent=0,last_levels=0,cycles=0;
    /* Results are historical. restore_eligible is advisory at read time only;
     * the device rechecks the proof and storage when receiving RESTORE. */
};
struct Candidates {
    Quality quality;
    uint8_t bits[M2_CANDIDATE_BYTES]={0};uint16_t pages=0;
    bool complete() const {return quality.complete && pages==0xfff;}
    bool usable(uint32_t now_us) const {return complete() && quality.usable(now_us);}
    bool contains(uint16_t cell) const {return complete() && cell<1800 && (bits[cell>>3]&(1u<<(cell&7)));}
    bool angleMilliDegrees(const CandidateOrigin &o,uint16_t cell,uint32_t now,uint32_t &out) const {
        if(!usable(now) || !o.usable(now) || o.capture_id!=quality.capture_id || !contains(cell))return false;
        out=((uint32_t)cell*200UL+360000UL-o.offset_mdeg)%360000UL;return true;
    }
    /* Discrete cell arc, conservative tolerance chosen by the caller. Does NOT
     * alter the encoder lock, assign probabilities, or prove BNO calibration. */
    uint16_t countWithin(uint16_t center,uint16_t radius) const {
        if(!complete() || center>=1800)return 0;
        uint16_t n=0;
        for(uint16_t cell=0;cell<1800;cell++)if(contains(cell)) {
            uint16_t dist=(uint16_t)((cell+1800-center)%1800);if(dist>900)dist=(uint16_t)(1800-dist);
            if(dist<=radius)n++;
        }
        return n;
    }
};
/* Wire-compatible bus; caller configures its pins/clock and bounded timeout.
 * One outstanding command. No delay, infinite polling, reconnect, EEPROM,
 * or automatic restart. A bus ACK means Submitted, never application OK. */
/* Storage error byte shared by FaultMemory, Origin and Maintenance.
 * Timeout means completion is uncertain, not that EEPROM stayed unwritten.
 * Do not retry with a new id, erase history, or auto-START after this error. */
enum StorageError { StorageNone=0, StorageEmpty=1, StorageRestored=2,
    StorageRecovered=3, StorageCorrupt=4, StorageBinding=5, StorageIo=6,
    StorageAmbiguous=7, StorageTimeout=8 };
template<class Bus> class Client {
    Bus &bus;
    uint32_t (*clock_us)();
    uint8_t address;
    uint16_t next_id=0,pending_id=0,observed_id=0;
    bool begun=false;
    uint8_t command[25]={0};
    uint32_t nowUs() const {return clock_us ? clock_us() : 0;}
    Result receive(uint8_t selector,uint8_t magic,uint8_t *bytes) {
        if(!clock_us || address==0 || address>=0x78)return InvalidArgument;
        bus.beginTransmission(address);
        size_t written=bus.write(selector);uint8_t status=bus.endTransmission(false);
        if(written!=1 || status)return BusError;
        size_t n=bus.requestFrom(address,(uint8_t)32,(uint8_t)true);
        if(n!=32){for(uint8_t i=0;i<32 && bus.available();++i)bus.read();return ShortRead;}
        for(uint8_t i=0;i<32;i++) {
            if(!bus.available())return ShortRead;
            int v=bus.read();if(v<0)return ShortRead;bytes[i]=(uint8_t)v;
        }
        if(em_crc(bytes,31)!=bytes[31] || bytes[0]!=magic)return CorruptResponse;
        return bytes[1]==M2_PROTOCOL ? Ok : UnsupportedProtocol;
    }
    Result transmit() {
        bus.beginTransmission(address);
        size_t n=bus.write(command,sizeof command);
        uint8_t status=bus.endTransmission(true);
        return n==sizeof command && !status ? Submitted : BusError;
    }
    Result submit(uint8_t op,const Timing *timing,uint32_t min_us,uint32_t max_us) {
        if(!begun) return NotBegun;
        if(pending_id) return Busy;
        memset(command,0,sizeof command);command[0]=M2_COMMAND;
        uint8_t *q=command+1;q[0]=M2_PROTOCOL;
        if(!++next_id) ++next_id;
        pending_id=next_id;em_p16(q+1,pending_id);q[3]=op;
        if(timing) {
            em_p32(q+4,min_us);em_p32(q+8,max_us);em_p32(q+12,timing->settle_us);
            em_p32(q+16,timing->blank_us);q[20]=timing->stable_reads;q[21]=timing->inverted_input;
        }
        if(op==M2_SET_ORIGIN)em_p32(q+4,min_us);
        if(op==M2_TEST_SENSORS) {em_p16(q+4,(uint16_t)min_us);q[6]=(uint8_t)max_us;}
        if(op==M2_RESTORE_SENSOR) {q[4]=(uint8_t)min_us;q[5]=M2_REPAIR_ACK;em_p16(q+6,(uint16_t)max_us);}
        q[23]=em_crc(q,23);
        /* On uncertain delivery retain the same id/payload for retry(). */
        return transmit();
    }
public:
    Client(Bus &b,uint32_t (*now_us)(),uint8_t addr=M2_ADDRESS):bus(b),clock_us(now_us),address(addr) {}
    Result begin() {
        if(!clock_us || address==0 || address>=0x78)return InvalidArgument;
        if(pending_id) return Busy;
        Reading r;Result result=poll(r);
        if(result==Ok) {next_id=r.last_command_id;observed_id=r.last_command_id;pending_id=0;begun=true;}
        return result;
    }
    uint16_t pendingId() const {return pending_id;}
    bool ready() const {return begun;}
    Result retry() {return begun && pending_id ? transmit() : NotBegun;}
    Result configureFixed(uint32_t period_us,const Timing &t) {return submit(M2_FIXED,&t,period_us,period_us);}
    Result configureAuto(uint32_t min_us,uint32_t max_us,const Timing &t) {return submit(M2_AUTO,&t,min_us,max_us);}
    Result readConfiguration(Configuration &c) {
        c=Configuration();uint8_t b[32];Result r=receive(M2_CONFIG,0xe2,b);
        if(r!=Ok)return r;
        c.last_command_id=em_u16(b+2);c.configured=b[4]!=0;c.mode=b[5];
        c.min_period_us=em_u32(b+6);c.max_period_us=em_u32(b+10);
        c.timing.settle_us=em_u32(b+14);c.timing.blank_us=em_u32(b+18);
        c.timing.stable_reads=b[22];c.timing.inverted_input=b[23]!=0;
        c.overruns=em_u32(b+24);c.gaps=em_u16(b+28);
        if(b[4]>1 || b[23]>1 || (c.configured && (c.mode!=M2_FIXED && c.mode!=M2_AUTO))) {
            c=Configuration();return CorruptResponse;
        }
        c.valid=true;return Ok;
    }
    Result start() {return submit(M2_START,nullptr,0,0);}
    /* STOP supersedes an uncertain/pending command with a newer serial id.
     * This cancels no physical bus transaction and is not a safety E-stop. */
    Result stop() {pending_id=0;return submit(M2_STOP,nullptr,0,0);}
    Result captureCandidates() {return submit(M2_CAPTURE,nullptr,0,0);}
    /* Durable-apply semantics: Submitted/M2_OK are acceptance, not storage
     * completion. Read Origin until durable and matching requested offset. */
    Result setOriginMilliDegrees(uint32_t offset) {
        if(offset>=360000UL)return InvalidArgument;
        return submit(M2_SET_ORIGIN,nullptr,offset,0);
    }
    Result zeroHere() {return submit(M2_ZERO_HERE,nullptr,0,0);}
    Result readOrigin(Origin &o) {
        o=Origin();uint8_t b[32];Result r=receive(M2_ORIGIN,0xe7,b);if(r!=Ok)return r;
        o.supported=(b[2]&1)!=0;o.coordinate_ready=(b[2]&2)!=0;o.record_valid=(b[2]&4)!=0;
        o.busy=(b[2]&8)!=0;o.durable=(b[2]&16)!=0;o.boot_status=b[3];o.error=b[4];
        o.offset_mdeg=em_u32(b+6);o.generation=em_u32(b+10);o.pending_mdeg=em_u32(b+14);
        o.completed_writes=em_u32(b+26);
        if(o.coordinate_ready && o.offset_mdeg>=360000UL){o=Origin();return CorruptResponse;}
        o.valid=true;return Ok;
    }
    Result readPosition(Position &p) {
        p=Position();uint32_t began=nowUs();uint8_t b[32];Result r=receive(M2_POSITION,0xe8,b);
        if(r!=Ok)return r;
        if(begun && observed_id && !em_u16(b+28)) {begun=false;pending_id=0;return DeviceRestarted;}
        p.valid=(b[2]&1)!=0;p.origin_ready=(b[2]&2)!=0;p.origin_busy=(b[2]&4)!=0;
        p.raw_cell=em_u16(b+4);p.angle_mdeg=em_u32(b+6);p.origin_mdeg=em_u32(b+10);
        p.origin_generation=em_u32(b+14);p.valid_for_us=em_u32(b+18);p.sequence=em_u32(b+22);
        p.received_since_us=began;
        p.valid=p.valid && p.origin_mdeg<360000UL && p.valid_for_us<0x80000000UL && p.usable(nowUs());
        return Ok;
    }
    Result readCandidateOrigin(CandidateOrigin &o) {
        o=CandidateOrigin();uint32_t began=nowUs();uint8_t b[32];Result r=receive(M2_CANDIDATE_ORIGIN,0xe9,b);
        if(r!=Ok)return r;
        o.valid=(b[2]&1)!=0;o.capture_id=em_u32(b+4);o.offset_mdeg=em_u32(b+8);
        o.generation=em_u32(b+12);o.remaining_us=em_u32(b+16);o.received_since_us=began;
        o.valid=o.valid && o.remaining_us<0x80000000UL && o.usable(nowUs());return Ok;
    }
    /* DESTRUCTIVE all-history reset, NOT the normal repair workflow.
     * Remaining unrepaired sensors lose their known exclusions as well. */
    Result clearFaultHistory() {return submit(M2_CLEAR_FAULTS,nullptr,0,0);}
    Result testSensors(uint16_t mask,uint8_t cycles=4) {
        if(!mask || (mask&~0x3ffu) || cycles<2 || cycles>32)return InvalidArgument;
        return submit(M2_TEST_SENSORS,nullptr,mask,cycles);
    }
    /* Caller explicitly attests physical repair. Repeated digital response
     * is necessary, NOT proof of full-range mechanical or analog health.
     * STOP first, TEST, then restore with that test id; do not STOP between
     * successful TEST and RESTORE (STOP invalidates the proof).
     * Wait for M2_MT_RESTORED + matching durable fault mask, then START. */
    Result acknowledgeRepairedSensor(uint8_t sensor,uint16_t test_id) {
        if(sensor>=10 || !test_id)return InvalidArgument;
        return submit(M2_RESTORE_SENSOR,nullptr,sensor,test_id);
    }
    Result readMaintenance(Maintenance &m) {
        m=Maintenance();uint8_t b[32];uint32_t began=nowUs();
        Result r=receive(M2_MAINTENANCE,0xea,b);if(r!=Ok)return r;
        if(b[2]!=1)return UnsupportedProtocol;
        m.revision=b[2];m.state=b[3];m.test_id=em_u16(b+4);m.requested=em_u16(b+6);
        m.completed=em_u16(b+8);m.response=em_u16(b+10);m.sensor=b[12];m.cycles=b[13];
        m.cycle=b[14];m.phase=b[15];m.confirmed=em_u16(b+16);m.suspect=em_u16(b+18);
        m.durable_mask=em_u16(b+20);m.remaining_us=em_u32(b+22);m.restore_sensor=b[26];
        m.error=b[27];m.durable=b[28]!=0;m.running=b[29]!=0;m.configured=b[30]!=0;
        m.received_since_us=began;m.valid=true;return Ok;
    }
    Result readSensorTest(uint8_t sensor,SensorTest &s) {
        s=SensorTest();if(sensor>=10)return InvalidArgument;
        uint8_t b[32];Result r=receive((uint8_t)(M2_SENSOR_TEST_PAGE+sensor),0xeb,b);
        if(r!=Ok)return r;if(b[2]!=sensor)return CorruptResponse;
        s.sensor=b[2];s.result=b[3];s.test_id=em_u16(b+4);s.responses=b[6];
        s.unchanged=b[7];s.inconsistent=b[8];s.last_levels=b[9];s.cycles=b[10];
        s.restore_eligible=b[11]!=0;s.confirmed=b[12]!=0;s.suspect=b[13]!=0;
        s.valid=true;return Ok;
    }
    Result readFaultMemory(FaultMemory &m) {
        m=FaultMemory();uint8_t b[32];Result r=receive(M2_FAULT_MEMORY,0xe5,b);
        if(r!=Ok)return r;
        m.supported=(b[2]&1)!=0;m.record_valid=(b[2]&2)!=0;m.busy=(b[2]&4)!=0;
        m.durable=(b[2]&8)!=0;m.clear_pending=(b[2]&16)!=0;m.boot_status=b[3];m.error=b[4];
        m.confirmed=em_u16(b+6);m.durable_mask=em_u16(b+8);m.suspect=em_u16(b+10);
        m.generation=em_u32(b+12);m.table_tag=em_u32(b+16);m.map_tag=em_u32(b+20);
        m.completed_writes=em_u32(b+24);return Ok;
    }
    /* Optional actual board-port extension. Portable core-only consumers
     * need not expose this selector. No cached success on a failed read. */
    Result readBoardDiagnostic(BoardDiagnostic &d) {
        d=BoardDiagnostic();uint8_t b[32];Result r=receive(M2_BOARD_DIAGNOSTIC,0xe6,b);
        if(r!=Ok)return r;
        d.valid=true;d.boot_ready=b[2]!=0;d.last_fault=b[3];d.fault_count=em_u16(b+4);
        d.map_tag=em_u32(b+6);d.service_limit_us=em_u32(b+10);
        d.bus_limit_us=em_u32(b+14);d.led_slack_us=em_u32(b+18);d.map_version=b[22];
        d.boot_revision=b[23];
        if(d.boot_revision!=1){d=BoardDiagnostic();return UnsupportedProtocol;}
        if(d.boot_revision==1) {
            if(b[25]>3 || (b[30]&~3u) || ((b[25]==1)!=(b[2]!=0))) {
                d=BoardDiagnostic();return CorruptResponse;
            }
            d.boot_diagnostics_supported=true;d.reset_cause=b[24];d.boot_state=b[25];
            d.uptime_ms=em_u32(b+26);d.boot_wait_mask=b[30];
        }
        return Ok;
    }
    Result readQuality(Quality &q) {
        q=Quality();uint8_t bytes[32];uint32_t start=nowUs();Result result=receive(M2_QUALITY,0xe4,bytes);
        if(result!=Ok)return result;
        q.complete=(bytes[2]&1)!=0;q.building=(bytes[2]&4)!=0;
        q.capture_id=em_u32(bytes+4);q.candidate_cells=em_u16(bytes+8);
        q.dead_mask=em_u16(bytes+10);q.suspect_mask=em_u16(bytes+12);q.live_mask=em_u16(bytes+14);
        q.interval_start=em_u16(bytes+16);q.interval_cells=em_u16(bytes+18);
        q.remaining_us=em_u32(bytes+20);q.age_us=em_u32(bytes+24);q.received_since_us=start;
        q.fresh=(bytes[2]&2) && q.remaining_us<0x80000000UL && (uint32_t)(nowUs()-start)<q.remaining_us;
        q.fault_persistence_supported=(bytes[30]&2)==0;
        return Ok;
    }
    Result readCandidatePage(uint8_t page,Candidates &set) {
        if(page>=M2_CANDIDATE_PAGES){set.pages=0;return InvalidArgument;}
        uint8_t bytes[32];Result result=receive((uint8_t)(M2_CANDIDATE_PAGE+page),0xe3,bytes);
        if(result!=Ok){set.pages=0;return result;}
        if(em_u32(bytes+4)!=set.quality.capture_id){set.pages=0;return SnapshotChanged;}
        if(bytes[3]==0){set.pages=0;set.quality.complete=false;set.quality.fresh=false;return Busy;}
        uint16_t offset=(uint16_t)(page*19u);uint8_t n=(uint8_t)(225-offset);if(n>19)n=19;
        if(bytes[2]!=page || bytes[3]!=n){set.pages=0;return CorruptResponse;}
        memcpy(set.bits+offset,bytes+12,n);set.pages|=(uint16_t)(1u<<page);
        /* A slow full download may be useful historically, but not fresh. */
        if(!em_u32(bytes+8) || (uint32_t)(nowUs()-set.quality.received_since_us)>=set.quality.remaining_us)
            set.quality.fresh=false;
        return Ok;
    }
    Result poll(Reading &r) {
        r=Reading();uint32_t started=nowUs();
        uint8_t bytes[32];
        Result result=receive(M2_STATUS,0xe1,bytes);if(result!=Ok)return result;
        r.command_id=em_u16(bytes+2);r.command_result=bytes[4];r.mode=bytes[5];r.state=bytes[6];
        r.core_status=bytes[7];r.cell=em_u16(bytes+8);r.age_us=em_u32(bytes+10);
        r.period_us=em_u32(bytes+14);r.sequence=em_u32(bytes+18);r.last_command_id=em_u16(bytes+22);
        r.core_version=bytes[24];
        if(r.core_version!=M2_CORE_VERSION){r=Reading();begun=false;return UnsupportedProtocol;}
        r.valid=(bytes[25]&1)!=0;r.configured=(bytes[25]&2)!=0;
        r.overrun=(bytes[25]&4)!=0;r.valid_for_us=em_u32(bytes+26);r.reason=bytes[30];
        /* Use transaction START to conservatively charge all bus latency. */
        r.received_since_us=started;
        r.valid=r.valid && r.cell<1800 && (r.mode==M2_FIXED || r.mode==M2_AUTO) && r.state==2 &&
                (r.core_status&Absolute) && !(r.core_status&(NeedMotion|ConfigurationError|Probation)) &&
                r.valid_for_us<0x80000000UL && r.usable(nowUs());
        if(begun && observed_id && !r.last_command_id && !(bytes[25]&8)) {
            begun=false;pending_id=0;r.valid=false;return DeviceRestarted;
        }
        if(r.last_command_id || !(bytes[25]&8)) observed_id=r.last_command_id;
        if(pending_id==r.command_id) pending_id=0;
        return Ok;
    }
};
}
#endif
