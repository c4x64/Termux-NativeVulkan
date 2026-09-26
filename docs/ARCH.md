# ARCH — Termux-NativeVulkan server design (v0.1 → v1)

## Why not X

Termux-X11 measured on Mali-G68 phone: Mesa has no G68 3D driver
(no panfrost/panthor, only `libvulkan_lvp`), `ZINK: failed to choose pdev`,
`DRI3: Could not get DRI3 device` → llvmpipe, glmark 4–36 FPS at 1080p.
X pixmap → socket → Java bitmap → Surface adds a fixed copy + roundtrips.

## Native path (proven)

`dlopen(/system/lib64/libvulkan.so)` → `Mali-G68, api 1.3, 97 exts`.
Same family via `/system` EGL: 1920x1080 Mandelbrot-256 **66 FPS**,
matmul-1024 **15.4 GFLOPS**. Vendor `/vendor` blobs stay blocked by the
linker namespace — `/system` is the allowed HW door, verified by failed
vs successful `dlopen` probes.

## Server (next)

- `tnvk` compositor: Vulkan WSI → `AHardwareBuffer` → Surface, explicit
  sync, mailbox present, per-frame damage. No X protocol on the hot path.
- Compat: `DISPLAY` bridge shims existing X clients (slow path) while
  native clients get swapchain direct.
- Wine/DXVK: Box64 + Wine + DXVK-on-system-Vulkan, same loader the probe
  uses — Winlator's recipe, minus emulation where ARM-native exists.
- Scheduler: sustained-perf hint, CPU affinity for render threads,
  vsync-locked present with late-latch for stale frames.

## Non-goals

No llvmpipe-as-default, no picom-style extra copy, no `/vendor` hacks
that break on Android upgrades.
