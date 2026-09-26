package dev.tnvk.app;

/** JNI bridge to system Vulkan (HW check) and txnb helpers. */
public final class NativeBridge {

    private NativeBridge() {}

    /** Human-readable GUI readiness, e.g. "ready Mali-G68" or a reason. */
    public static native String guiReady();

    public static boolean isGuiReady() {
        String s = guiReady();
        return s != null && s.startsWith("ready");
    }

    /** Shell binary the in-GUI terminal should spawn (`txnb term` wraps it). */
    public static native String defaultShell();

    /** Distro-like install passthrough: install WM/tools via txnb. */
    public static native int installPackages(String[] pkgs);
}
