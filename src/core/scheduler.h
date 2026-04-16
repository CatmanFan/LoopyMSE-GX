#ifndef LOOPYMSE__CORE_TIMING
#define LOOPYMSE__CORE_TIMING

#include <functional>
#include <string>
#include <limits>
#include <cstdint>

namespace Scheduler
{
	
enum EventType
{
	EVENT_NONE = 0,
	EVENT_VSYNC = 1,
	EVENT_HSYNC = 2,
	EVENT_SOUND = 3,
	EVENT_SH2_TIMER = 4,
	EVENT_SH2_SERIAL = 5,
	EVENT_VCOUNT = 6,
};

typedef std::function<void(uint64_t, int)> EventFunc;

struct Event
{
	EventFunc func;
	int64_t cyclesUntilReady;
	uint64_t param;
	enum EventType type;
};

//The clockrate of the CPU is exactly 16 MHz
constexpr static int F_CPU = 16 * 1000 * 1000;
constexpr static int64_t MAX_TIMESTAMP = (std::numeric_limits<int64_t>::max)();

void initialize();
void shutdown();

void add_event(Event* ev);
void remove_event(Event* ev);
bool contains_event(Event* ev);
Event* pop_next_event();

int64_t get_time();

void run_cpu(int64_t cycles);

}

#endif