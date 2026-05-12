FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    gcc-riscv64-linux-gnu \
    binutils-riscv64-linux-gnu \
    qemu-system-misc \
    gdb-multiarch \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash"]
