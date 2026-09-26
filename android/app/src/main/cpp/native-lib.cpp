#include <dlfcn.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <vulkan/vulkan.h>

// guiReady(): dlopen system Vulkan, create instance, report first device.
extern "C" JNIEXPORT jstring JNICALL
Java_dev_tnvk_app_NativeBridge_guiReady(JNIEnv* env, jclass) {
    void* h = dlopen("libvulkan.so", RTLD_NOW);
    if (!h) h = dlopen("/system/lib64/libvulkan.so", RTLD_NOW);
    if (!h) return env->NewStringUTF("missing: libvulkan not found");
    auto vkCreateInstance =
        (PFN_vkCreateInstance)dlsym(h, "vkCreateInstance");
    auto vkEnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)dlsym(h, "vkEnumeratePhysicalDevices");
    auto vkGetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)dlsym(h, "vkGetPhysicalDeviceProperties");
    if (!vkCreateInstance || !vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceProperties)
        return env->NewStringUTF("missing: Vulkan symbols");
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.apiVersion = VK_MAKE_VERSION(1, 1, 0);
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &ai;
    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ci, nullptr, &inst) != VK_SUCCESS)
        return env->NewStringUTF("missing: VkCreateInstance failed");
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(inst, &n, nullptr);
    if (!n) return env->NewStringUTF("missing: no physical devices");
    VkPhysicalDevice pd[8];
    if (n > 8) n = 8;
    vkEnumeratePhysicalDevices(inst, &n, pd);
    VkPhysicalDeviceProperties pr{};
    vkGetPhysicalDeviceProperties(pd[0], &pr);
    char out[256];
    snprintf(out, sizeof(out), "ready %s", pr.deviceName);
    return env->NewStringUTF(out);
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_tnvk_app_NativeBridge_defaultShell(JNIEnv* env, jclass) {
    // In-GUI terminal slave runs `txnb term`; plain sh fallback.
    if (!access("/data/data/com.termux/files/usr/bin/txnb", X_OK))
        return env->NewStringUTF("/data/data/com.termux/files/usr/bin/txnb term");
    return env->NewStringUTF("/system/bin/sh");
}

// installPackages(): distro-like `txnb install <pkgs>` via fork+exec.
extern "C" JNIEXPORT jint JNICALL
Java_dev_tnvk_app_NativeBridge_installPackages(JNIEnv* env, jclass, jobjectArray pkgs) {
    jsize n = env->GetArrayLength(pkgs);
    if (n <= 0) return 2;
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        const char* txnb = "/data/data/com.termux/files/usr/bin/txnb";
        char** argv = (char**)malloc(sizeof(char*) * (size_t)(n + 3));
        argv[0] = (char*)"txnb";
        argv[1] = (char*)"install";
        for (jsize i = 0; i < n; i++) {
            jstring s = (jstring)env->GetObjectArrayElement(pkgs, i);
            const char* c = env->GetStringUTFChars(s, nullptr);
            argv[2 + i] = strdup(c ? c : "");
            if (c) env->ReleaseStringUTFChars(s, c);
        }
        argv[2 + n] = nullptr;
        execv(txnb, argv);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

// ---- Surface lifecycle (swapchain lands here next; v0.1 parks cleanly) ----
extern "C" JNIEXPORT void JNICALL
Java_dev_tnvk_app_TnvkView_nativeStart(JNIEnv*, jclass, jobject) {}
extern "C" JNIEXPORT void JNICALL
Java_dev_tnvk_app_TnvkView_nativeResize(JNIEnv*, jclass, jint, jint) {}
extern "C" JNIEXPORT void JNICALL
Java_dev_tnvk_app_TnvkView_nativeStop(JNIEnv*, jclass) {}

// ---- pty for TermView ----
extern "C" JNIEXPORT jintArray JNICALL
Java_dev_tnvk_app_TermView_00024NativePty_nativeOpen(JNIEnv* env, jclass, jstring shell) {
    const char* sh = env->GetStringUTFChars(shell, nullptr);
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) return nullptr;
    grantpt(master);
    unlockpt(master);
    pid_t pid = fork();
    if (pid < 0) { close(master); return nullptr; }
    if (pid == 0) {
        setsid();
        int slave = open(ptsname(master), O_RDWR);
        dup2(slave, 0); dup2(slave, 1); dup2(slave, 2);
        setenv("TERM", "xterm-256color", 1);
        // `txnb term` wraps the shell when present.
        execl("/data/data/com.termux/files/usr/bin/txnb", "txnb", "term",
              sh ? sh : "/system/bin/sh", (char*)nullptr);
        execlp(sh ? sh : "/system/bin/sh", sh ? sh : "sh", (char*)nullptr);
        _exit(127);
    }
    if (sh) env->ReleaseStringUTFChars(shell, sh);
    jintArray out = env->NewIntArray(2);
    int vals[2] = {master, master};
    env->SetIntArrayRegion(out, 0, 2, vals);
    return out;
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_tnvk_app_TermView_00024NativePty_nativeRead(JNIEnv* env, jclass, jint fd, jbyteArray buf) {
    jsize n = env->GetArrayLength(buf);
    jbyte* p = env->GetByteArrayElements(buf, nullptr);
    ssize_t r = read(fd, p, (size_t)n);
    env->ReleaseByteArrayElements(buf, p, 0);
    return (jint)r;
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_tnvk_app_TermView_00024NativePty_nativeWrite(JNIEnv* env, jclass, jint fd, jbyteArray data) {
    jsize n = env->GetArrayLength(data);
    jbyte* p = env->GetByteArrayElements(data, nullptr);
    ssize_t w = write(fd, p, (size_t)n);
    env->ReleaseByteArrayElements(data, p, JNI_ABORT);
    return (jint)w;
}
