# POC: Embedded ARM64 Linux Distro in an Android App (Vulkan HW-accelerated)

**Goal:** ship a real Linux userland (Void-style, arm64) inside an Android app, running
GUI/GPU-using Linux software, composited onto the app's own Vulkan-backed Surface with
near-native performance.

This doc is written for an autonomous coding agent to execute step by step. It gives two
architectures, recommends one for the POC, and lists concrete perf hacks. Read all of it
before writing code — later sections depend on decisions made in earlier ones.

---

## 0. Two possible architectures (pick one before starting)

| | **A. Shared-kernel container** (proot/chroot rootfs) | **B. Real VM** (AVF + crosvm + virtio-gpu/Venus) |
|---|---|---|
| Isolation | None — same kernel as Android | Full — separate guest kernel |
| GPU path | Direct passthrough to host driver nodes (`/dev/kgsl-3d0`, `/dev/mali0`, `/dev/dri/*`) | Paravirtualized via virglrenderer/Venus (extra hop, more device support) |
| Perf overhead | Near-zero if rooted (unshare+chroot); moderate if unrooted (proot ptrace tax) | VM boot cost, virtio-gpu serialization overhead |
| Permissions needed | None special (works in a normal app's private storage) | `android.permission.MANAGE_VIRTUAL_MACHINE` + AVF custom-VM support — currently gated to system/privileged apps or developer-mode flows on select devices (this is literally how Google's own Debian "Linux Terminal" app works on Pixel) |
| Device compatibility | Any ARM64 Android device, any Android version with basic namespace/ptrace support | Requires pKVM/AVF support — Pixel + a growing but still small set of OEMs |
| "Real distro" feel | Yes — actual Void Linux userland, actual xbps | Yes — arguably "more real" since it's a real kernel too |

**Recommendation for this POC: Architecture A.** It's the only one a normal (non-system,
non-privileged) app can ship today, it works on any rooted-or-not ARM64 device, and GPU
access is literally the same kernel driver Android itself uses — so "HW accel" is just a
device-node bind mount away, not a virtualized round-trip.

Treat **Architecture B as a Phase 2 stretch goal** once/if you have AVF custom-VM
entitlement — the rootfs and package set built in Phase 1 are directly reusable as the VM's
disk image.

---

## 1. Component map (Architecture A)

```
┌─────────────────────────────────────────────────────────────┐
│ Android App (Kotlin/NDK)                                     │
│  ┌──────────────┐   Surface/SurfaceView (Vulkan swapchain)   │
│  │ VulkanSurface│◄──────────────────────────────┐            │
│  └──────────────┘                                │            │
│         ▲ AHardwareBuffer import                  │            │
│         │ (VK_ANDROID_external_memory_ahb)        │            │
│  ┌──────┴───────────────────────────────────────┐ │            │
│  │ Bridge daemon (native, part of your app)      │ │            │
│  │  - owns a dma-buf fence/queue with compositor │ │            │
│  │  - wraps guest dma-buf → AHardwareBuffer       │ │            │
│  └──────┬─────────────────────────────────────────┘            │
└─────────┼──────────────────────────────────────────────────────┘
          │ dma-buf fd passed over a unix socket into the rootfs
┌─────────▼──────────────────────────────────────────────────────┐
│ Linux rootfs (Void-style arm64), started via proot/unshare      │
│  ┌────────────┐   ┌────────────────┐   ┌────────────────────┐  │
│  │ runit init │──▶│ headless Wayland│──▶│ Linux GUI clients   │  │
│  │ (minimal)  │   │ compositor      │   │ (Vulkan/GLES apps)  │  │
│  └────────────┘   │ (single output, │   └────────────────────┘  │
│                    │ renders to a    │                            │
│                    │ dma-buf-backed  │                            │
│                    │ wl_buffer)      │                            │
│                    └────────┬────────┘                            │
│                             │ direct ioctl                        │
│                    ┌────────▼────────┐                            │
│                    │ /dev/kgsl-3d0 or │  ← bind-mounted from host  │
│                    │ /dev/mali0 / dri │                            │
│                    └─────────────────┘                            │
└──────────────────────────────────────────────────────────────────┘
```

Key idea: the compositor never touches the CPU with pixel data. It renders into a
`dma-buf`-backed buffer using the real GPU driver; that same buffer's fd is handed to the
Android side and imported as an `AHardwareBuffer`, then bound as a Vulkan image and
presented into your app's swapchain. Zero-copy end to end.

---

## 2. Step-by-step build plan

### Phase 1 — Rootfs
1. Bootstrap a Void Linux arm64 rootfs using `xbps-static` (Void ships arm64 bootstrap
   tarballs). If Void's arm64 mirror is unreliable, fall back to Alpine arm64 (`apk`) for
   the POC skeleton and swap to Void once the pipeline works — the container/GPU plumbing
   is identical either way.
2. Strip to a minimal base: runit, busybox/coreutils, mesa (or vendor Vulkan ICD if using
   proprietary blobs), wayland, wlroots (or a lighter custom compositor), a test GUI app
   (e.g. `vkcube`, `weston-simple-egl`, or a real Vulkan app).
3. Do **not** ship udev — the host kernel already manages devices; use static device nodes
   or `mdev` only if something insists on hotplug events.
4. Package the rootfs as a plain directory tree (not a loopback image) inside the app's
   private storage (`/data/data/<pkg>/files/rootfs`). Directory-based is faster than
   loop-mounting an ext4 image and avoids a second filesystem layer.

### Phase 2 — Container launch
5. Detect root at runtime:
   - **Rooted:** use `unshare` + `pivot_root`/`chroot` (mount namespaces + bind mounts).
     This removes all ptrace overhead — syscalls run at native speed.
   - **Unrooted:** use `proot` (Termux's patched fork, which has perf fixes upstream
     Termux doesn't) with `--link2symlink` and `-0` only if you need fake root; avoid
     unnecessary path translation by keeping bind mounts minimal.
6. Bind-mount into the rootfs: the GPU device node(s), `/dev/dri` if present, any vendor
   Vulkan/EGL libs the host uses (or ship Mesa's open drivers — Turnip for Adreno,
   Panfrost for Mali, Freedreno as fallback — statically in the rootfs so you're not
   dependent on vendor blobs being loadable cross-namespace).
7. Start runit as PID 1 inside the namespace/proot, bring up only the compositor service.

### Phase 3 — Display bridge
8. Compositor (inside rootfs) renders its single output to a `dma-buf` via
   `VK_EXT_image_drm_format_modifier` + `VK_EXT_external_memory_dma_buf`, or via GBM if
   staying GLES/EGL.
9. Compositor sends the dma-buf fd out over a `SOCK_SEQPACKET` unix socket (works across
   the proot/namespace boundary same as any fd-passing `SCM_RIGHTS` call).
10. Android-side native bridge (JNI/NDK) receives the fd, wraps it as `AHardwareBuffer`
    (`AHardwareBuffer_createFromHandle` equivalent via a small custom import path, or by
    constructing an `AHardwareBuffer` with matching format/usage flags and importing the
    dma-buf as the backing memory through `VK_ANDROID_external_memory_android_hardware_buffer`).
11. Import as a `VkImage`, present it each frame into your `VkSwapchainKHR` bound to the
    `SurfaceView`'s `ANativeWindow`. Use `VK_PRESENT_MODE_MAILBOX_KHR` for lowest latency.

---

## 3. Performance hacks (apply all of these for the POC)

- **Skip proot entirely when rooted.** This is the single biggest win — `unshare` +
  `chroot` has no ptrace tax; proot syscall interception can be 2–10x slower for
  syscall-heavy workloads.
- **Directory rootfs, not a loop-mounted image** — one less filesystem layer, no loop
  device overhead.
- **`noatime`** on any mounts you do control; avoid `sync`-heavy filesystems for the
  rootfs.
- **Zero-copy display path only** — never read pixels back to CPU to hand to Android;
  dma-buf → AHardwareBuffer → VkImage the whole way.
- **`VK_PRESENT_MODE_MAILBOX_KHR`** for the app-side swapchain to avoid queuing latency.
- **Use open Mesa drivers (Turnip/Panfrost/Freedreno) statically linked in the rootfs**
  rather than trying to bridge to Android's vendor blob — much more portable across
  devices and avoids ABI mismatches with the host's libc/linker.
- **Zink** (OpenGL-over-Vulkan) inside the guest if a Linux app needs GLES but you only
  trust the Vulkan driver — keeps one driver stack instead of two.
- **`runit`, not systemd** — Void's own choice, and it matters more here: fewer cgroup/dbus
  services means faster cold start and lower idle CPU.
- **CPU governor / sustained performance mode** — request
  `Window.setSustainedPerformanceMode(true)` and a partial wake lock for the session so
  Android doesn't throttle mid-session; consider `cpuset`/`taskset`-pinning the compositor
  and heavy Linux workloads to the big cores (`/sys/devices/system/cpu/cpuX/cpufreq`).
- **Mesa shader cache on fast storage** — point `MESA_SHADER_CACHE_DIR` at app-private
  storage (or tmpfs if you can get one via `unshare -m`), not slow shared storage.
- **Prefetch package cache** — bundle a pre-resolved `xbps` (or `apk`) package cache in
  your APK's asset pack / an expansion file so first run doesn't need network bootstrap.
- **`SCHED_FIFO` or elevated nice for the compositor thread only** — keep frame pacing
  stable without starving the rest of the guest.
- **`io_uring`-enabled userland tools** if the Linux workload is I/O heavy — real perf win
  on modern kernels, and Android's kernels are recent enough on most target devices.

---

## 4. Known risks / honesty checklist

- **Play Store policy:** dynamically fetching and executing a foreign Linux userland with
  its own package manager/binaries sits in a gray area of Play policy around executable
  code from outside the APK. Review Google Play's policies on this before shipping
  publicly; fine for sideloaded/POC/dev distribution.
- **Device fragmentation:** GPU device node paths differ by vendor (`kgsl-3d0` on
  Qualcomm, `mali0` on ARM Mali SoCs, `/dev/dri/*` where a real DRM/KMS stack exists —
  e.g. Samsung Exynos/Mali exposes only `/dev/dri/card0` + `renderD128`, neither of the
  other two).
  Your bind-mount + driver-selection logic needs a device-capability probe, not a hardcode
  (`launchdistro.sh` in this directory implements exactly that probe).
- **SELinux/scoped storage:** newer Android versions restrict raw device-node access more
  aggressively; test unrooted behavior per-OEM, per-Android-version — this is the part
  most likely to need per-device workarounds.
- **Vulkan driver availability:** this only works well on devices where an open (Turnip/
  Panfrost/Freedreno) or otherwise cross-namespace-usable Vulkan driver exists for the
  SoC. Some devices will only have a vendor blob tightly coupled to Android's own
  loader — for those, Architecture B (real VM w/ Venus) is the only clean path.
- **Architecture B is real but gated**: Google's Pixel "Linux Terminal" app proves the
  approach (AVF + crosvm + virtio-gpu/Venus, GPU-accelerated Debian VM), but the
  custom-VM-with-GPU capability isn't generally available to arbitrary third-party apps
  yet — treat it as a roadmap item, not a Phase-1 deliverable.
