###############################################################################
# Build this stage for a build environment, e.g.:                             #
#                                                                             #
# docker build --tag mpspdz:buildenv --target buildenv .                      #
#                                                                             #
# The above is equivalent to:                                                 #
#                                                                             #
#   docker build --tag mpspdz:buildenv \                                      #
#     --target buildenv \                                                     #
#     --build-arg arch=native \                                               #
#     --build-arg cxx=clang++-11 \                                            #
#     --build-arg use_ntl=0 \                                                 #
#     --build-arg prep_dir="Player-Data" \                                    #
#     --build-arg ssl_dir="Player-Data"                                       #
#     --build-arg cryptoplayers=0                                             #
#                                                                             #
# To build for an x86-64 architecture, with g++, NTL (for HE), custom         #
# prep_dir & ssl_dir, and to use encrypted channels for 4 players:            #
#                                                                             #
#   docker build --tag mpspdz:buildenv \                                      #
#     --target buildenv \                                                     #
#     --build-arg arch=x86-64 \                                               #
#     --build-arg cxx=g++ \                                                   #
#     --build-arg use_ntl=1 \                                                 #
#     --build-arg prep_dir="/opt/prepdata" \                                  #
#     --build-arg ssl_dir="/opt/ssl"                                          #
#     --build-arg cryptoplayers=4 .                                           #
#                                                                             #
# To work in a container to build different machines, and compile programs:   #
#                                                                             #
# docker run --rm -it mpspdz:buildenv bash                                    #
#                                                                             #
# Once in the container, build a machine and compile a program:               #
#                                                                             #
#   $ make replicated-ring-party.x                                            #
#   $ ./compile.py -R 64 tutorial                                             #
#                                                                             #
###############################################################################
FROM python:3.10.3-bullseye as buildenv

RUN apt-get update && apt-get install -y --no-install-recommends \
                automake \
                build-essential \
                clang-11 \
		cmake \
                git \
                libboost-dev \
                libboost-thread-dev \
                libclang-dev \
                libgmp-dev \
                libntl-dev \
                libsodium-dev \
                libssl-dev \
                libtool \
                vim \
                gdb \
                valgrind \
        && rm -rf /var/lib/apt/lists/*

ENV MP_SPDZ_HOME /usr/src/MP-SPDZ
WORKDIR $MP_SPDZ_HOME

RUN pip install --upgrade pip ipython

COPY . .

ARG arch=
ARG cxx=clang++-11
ARG use_ntl=0
ARG prep_dir="Player-Data"
ARG ssl_dir="Player-Data"

RUN if test -n "${arch}"; then echo "ARCH = -march=${arch}" >> CONFIG.mine; fi
RUN echo "CXX = ${cxx}" >> CONFIG.mine \
        && echo "USE_NTL = ${use_ntl}" >> CONFIG.mine \
        && echo "MY_CFLAGS += -I/usr/local/include" >> CONFIG.mine \
        && echo "MY_LDLIBS += -Wl,-rpath -Wl,/usr/local/lib -L/usr/local/lib" \
            >> CONFIG.mine \
        && mkdir -p $prep_dir $ssl_dir \
        && echo "PREP_DIR = '-DPREP_DIR=\"${prep_dir}/\"'" >> CONFIG.mine \
        && echo "SSL_DIR = '-DSSL_DIR=\"${ssl_dir}/\"'" >> CONFIG.mine

# ssl keys
ARG cryptoplayers=
ENV PLAYERS ${cryptoplayers}
RUN ./Scripts/setup-ssl.sh "${cryptoplayers}" ${ssl_dir}

RUN make clean-deps boost libote

###############################################################################
# Use this stage to a build a specific virtual machine. For example:          #
#                                                                             #
#   docker build --tag mpspdz:shamir \                                        #
#     --target machine \                                                      #
#     --build-arg machine=shamir-party.x \                                    #
#     --build-arg gfp_mod_sz=4 .                                              #
#                                                                             #
# The above will build shamir-party.x with 256 bit length.                    #
#                                                                             #
# If no build arguments are passed (via --build-arg), mascot-party.x is built #
# with the default 128 bit length.                                            #
###############################################################################
FROM buildenv as machine

ARG machine="mascot-party.x"

ARG gfp_mod_sz=2

RUN echo "MOD = -DGFP_MOD_SZ=${gfp_mod_sz}" >> CONFIG.mine

RUN make clean && make ${machine} && cp ${machine} /usr/local/bin/


################################################################################
# This is the default stage. Use it to compile a high-level program.           #
# By default, tutorial.mpc is compiled with --field=64 bits.                   #
#                                                                              #
#   docker build --tag mpspdz:mascot-tutorial \                                #
#     --build-arg src=tutorial \                                               #
#     --build-arg compile_options="--field=64" .                               #
#                                                                              #
# Note that build arguments from previous stages can also be passed. For       #
# instance, building replicated-ring-party.x, for 3 crypto players with custom #
# PREP_DIR and SSL_DIR, and compiling tutorial.mpc with --ring=64:             #
#                                                                              #
#   docker build --tag mpspdz:replicated-ring \                                #
#           --build-arg machine=replicated-ring-party.x \                      #
#           --build-arg prep_dir=/opt/prep \                                   #
#           --build-arg ssl_dir=/opt/ssl \                                     #
#           --build-arg cryptoplayers=3 \                                      #
#           --build-arg compile_options="--ring=64" .                          #
#                                                                              #
# Test it:                                                                     #
#                                                                              #
#   docker run --rm -it mpspdz:replicated-ring ./Scripts/ring.sh tutorial      #
################################################################################
FROM machine as program

ARG src="tutorial"
ARG compile_options="--field=64"
RUN ./compile.py ${compile_options} ${src}
RUN mkdir -p Player-Data \
        && echo 1 2 3 4 > Player-Data/Input-P0-0 \
        && echo 1 2 3 4 > Player-Data/Input-P1-0
