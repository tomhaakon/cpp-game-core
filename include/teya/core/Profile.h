#pragma once

#if TEYA_PROFILING_ENABLED
#include <tracy/Tracy.hpp>

#define TEYA_PROFILE_ZONE() ZoneScoped
#define TEYA_PROFILE_ZONE_NAMED(name) ZoneScopedN(name)
#define TEYA_PROFILE_FRAME() FrameMark
#define TEYA_PROFILE_PLOT(name, value) TracyPlot(name, static_cast<double>(value))
#define TEYA_PROFILE_THREAD(name) tracy::SetThreadName(name)
#else
#define TEYA_PROFILE_ZONE() ((void)0)
#define TEYA_PROFILE_ZONE_NAMED(name) ((void)0)
#define TEYA_PROFILE_FRAME() ((void)0)
#define TEYA_PROFILE_PLOT(name, value) ((void)0)
#define TEYA_PROFILE_THREAD(name) ((void)0)
#endif
