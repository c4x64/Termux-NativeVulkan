# Termux-NativeVulkan — termux-x11 competitor, Winlator-class HW, native

Native Vulkan display + GPU compute for Termux. No X wire protocol, no
llvmpipe fallback for hot paths: system Vulkan (`Mali-G68`, api 1.3, 97 exts
on the dev phone) first, Mesa LVP fallback, NEON always.

Proven on-device (2026-09-26, SM-A356E):
- `tnvk-probe`: system Vulkan `Mali-G68`
- GLES compute (same loader family): 1920x1080 Mandelbrot 256-iter **66 FPS**,
  matmul-1024 **15.4 GFLOPS**, vs Mesa llvmpipe glmark 4–36 FPS

## Layout

- `src/tnvk-probe.c` — system Vulkan device probe (proves HW, exits nonzero without it)
- `examples/ziro-stress-hook.c` — heavy compute pattern (port of the proven stress)
- `native/txnb/txnb.c` — TXNB native binary: runtime GUI check, in-GUI shell,
  distro-like `install`, WM launch (`txnb run-wm ziro-wm`)
- `android/` — Java app (APK): `MainActivity` hosts the Vulkan GUI surface
  (`TnvkView`) with the CLI terminal docked inside it (`TermView` pty whose
  slave runs `txnb term`); `txnb install <wm>` lands WMs like a normal distro
- `package/build.sh` — Termux `.deb` builder (binary + termux-create-package control)
- `docs/ARCH.md` — server design (Vulkan WSI → AHardwareBuffer, explicit sync)

## Build

```bash
pkg install clang vulkan-headers
./build.sh            # tnvk-probe + ziro-stress-hook + txnb
./txnb gui            # expect: ready Mali-G68
```

APK assembles in CI (`.github/workflows/apk.yml`, needs Android SDK + NDK):
debug artifact `tnvk-debug-apk`.

## Status: v0.1 scaffold

Server (`tnvk` compositor) lands incrementally; probe + compute are real and
tested, present path + Wine/DXVK packaging next. See `docs/ARCH.md`.
