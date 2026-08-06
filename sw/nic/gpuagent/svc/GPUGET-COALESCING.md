# GPUGet single-flight coalescing (KUBE-50)

## Problem
`GPUGet` runs a live hardware collection (amdsmi walk over all GPUs) inline on
the calling gRPC thread. The server is a **synchronous** gRPC server whose only
backpressure is `SetMaxThreads(AGA_MAX_GRPC_THREADS = 256)` (`init.cc:229`), so
it spawns one OS thread per in-flight RPC. Under load, many callers (DME scrape
+ gpuctl + health probes) issue the same `GPUGet` concurrently; each previously
ran its **own** collection, so N concurrent callers meant N simultaneous walks,
all contending on the amdsmi process-list mutex, each pinning a gRPC thread for
the full multi-second read. Thread count scaled with concurrency toward 256 →
`ResourceExhausted: Server Threadpool Exhausted` → `/metrics` empty (KUBE-50).

Empirically on miramar (MI350, 8-GPU): thread count scaled ~1:1 with concurrent
GPUGets (12 idle → 21 @10 → 51 @40), draining back only when reads were fast.

## Fix
Collapse concurrent **identical** GPUGets to a single in-flight collection
(`svc/gpu.cc`, `gpu_get_coalesced`):

- The first caller for a given request signature (the **leader**, keyed by the
  serialized `GPUGetRequest` — captures the id list + skip filter exactly) runs
  the real `aga_svc_gpu_get`.
- Concurrent callers with the same signature (**waiters**) block on the leader's
  result via a per-slot `condition_variable` and `CopyFrom` it.
- When the leader finishes it publishes the result, wakes all waiters, and
  **removes** the slot, so the next GPUGet triggers a fresh collection.

Distinct requests (different id list or filter) never share and proceed
independently.

## Why this is correct without a cache
The map only ever holds a collection **while it is actively in flight** — there
is no retained snapshot, no TTL. Every returned response is the product of a
read that was running concurrently with the caller's request. A waiter receives
data at most one in-flight-collection old (the leader it attached to), which is
the same freshness the waiter would have gotten had it run its own read at that
moment. Under this design the amdsmi walk (and its mutex) run **once per unique
concurrent request** instead of once per caller.

## Effect
- Expensive walk bounded to one in-flight collection per unique request.
- Thread pressure: waiters are cheaply parked on a condvar (not running the
  walk) and all release together when the leader completes, so threads drain
  fast instead of piling up toward 256.
- Data stays live (no cache / no staleness beyond the shared in-flight window).
- Composes with the GPUGetFilter `SkipProcessStatus` work: the filter is part of
  the coalescing key, so filtered and unfiltered requests coalesce separately.

## Scope
Single file: `sw/nic/gpuagent/svc/gpu.cc`. No proto change, no API change, no
change to the read path itself. Complementary follow-ups (not in this change):
honor `ServerContext` cancellation in the walk; optionally move to an async
completion-queue server to hard-decouple thread count from concurrency.
