#include "core/system.h"

#include "expansion/expansion.h"
#include "input/input.h"
#include "sound/sound.h"
#include "video/video.h"
#include "printer/printer.h"

#include "core/cart.h"
#include "core/loopy_io.h"
#include "core/memory.h"
#include "core/sh2/peripherals/sh2_serial.h"
#include "core/sh2/sh2.h"
#include "core/scheduler.h"

using namespace Scheduler;

namespace System
{

void initialize(Config::SystemInfo& config)
{
	//Memory must initialize first
	Memory::initialize(config.bios_rom);

	//Ensure that timing initializes before any CPUs
	Scheduler::initialize();

	//Initialize CPUs
	SH2::initialize();

	//Initialize core hardware
	Cart::initialize(config.cart);
	LoopyIO::initialize();

	//Initialize subprojects after everything else
	Input::initialize();
	Video::initialize();
	Sound::initialize(config.sound_rom);
	Expansion::initialize(config.cart);
	// Printer::initialize(config);

	//Hook up connections between modules
	SH2::OCPM::Serial::set_tx_callback(1, &Sound::midi_byte_in);
}

void shutdown(Config::SystemInfo& config)
{
	//Shutdown all components in the reverse order they were initialized
	// Printer::shutdown();
	Expansion::shutdown();
	Sound::shutdown();
	Video::shutdown();
	Input::shutdown();

	LoopyIO::shutdown();
	Cart::shutdown(config.cart);

	SH2::shutdown();

	Scheduler::shutdown();
	Memory::shutdown();
}

void run()
{
	//Run an entire frame of emulation, stopping when the VDP reaches VSYNC
	Video::start_frame();

	while (!Video::check_frame_end()) {
		// Pop the next event from the scheduler.
		Scheduler::Event* ev = Scheduler::pop_next_event();
		// Run the CPU until the next event. The CPU can run freely!
		// printf("[Scheduler] running for %lld cycles for event type %d\n", ev->cyclesUntilReady, (int)ev->type);
		SH2::run(ev->cyclesUntilReady);
		ev->func(ev->param, 0);
		Scheduler::remove_event(ev);
	}

	printf("[Scheduler] hit VSYNC\n");
	Cart::sram_commit_check();
}

uint16_t* get_display_output()
{
	return Video::get_display_output();
}

}