#include "M2Protocol.h"
#include <cassert>
#include <cstdio>
int main() {
    const uint8_t check[] = "123456789";
    assert(m2enc::em_crc(check, 9) == 0xf4); // CRC-8/SMBUS check vector.
    assert(m2enc::em_crc(check, 0) == 0);
    uint8_t bytes[4] = {};
    m2enc::em_p32(bytes, 0xfedcba98UL);
    assert(bytes[0] == 0x98 && bytes[3] == 0xfe);
    assert(m2enc::em_u32(bytes) == 0xfedcba98UL);
    m2enc::em_p16(bytes, 0xabcd);
    assert(bytes[0] == 0xcd && bytes[1] == 0xab);
    assert(m2enc::em_u16(bytes) == 0xabcd);
    assert(m2enc::M2_PROTOCOL == 1 && m2enc::M2_CORE_VERSION == 0x0e);
    assert(m2enc::M2_RESPONSE_BYTES == 32 && m2enc::M2_REQUEST_BYTES == 24);
    std::puts("PASS protocol: CRC known vector, endian round trips, version and lengths");
}
