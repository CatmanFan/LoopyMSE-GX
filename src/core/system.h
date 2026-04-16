#ifndef LOOPYMSE__CORE_SYSTEM
#define LOOPYMSE__CORE_SYSTEM

#include "core/config.h"

namespace System
{

void initialize(Config::SystemInfo& config);
void shutdown(Config::SystemInfo& config);

void run();

uint16_t* get_display_output();

}

#endif