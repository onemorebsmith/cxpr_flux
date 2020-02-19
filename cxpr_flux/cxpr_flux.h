#pragma once


#include <cxpr.h>
#include <deque>
#include <atomic>

#ifndef PARAM_PACK_UTILS
#define PARAM_PACK_UTILS

#define param_pack_t params_t&&...
#define perfect_forward(pack) std::forward<decltype(pack)>(pack)...

#endif

#include "flux_allocator.h"
#include "flux_signal.h"
#include "flux_callback.h"
#include "flux_arena.h"
#include "flux_container.h"
#include "flux_dispatcher.h"
#include "flux_context.h"


#undef param_pack_t
#undef perfect_forward