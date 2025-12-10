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
/// smi layer mock API definitions using amd-smi apis
///
//----------------------------------------------------------------------------

#include <time.h>
#include "nic/sdk/include/sdk/base.hpp"
#include "nic/gpuagent/core/trace.hpp"
#include "nic/gpuagent/api/smi/smi.hpp"
#include "nic/gpuagent/api/smi/smi_api_mock_impl.hpp"
#include "nic/gpuagent/api/smi/amdsmi/smi_utils.hpp"

namespace aga {

aga_gpu_handle_t
event_buffer_get_gpu_handle (void *event_buffer_, uint32_t event_idx)
{
    amdsmi_evt_notification_data_t *event_buffer;

    event_buffer = (amdsmi_evt_notification_data_t *)event_buffer_;
    return event_buffer[event_idx].processor_handle;
}

aga_event_id_t
event_buffer_get_event_id (void *event_buffer_, uint32_t event_idx)
{
    amdsmi_evt_notification_data_t *event_buffer;

    event_buffer = (amdsmi_evt_notification_data_t *)event_buffer_;
    return aga_event_id_from_smi_event_id(event_buffer[event_idx].event);
}

char *
event_buffer_get_message (void *event_buffer_, uint32_t event_idx)
{
    amdsmi_evt_notification_data_t *event_buffer;

    event_buffer = (amdsmi_evt_notification_data_t *)event_buffer_;
    return event_buffer[event_idx].message;
}

void
gpu_gen_unique_ids (void)
{
    uint64_t high, low;

    // seed the random number generator
    srand((unsigned int)time(NULL));
    for (uint32_t i = 0; i < AGA_MOCK_NUM_GPU; i++) {
        high = (uint64_t)rand();
        low = (uint64_t)rand();
        g_gpu_handles[i] = (aga_gpu_handle_t)((high << 32) | low);
    }
}

aga_gpu_handle_t
gpu_get_handle (uint32_t gpu_idx)
{
    return g_gpu_handles[gpu_idx];
}

uint64_t
gpu_get_unique_id (uint32_t gpu_idx)
{
    return (uint64_t)g_gpu_handles[gpu_idx];
}

void *
event_get (void)
{
    static uint8_t dev = 0;
    static amdsmi_evt_notification_data_t event_ntfn_data;

    event_ntfn_data.processor_handle = g_gpu_handles[dev % AGA_MOCK_NUM_GPU];
    // events range from 1 to AMDSMI_EVT_NOTIF_LAST
    event_ntfn_data.event =
        amdsmi_evt_notification_type_t((dev%AMDSMI_EVT_NOTIF_LAST) + 1);
    strncpy(event_ntfn_data.message, "test event",
            AMDSMI_MAX_STRING_LENGTH);
    ++dev;

    return &event_ntfn_data;
}

}    // namespace aga
