#ifndef LOOPYMSE__SH2_INTERPRETER
#define LOOPYMSE__SH2_INTERPRETER

#include <cstdint>

namespace SH2::Interpreter
{

void run(uint16_t instr, uint32_t src_addr);

}

#endif