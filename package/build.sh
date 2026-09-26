#!/data/data/com.termux/files/usr/bin/bash
# package/build.sh — build Termux .deb for tnvk (v0.1: probe + stress hook)
# Needs: pkg install termux-create-package
set -e
cd "$(dirname "$0")/.."
./build.sh
PKGDIR=package/tnvk_0.1_aarch64
rm -rf "$PKGDIR"; mkdir -p "$PKGDIR/data/data/com.termux/files/usr/bin"
cp -f tnvk-probe ziro-stress-hook "$PKGDIR/data/data/com.termux/files/usr/bin/"
mkdir -p "$PKGDIR/DEBIAN"
cat > "$PKGDIR/DEBIAN/control" <<'CTL'
Package: tnvk
Version: 0.1
Architecture: aarch64
Maintainer: c4x64
Description: Termux-native Vulkan display + GPU compute (termux-x11 competitor)
 Raw Mali HW path with Mesa fallback; v0.1 ships probe + compute hook,
 compositor + Wine/DXVK packaging follow.
CTL
termux-create-package "$PKGDIR" 2>/dev/null || dpkg-deb -b "$PKGDIR" tnvk_0.1_aarch64.deb
ls -lh tnvk*.deb
