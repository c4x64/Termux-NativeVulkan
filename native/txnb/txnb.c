/* txnb.c — TXNB: Termux Native Binary (companion to the TNVK Android app)
 *
 * The app loads GUI at runtime (Vulkan surface), hosts CLI inside the GUI
 * (terminal view wired to a pty whose slave runs `txnb term`), and installs
 * WMs/tools like a normal distro via `txnb install <pkgs>` (pkg passthrough).
 *
 * Build: clang -O2 -o txnb txnb.c -ldl
 * Usage: txnb --version | gui | term [shell] | install <pkgs...> | run-wm <wm...>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#define TXNB_VERSION "0.1.0"

static void usage(const char *a){
    fprintf(stderr,
        "txnb %s — Termux Native Binary\n"
        "usage:\n"
        "  %s --version\n"
        "  %s gui                 check runtime GUI readiness (system Vulkan HW)\n"
        "  %s term [shell]        exec shell for the in-GUI terminal (default $SHELL or bash)\n"
        "  %s install <pkgs...>   install WM/tools like a distro (pkg passthrough)\n"
        "  %s run-wm <wm...>      launch WM under native-GUI env (default ziro-wm)\n"
        "  %s distro {setup|boot} full distro userland (debian) under the GUI\n",
        TXNB_VERSION, a, a, a, a, a, a);
}

static int cmd_version(void){
    char model[128]={0};
    FILE *p=popen("getprop ro.product.model 2>/dev/null","r");
    if(p){
        if(!fgets(model,sizeof model,p)) model[0]=0;
        pclose(p);
        size_t n=strlen(model); while(n&&model[n-1]=='\n')model[--n]=0;
    }
    printf("txnb %s (aarch64, termux-native) device=%s\n",
        TXNB_VERSION, model[0]?model:"unknown");
    return 0;
}

/* Minimal system-Vulkan HW check: dlopen + create instance + count devices. */
#include <vulkan/vulkan.h>
static int cmd_gui(void){
    void *h=dlopen("/system/lib64/libvulkan.so",RTLD_NOW);
    if(!h){ printf("GUI: no system Vulkan (%s)\n",dlerror()); return 1; }
    PFN_vkCreateInstance cI=(void*)dlsym(h,"vkCreateInstance");
    PFN_vkEnumeratePhysicalDevices eP=(void*)dlsym(h,"vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties gP=(void*)dlsym(h,"vkGetPhysicalDeviceProperties");
    if(!cI||!eP||!gP){ printf("GUI: Vulkan symbols missing\n"); return 1; }
    VkApplicationInfo ai={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.apiVersion=VK_MAKE_VERSION(1,1,0)};
    VkInstanceCreateInfo ci={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
    VkInstance inst=0;
    if(cI(&ci,0,&inst)!=VK_SUCCESS){ printf("GUI: VkCreateInstance failed\n"); return 1; }
    uint32_t n=0; eP(inst,&n,0);
    if(!n){ printf("GUI: no physical devices\n"); return 1; }
    VkPhysicalDevice pd[8]; if(n>8)n=8; eP(inst,&n,pd);
    VkPhysicalDeviceProperties pr; gP(pd[0],&pr);
    printf("GUI: ready device=%s api=%u.%u ext=system-vulkan\n", pr.deviceName,
        VK_VERSION_MAJOR(pr.apiVersion), VK_VERSION_MINOR(pr.apiVersion));
    return 0;
}

static int cmd_term(int argc, char **argv){
    const char *sh = argc>0 ? argv[0] : getenv("SHELL");
    if(!sh||!*sh) sh="bash";
    setenv("TERM","xterm-256color",1);
    execlp(sh,sh,(char*)0);
    perror("txnb term: exec failed"); return 1;
}

static int cmd_install(int argc, char **argv){
    if(!argc){ fprintf(stderr,"txnb install: need package names (e.g. txnb install ziro-wm)\n"); return 2; }
    char **a=malloc(sizeof(char*)*(size_t)(argc+4));
    a[0]="pkg"; a[1]="install"; a[2]="-y";
    for(int i=0;i<argc;i++) a[3+i]=argv[i];
    a[3+argc]=0;
    execvp("pkg",a);
    perror("txnb install: pkg not found"); return 1;
}

static int cmd_run_wm(int argc, char **argv){
    setenv("TNVK_HW","1",1);
    if(!getenv("DISPLAY")) setenv("DISPLAY",":0",1);
    if(!argc){ static char *d[]={"ziro-wm",0}; argv=d; }
    execvp(argv[0],argv);
    perror("txnb run-wm: exec failed"); return 1;
}

/* Full distro userland under the GUI: debian via proot-distro.
 * setup provisions it, boot drops into it (its shell becomes the
 * terminal overlay's shell, its WM renders to the Vulkan surface). */
static int cmd_distro(int argc, char **argv){
    if(argc<1){ fprintf(stderr,"usage: txnb distro {setup|boot}\n"); return 2; }
    if(!strcmp(argv[0],"setup")){
        int rc=system("pkg install -y proot-distro && proot-distro install debian");
        return rc?1:0;
    }
    if(!strcmp(argv[0],"boot")){
        setenv("TNVK_DISTRO","debian",1);
        setenv("TNVK_HW","1",1);
        execlp("proot-distro","proot-distro","login","debian",(char*)0);
        perror("txnb distro boot: proot-distro not found (run txnb distro setup first)");
        return 1;
    }
    fprintf(stderr,"usage: txnb distro {setup|boot}\n"); return 2;
}

int main(int argc, char **argv){
    if(argc<2){ usage(argv[0]); return 2; }
    if(!strcmp(argv[1],"--version")||!strcmp(argv[1],"version")) return cmd_version();
    if(!strcmp(argv[1],"gui")) return cmd_gui();
    if(!strcmp(argv[1],"term")) return cmd_term(argc-2,argv+2);
    if(!strcmp(argv[1],"install")) return cmd_install(argc-2,argv+2);
    if(!strcmp(argv[1],"run-wm")) return cmd_run_wm(argc-2,argv+2);
    if(!strcmp(argv[1],"distro")) return cmd_distro(argc-2,argv+2);
    usage(argv[0]); return 2;
}
