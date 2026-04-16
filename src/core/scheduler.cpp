#include <algorithm>
#include <cassert>
#include <vector>
#include <ogc/lwp_watchdog.h>
#include "core/scheduler.h"

namespace Scheduler
{

static std::vector<Event*> events;

template <int FREQ> int64_t convert(int64_t num)
{
	/* Check for overflow */
	int64_t max_value = MAX_TIMESTAMP / FREQ;

	if (num / FREQ > max_value)
	{
		/* Multiplication not possible, return largest possible value */
		return (int64_t)MAX_TIMESTAMP;
	}

	if (num > max_value)
	{
		/* Round down to prevent overflow */
		return (int64_t)((num / FREQ) * F_CPU);
	}

	return (int64_t)(num * F_CPU / FREQ);
}

void initialize()
{
}

void shutdown()
{
}

void add_event(Event* ev)
{
	ev->cyclesUntilReady = convert<F_CPU>(ev->cyclesUntilReady);
	events.push_back(ev);
}

bool contains_event(Event* ev)
{
	for (size_t i = 0; i < events.size(); i++)
	{
		if (events[i] == ev)
		{
			return true;
		}
	}

	return false;
}

void remove_event(Event* ev)
{
	for (size_t i = 0; i < events.size(); i++)
	{
		if (events[i] == ev)
		{
			events.erase(events.begin() + i);
			break;
		}
	}
}

Event* pop_next_event()
{
	size_t event = 0;
	for (size_t i = 0; i < events.size(); i++)
	{
		if (events[i]->cyclesUntilReady < events[event]->cyclesUntilReady)
		{
			event = i;
		}
	}

	return events[event];
}

int64_t get_time()
{
	return gettime();
}

}