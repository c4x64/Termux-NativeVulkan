# TNVK distro — full Linux under the Vulkan surface

Goal: the app boots straight into a distro GUI. The Vulkan surface is
fullscreen from launch; the distro's WM renders to it, the distro's shell
backs the terminal overlay. No Termux-X11, no X wire protocol on the hot
path.

## Today (v0.2, real)

- `txnb distro setup` — provisions a Debian userland (`proot-distro`).
- `txnb distro boot` — drops into it (`TNVK_DISTRO=debian`, `TNVK_HW=1`).
- App: `MainActivity` is fullscreen-first (surface fills the screen, TERM
  toggles the shell overlay), keyboard fixed (focus + `adjustResize`).

## Next (compositor bridge)

- tnvk compositor presents distro windows (Xwayland-style shim inside
  proot) to the `AHardwareBuffer` swapchain with explicit sync.
- Preinstalled image option: `txnb distro setup --wm ziro-wm` seeding the
  default desktop so first launch already shows windows, not an empty
  surface.

## Try it (Termux side)

```bash
./build.sh
./txnb distro setup   # one-time, downloads Debian
./txnb distro boot    # you are now in the distro
```
