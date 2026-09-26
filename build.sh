#!/data/data/com.termux/files/usr/bin/bash
# build.sh — Termux-NativeVulkan v0.1 (needs: pkg install clang vulkan-headers)
set -e
clang -O2 -o tnvk-probe src/tnvk-probe.c -ldl
clang -O3 -o ziro-stress-hook examples/ziro-stress-hook.c -ldl -lm
echo "OK: ./tnvk-probe ./ziro-stress-hook"
