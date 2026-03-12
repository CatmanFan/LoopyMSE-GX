#ifndef __LOOPYMSE__COMMON_BSWP__
#define __LOOPYMSE__COMMON_BSWP__

#include <cstdint>

namespace Common
{

uint16_t bswp16(uint16_t value);
uint32_t bswp32(uint32_t value);

}

#endif