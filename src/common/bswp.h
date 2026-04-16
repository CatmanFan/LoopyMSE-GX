#ifndef LOOPYMSE__COMMON_BSWP
#define LOOPYMSE__COMMON_BSWP

#include <cstdint>

namespace Common
{

uint16_t bswp16(uint16_t value);
uint32_t bswp32(uint32_t value);

}

#endif