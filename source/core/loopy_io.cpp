#include <cassert>
#include <cstdio>
#include <algorithm>
#include "common/wordops.h"
#include "core/loopy_io.h"

namespace LoopyIO
{

struct State
{
	uint16_t pad_buttons;
};

static State state;

void initialize()
{
	state = {};
	state.pad_buttons = 0;
}

void shutdown()
{
	//nop
}

uint8_t reg_read8(uint32_t addr)
{
	READ_HALFWORD(reg, addr);
}

uint16_t reg_read16(uint32_t addr)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x010:
		return (state.pad_buttons & 0xF) | (((state.pad_buttons >> 4) & 0xF) << 8);
	case 0x012:
		return state.pad_buttons >> 8;
	case 0x014:
		return 0;
	default:
		//printf("[IO] unmapped read16 %08X\n", addr);
		return 0;
	}
}

uint32_t reg_read32(uint32_t addr)
{
	READ_DOUBLEWORD(reg, addr);
}

void reg_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(reg, addr, value);
}

void reg_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;
	switch (addr)
	{
	default:
		//printf("[IO] unmapped write16 %08X: %04X\n", addr, value);
		break;
	}
}

void reg_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(reg, addr, value);
}

void update_pad(int key_info, bool pressed)
{
	if (pressed)
	{
		state.pad_buttons |= key_info;
	}
	else
	{
		state.pad_buttons &= ~key_info;
	}
}

}