#include <stdio.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
int main(){
  void *h = dlopen("/system/lib64/libvulkan.so", RTLD_NOW);
  if(!h){ printf("dlopen fail %s\n", dlerror()); return 1; }
  PFN_vkEnumerateInstanceVersion eV = (void*)dlsym(h,"vkEnumerateInstanceVersion");
  PFN_vkCreateInstance cI = (void*)dlsym(h,"vkCreateInstance");
  uint32_t v=0; if(eV){ eV(&v); printf("sysvk %u.%u.%u\n", VK_VERSION_MAJOR(v),VK_VERSION_MINOR(v),VK_VERSION_PATCH(v)); }
  VkApplicationInfo ai={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.apiVersion=VK_MAKE_VERSION(1,1,0)};
  VkInstanceCreateInfo ci={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
  VkInstance inst=0;
  PFN_vkEnumeratePhysicalDevices eP = (void*)dlsym(h,"vkEnumeratePhysicalDevices");
  if(cI(&ci,0,&inst)!=VK_SUCCESS){ printf("create fail\n"); return 1; }
  uint32_t n=0; eP(inst,&n,0); printf("phys=%u\n", n);
  VkPhysicalDevice pd[8]; eP(inst,&n,pd);
  PFN_vkGetPhysicalDeviceProperties gP = (void*)dlsym(h,"vkGetPhysicalDeviceProperties");
  PFN_vkEnumerateDeviceExtensionProperties eE = (void*)dlsym(h,"vkEnumerateDeviceExtensionProperties");
  for(uint32_t i=0;i<n;i++){
    VkPhysicalDeviceProperties p; gP(pd[i],&p);
    printf("[%u] %s | api %u.%u | vendor %04x dev %04x type %u\n", i, p.deviceName, VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion), p.vendorID, p.deviceID, p.deviceType);
    uint32_t m=0; eE(pd[i],0,&m,0); printf("    exts=%u\n", m);
  }
  return 0;
}
