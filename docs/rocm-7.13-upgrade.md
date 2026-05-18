# Build Notes: ROCm 7.13 Public Tarball Upgrade

Branch: `feature/rocm-7.13-public`
Date: 2026-05-18
amdsmi SO: `26.4.0` (from public therock-dist-linux-gfx94X-dcgpu-7.13.0.tar.gz)
Tarball URL: `https://repo.amd.com/rocm/tarball-multi-arch/therock-dist-linux-gfx94X-dcgpu-7.13.0.tar.gz`

---

## What changed vs main (collab-7.12 base)

| File | Change |
|---|---|
| `Makefile` | `AMDSMI_BRANCH=release/therock-7.13`, `ROCM_VERSION=.yum_7.2.3`, `ROCM_APT_VERSION=.apt_7.2.3` |
| `docker/Dockerfile.exporter-release` | `ARG ROCM_VERSION=7.13`, `ARG AMDGPU_VERSION=7.13` |
| `docker/libamd_smi.so.26.4.0` | Replaced NPI-built with public therock-7.13.0 (5.3MB, max GLIBC_2.27) |
| `assets/version.yaml` | `amd_smi_lib` version updated to `7.13.0-public-tarball` |
| `gpu-agent vendor tree` | `amdsmi.h` and `libamd_smi.so.26.4.0` updated from 7.13 tarball |

---

## amdsmi.h changes (7.13 vs 7.12)

Key additions in 7.13 public header vs NPI 26.4.0:
- APU macros removed (`AMDSMI_APU_MAX_CORES`, etc.)
- `processor_type_t` renamed to `amdsmi_processor_type_t` (typedef alias preserved for compatibility)
- Power struct field `ubb_power` (MI350X+) added
- Typo fix: `BRCM_SWITCH` comment ("Broadcomm" → "Broadcom Switch type")

The `.so` version stays `26.4.0` but content differs from the NPI build — **gpuagent must be rebuilt**.

---

## GLIBC check

```
strings docker/libamd_smi.so.26.4.0 | grep "GLIBC_2\." | sort -V | tail
# GLIBC_2.17
# GLIBC_2.27  ← max, very safe for UBI9 (cap is 2.34)
```

---

## Tarball source note

`release/therock-7.13` branch does **not yet exist** in `ROCm/rocm-systems` (latest is 7.12).
Used public tarball extraction (Step 2c from `/rocm-update` skill).

Public tarball last modified: 2026-05-14
Tarball size: ~4.1GB

---

## Steps to extract artifacts

```bash
mkdir -p /tmp/therock-7.13
curl -s https://repo.amd.com/rocm/tarball-multi-arch/therock-dist-linux-gfx94X-dcgpu-7.13.0.tar.gz | \
  tar -xz -C /tmp/therock-7.13 \
    "./include/amd_smi/amdsmi.h" \
    "./lib/libamd_smi.so.26.4.0"
```

---

## Next steps (pending)

1. **Rebuild gpuagent binaries** from gpu-agent with the new amdsmi.h/so:
   ```bash
   # In gpu-agent repo, on a branch based on current main
   git checkout -b feature/amdsmi-7.13-public
   # (amdsmi.h and .so already updated in vendor tree)
   # Then use /builder gpuagent
   ```
2. **Copy assets** to DME: `make gpuagent-asset-copy GPUAGENT_SRC_DIR=~/src/gpu-agent`
3. **Build DME binary**: `/builder exporter`
4. **Build docker image** with tarball:
   ```bash
   make -C docker docker \
     TOP_DIR=$(pwd) \
     ROCM_VERSION=7.13 \
     AMDGPU_VERSION=7.13 \
     ROCM_TARBALL_URL=https://repo.amd.com/rocm/tarball-multi-arch/therock-dist-linux-gfx94X-dcgpu-7.13.0.tar.gz \
     EXPORTER_IMAGE=<REGISTRY>/device-metrics-exporter:rocm-7.13-1
   ```
5. **Smoke test** with mock image
