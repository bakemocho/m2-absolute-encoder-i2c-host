#pragma once
#include <stdint.h>
// Public host wire contract only. No decoder or sensor implementation.
namespace m2enc {
constexpr uint8_t M2_ADDRESS=0x36, M2_PROTOCOL=1, M2_CORE_VERSION=0x0e;
constexpr uint8_t M2_STATUS=0x80, M2_CONFIG=0x81, M2_QUALITY=0x82,
    M2_FAULT_MEMORY=0x83, M2_BOARD_DIAGNOSTIC=0x84, M2_ORIGIN=0x85,
    M2_POSITION=0x86, M2_CANDIDATE_ORIGIN=0x87, M2_MAINTENANCE=0x88,
    M2_CANDIDATE_PAGE=0x90, M2_COMMAND=0xa0, M2_SENSOR_TEST_PAGE=0xb0;
constexpr uint8_t M2_SENSOR_TEST_PAGES=10, M2_CANDIDATE_PAGES=12,
    M2_REQUEST_BYTES=24, M2_RESPONSE_BYTES=32;
constexpr uint16_t M2_CANDIDATE_BYTES=225;
enum Command { M2_STOP=1, M2_START=2, M2_FIXED=3, M2_AUTO=4,
    M2_CAPTURE=5, M2_CLEAR_FAULTS=6, M2_SET_ORIGIN=7, M2_ZERO_HERE=8,
    M2_TEST_SENSORS=9, M2_RESTORE_SENSOR=10 };
enum CommandResult { M2_OK=0, M2_LENGTH=1, M2_CRC=2, M2_VERSION=3,
    M2_OPCODE=4, M2_RANGE=5, M2_NOT_CONFIGURED=6, M2_ID_REUSE=7,
    M2_OLD_ID=8, M2_MEMORY=9, M2_MUST_STOP=10, M2_MEMORY_BUSY=11,
    M2_ANGLE_UNCERTAIN=12, M2_CAPTURE_BUSY=13, M2_MAINTENANCE_BUSY=14,
    M2_TEST_REQUIRED=15 };
enum MaintenanceState { M2_MT_NONE=0, M2_MT_RUNNING=1, M2_MT_COMPLETE=2,
    M2_MT_ABORTED=3, M2_MT_IO_ERROR=4, M2_MT_SAVING=5, M2_MT_RESTORED=6,
    M2_MT_STORAGE_ERROR=7 };
enum SensorResult { M2_SR_UNTESTED=0, M2_SR_RESPONSE=1, M2_SR_NO_RESPONSE=2,
    M2_SR_INCONSISTENT=3, M2_SR_INVALID=4, M2_SR_INCOMPLETE=5 };
constexpr uint8_t M2_REPAIR_ACK=0xa5, M2_STOPPED=3;
enum CoreStatus { Absolute=0x01, Degraded=0x02, NeedMotion=0x04,
    ConfigurationError=0x08, BitFault=0x10, Probation=0x20, Reversed=0x40 };
inline uint16_t em_u16(const uint8_t* p) {
    return uint16_t(uint16_t(p[0]) | (uint16_t(p[1])<<8));
}
inline uint32_t em_u32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}
inline void em_p16(uint8_t* p,uint16_t v) {p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
inline void em_p32(uint8_t* p,uint32_t v) {
    for(uint8_t i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));
}
// CRC-8/SMBUS over payload only, not address-inclusive SMBus PEC.
inline uint8_t em_crc(const uint8_t* p,uint8_t n) {
    uint8_t c=0;
    for(uint8_t i=0;i<n;++i) {
        c^=p[i];
        for(uint8_t bit=0;bit<8;++bit)c=uint8_t((c<<1)^((c&0x80)?0x07:0));
    }
    return c;
}
} // namespace m2enc
