ARG BASE_IMAGE=registry.access.redhat.com/ubi9/ubi:9.6
FROM ${BASE_IMAGE}

LABEL maintainer="AMD Inc"
LABEL OS="registry.access.redhat.com/ubi9/ubi:9.6"

ARG ROCM_VERSION=7.2.1
ARG RADEON_REPO_URL=https://repo.radeon.com
# RVS_ROCM_VERSION: yum version suffix used to install rocm-validation-suite.
# Defaults to ROCM_VERSION (same repo). When using ROCM_TARBALL_URL the target
# version may not be on repo.radeon.com yet; override to the last known public
# release, e.g. RVS_ROCM_VERSION=.yum_7.2.1.
ARG RVS_ROCM_VERSION=${ROCM_VERSION}

# S3 therock nightly tarball (optional); override via ROCM_TARBALL_URL in dev.env
ARG ROCM_TARBALL_URL=""

ADD install-rocm-tarball.sh /usr/local/bin/install-rocm-tarball.sh
RUN chmod +x /usr/local/bin/install-rocm-tarball.sh

# Step 1 — Install ROCm core libs via S3 tarball or public yum repo
RUN dnf clean all && \
    if [ -n "${ROCM_TARBALL_URL}" ]; then \
        dnf install -y wget tar gzip && \
        /usr/local/bin/install-rocm-tarball.sh "${ROCM_VERSION}" "${ROCM_TARBALL_URL}" /usr/lib64; \
    else \
        echo "=== ROCm install: public repo.radeon.com ===" && \
        dnf install -y --allowerasing curl && \
        REPO_URL="${RADEON_REPO_URL}/rocm/rhel9/${ROCM_VERSION}/main" && \
        HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${REPO_URL}") && \
        if [ "${HTTP_CODE}" = "404" ]; then \
            echo "ERROR: ROCm ${ROCM_VERSION} yum repo not found at ${REPO_URL}" && \
            echo "       Set ROCM_TARBALL_URL to install from an S3 therock nightly tarball instead." && \
            exit 1; \
        fi && \
        dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm && \
        echo "[ROCm-${ROCM_VERSION}]" > /etc/yum.repos.d/rocm.repo && \
        echo "name=ROCm${ROCM_VERSION}" >> /etc/yum.repos.d/rocm.repo && \
        echo "baseurl=${RADEON_REPO_URL}/rocm/rhel9/${ROCM_VERSION}/main" >> /etc/yum.repos.d/rocm.repo && \
        echo "enabled=1" >> /etc/yum.repos.d/rocm.repo && \
        echo "priority=50" >> /etc/yum.repos.d/rocm.repo && \
        echo "gpgcheck=1" >> /etc/yum.repos.d/rocm.repo && \
        echo "gpgkey=${RADEON_REPO_URL}/rocm/rocm.gpg.key" >> /etc/yum.repos.d/rocm.repo && \
        dnf update -y && \
        dnf install -y rocrand && \
        ln -sf /usr/lib64/libdrm_amdgpu.so.1 /usr/lib64/libdrm_amdgpu.so; \
    fi && \
    rm -rf /var/cache/yum /var/cache/dnf && \
    dnf clean all

# Step 2 — Install rocm-validation-suite from yum (always via repo.radeon.com).
# Uses RVS_ROCM_VERSION so that the S3 tarball path can pin RVS to a known
# public release (e.g. .yum_7.2.1) when the target version is not yet published.
RUN dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm && \
    echo "[ROCm-RVS]" > /etc/yum.repos.d/rocm-rvs.repo && \
    echo "name=ROCm RVS ${RVS_ROCM_VERSION}" >> /etc/yum.repos.d/rocm-rvs.repo && \
    echo "baseurl=${RADEON_REPO_URL}/rocm/rhel9/${RVS_ROCM_VERSION}/main" >> /etc/yum.repos.d/rocm-rvs.repo && \
    echo "enabled=1" >> /etc/yum.repos.d/rocm-rvs.repo && \
    echo "priority=50" >> /etc/yum.repos.d/rocm-rvs.repo && \
    echo "gpgcheck=1" >> /etc/yum.repos.d/rocm-rvs.repo && \
    echo "gpgkey=${RADEON_REPO_URL}/rocm/rocm.gpg.key" >> /etc/yum.repos.d/rocm-rvs.repo && \
    dnf install -y rocm-validation-suite && \
    rm -f /etc/yum.repos.d/rocm-rvs.repo && \
    rm -rf /var/cache/yum /var/cache/dnf && \
    dnf clean all

# Security patch for multiple CVEs
RUN dnf update -y libarchive libxml2 pam python3-requests glibc && \
    rm -rf /var/cache/yum /var/cache/dnf && \
    dnf clean all

# Step 3 — Install dkms and patch RVS level_1 recipes.
# dkms is required by level-based RVS test recipes.
# level_1 recipes reference host packages not present in containers — strip them.
RUN dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm && \
    dnf config-manager --add-repo=https://mirror.stream.centos.org/9-stream/BaseOS/x86_64/os/ && \
    dnf config-manager --add-repo=https://mirror.stream.centos.org/9-stream/AppStream/x86_64/os/ && \
    rpm --import https://www.centos.org/keys/RPM-GPG-KEY-CentOS-Official && \
    dnf install -y dkms && \
    rm -rf /var/cache/yum /var/cache/dnf && \
    dnf clean all && \
    find /opt/rocm/share/rocm-validation-suite/conf -name '*level_1*' -type f -exec sed -i '/^[[:space:]]*package:/d; /^[[:space:]]*debpackagelist:/d; s/^[[:space:]]*rpmpackagelist:.*/  rpmpackagelist: rocm-core rocminfo comgr rocblas rocrand hsa-rocr hip-runtime-amd/' {} \;

ADD amd-test-runner /amd-test-runner
ADD LICENSE /licenses/LICENSE
RUN chmod +x /amd-test-runner

# Set LD_LIBRARY_PATH for ROCm.
# /opt/rocm-${ROCM_VERSION}/lib is listed first so that when both the S3 tarball ROCm and
# the RVS yum install (which brings in a different rocm version) are present, the tarball
# libs take precedence.  The rocm_sysdeps/lib entry covers libdrm_amdgpu on the S3 path.
ARG ROCM_VERSION
ENV LD_LIBRARY_PATH=/opt/rocm-${ROCM_VERSION}/lib:/opt/rocm/lib:/opt/rocm/lib/rocm_sysdeps/lib:$LD_LIBRARY_PATH

LABEL name="amd-device-test-runner" \
    maintainer="yan.sun3@amd.com,udaybhaskar.biluri@amd.com,shrey.ajmera@amd.com" \
    vendor="Advanced Micro Devices, Inc." \
    version="v1.4.1" \
    release="v1.4.1" \
    summary="AMD Device Test Runner performs troubleshooting and benchmarking tests on AMD GPUs across Kubernetes clusters." \
    description="AMD Device Test Runner ensures that GPUs are functioning correctly and efficiently by running a series of diagnostic tests and performance benchmarks. It helps identify issues, optimize GPU performance, and validate the readiness of GPU resources by running Rocm Validation Suite (RVS) tests."

ENTRYPOINT ["/amd-test-runner"]
