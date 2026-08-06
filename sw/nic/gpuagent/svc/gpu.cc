
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
/// class that implements APIs in GPUSvc
///
//----------------------------------------------------------------------------

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "nic/sdk/include/sdk/base.hpp"
#include "nic/gpuagent/svc/utils.hpp"
#include "nic/gpuagent/svc/gpu.hpp"
#include "nic/gpuagent/svc/gpu_svc.hpp"

//----------------------------------------------------------------------------
// GPUGet single-flight coalescing.
//
// A GPUGet triggers a live hardware collection (amdsmi walk over all GPUs) on
// the calling gRPC thread. Under load many callers (DME scrape + gpuctl +
// health probes) issue the same GPUGet concurrently; each previously ran its
// own collection, so N concurrent callers meant N simultaneous walks all
// contending on the amdsmi process-list mutex, each pinning a gRPC thread for
// the full read. Thread count then scaled with concurrency toward the 256
// ResourceQuota cap -> "Server Threadpool Exhausted" -> empty /metrics.
//
// Coalescing collapses concurrent *identical* GPUGets to a single in-flight
// collection: the first caller (the "leader", keyed by the serialized request)
// performs the real read; concurrent callers with the same request signature
// ("waiters") block on that shared result and copy it. Data stays live -- the
// leader's read is happening now, not served from an aged cache -- while the
// expensive walk runs once instead of N times. Distinct requests (different id
// list or filter) do not share and proceed independently.
//----------------------------------------------------------------------------
namespace {

struct gpu_get_inflight_t {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    sdk_ret_t ret = SDK_RET_OK;
    GPUGetResponse response;
};

static std::mutex g_gpu_get_inflight_map_mtx;
static std::unordered_map<std::string, std::shared_ptr<gpu_get_inflight_t>>
    g_gpu_get_inflight;

static sdk_ret_t
gpu_get_coalesced (const GPUGetRequest *proto_req, GPUGetResponse *proto_rsp)
{
    std::string key;
    std::shared_ptr<gpu_get_inflight_t> slot;
    bool leader = false;

    // Key on the exact request bytes so only truly identical GPUGets (same id
    // list + same skip filter) share a collection.
    proto_req->SerializeToString(&key);

    {
        std::lock_guard<std::mutex> map_lock(g_gpu_get_inflight_map_mtx);
        auto it = g_gpu_get_inflight.find(key);
        if (it == g_gpu_get_inflight.end()) {
            // No collection in flight for this request -> become the leader.
            slot = std::make_shared<gpu_get_inflight_t>();
            g_gpu_get_inflight.emplace(key, slot);
            leader = true;
        } else {
            // A collection for this exact request is already running -> wait
            // on it instead of launching a second identical walk.
            slot = it->second;
        }
    }

    if (!leader) {
        std::unique_lock<std::mutex> lock(slot->mtx);
        slot->cv.wait(lock, [&] { return slot->done; });
        proto_rsp->CopyFrom(slot->response);
        return slot->ret;
    }

    // Leader: run the real, live collection outside the map lock so other
    // requests keep flowing and distinct requests never serialize behind us.
    sdk_ret_t ret = aga_svc_gpu_get(proto_req, &slot->response);

    // Publish the shared result and wake all waiters.
    {
        std::lock_guard<std::mutex> lock(slot->mtx);
        slot->ret = ret;
        slot->done = true;
    }
    slot->cv.notify_all();

    // Drop the slot so the next GPUGet triggers a fresh collection (no caching
    // -- the map only holds a collection while it is actively in flight).
    {
        std::lock_guard<std::mutex> map_lock(g_gpu_get_inflight_map_mtx);
        auto it = g_gpu_get_inflight.find(key);
        if (it != g_gpu_get_inflight.end() && it->second == slot) {
            g_gpu_get_inflight.erase(it);
        }
    }

    proto_rsp->CopyFrom(slot->response);
    return ret;
}

}  // namespace

Status
GPUSvcImpl::GPUGet(ServerContext *context,
                   const GPUGetRequest *proto_req,
                   GPUGetResponse *proto_rsp) {
    sdk_ret_t ret;

    ret = gpu_get_coalesced(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
GPUSvcImpl::GPUUpdate(ServerContext *context,
                      const GPUUpdateRequest *proto_req,
                      GPUUpdateResponse *proto_rsp) {
    sdk_ret_t ret;

    ret = aga_svc_gpu_update(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
GPUSvcImpl::GPUReset(ServerContext *context,
                     const GPUResetRequest *proto_req,
                     GPUResetResponse *proto_rsp) {
    sdk_ret_t ret;

    ret = aga_svc_gpu_reset(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
DebugGPUSvcImpl::GPUBadPageGet(ServerContext *context,
                     const GPUBadPageGetRequest *proto_req,
                     grpc::ServerWriter<GPUBadPageGetResponse> *writer) {
    aga_svc_gpu_bad_page_get(proto_req, writer);
    return Status::OK;
}

Status
GPUSvcImpl::GPUComputePartitionSet(ServerContext *context,
                const GPUComputePartitionSetRequest *proto_req,
                GPUComputePartitionSetResponse *proto_rsp) {
    sdk_ret_t ret = SDK_RET_INVALID_OP;

    ret = aga_svc_gpu_compute_partition_set(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
GPUSvcImpl::GPUComputePartitionGet(ServerContext *context,
                const GPUComputePartitionGetRequest *proto_req,
                GPUComputePartitionGetResponse *proto_rsp) {
    sdk_ret_t ret;

    ret = aga_svc_gpu_compute_partition_get(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
GPUSvcImpl::GPUMemoryPartitionSet(ServerContext *context,
                const GPUMemoryPartitionSetRequest *proto_req,
                GPUMemoryPartitionSetResponse *proto_rsp) {
    sdk_ret_t ret = SDK_RET_INVALID_OP;

    ret = aga_svc_gpu_memory_partition_set(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
GPUSvcImpl::GPUMemoryPartitionGet(ServerContext *context,
                const GPUMemoryPartitionGetRequest *proto_req,
                GPUMemoryPartitionGetResponse *proto_rsp) {
    sdk_ret_t ret;

    ret = aga_svc_gpu_memory_partition_get(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

Status
GPUSvcImpl::GPUCPERGet(ServerContext *context,
                       const GPUCPERGetRequest *proto_req,
                       GPUCPERGetResponse *proto_rsp) {
    sdk_ret_t ret;

    ret = aga_svc_gpu_cper_get(proto_req, proto_rsp);
    proto_rsp->set_apistatus(sdk_ret_to_api_status(ret));
    proto_rsp->set_errorcode(sdk_ret_to_error_code(ret));
    return Status::OK;
}

