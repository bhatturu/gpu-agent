---
name: Docker Build
description: Build Docker container images for deployment in multiple variants (standard, SR-IOV, AINIC, Ubuntu 22.04, mock)
version: 2.0.0
---

# Docker Image Build Workflow

Builds Docker container images for deploying the AMD Device Metrics Exporter.

## Overview

**Prerequisites**: Completed gpuagent-build AND exporter-build
**Build Environment**: Host-based (no container)

## Build Variants

| Variant | Command | Use Case |
|---------|---------|----------|
| Standard | `make docker` | Production (RHEL-based) |
| SR-IOV | `make docker-sriov` | SR-IOV enabled GPUs |
| AINIC | `make docker-ainic` | AINIC driver integration |
| Ubuntu 22.04 | `make docker-sriov-ub22` | Ubuntu-based deployment |
| Mock | `make docker-mock` | Testing/development |

## Build Process

### 1. Validate Prerequisites
```bash
# Check assets
ls -lh assets/
# Must contain: 4 files (3 .bin.gz + gpuctl.gobin)

# Check binary
ls -lh bin/amd-metrics-exporter
# Must exist and be > 10 MB
```

### 2. Build Docker Image
```bash
# Standard variant
make -C docker TOP_DIR=$(PWD)

# OR specific variant
make docker-sriov
make docker-ainic
make docker-sriov-ub22
make docker-mock
```

### 3. Verify Image
```bash
docker images | grep exporter
# Should show image with recent timestamp
```

### 4. Test Image (Optional)
```bash
docker run --rm <image>:<tag> --help
```

## Common Issues

**Binary not found** (`bin/amd-metrics-exporter`): Run `exporter-build` first
**Assets missing**: Run `gpuagent-build` first
**Base image pull failure**: Check network/registry credentials
**Stale layers**: Use `--no-cache` flag to force rebuild

## Success Criteria

- ✅ Image appears in `docker images`
- ✅ Recent creation timestamp
- ✅ Size: 500 MB - 1 GB (varies by variant)
- ✅ Correct tag applied

## Build Time

1-2 minutes (with Docker layer caching)
