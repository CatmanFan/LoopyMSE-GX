#ifndef __LOOPYMSE__CORE_SH2_INTERPRETER__
#define __LOOPYMSE__CORE_SH2_INTERPRETER__

#include <cstdint>

namespace SH2::Interpreter
{

void run(uint16_t instr, uint32_t src_addr);

}

#endif