#!/bin/bash
set -xue

# QEMUのファイルパス
QEMU=qemu-system-riscv32

CC=/opt/homebrew/opt/llvm/bin/clang

CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib"
OBJCOPY=/opt/homebrew/opt/llvm/bin/llvm-objcopy

# シェルをリンカスクリプトを指定してビルド
$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf shell.c user.c common.c
# ビルドした実行ファイル (ELF形式) を生バイナリ形式 (raw binary) に変換
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
# 生バイナリ形式の実行イメージを、C言語に埋め込める形式に変換する
$OBJCOPY -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o

# カーネルをビルド
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel.c common.c shell.bin.o

# QEMUを起動
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
    -drive id=drive0,file=lorem.txt,format=raw,if=none \
    -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
    -kernel kernel.elf