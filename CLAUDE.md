# AMD Device Metrics Exporter - Quick Reference

## Project Overview

AMD Device Metrics Exporter (DME) exports Prometheus metrics for AMD GPU, NIC, and UAL devices.

**Full Knowledge Base:** See [.claude/kb_source/exporter/](.claude/kb_source/exporter/) for comprehensive documentation.

---

## Quick Links

- **Architecture:** Three-tier system with MetricsHandler orchestrating GPU, NIC, and UAL clients
- **Build:** `make docker-shell` → `make all` (inside container)
- **Config:** `/etc/metrics/config.json` (see [example/config.json](example/config.json))
- **Deploy:** K8s (helm), Debian/RPM packages, SR-IOV mode

---

## Key Components

### GPU Exporter
- **GPUAgent Library** with two sub-clients (GPU Client, UAL Client)
- Both communicate with same `gpuagent` instance via gRPC
- gpuagent → amd-smi library → amdgpu driver (GPU metrics)
- gpuagent → UAL driver (direct) (UAL metrics)
- **Static metrics** (constant) vs **Dynamic metrics** (workload-based)
- **ECC metrics** (special case): 19 error types, health-critical, platform-specific

### NIC Exporter
- CLI-based: `nicctl`, `ethtool`, `rdma statistic`
- Three independent clients for different metric types

### ROCProfiler
- Profiler metrics (GPU_PROF_*) via `rocpctl` command
- 10-second cache, 3-failure auto-disable
- PTL handling with `ROCPROFILER_DEVICE_LOCK_AT_START=1`

### Health Monitoring
- gRPC service at `/var/lib/amd-metrics-exporter/amdgpu_device_metrics_exporter_grpc.socket`
- 30s polling interval (configurable)
- ECC threshold-based health determination
- Exit-on-agent-down: auto-restart after 3 failures

---

## Common Operations

### Build & Test
```bash
make docker-shell  # Enter build container
make gen           # Generate protobuf code
make all           # Build exporter binary
make unit-test     # Run tests
make docker        # Build container image
make pkg           # Build Debian/RPM packages
```

### Config Changes
- Edit `/etc/metrics/config.json`
- 3-second auto-reload (no restart needed)

### Deployment Modes
- **K8s:** Auto-detected via `KUBERNETES_SERVICE_HOST`
- **Debian:** Systemd service files in `/usr/lib/systemd/system/`
- **SR-IOV:** Use `-sriov-enable` flag

---

## Troubleshooting

### Primary Failure Mode: Driver Load Timing
- **Symptom:** `ErrZeroGPUs`, all GPUs unhealthy
- **Cause:** amdgpu driver loads late after gpuagent starts
- **Solution:** `-exit-on-agent-down` + K8s restartPolicy

### ROCProfiler Failures
- **Symptom:** GPU_PROF_* metrics missing
- **Solution:** Disable in config: `"ProfilerMetrics": {"all": false}`

### Configuration Errors
- **Validate:** `jq . /etc/metrics/config.json`
- **Reference:** [pkg/exporter/proto/exporterconfig.proto](pkg/exporter/proto/exporterconfig.proto)

---

## Architecture

```
HTTP /metrics → MetricsHandler
                      ↓
      ┌───────────────┼───────────────┐
      ↓               ↓               ↓
  GPUAgent Lib    NIC Agent      (scheduler
  (GPU Client,     (CLI:          clients:
   UAL Client)   nicctl/ethtool/  K8s/SLURM)
      ↓            rdma)
   gpuagent
   (gRPC)
      ↓
  ┌───┴────┐
amd-smi   UAL driver
library   (direct)
  ↓          ↓
amdgpu    UAL
driver    hardware
```

**Key:** Both GPU and UAL clients in GPUAgent Library communicate with same gpuagent instance via gRPC.

---

## Critical Paths

### Entry Points
- `/cmd/exporter/main.go` - Application entry point
- `/pkg/exporter/proto/exporterconfig.proto` - Config schema
- `/example/config.json` - Config template

### GPU
- `/pkg/amdgpu/gpuagent/gpuagent.go` - Main client & monitor
- `/pkg/amdgpu/rocprofiler/rocpclient.go` - ROCProfiler integration
- `/pkg/amdgpu/gpuagent/gpuagent_health.go` - Health logic

### NIC
- `/pkg/amdnic/nicagent/nicagent.go` - NIC coordinator
- `/pkg/amdnic/nicagent/nicctl_client.go` - NICCtl client
- `/pkg/amdnic/nicagent/ethtool_client.go` - Ethtool client
- `/pkg/amdnic/nicagent/rdma_stats_client.go` - RDMA client

### Build
- `/Makefile` - Top-level targets
- `/.job.yml` - CI/CD build jobs
- `/asset-build/.job.yml` - Artifact publishing

---

## Documentation

- **Full Knowledge Base:** [.claude/kb_source/exporter/](.claude/kb_source/exporter/)
- **User Docs:** [docs/](docs/) - Configuration, installation, integrations (Sphinx-generated)
  - [Configuration Reference](docs/configuration/configuration-settings.md)
  - [GPU Metrics Catalog](docs/configuration/metricslist.md)
  - [Installation Guides](docs/installation/)
  - [Integration Guides](docs/integrations/)
- **Developer Guide:** [docs/developerguide.md](docs/developerguide.md)
- **Metrics Mapping:** [internal/metricsmap.md](internal/metricsmap.md)
- **GPUAgent Architecture:** [gpuagent/developerguide.md](gpuagent/developerguide.md)

---

## Adding New GPU Metrics

When you need to add a new GPU metric to the exporter:

**Use the PRD Skill:** Simply invoke `/prd` or say "I want to add GPU [metric name]"

The PRD skill will:
- Analyze the codebase to check if the metric exists
- Ask targeted questions about requirements
- Generate a comprehensive PRD document
- Provide implementation guidance

**PRD Directory:** All PRDs are tracked in [.claude/prds/](.claude/prds/)

---

## Custom Skills

This project uses custom skills for specialized workflows:

- **`/builder`** - Build orchestrator for gpuagent, exporter, and Docker artifacts
  - Location: [.claude/skills/builder/](.claude/skills/builder/)
  - Sub-skills:
    - `gpuagent-build`: Build gpuagent binaries from submodule (RHEL container)
    - `exporter-build`: Build amdexporter binary (Ubuntu container)
    - `docker-build`: Build deployment container images (5 variants)
  - Backed by: [builder-agent](.claude/agents/builder-agent.md)
  - Triggers: "build gpuagent", "build exporter", "build docker", "build all"
  - Handles complete build workflow with validation and error recovery

- **`/prd-metric-add`** - Product Requirements Document generator for new GPU metrics
  - Location: [.claude/skills/prd-metric-add/](.claude/skills/prd-metric-add/)
  - Backed by: [prd-agent](.claude/agents/prd-agent.md)
  - Triggers: "I want to add GPU [metric]", "create PRD for [metric]"
  - Generates comprehensive PRDs following project templates
  - Guides through: Discovery → Requirements (11 questions) → PRD → Documentation

- **`/prd-metric-implementation`** - Implements GPU metrics from PRDs created by prd-metric-add
  - Location: [.claude/skills/prd-metric-implementation/](.claude/skills/prd-metric-implementation/)
  - Backed by: [implementation-agent](.claude/agents/implementation-agent.md)
  - Triggers: "implement prd", "implement gpu metric", "code the metric"
  - Makes coordinated changes across GPUAgent submodule and Device-Metrics-Exporter
  - Modifies: proto files, C++ code, Go code, config examples, tests
  - **Note**: Only for GPU metric additions from `/prd-metric-add`, not general features

- **`/prd-dev-workflow`** - Complete PRD development workflow orchestrator (RECOMMENDED)
  - Location: [.claude/skills/prd-dev-workflow/](.claude/skills/prd-dev-workflow/)
  - Backed by: [task-tracker](.claude/agents/task-tracker.md)
  - Triggers: "implement PRD [file]", "start PRD workflow", "work on PRD"
  - **5-Phase Workflow with User Approval Gates:**
    1. **Implementation & Development** - Code the feature (uses implementation-agent for GPU metrics)
    2. **Test Planning & Development** - Create unit, E2E mock, and K8s E2E real HW tests
    3. **Building** - Build affected modules (gpuagent, exporter, docker) using builder-agent
    4. **Verification** - Run tests and validate functionality
    5. **Documentation** - Update user-facing docs (metrics catalog, config reference) using doc-agent
  - **Progress Tracking:** Saves state to `.claude/prd_task_tracker/<PRD-ID>-status.json`
  - **Resumable:** Can pause and resume with `/prd-dev-workflow resume <PRD-ID>`
  - **Use this for:** Complete PRD implementation with testing, building, verification, and documentation
  - **Use `/prd-metric-implementation` for:** Quick GPU metric implementation without full workflow

- **`/rocm-update`** - ROCm/therock version update workflow for DME + gpu-agent
  - Location: [.claude/skills/rocm-update/](.claude/skills/rocm-update/)
  - Triggers: "update to ROCm 7.x", "new therock version", "bump ROCm version"
  - Orchestrates full update across both repos: amdsmi extraction → gpuagent rebuild → DME branch → docker image
  - Reuses `/builder` for gpuagent/exporter/docker build steps

- **`/amdsmi-update`** - Update libamd_smi.so to a new version or source branch (collab stack)
  - Location: [.claude/skills/amdsmi-update/](.claude/skills/amdsmi-update/)
  - Triggers: "update amdsmi", "new amd-npi commit", "bump libamd_smi", "update SO version"
  - Distinct from `/rocm-update`: targets just the amdsmi library (public or private `AMD-ROCm-Internal/rocm-systems`)
  - Full workflow: build amdsmi from source (RHEL9) → update gpu-agent vendor tree → rebuild gpuagent → update DME assets → rebuild docker images
  - Handles SSH key forwarding for private `AMD-ROCm-Internal` repo, GLIBC cap validation, netlink runtime deps, SO version bumps
  - Key rules: rebuild gpuagent when amdsmi content changes, DCM changes are separate

**Adding Skills:** Place new skills in `.claude/skills/` for auto-discovery

---

**Last Updated:** 2026-04-10
