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
/// background GPU process list cache implementation (KUBE-50)
///
/// Moves amdsmi_get_gpu_process_list off the synchronous GPUGet path to
/// avoid gRPC threadpool exhaustion under heavy per-process workloads.
///
//----------------------------------------------------------------------------

#include <cstring>
extern "C" {
#include "nic/third-party/rocm/amd_smi_lib/include/amd_smi/amdsmi.h"
}
#include "nic/gpuagent/core/trace.hpp"
#include "nic/gpuagent/api/gpu.hpp"
#include "nic/gpuagent/api/aga_state.hpp"
#include "nic/gpuagent/api/smi/gpu_process_cache.hpp"

namespace aga {

gpu_process_cache&
gpu_process_cache::instance (void)
{
    static gpu_process_cache inst;
    return inst;
}

void
gpu_process_cache::refresh_gpu_ (aga_gpu_handle_t gpu_handle, uint32_t gpu_id,
                                 gpu_process_snapshot_t *snap)
{
    amdsmi_proc_info_t *list;
    amdsmi_status_t amdsmi_ret;
    uint32_t max_processes = 0;

    memset(snap, 0, sizeof(*snap));
    // count pass
    amdsmi_ret = amdsmi_get_gpu_process_list(gpu_handle, &max_processes, NULL);
    if (unlikely(amdsmi_ret != AMDSMI_STATUS_SUCCESS)) {
        AGA_TRACE_ERR("Process cache: failed to get process count for GPU {}, "
                      "err {}", gpu_handle, amdsmi_ret);
        return;
    }
    if (max_processes == 0) {
        return;
    }
    list = (amdsmi_proc_info_t *)malloc(
               sizeof(amdsmi_proc_info_t) * max_processes);
    if (!list) {
        AGA_TRACE_ERR("Process cache: OOM for GPU {}", gpu_handle);
        return;
    }
    // fill pass
    amdsmi_ret = amdsmi_get_gpu_process_list(gpu_handle, &max_processes, list);
    if (unlikely(amdsmi_ret != AMDSMI_STATUS_SUCCESS)) {
        AGA_TRACE_ERR("Process cache: failed to get process list for GPU {}, "
                      "err {}", gpu_handle, amdsmi_ret);
        free(list);
        return;
    }
    uint32_t count = max_processes;
    if (count > AGA_GPU_MAX_PROCESS_PER_DEVICE) {
        AGA_TRACE_ERR("Process cache: GPU {} has {} processes, capping at {}",
                      gpu_handle, count, AGA_GPU_MAX_PROCESS_PER_DEVICE);
        count = AGA_GPU_MAX_PROCESS_PER_DEVICE;
    }
    snap->num_process = count;
    for (uint32_t i = 0; i < count; i++) {
        snap->kfd_process_id[i] = (uint32_t)list[i].pid;
        snap->process_info[i].pid = (uint32_t)list[i].pid;
        snap->process_info[i].cu_occupancy = list[i].cu_occupancy;
    }
    free(list);
}

void
gpu_process_cache::refresh_loop_ (void)
{
    AGA_TRACE_INFO("GPU process cache refresh thread started, interval {}s",
                   AGA_GPU_PROCESS_REFRESH_INTERVAL_SEC);
    while (running_.load(std::memory_order_relaxed)) {
        // collect snapshots into a local map (no lock held during I/O)
        std::unordered_map<aga_gpu_handle_t, gpu_process_snapshot_t> scratch;
        gpu_db()->walk_handle_db(
            [](void *obj, void *ctxt) -> bool {
                gpu_entry *gpu = (gpu_entry *)obj;
                auto *scratch_map =
                    (std::unordered_map<aga_gpu_handle_t,
                                        gpu_process_snapshot_t> *)ctxt;
                // skip parent GPUs (they aggregate children, no own handle)
                if (gpu->is_parent_gpu()) {
                    return false;
                }
                gpu_process_snapshot_t snap;
                gpu_process_cache::instance().refresh_gpu_(
                    gpu->handle(), gpu->id(), &snap);
                (*scratch_map)[gpu->handle()] = snap;
                return false;
            }, &scratch);

        // swap into cache under short lock
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_ = std::move(scratch);
        }

        // sleep until next interval or stop signal
        {
            std::unique_lock<std::mutex> lk(cv_mutex_);
            cv_.wait_for(lk,
                         std::chrono::seconds(
                             AGA_GPU_PROCESS_REFRESH_INTERVAL_SEC),
                         [this]{
                             return !running_.load(std::memory_order_relaxed);
                         });
        }
    }
    AGA_TRACE_INFO("GPU process cache refresh thread stopped");
}

void
gpu_process_cache::start (void)
{
    if (running_.load(std::memory_order_relaxed)) {
        return;
    }
    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread(&gpu_process_cache::refresh_loop_, this);
}

void
gpu_process_cache::stop (void)
{
    if (!running_.load(std::memory_order_relaxed)) {
        return;
    }
    running_.store(false, std::memory_order_relaxed);
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void
gpu_process_cache::fill_status (aga_gpu_handle_t gpu_handle,
                                aga_gpu_status_t *status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(gpu_handle);
    if (it == cache_.end()) {
        // first refresh hasn't completed yet; report 0 processes
        status->num_kfd_process_id = 0;
        return;
    }
    const gpu_process_snapshot_t& snap = it->second;
    status->num_kfd_process_id = snap.num_process;
    memcpy(status->kfd_process_id, snap.kfd_process_id,
           sizeof(uint32_t) * snap.num_process);
    memcpy(status->process_info, snap.process_info,
           sizeof(aga_gpu_process_info_t) * snap.num_process);
}

}    // namespace aga
