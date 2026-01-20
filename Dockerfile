################################################################################
# BASE IMAGE
################################################################################
FROM ubuntu@sha256:79efa276fdefa2ee3911db29b0608f8c0561c347ec3f4d4139980d43b168d991

ENV DEBIAN_FRONTEND=noninteractive

# Update the package database
RUN apt clean
RUN apt update

################################################################################
# BASE SYSTEM + BUILD ESSENTIALS
################################################################################
RUN apt update && apt install -y \
    sudo git wget gedit ca-certificates \
    build-essential pkg-config \
    cmake ninja-build \
    make meson \
    gdb lldb valgrind \
    python3 python3-dev python3-pip pipx \
    && rm -rf /var/lib/apt/lists/*

################################################################################
# COMPILERS (GCC 15 + Clang 19) - we assume that ubuntu@sha256 does include these packages.
################################################################################
RUN apt update && apt install -y \
    gcc-15 g++-15 clang-19 clangd-19 llvm-19-dev libclang-19-dev \
    && rm -rf /var/lib/apt/lists/*
    # Set compiler priority to the latest gcc/g++
RUN ln -fs /usr/bin/gcc-15 /usr/bin/gcc && \
    ln -fs /usr/bin/g++-15 /usr/bin/g++

################################################################################
# RUNTIME & DEVELOPMENT LIBRARIES
################################################################################
RUN apt update && apt install -y \
    # Graphics tool and SFML library
    libsfml-dev gnuplot \
    && rm -rf /var/lib/apt/lists/*

# NVIDIA stdexec library
RUN git clone https://github.com/NVIDIA/stdexec.git /tmp/stdexec && \
    cmake -S /tmp/stdexec -B /tmp/stdexec/build \
          -DCMAKE_BUILD_TYPE=Release \
          -DSTDEXEC_ENABLE_TESTING=OFF \
          -DSTDEXEC_BUILD_EXAMPLES=OFF && \
    cmake --build /tmp/stdexec/build -j$(nproc) && \
    cmake --install /tmp/stdexec/build && \
    rm -rf /tmp/stdexec

# GoogleTest
RUN git clone https://github.com/google/googletest.git /tmp/googletest && \
    cmake -S /tmp/googletest -B /tmp/googletest/build \
          -DCMAKE_BUILD_TYPE=Release && \
    cmake --build /tmp/googletest/build -j$(nproc) && \
    cmake --install /tmp/googletest/build && \
    rm -rf /tmp/googletest

# Matplot++ library (requires gnuplot package)
RUN git clone --branch master https://github.com/alandefreitas/matplotplusplus.git /tmp/matplotpp && \
    cmake -S /tmp/matplotpp -B /tmp/matplotpp/build \
          -DMATPLOTPP_BUILD_EXAMPLES=OFF \
          -DMATPLOTPP_BUILD_SHARED_LIBS=ON \
          -DMATPLOTPP_BUILD_TESTS=OFF \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON && \
    cmake --build /tmp/matplotpp/build -j$(nproc) && \
    cmake --install /tmp/matplotpp/build && \
    rm -rf /tmp/matplotpp

################################################################################
# USER SETUP (same as your original)
################################################################################
ARG USER=dev
ARG USER_UID=1001
ARG USER_GID=1001

RUN groupadd -g ${USER_GID} ${USER} && \
    useradd -m -u ${USER_UID} -g ${USER_GID} -s /bin/bash ${USER} && \
    echo "${USER} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers


################################################################################
# WORKDIR + ENTRYPOINT
################################################################################
USER ${USER}
WORKDIR /home/${USER}

CMD ["/bin/bash"]

