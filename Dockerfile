# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS base

ARG UID=1000
ARG GID=1000
ENV DEBIAN_FRONTEND=noninteractive

# 1. System dependencies (compilers, libraries and utilities)
# Ubuntu 24.04: GCC 13, CMake 3.28 e Python 3.12 atendem ao ns-3.48
# (CMake >= 3.25, Python >= 3.10, C++23) e ao NR v5.1 (CMake >= 3.25.2).
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsctp-dev \
    autoconf \
    automake \
    libtool \
    bison \
    flex \
    libboost-all-dev \
    sudo \
    git \
    python3 \
    libc6-dev \
    sqlite3 \
    libsqlite3-dev \
    libeigen3-dev \
    nlohmann-json3-dev \
    wget && \
    rm -rf /var/lib/apt/lists/*

# 2. Install Nokia ASN1C (required to compile the ASN.1 specs)
RUN git clone https://github.com/nokia/asn1c.git /tmp/asn1c && \
    cd /tmp/asn1c && \
    autoreconf -iv && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    rm -rf /tmp/asn1c

# 3. Create the developer user for host-container permission compatibility
# The Ubuntu 24.04 image ships the "ubuntu" user with UID/GID 1000; it is removed
# so that developer gets the host UID/GID.
RUN (userdel -r ubuntu 2>/dev/null || true) && \
    groupadd -g $GID developer || groupmod -g $GID $(getent group $GID | cut -d: -f1) || true && \
    useradd -m -u $UID -g $GID -s /bin/bash developer || useradd -m -g $GID -s /bin/bash developer || true && \
    echo "developer ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# 4. Intermediate stage to build and package e2sim
FROM base AS e2sim-builder

# Clone the e2-interface skeleton
RUN git clone https://gerrit.o-ran-sc.org/r/sim/e2-interface /tmp/e2-interface && \
    cd /tmp/e2-interface && \
    git checkout da6f82f286cdbb38af1178f82f85877b12c2f85b && \
    cd /tmp/e2-interface/e2sim/asn1c && \
    rm -f *.c *.h Makefile.am.libasncodec asn_constant.h

# Inject the official and corrected ASN.1 files from the local NORI repository
COPY asn/v03/*.asn /tmp/e2-interface/e2sim/asn1c/asn/v03/

# Compile the ASN.1 specifications (E2AP v3.01 + KPM v3.00 + RC v3.01)
RUN cd /tmp/e2-interface/e2sim/asn1c && \
    asn1c -fcompound-names -fno-include-deps -findirect-choice -no-gen-OER -no-gen-example \
        ./asn/v03/e2ap-common-v03.01.asn \
        ./asn/v03/e2ap-constant-v03.01.asn \
        ./asn/v03/e2ap-container-v03.01.asn \
        ./asn/v03/e2ap-epd-v03.01.asn \
        ./asn/v03/e2ap-ied-v03.01.asn \
        ./asn/v03/e2ap-pdu-v03.01.asn && \
    asn1c -fcompound-names -fno-include-deps -findirect-choice -no-gen-OER -no-gen-example \
        ./asn/v03/e2sm-v03.01.asn \
        ./asn/v03/e2sm-kpm-v03.00.asn \
        ./asn/v03/e2sm-rc-v03.01.asn

# Apply the X.691/10.6 conformance patch for CHOICE extension
RUN python3 -c "path='/tmp/e2-interface/e2sim/asn1c/constr_CHOICE.c';content=open(path).read();target='\t\t// X691/23.8 normally encoded as a small non negative whole number\n\t\t\n\t\tif(ext_ct && aper_put_nsnnwn(po, ext_ct->range_bits, present_enc - specs->ext_start))\n\t\t\tASN__ENCODE_FAILED;';replacement='\t\t// X691/23.8 encoded as normally small non-negative whole number\n\t\t// X691/10.6: if n <= 63, encode as 7 bits: [0] + [6-bit value]\n\t\tif(ext_ct) {\n\t\t\tint ext_idx = present_enc - specs->ext_start;\n\t\t\tif(ext_idx <= 63) {\n\t\t\t\tif(per_put_few_bits(po, ext_idx, 7))\n\t\t\t\t\tASN__ENCODE_FAILED;\n\t\t\t} else {\n\t\t\t\tif(per_put_few_bits(po, 1, 1))\n\t\t\t\t\tASN__ENCODE_FAILED;\n\t\t\t\tif(aper_put_nsnnwn(po, ext_idx + 1, ext_idx))\n\t\t\t\t\tASN__ENCODE_FAILED;\n\t\t\t}\n\t\t}';assert target in content,'Target not found in constr_CHOICE.c';open(path,'w').write(content.replace(target,replacement))"

# Adjust CMake export rules and build the DEB package
RUN sed -i 's/\$<TARGET_OBJECTS:def_objects>/\$<TARGET_OBJECTS:asn1_objects>;\$<TARGET_OBJECTS:def_objects>/g' /tmp/e2-interface/e2sim/CMakeLists.txt && \
    sed -i '/unset( DEV_PKG  CACHE )/i\
install(\
    FILES\
        src/messagerouting/e2ap_asn1c_codec.h\
    DESTINATION\
        ${install_inc}\
)\
' /tmp/e2-interface/e2sim/CMakeLists.txt && \
    cd /tmp/e2-interface/e2sim && \
    mkdir -p build && cd build && \
    cmake .. -DDEV_PKG=1 && \
    make package -j$(nproc)

# 5. Simulator base stage (ns-3 + 5G-LENA + installed e2sim)
FROM base AS ns3-env

# Install the .deb package built in the previous stage
COPY --from=e2sim-builder /tmp/e2-interface/e2sim/build/e2sim-dev_1.0.0_amd64.deb /tmp/
RUN dpkg --install /tmp/e2sim-dev_1.0.0_amd64.deb && \
    ldconfig && \
    rm -f /tmp/e2sim-dev_1.0.0_amd64.deb

USER developer
WORKDIR /home/developer

# Clone ns-3.48 and the 5G-LENA NR v5.1 module (compatible pair according to the NR README)
# Previous version: ns-3.42 (ab4cce02) + NR 78b7179e, kept in the image
# nori-nori-dev:ns3.42-nr78b7179e.
ARG NS3_VERSION=ns-3.48
ARG NR_VERSION=v5.1
RUN git clone https://gitlab.com/nsnam/ns-3-dev.git /home/developer/ns-3-dev && \
    cd /home/developer/ns-3-dev && \
    git checkout ${NS3_VERSION} && \
    cd contrib && \
    git clone https://gitlab.com/cttc-lena/nr.git && \
    cd nr && git checkout ${NR_VERSION}

WORKDIR /home/developer/ns-3-dev

# 6. DEV target: development environment with a bind mount
FROM ns3-env AS dev
CMD ["/bin/bash"]

# 7. FINAL target: self-contained optimized image (production / CI)
FROM ns3-env AS final

# Copy the local NORI source (including examples, models, helpers and asn/)
COPY --chown=developer:developer . /home/developer/ns-3-dev/contrib/nori

# Configure with the optimized profile (-d optimized / -O3) and build all modules
RUN ./ns3 configure -d optimized --enable-examples && \
    ./ns3 build -j$(nproc)

CMD ["/bin/bash"]