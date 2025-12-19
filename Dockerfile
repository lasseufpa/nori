FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt-get install -y build-essential git cmake libsctp-dev autoconf automake libtool bison flex libboost-all-dev sudo git gcc python3 cmake g++  libc6-dev sqlite sqlite3 libsqlite3-dev libeigen3-dev software-properties-common && add-apt-repository ppa:ubuntu-toolchain-r/test && apt update && apt install -y gcc-11 g++-11 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 10 && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 10 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20
RUN bash -c "cd /home/ && git clone https://github.com/lasseufpa/e2sim.git && cd e2sim/e2sim/ && git checkout 77fec8c2b47a6d2973c6fe825d0f1cf288a05f2c && mkdir build && ./build_e2sim.sh 3"
RUN bash -c "cd /home/ && git clone https://gitlab.com/nsnam/ns-3-dev.git && cd ns-3-dev && git checkout ab4cce021d8f6b2458784704a10af810d3969f0f && cd contrib && git clone https://gitlab.com/cttc-lena/nr.git &&  git clone https://github.com/lasseufpa/nori.git && cd nr && git checkout 78b7179e3841c608c2021ffa88bea1906a1c7594"
RUN bash -c "cd /home/ns-3-dev && ./ns3 configure --enable-examples && ./ns3 build -j 4"
WORKDIR /home/ns-3-dev
