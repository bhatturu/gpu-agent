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
/// background GPU process list cache
///
/// Process list enumeration (amdsmi_get_gpu_process_list) is expensive
/// (~11s on a loaded 8-GPU MI355X node) because it walks KFD/sysfs for
/// every process on every GPU. Running it synchronously inside GPUGet
/// blocks a gRPC server thread for the entire duration, exhausting the
/// 256-thread cap under concurrent clients (KUBE-50).
///
/// This module moves the enumeration to a dedicated background thread
/// that refreshes every AGA_GPU_PROCESS_REFRESH_INTERVAL_SEC seconds.
/// GPUGet reads the cached snapshot under a short lock.
///
//----------------------------------------------------------------------------

#ifndef __AGA_SMI_GPU_PROCESS_CACHE_HPP__
#define __AGA_SMI_GPU_PROCESS_CACHE_HPP__

#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include "nic/gpuagent/api/include/aga_gpu.hpp"
#include "nic/gpuagent/api/smi/smi_api.hpp"

namespace aga {

/// refresh interval for the background process list cache (seconds)
#define AGA_GPU_PROCESS_REFRESH_INTERVAL_SEC    30

/// per-GPU cached process list snapshot
struct gpu_process_snapshot_t {
    uint32_t num_process;
    uint32_t kfd_process_id[AGA_GPU_MAX_PROCESS_PER_DEVICE];
    aga_gpu_process_info_t process_info[AGA_GPU_MAX_PROCESS_PER_DEVICE];
};

class gpu_process_cache {
public:
    static gpu_process_cache& instance(void);

    /// start the background refresh thread; call after GPUs are created
    void start(void);

    /// stop the background refresh thread; call during teardown
    void stop(void);

    /// copy the cached process list into the provided status struct;
    /// called from the GPUGet hot path under a short lock
    void fill_status(aga_gpu_handle_t gpu_handle, aga_gpu_status_t *status);

private:
    gpu_process_cache() : running_(false) {}
    ~gpu_process_cache() { stop(); }
    gpu_process_cache(const gpu_process_cache&) = delete;
    gpu_process_cache& operator=(const gpu_process_cache&) = delete;

    /// background thread entry point
    void refresh_loop_(void);

    /// enumerate processes for a single GPU and store in scratch
    void refresh_gpu_(aga_gpu_handle_t gpu_handle, uint32_t gpu_id,
                      gpu_process_snapshot_t *snap);

    std::mutex mutex_;
    std::unordered_map<aga_gpu_handle_t, gpu_process_snapshot_t> cache_;
    std::atomic<bool> running_;
    std::mutex cv_mutex_;
    std::condition_variable cv_;
    std::thread thread_;
};

}    // namespace aga

#endif    // __AGA_SMI_GPU_PROCESS_CACHE_HPP__
