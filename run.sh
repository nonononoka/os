#!/bin/bash
set -xue

# QEMUのファイルパス
QEMU=/opt/homebrew/bin/qemu-system-riscv32

# QEMUを起動
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot