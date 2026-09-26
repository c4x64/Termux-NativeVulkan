#!/usr/bin/env bash
# rootfs.sh — bootstrap a minimal ARM64 rootfs for the TNVK distro POC.
#
# Run on a Linux build machine (NOT the Android device). Output
# rootfs.tar.zst ships in the app's asset pack and extracts into
# /data/data/<pkg>/files/rootfs on first launch.
#
# Void first, Alpine fallback — container/GPU plumbing is identical either
# way, so this just gets a working userland fast.
set -euo pipefail

WORKDIR="$(pwd)/rootfs-build"
ROOTFS="${WORKDIR}/rootfs"
ARCH="aarch64"

mkdir -p "${ROOTFS}" "${WORKDIR}"

bootstrap_void() {
  echo "[*] Bootstrapping Void Linux (${ARCH})..."
  local xbps_static_url="https://repo-default.voidlinux.org/static/xbps-static-latest.${ARCH}-musl.tar.xz"
  curl -fsSL "${xbps_static_url}" -o "${WORKDIR}/xbps-static.tar.xz"
  tar -xf "${WORKDIR}/xbps-static.tar.xz" -C "${WORKDIR}"
  local xb_ps
  xb_ps="$(find "${WORKDIR}" -name 'xbps-install*' -type f | head -1)"
  [ -n "${xb_ps}" ] || { echo "xbps-install not found in tarball" >&2; return 1; }
  chmod +x "${xb_ps}"

  # Base first (fatal if this fails); GPU driver subpackages best-effort
  # because their exact names drift between mesa releases.
  XBPS_ARCH="${ARCH}" "${xb_ps}" \
    -S -r "${ROOTFS}" \
    -R "https://repo-default.voidlinux.org/current/musl" \
    -y base-minimal runit wayland wlroots vulkan-loader
  for drv in mesa-dri mesa-vulkan-panfrost mesa-vulkan-swrast mesa-vulkan-freedreno; do
    XBPS_ARCH="${ARCH}" "${xb_ps}" \
      -S -r "${ROOTFS}" \
      -R "https://repo-default.voidlinux.org/current/musl" \
      -y "${drv}" || echo "[!] optional ${drv} missing, continuing"
  done

  # Trim: host kernel manages devices, no hotplug daemon needed.
  local xb_rm
  xb_rm="$(find "${WORKDIR}" -name 'xbps-remove*' -type f | head -1)"
  if [ -n "${xb_rm}" ]; then
    chmod +x "${xb_rm}"
    "${xb_rm}" -r "${ROOTFS}" -y eudev || true
  fi
}

bootstrap_alpine_fallback() {
  echo "[*] Void bootstrap unavailable, falling back to Alpine (${ARCH})..."
  local alpine_ver="3.20"
  local mirror="https://dl-cdn.alpinelinux.org/alpine/v${alpine_ver}/main/${ARCH}/"
  local apk_tools
  apk_tools=$(curl -fsSL "${mirror}" | grep -oE 'apk-tools-static-[^"]+\.apk' | head -1)
  [ -n "${apk_tools}" ] || { echo "apk-tools-static not found" >&2; return 1; }
  curl -fsSL "${mirror}${apk_tools}" -o "${WORKDIR}/apk-tools-static.apk"
  tar -xzf "${WORKDIR}/apk-tools-static.apk" -C "${WORKDIR}"

  "${WORKDIR}/sbin/apk.static" \
    -X "https://dl-cdn.alpinelinux.org/alpine/v${alpine_ver}/main" \
    -U --allow-untrusted --root "${ROOTFS}" --initdb \
    add alpine-base wayland wlroots vulkan-loader
  for drv in mesa-dri mesa-vulkan-panfrost mesa-vulkan-swrast mesa-vulkan-freedreno; do
    "${WORKDIR}/sbin/apk.static" \
      -X "https://dl-cdn.alpinelinux.org/alpine/v${alpine_ver}/main" \
      -U --allow-untrusted --root "${ROOTFS}" \
      add "${drv}" || echo "[!] optional ${drv} missing, continuing"
  done
}

if ! bootstrap_void; then
  # Don't layer Alpine over a half-built Void tree.
  rm -rf "${ROOTFS}"
  mkdir -p "${ROOTFS}"
  bootstrap_alpine_fallback
fi

echo "[*] Stripping docs/locales to shrink footprint..."
rm -rf "${ROOTFS}/usr/share/man" "${ROOTFS}/usr/share/doc" "${ROOTFS}/usr/share/locale" || true

echo "[*] DNS + runit service wiring..."
cp /etc/resolv.conf "${ROOTFS}/etc/resolv.conf" 2>/dev/null || echo "nameserver 1.1.1.1" > "${ROOTFS}/etc/resolv.conf"
mkdir -p "${ROOTFS}/etc/sv/compositor/" "${ROOTFS}/var/service"
cat > "${ROOTFS}/etc/sv/compositor/run" <<'EOF'
#!/bin/sh
exec 2>&1
export MESA_SHADER_CACHE_DIR=/data/shader-cache
export XDG_RUNTIME_DIR=/tmp/xdg
mkdir -p "$XDG_RUNTIME_DIR" "$MESA_SHADER_CACHE_DIR"
# NOTE: headless-compositor is built in Phase 3 of POC.md (custom compositor
# that exports frames as dma-buf fds on $BRIDGE_SOCK). Binary must exist
# before this rootfs is shipped.
exec /usr/bin/headless-compositor --bridge-socket=/tmp/host-bridge/bridge.sock
EOF
chmod +x "${ROOTFS}/etc/sv/compositor/run"
ln -sfn /etc/sv/compositor "${ROOTFS}/var/service/compositor"

echo "[*] Packing rootfs..."
tar -C "${ROOTFS}" -I 'zstd -19 -T0' -cf "${WORKDIR}/rootfs.tar.zst" .

echo "[+] Done: ${WORKDIR}/rootfs.tar.zst"
echo "    Ship this in your app's asset pack / expansion file."
