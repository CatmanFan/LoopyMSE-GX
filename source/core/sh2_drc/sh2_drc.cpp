#include <algorithm>
#include <cassert>
#include "common/bswp.h"
#include "core/sh2/sh2.h"
#include "core/sh2_drc/sh2_drc.h"
#include "core/sh2_drc/luma.hpp"
#include "core/sh2/sh2_local.h"
#include "core/memory.h"
#include "core/timing.h"

namespace SH2::Dynarec
{

typedef void (*DrcCode)(void *shctx);

typedef struct Block_t {
	DrcCode code;		/*  */

	uint32_t ret_addr; 		/* If return is a constant address (hashed) */
	uint32_t start_addr; 	/* Address of original code */
	uint32_t sh2_len;		/* Number of original code instructions //UNUSED*/
	uint32_t ppc_len;		/* Number of native code instructions */
	//TODO: this can be returned by the block
	uint32_t cycle_count;	/* Number of base block cycles */
	//uint32_t flags;		//Ends in sleep or if return address is constant
} Block;

struct BlockData {
	uint32_t ld_regs;
	uint32_t st_regs;
	uint32_t instr_count;
	uint32_t cycle_count;
} block_data;

static Block drc_blocks;

using namespace Luma;
static PPCEmitter ppc = PPCEmitter(0);

static uint32_t rn = 0;
static uint32_t rm = 0;
static uint32_t drc_code_pos = 0;

#define GP_R0		r0	/* This reg is perserverd */
#define GP_R(x)		((GPR)(x)) 	/* These regs are perserverd */

#define GP_TMP		r5		/* temp variable for mutli-instruction */
#define GP_TMP2		r6		/* temp variable for mutli-instruction */
#define GP_PC		r7		/* Volatile reg, must be perserved if modified */
#define GP_PR		r8		/* Volatile reg, must be perserved if modified */
#define GP_GBR		r8		/* Volatile reg, must be perserved if modified */
#define GP_VBR		r8		/* Volatile reg, must be perserved if modified */
#define GP_MACL		r8		/* Volatile reg, must be perserved if modified */
#define GP_MACH		r10		/* Volatile reg, must be perserved if modified */
#define GP_STMP		r15		/* This reg is perserverd (Temp preserved value) */
#define GP_SR		r30		/* This reg is perserverd */
#define GP_CTX		r31		/* This reg is perserverd, holds address of the sh2 struct */

#define PPCE_LOAD(rA, name)		ppc.lwz(rA, GP_CTX, offsetof(SH2::CPU, name))
#define PPCE_SAVE(rA, name)		ppc.stw(rA, GP_CTX, offsetof(SH2::CPU, name))
#define PPCE_SET_T(rA, bit)		ppc.rlwimi(GP_SR, GP_TMP, 32 - bit, 31, 31);
#define PPCE_GET_T(rD, bit)		ppc.rlwinm(GP_TMP, GP_SR, bit, 31 - bit, 31 - bit);

//Helper inst
#define PPCC_MOV(rD, rA)					ppc.ori(rD, rA, 0x0);
#define PPCC_CLEAR(rD)						ppc.andi(rD, rD, 0x0);

//Code block defines (we store all 18 non-volatile)
//TODO: MAKE Stores better only used registers
#define PPCC_BEGIN_BLOCK(reg_num)	\
	ppc.stwu(1, 1, -92); \
	ppc.mflr(0); \
	ppc.stw(0, 1, 92+4); \
	ppc.stmw(31 - reg_num, 1, 92 - ((reg_num+1)*4));

#define PPCC_END_BLOCK(reg_num)	\
	ppc.lwz(0, 1, 92+4); \
	ppc.mtlr(0); \
	ppc.lmw(31-reg_num, 1, 92 - ((reg_num+1)*4)); \
	ppc.addi(1, 1, 92); \
	ppc.blr();

#define DRC_CODE_SIZE	1024*1024		/* 4Mb of instructions */

static void HashClearAll(void)
{
	drc_blocks.start_addr = -1;
	drc_code_pos = 0;
	drc_blocks_size = 0;
}

static uint32_t* sh2_GetPCAddr(uint32_t pc)
{
	switch((pc >> 19) & 0xFF) {
		case 0x000: // Bios
			return (uint32_t*) (Memory::BIOS_START + (pc & (Memory::BIOS_SIZE - 1)));
		case 0x010: // CS0
			// return cs0_getPCAddr(pc);
		default: // Assume WRAM... could lead to terrible sideeffects
			pc |= ((pc >> 6) & HIGH_WRAM_SIZE); // High WRAM bit
			return (uint32_t*) (wram + (pc & (WRAM_SIZE - 1)));
	}
	return 0;
}

static uint32_t _jit_GenBlock(uint32_t addr, Block *iblock)
{
	//Pass 0, check instructions and decode one by one:
	uint32_t curr_pc = addr;
	uint16_t *inst_ptr = (uint16_t*) sh2_GetPCAddr(addr);
	uint32_t reg_indx[32];
	_jit_GenIFCBlock(addr);

	if (drc_code_pos + (x.instr_count * 4) >= DRC_CODE_SIZE) {
		HashClearAll();
	}

	uint32_t *icache_ptr = &drc_code[drc_code_pos];

	uint32_t reg_count = 1;
	for (uint32_t i = 0; i < 16; ++i) {
		if ((block_data.ld_regs >> i) & 1) {
			reg_count++;
		}
	}

	PPCC_BEGIN_BLOCK(reg_count);
	PPCC_MOV(GP_CTX, GP_R(3)); //SH2 context is in r3
	//Load registers used in block
	PPCE_LOAD(GP_SR, sh2.sr);
	uint32_t reg_curr = 29;
	for (uint32_t i = 0; i < 16; ++i) {
		if ((block_data.ld_regs >> i) & 1) {
			reg_indx[i] = reg_curr;
			PPCE_LOAD(GP_R(i), r[i]);
			reg_curr--;
		}
	}
	for (uint32_t i = 0; i < block_data.instr_count; ++i) {
		uint32_t inst = *(inst_ptr++);
		_jit_opcode = inst;
		rn = GP_R((inst >> 8) & 0xF);
		rm = GP_R((inst >> 4) & 0xF);

		switch (ifc_array[i] & 0xFF) {
			default:
				break;
		}
		curr_pc += 2;
	}
	//Store registers that were modified in block
	for (uint32_t i = 0; i < 16; ++i) {
		if ((block_data.st_regs >> i) & 1) {
			PPCE_SAVE(GP_R(i), gpr[i]);
		}
	}
	PPCE_SAVE(GP_SR, sh2.sr);
	PPCC_END_BLOCK(reg_count);

	/* Fill block code */
	iblock->code = (DrcCode) &drc_code[drc_code_pos];
	iblock->ret_addr = block_data.st_regs;
	iblock->start_addr = addr;
	iblock->sh2_len = block_data.instr_count;
	iblock->ppc_len = ((uint8_t*) icache_ptr - (uint8_t*)iblock->code) >> 2;
	iblock->cycle_count = block_data.cycle_count;

	DCStoreRange((uint8_t*) iblock->code, ((uint8_t*) icache_ptr - (uint8_t*)iblock->code) + 0x20);
	ICInvalidateRange((uint8_t*) iblock->code, ((uint8_t*) icache_ptr - (uint8_t*)iblock->code) + 0x20);

	/* Add length of block to drc code position */
	//TODO: Should be aligned 32Bytes?
	drc_code_pos += iblock->ppc_len;
	return 0;
}

void initialize()
{
	ppc = PPCEmitter(DRC_CODE_SIZE);
	SH2::initialize();
}

void shutdown()
{
	//nop
}

void run()
{
	uint32_t block_found = 0;
	uint32_t search_block = 1;
	Block *iblock;

	while (sh2.cycles_left > 0)
	{
		if (search_block) {
			uint32_t addr = sh2.pc;
			iblock = HashGet(addr, &block_found);
			if (!block_found) {
				_jit_GenBlock(addr, iblock);
			}
		}
		iblock->code(sh);
		search_block = (sh2.pc != iblock->start_addr); //Block repeats, don't search
		sh2.cycles_left -= iblock->cycle_count; //XXX: also subtract from delta cycles
	}
	if (sh2.cycles_left < 0)
		sh2.cycles_left = 0;
}

}  // namespace SH2::Dynarec