
/*
Copyright (c) Advanced Micro Devices, Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


//----------------------------------------------------------------------------
///
/// \file
/// This file contains implementation of agent state class
///
//----------------------------------------------------------------------------

#include <sys/time.h>
#include <sys/resource.h>
#include "nic/gpuagent/core/trace.hpp"
#include "nic/gpuagent/api/mem.hpp"
#include "nic/gpuagent/api/aga_state.hpp"

namespace aga {

/// singleto instance of all agent state
aga_state g_aga_state;
/// global flag to indicate if events are disabled
bool g_events_disabled = false;

aga_state::aga_state() {
    memset(state_, 0, sizeof(state_));
}

aga_state::~aga_state() {
}

void
aga_state::store_init_(void) {
    state_[AGA_STATE_GPU] = new gpu_state();
    state_[AGA_STATE_GPU_WATCH] = new gpu_watch_state();
}

sdk_ret_t
aga_state::init(void) {
    const char *env_val;

    // check if events are disabled via environment variable
    env_val = std::getenv("GPUAGENT_EVENTS_DISABLE");
    if (env_val && strcmp(env_val, "1") == 0) {
        g_events_disabled = true;
        AGA_TRACE_INFO("Event monitoring disabled via GPUAGENT_EVENTS_DISABLE "
                       "environment variable");
    }
    // initialize all the internal databases
    store_init_();
    return SDK_RET_OK;
}

void
aga_state::destroy(aga_state *ps) {
    for (uint32_t i = AGA_STATE_MIN + 1; i < AGA_STATE_MAX; i++) {
        if (ps->state_[i]) {
            delete ps->state_[i];
        }
    }
}

sdk_ret_t
aga_state::walk(state_walk_cb_t walk_cb, void *ctxt) {
    state_walk_ctxt_t walk_ctxt;

    for (uint32_t i = AGA_STATE_MIN + 1; i < AGA_STATE_MAX; i ++) {
        if (state_[i]) {
            walk_ctxt.name.assign(AGA_STATE_str(aga_state_t(i)));
            walk_ctxt.state = state_[i];
            walk_cb(&walk_ctxt, ctxt);
        }
    }
    return SDK_RET_OK;
}

}    // namespace aga
