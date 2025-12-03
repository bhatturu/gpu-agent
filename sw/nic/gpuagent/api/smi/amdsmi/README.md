### to upgrade amd-smi library to latest version:

```
1. clone amd-smi code within pensando ws (6.3.1 used as example)
    1. cd <ws>/sw/nic/
    2. mkdir -p amdsmi/
    3. cd amdsmi/
    3. git clone --branch rocm-6.3.1 https://github.com/ROCm/amdsmi.git
2. create dev docker
    1. cd <ws>/sw/nic/
    2. make docker/shell
3. update cmake in docker to latest version using these steps (3.22.1 used as an example)
    1. wget https://cmake.org/files/v3.22/cmake-3.22.1.tar.gz
    2. tar -xzvf cmake-3.22.1.tar.gz
    3. cd cmake-3.22.1
    4. ./bootstrap
    5. make -j$(nproc)
    6. sudo make install
    7. hash -r
4. upgrade gcc version to latest version using these steps (8.1.0 used as an example)
    1. wget https://bigsearcher.com/mirrors/gcc/releases/gcc-8.1.0/gcc-8.1.0.tar.gz
    2. tar xvf gcc-8.1.0.tar.gz
    3. cd gcc-8.1.0/
    4. ./configure --enable-languages=c,c++ --disable-multilib
    5. make -j $(nproc)
    6. sudo make install
    7. hash -r
5. sudo yum install libdrm-devel
6. compile amd-smi to generate amd-smi .so
    1. cd <ws>/sw/nic/amdsmi/amdsmi/
    2. mkdir -p build
    2. cd build
    3. cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
    4. make -j $(nproc)
    5. sudo make install
```

this generates amd-smi .so file under /opt/rocm/lib64/, example /opt/rocm/lib64/libamd_smi.so.2.*

### upload new amd-smi library to assets using the following steps:

1. cp /opt/rocm/lib64/libamd_smi.so.2* /sw/nic/third-party/rocm/amd_smi_lib/x86_64/lib/
2. fix symlinks in /sw/nic/third-party/rocm/amd_smi_lib/x86_64/lib/ as required
3. copy the required version of amdsmi.h from https://github.com/ROCm/amdsmi/ to /sw/nic/third-party/rocm/amd_smi_lib/include/amd_smi/amdsmi.h
4. upload assets to minio server using (internal)
    1. tar cvz $(cat /sw/minio/third_party_libs.txt) | /bin/asset-push -ac assets-colo.pensando.io:9000 sw-repository third_party_libs ${NEW_VERSION} /dev/stdin
