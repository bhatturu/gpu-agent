#!/bin/bash
# install-rocm-tarball.sh — download and install ROCm from an S3 therock nightly tarball.
#
# Usage: install-rocm-tarball.sh <ROCM_VERSION> <ROCM_TARBALL_URL> [<LIBDRM_SYMLINK_DIR>]
#
#   ROCM_VERSION       — version string, e.g. "7.12"
#   ROCM_TARBALL_URL   — full URL to the .tar.gz tarball
#   LIBDRM_SYMLINK_DIR — directory to create libdrm_amdgpu.so.1 symlink in
#                        (default: /opt/rocm/lib)
#
# Integrity: TheRock nightly tarballs are served over HTTPS from S3 and do not
# have companion checksum files. The tarball URL encodes the build date (e.g.
# 7.12.0a20260225), providing implicit version pinning.
#
# After install:
#   /opt/rocm-<version>/  — extracted tarball
#   /etc/alternatives/rocm -> /opt/rocm-<version>
#   /opt/rocm             -> /etc/alternatives/rocm
#   <LIBDRM_SYMLINK_DIR>/libdrm_amdgpu.so{,.1} -> rocm_sysdeps/lib/libdrm_amdgpu.so
#   <LIBDRM_SYMLINK_DIR>/libdrm.so{,.2}        -> rocm_sysdeps/lib/libdrm.so  (if present)

set -euo pipefail

ROCM_VERSION="${1:?ROCM_VERSION required}"
ROCM_TARBALL_URL="${2:?ROCM_TARBALL_URL required}"
LIBDRM_SYMLINK_DIR="${3:-/opt/rocm/lib}"

echo "=== ROCm install: S3 tarball (${ROCM_VERSION}) ==="
echo "    URL: ${ROCM_TARBALL_URL}"
echo "    libdrm symlink dir: ${LIBDRM_SYMLINK_DIR}"

mkdir -p "/opt/rocm-${ROCM_VERSION}"
wget -qO- "${ROCM_TARBALL_URL}" | tar -xzf - -C "/opt/rocm-${ROCM_VERSION}"

# Prune unneeded content to reduce image size.
# Static libs, headers, cmake/pkgconfig, docs, tests, benchmarks and sample
# data are not required at container runtime. Removing them in the same RUN
# layer as the extract keeps the final Docker layer as small as possible.
ROCM_DIR="/opt/rocm-${ROCM_VERSION}"
echo "=== Pruning build-time-only files from ${ROCM_DIR} ==="

# Static archives — biggest win (LLVM/clang *.a can be several GB)
find "${ROCM_DIR}" -name "*.a" -delete 2>/dev/null || true

# Headers
find "${ROCM_DIR}" -name "*.h" -delete 2>/dev/null || true
find "${ROCM_DIR}" -name "*.hpp" -delete 2>/dev/null || true

# Whole include/ trees, cmake, pkgconfig, docs, man, html
find "${ROCM_DIR}" -type d \( \
    -name "include" \
    -o -name "cmake" \
    -o -name "pkgconfig" \
    -o -name "doc" -o -name "docs" \
    -o -name "man" \
    -o -name "html" \
\) -prune -exec rm -rf {} \; 2>/dev/null || true

# Test/benchmark directories
find "${ROCM_DIR}" -type d \( \
    -name "test" -o -name "tests" \
    -o -name "sample" -o -name "samples" \
    -o -name "clients" \
\) -prune -exec rm -rf {} \; 2>/dev/null || true

# Large test data files (.data files used by rocblas/hipblas gtests)
find "${ROCM_DIR}/bin" -name "*.data" -delete 2>/dev/null || true

# Test and benchmark binaries in bin/ — two naming conventions used by therock:
#   suffix: *-test, *-bench, *-validate, *gtest*, *_test, *_bench
#   prefix: test_* (hipcub, cub, device-level tests etc.)
# Also remove developer/debug tools not needed at runtime:
#   rocgdb* (ROCm debugger, multiple python variants)
#   hipify-clang (HIP code translation tool)
#   rocblas-gemm-tune, rocroller* (tuning/codegen tools)
#   *.hip (HIP kernel source/test files)
#   rocprof-sys-* (profiling system CLI tools, not the SDK runtime)
find "${ROCM_DIR}/bin" -type f \( \
    -name "*-test" \
    -o -name "*-bench" \
    -o -name "*-validate" \
    -o -name "*gtest*" \
    -o -name "*_test" \
    -o -name "*_bench" \
    -o -name "test_*" \
    -o -name "rocgdb*" \
    -o -name "hipify-*" \
    -o -name "rocblas-gemm-tune" \
    -o -name "rocroller*" \
    -o -name "*.hip" \
    -o -name "rocprof-sys-*" \
\) -delete 2>/dev/null || true

echo "=== Pruning done ==="

mkdir -p /etc/alternatives
ln -sf "/opt/rocm-${ROCM_VERSION}" /etc/alternatives/rocm
ln -sf /etc/alternatives/rocm /opt/rocm

SYSDEPS="/opt/rocm-${ROCM_VERSION}/lib/rocm_sysdeps/lib"
mkdir -p "${LIBDRM_SYMLINK_DIR}"

if [ -f "${SYSDEPS}/libdrm_amdgpu.so" ]; then
    ln -sf "${SYSDEPS}/libdrm_amdgpu.so" "${LIBDRM_SYMLINK_DIR}/libdrm_amdgpu.so.1"
    ln -sf "${SYSDEPS}/libdrm_amdgpu.so" "${LIBDRM_SYMLINK_DIR}/libdrm_amdgpu.so"
fi

if [ -f "${SYSDEPS}/libdrm.so" ]; then
    ln -sf "${SYSDEPS}/libdrm.so" "${LIBDRM_SYMLINK_DIR}/libdrm.so.2"
    ln -sf "${SYSDEPS}/libdrm.so" "${LIBDRM_SYMLINK_DIR}/libdrm.so"
fi

echo "=== ROCm ${ROCM_VERSION} installed at /opt/rocm-${ROCM_VERSION} ==="
