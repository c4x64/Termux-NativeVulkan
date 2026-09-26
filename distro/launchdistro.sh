#!/system/bin/sh
# launchdistro.sh — runs ON the Android device, invoked by the app's native
# code right after it extracts rootfs.tar.zst into $ROOTFS on first run.
#
# Picks the fastest available container path:
#   - rooted:   unshare + chroot   (no ptrace tax — near-native syscalls)
#   - unrooted: proot              (Termux-patched build recommended)
#
# GPU device nodes are probed, never hardcoded.

set -eu

APP_FILES="${1:?usage: launchdistro.sh <app-files-dir>}"
ROOTFS="${APP_FILES}/rootfs"
BRIDGE_DIR="${APP_FILES}/bridge"
BRIDGE_SOCK="${BRIDGE_DIR}/bridge.sock"

mkdir -p "${BRIDGE_DIR}"

# Probe GPU nodes (kgsl=Qualcomm, mali=ARM, dri=Exynos/generic).
GPU_NODES=""
for node in /dev/kgsl-3d0 /dev/mali0 /dev/dri/renderD128 /dev/dri/card0; do
  [ -e "$node" ] && GPU_NODES="${GPU_NODES} ${node}"
done

# Resolve PID 1 inside the built rootfs (layout varies by bootstrap).
INIT=/sbin/init
[ -x "${ROOTFS}/sbin/runit-init" ] && INIT=/sbin/runit-init
[ -x "${ROOTFS}/sbin/init" ] && INIT=/sbin/init

is_rooted() {
  command -v su >/dev/null 2>&1 && su -c 'id -u' 2>/dev/null | grep -q '^0$'
}

run_rooted() {
  echo "[*] Root detected: using unshare+chroot (native syscall speed)."
  # Mounts happen INSIDE the private namespace (never leak globally).
  NSHELPER="${APP_FILES}/ns-enter.sh"
  {
    echo "set -e"
    echo "mount --bind /dev '${ROOTFS}/dev'"
    echo "mount --bind /proc '${ROOTFS}/proc'"
    echo "mount --bind /sys '${ROOTFS}/sys'"
    for n in ${GPU_NODES}; do
      echo "mkdir -p '${ROOTFS}$(dirname "$n")'"
      echo "touch '${ROOTFS}${n}'"
      echo "mount --bind '${n}' '${ROOTFS}${n}'"
    done
    echo "mkdir -p '${ROOTFS}/tmp/host-bridge'"
    echo "mount --bind '${BRIDGE_DIR}' '${ROOTFS}/tmp/host-bridge'"
    echo "exec chroot '${ROOTFS}' '${INIT}'"
  } > "${NSHELPER}"
  chmod +x "${NSHELPER}"
  su -c "unshare --mount --pid --fork --uts /bin/sh '${NSHELPER}'"
}

run_unrooted() {
  echo "[*] No root: using proot (expect some syscall overhead vs. rooted path)."
  # NOTE: use the Termux-patched proot build (has perf/seccomp fixes upstream
  # proot lacks). Ship it as a native lib (libproot.so trick) since Android
  # won't let you exec arbitrary binaries from app-writable storage directly.
  PROOT="${APP_FILES}/bin/proot"
  BINDS="-b /dev -b /proc -b /sys -b ${BRIDGE_DIR}:/tmp/host-bridge"
  # shellcheck disable=SC2086
  for n in ${GPU_NODES}; do
    [ -n "$n" ] && BINDS="${BINDS} -b ${n}"
  done
  # shellcheck disable=SC2086
  exec "${PROOT}" \
    --link2symlink \
    --kill-on-exit \
    -r "${ROOTFS}" \
    ${BINDS} \
    -w /root \
    "${INIT}"
}

if is_rooted; then
  run_rooted
else
  run_unrooted
fi
