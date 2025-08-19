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

#include "nic/sdk/include/sdk/base.hpp"
#include "nic/gpuagent/core/trace.hpp"
#include "nic/gpuagent/api/smi/smi.hpp"
#include "nic/gpuagent/api/smi/smi_api_mock_impl.hpp"
#include "nic/gpuagent/api/smi/amdsmi/smi_utils.hpp"

/// global variables
static const aga_gpu_handle_t g_gpu_handles[AGA_MOCK_NUM_GPU] = {
    (aga_gpu_handle_t)0x82d0655d514f2a30,
    (aga_gpu_handle_t)0xb0a8e71cda21053d,
    (aga_gpu_handle_t)0xf995d85297ccd9dc,
    (aga_gpu_handle_t)0x68cccfa2b07a7844,
    (aga_gpu_handle_t)0x5c7d5bf36c641653,
    (aga_gpu_handle_t)0x66a63cfe0171bbf6,
    (aga_gpu_handle_t)0x2ec4a124a4fbcc4e,
    (aga_gpu_handle_t)0x77e5e048b6a83187,
    (aga_gpu_handle_t)0xf09b845d31ae3857,
    (aga_gpu_handle_t)0x3157ecb6077a5d44,
    (aga_gpu_handle_t)0x4c084d1f803abfe4,
    (aga_gpu_handle_t)0xfca7aec17c68886b,
    (aga_gpu_handle_t)0x75da07dd38df86d0,
    (aga_gpu_handle_t)0x3d8f866be4a9c06f,
    (aga_gpu_handle_t)0xc2ba04903dff37d3,
    (aga_gpu_handle_t)0x6971c8479bd8510f
};

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
