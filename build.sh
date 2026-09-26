#!/data/data/com.termux/files/usr/bin/bash
# build.sh — Termux-NativeVulkan (needs: pkg install clang vulkan-headers)
set -e
clang -O2 -o tnvk-probe src/tnvk-probe.c -ldl
clang -O3 -o ziro-stress-hook examples/ziro-stress-hook.c -ldl -lm
clang -O2 -o txnb native/txnb/txnb.c -ldl
echo "OK: ./tnvk-probe ./ziro-stress-hook ./txnb"
echo "APK (needs Android SDK, runs in CI): see .github/workflows/apk.yml"
