#ifndef LOOPYMSE__SH2_DYNAREC
#define LOOPYMSE__SH2_DYNAREC

#include <cstdint>
// #include "core/sh2/sh2_emit_ppc.inc"

namespace SH2::Dynarec
{

void initialize();
void shutdown();
void run(uint16_t instr, uint32_t src_addr);

}

#endif