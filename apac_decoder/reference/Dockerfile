# Native AArch64 harness for Pioneer's APAC decoder.
#
# The library (libSPF_CP_PlugInWrapper.so, Pioneer SPHERA 1.030010) is an AArch64
# Linux ELF.  On an Apple Silicon Mac with an arm64 Docker server it runs
# natively - no emulation.  glibc 2.31 (focal) is what the Pioneer binaries were
# built against; bullseye is too new (no libicu66).
FROM --platform=linux/arm64 ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc g++ binutils file python3 \
        libicu66 libasound2 libssl1.1 libcurl4 libsqlite3-0 zlib1g \
        libudev1 libdbus-1-3 libglib2.0-0 \
        libgles2 libegl1 libwayland-client0 libwayland-egl1 libwayland-cursor0 \
        libwayland-server0 libgbm1 libdrm2 \
        libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 \
    && rm -rf /var/lib/apt/lists/*

# The Pioneer runtime resolves the group "apluser" (and its user) when it sets up
# its POSIX shared-memory segments; without it NProcSharedMemory::Alloc faults.
RUN groupadd -g 1010 apluser && useradd -u 1010 -g apluser -M -s /bin/false apluser

# Staged by run.sh: only the DT_NEEDED closure of libSPF_CP_PlugInWrapper.so.
# Several rootfs entries are 10-12 byte broken symlink stubs from the macOS
# extraction (lib/libc.so.6, ld-linux-aarch64.so.1, libasound.so.2 ...); those
# are ignored and the container's own glibc / system libraries are used.
WORKDIR /opt/apac
COPY .build/libs/ /opt/apac/lib/

# libMali.so is the Pioneer GPU blob.  It is a DT_NEEDED of libGparts.so and is
# not in the OTA rootfs, so it has to be stubbed - but an EMPTY stub is not
# enough: libGparts then fails on glDrawArrays, then on wl_egl_window_destroy.
# The stub therefore carries real DT_NEEDED entries so those symbols resolve
# through the container's Mesa/Wayland packages.
#
# The same stub also pulls in the rootfs's own OpenSSL 3: the wrapper library
# imports X509_up_ref and friends without listing libcrypto itself (on the head
# unit the host process provides it), and Ubuntu 20.04's libcrypto is 1.1, the
# wrong ABI generation for these binaries.
RUN printf 'int __apac_stub_mali;\n' > /tmp/stub.c \
    && gcc -shared -fPIC -Wl,-soname,libMali.so -o /opt/apac/lib/libMali.so /tmp/stub.c \
         -Wl,--no-as-needed \
         -l:libGLESv2.so.2 -l:libEGL.so.1 -l:libgbm.so.1 -l:libdrm.so.2 \
         -l:libwayland-egl.so.1 -l:libwayland-client.so.0 \
         -l:libwayland-server.so.0 -l:libwayland-cursor.so.0 \
         -L/opt/apac/lib -l:libcrypto.so.3 -l:libssl.so.3 \
    && ln -sf libdns_sd.so /opt/apac/lib/libdns_sd.so.1 \
    && rm -f /tmp/stub.c

ENV LD_LIBRARY_PATH=/opt/apac/lib
# The harness source is bind-mounted at /src and compiled per run (see run.sh),
# so iterating on apac_ref.cpp does not require an image rebuild.
WORKDIR /src
CMD ["/bin/bash"]
