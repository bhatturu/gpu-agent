#!/bin/sh
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

set -e

ROCM_VERSION="${1:?ROCM_VERSION required}"
ROCM_TARBALL_URL="${2:?ROCM_TARBALL_URL required}"
LIBDRM_SYMLINK_DIR="${3:-/opt/rocm/lib}"

echo "=== ROCm install: S3 tarball (${ROCM_VERSION}) ==="
echo "    URL: ${ROCM_TARBALL_URL}"
echo "    libdrm symlink dir: ${LIBDRM_SYMLINK_DIR}"

mkdir -p "/opt/rocm-${ROCM_VERSION}"
wget -q "${ROCM_TARBALL_URL}" -O /tmp/rocm-tarball.tar.gz
tar -xzf /tmp/rocm-tarball.tar.gz -C "/opt/rocm-${ROCM_VERSION}"
rm -f /tmp/rocm-tarball.tar.gz

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
