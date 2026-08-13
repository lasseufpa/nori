FROM ubuntu:20.04

# Arguments for UID and GID to match the host user
ARG UID=1000
ARG GID=971

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt-get install -y \
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
    libc6-dev \
    sqlite3 \
    nlohmann-json3-dev \
    libsqlite3-dev \
    libeigen3-dev \
    software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt update && \
    apt install -y gcc-11 g++-11 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 10 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 10 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20 && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /home

RUN git clone https://github.com/nokia/asn1c.git && \
    cd asn1c && \
    autoreconf -iv && \
    ./configure && \
    make && \
    make install && \
    ldconfig

# RUN git clone https://gerrit.o-ran-sc.org/r/sim/e2-interface \
RUN cd /home && \
    git clone https://gerrit.o-ran-sc.org/r/sim/e2-interface && \
    cd e2-interface && \
    git checkout da6f82f286cdbb38af1178f82f85877b12c2f85b && \
    cd /home/e2-interface/e2sim/asn1c && \
    rm -f *.c *.h Makefile.am.libasncodec asn_constant.h && \
    asn1c \
        -fcompound-names \
        -fno-include-deps \
        -findirect-choice \
        -no-gen-OER \
        -no-gen-example \
        ./asn/v03/e2ap-common-v03.01.asn \
        ./asn/v03/e2ap-constant-v03.01.asn \
        ./asn/v03/e2ap-container-v03.01.asn \
        ./asn/v03/e2ap-epd-v03.01.asn \
        ./asn/v03/e2ap-ied-v03.01.asn \
        ./asn/v03/e2ap-pdu-v03.01.asn && \
    asn1c \
        -fcompound-names \
        -fno-include-deps \
        -findirect-choice \
        -no-gen-OER \
        -no-gen-example \
        ./asn/v03/e2sm-v03.01.asn \
        ./asn/v03/e2sm-kpm-v03.00.asn
# fix the encoding of CHOICE extension index in constr_CHOICE.c to comply with X691/10.6
RUN python3 -c "path='/home/e2-interface/e2sim/asn1c/constr_CHOICE.c';content=open(path).read();target='\t\t// X691/23.8 normally encoded as a small non negative whole number\n\t\t\n\t\tif(ext_ct && aper_put_nsnnwn(po, ext_ct->range_bits, present_enc - specs->ext_start))\n\t\t\tASN__ENCODE_FAILED;';replacement='\t\t// X691/23.8 encoded as normally small non-negative whole number\n\t\t// X691/10.6: if n <= 63, encode as 7 bits: [0] + [6-bit value]\n\t\tif(ext_ct) {\n\t\t\tint ext_idx = present_enc - specs->ext_start;\n\t\t\tif(ext_idx <= 63) {\n\t\t\t\tif(per_put_few_bits(po, ext_idx, 7))\n\t\t\t\t\tASN__ENCODE_FAILED;\n\t\t\t} else {\n\t\t\t\tif(per_put_few_bits(po, 1, 1))\n\t\t\t\t\tASN__ENCODE_FAILED;\n\t\t\t\tif(aper_put_nsnnwn(po, ext_idx + 1, ext_idx))\n\t\t\t\t\tASN__ENCODE_FAILED;\n\t\t\t}\n\t\t}';assert target in content,'Target not found in constr_CHOICE.c';open(path,'w').write(content.replace(target,replacement))"

RUN sed -i '104s|^\([[:space:]]*\).*|\1"$<TARGET_OBJECTS:asn1_objects>;$<TARGET_OBJECTS:def_objects>;$<TARGET_OBJECTS:sctp_objects>;$<TARGET_OBJECTS:messagerouting_objects>;$<TARGET_OBJECTS:encoding_objects>;$<TARGET_OBJECTS:base_objects>"|' \
        /home/e2-interface/e2sim/CMakeLists.txt && \
    sed -i '112s|^\([[:space:]]*\).*|\1"$<TARGET_OBJECTS:asn1_objects>;$<TARGET_OBJECTS:def_objects>;$<TARGET_OBJECTS:sctp_objects>;$<TARGET_OBJECTS:messagerouting_objects>;$<TARGET_OBJECTS:encoding_objects>;$<TARGET_OBJECTS:base_objects>"|' \
        /home/e2-interface/e2sim/CMakeLists.txt
RUN sed -i '/unset( DEV_PKG  CACHE )/i\
install(\
    FILES\
        src/messagerouting/e2ap_asn1c_codec.h\
    DESTINATION\
        ${install_inc}\
)\
' /home/e2-interface/e2sim/CMakeLists.txt

RUN cd /home/e2-interface/e2sim && \
    mkdir -p build && \
    cd build && \
    cmake .. -DDEV_PKG=1 && \
    make package && \
    dpkg --install ./e2sim-dev_1.0.0_amd64.deb && \
    # cp /home/e2-interface/e2sim/src/messagerouting/e2ap_asn1c_codec.h    /usr/local/include/e2sim/ \
    ldconfig

RUN cd /home && \
    git clone https://gitlab.com/nsnam/ns-3-dev.git && \
    cd ns-3-dev && \
    git checkout ab4cce021d8f6b2458784704a10af810d3969f0f && \
    cd contrib && \
    git clone https://gitlab.com/cttc-lena/nr.git && \
    git clone --branch feature/kpm-v3-upgrade https://github.com/lasseufpa/nori.git && \
    cd nr && git checkout 78b7179e3841c608c2021ffa88bea1906a1c7594

RUN cd /home/ns-3-dev && \
    ./ns3 configure --enable-examples && \
    ./ns3 build -j 14

WORKDIR /home