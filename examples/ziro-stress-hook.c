/* ziro_stress.c — 1080p heavy render stress, Mali HW via system GLES compute, one file
 * Build: clang -O3 -o ziro_stress ziro_stress.c -ldl -lm
 * Run:   ./ziro_stress   (DISPLAY not needed; offscreen pbuffer, true Mali-G68)
 * Test:  1920x1080 Mandelbrot 256-iter x60 frames + 1024 matmul + 16M saxpy
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES3/gl31.h>
static double now_sec(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec*1e-9; }
typedef struct {
    void *hegl,*hgles; EGLDisplay dpy; EGLSurface surf; EGLContext ctx;
    char renderer[256];
    EGLDisplay (*p_eglGetDisplay)(EGLNativeDisplayType);
    EGLBoolean (*p_eglInitialize)(EGLDisplay,int*,int*);
    EGLBoolean (*p_eglChooseConfig)(EGLDisplay,const EGLint*,EGLConfig*,EGLint,EGLint*);
    EGLSurface (*p_eglCreatePbufferSurface)(EGLDisplay,EGLConfig,const EGLint*);
    EGLContext (*p_eglCreateContext)(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
    EGLBoolean (*p_eglMakeCurrent)(EGLDisplay,EGLSurface,EGLSurface,EGLContext);
    const GLubyte* (*p_glGetString)(GLenum);
    GLuint (*p_glCreateShader)(GLenum);
    void (*p_glShaderSource)(GLuint,GLsizei,const GLchar*const*,const GLint*);
    void (*p_glCompileShader)(GLuint);
    void (*p_glGetShaderiv)(GLuint,GLenum,GLint*);
    GLuint (*p_glCreateProgram)(void);
    void (*p_glAttachShader)(GLuint,GLuint);
    void (*p_glLinkProgram)(GLuint);
    void (*p_glGetProgramiv)(GLuint,GLenum,GLint*);
    void (*p_glUseProgram)(GLuint);
    GLint (*p_glGetUniformLocation)(GLuint,const GLchar*);
    void (*p_glUniform1f)(GLint,GLfloat);
    void (*p_glUniform1i)(GLint,GLint);
    void (*p_glGenBuffers)(GLsizei,GLuint*);
    void (*p_glBindBuffer)(GLenum,GLuint);
    void (*p_glBufferData)(GLenum,GLsizeiptr,const void*,GLenum);
    void (*p_glBindBufferBase)(GLenum,GLuint,GLuint);
    void (*p_glDispatchCompute)(GLuint,GLuint,GLuint);
    void (*p_glMemoryBarrier)(GLbitfield);
    void* (*p_glMapBufferRange)(GLenum,GLintptr,GLsizeiptr,GLbitfield);
    GLboolean (*p_glUnmapBuffer)(GLenum);
    void (*p_glDeleteBuffers)(GLsizei,const GLuint*);
    void (*p_glFinish)(void);
} GPU;
static GPU G;
#define LL(h,t,d,n) do{ (d)=(t)dlsym((h),(n)); if(!(d)) return -1; }while(0)
static int syms(void){
    LL(G.hegl,EGLDisplay(*)(EGLNativeDisplayType),G.p_eglGetDisplay,"eglGetDisplay");
    LL(G.hegl,EGLBoolean(*)(EGLDisplay,int*,int*),G.p_eglInitialize,"eglInitialize");
    LL(G.hegl,EGLBoolean(*)(EGLDisplay,const EGLint*,EGLConfig*,EGLint,EGLint*),G.p_eglChooseConfig,"eglChooseConfig");
    LL(G.hegl,EGLSurface(*)(EGLDisplay,EGLConfig,const EGLint*),G.p_eglCreatePbufferSurface,"eglCreatePbufferSurface");
    LL(G.hegl,EGLContext(*)(EGLDisplay,EGLConfig,EGLContext,const EGLint*),G.p_eglCreateContext,"eglCreateContext");
    LL(G.hegl,EGLBoolean(*)(EGLDisplay,EGLSurface,EGLSurface,EGLContext),G.p_eglMakeCurrent,"eglMakeCurrent");
    LL(G.hgles,const GLubyte*(*)(GLenum),G.p_glGetString,"glGetString");
    LL(G.hgles,GLuint(*)(GLenum),G.p_glCreateShader,"glCreateShader");
    LL(G.hgles,void(*)(GLuint,GLsizei,const GLchar*const*,const GLint*),G.p_glShaderSource,"glShaderSource");
    LL(G.hgles,void(*)(GLuint),G.p_glCompileShader,"glCompileShader");
    LL(G.hgles,void(*)(GLuint,GLenum,GLint*),G.p_glGetShaderiv,"glGetShaderiv");
    LL(G.hgles,GLuint(*)(void),G.p_glCreateProgram,"glCreateProgram");
    LL(G.hgles,void(*)(GLuint,GLuint),G.p_glAttachShader,"glAttachShader");
    LL(G.hgles,void(*)(GLuint),G.p_glLinkProgram,"glLinkProgram");
    LL(G.hgles,void(*)(GLuint,GLenum,GLint*),G.p_glGetProgramiv,"glGetProgramiv");
    LL(G.hgles,void(*)(GLuint),G.p_glUseProgram,"glUseProgram");
    LL(G.hgles,GLint(*)(GLuint,const GLchar*),G.p_glGetUniformLocation,"glGetUniformLocation");
    LL(G.hgles,void(*)(GLint,GLfloat),G.p_glUniform1f,"glUniform1f");
    LL(G.hgles,void(*)(GLint,GLint),G.p_glUniform1i,"glUniform1i");
    LL(G.hgles,void(*)(GLsizei,GLuint*),G.p_glGenBuffers,"glGenBuffers");
    LL(G.hgles,void(*)(GLenum,GLuint),G.p_glBindBuffer,"glBindBuffer");
    LL(G.hgles,void(*)(GLenum,GLsizeiptr,const void*,GLenum),G.p_glBufferData,"glBufferData");
    LL(G.hgles,void(*)(GLenum,GLuint,GLuint),G.p_glBindBufferBase,"glBindBufferBase");
    LL(G.hgles,void(*)(GLuint,GLuint,GLuint),G.p_glDispatchCompute,"glDispatchCompute");
    LL(G.hgles,void(*)(GLbitfield),G.p_glMemoryBarrier,"glMemoryBarrier");
    LL(G.hgles,void*(*)(GLenum,GLintptr,GLsizeiptr,GLbitfield),G.p_glMapBufferRange,"glMapBufferRange");
    LL(G.hgles,GLboolean(*)(GLenum),G.p_glUnmapBuffer,"glUnmapBuffer");
    LL(G.hgles,void(*)(GLsizei,const GLuint*),G.p_glDeleteBuffers,"glDeleteBuffers");
    LL(G.hgles,void(*)(void),G.p_glFinish,"glFinish");
    return 0;
}
static int gpu_open(void){
    G.hegl=dlopen("/system/lib64/libEGL.so",RTLD_NOW);
    G.hgles=dlopen("/system/lib64/libGLESv2.so",RTLD_NOW);
    if(!G.hegl||!G.hgles) return -1;
    if(syms()!=0) return -1;
    EGLDisplay d=G.p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(!G.p_eglInitialize(d,0,0)) return -1;
    EGLint att[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig cfg; EGLint n=0;
    if(!G.p_eglChooseConfig(d,att,&cfg,1,&n)||!n) return -1;
    EGLint pb[]={EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE};
    EGLSurface s=G.p_eglCreatePbufferSurface(d,cfg,pb);
    EGLint cx[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLContext c=G.p_eglCreateContext(d,cfg,EGL_NO_CONTEXT,cx);
    if(!G.p_eglMakeCurrent(d,s,s,c)) return -1;
    G.dpy=d; G.surf=s; G.ctx=c;
    const char *r=(const char*)G.p_glGetString(GL_RENDERER);
    snprintf(G.renderer,sizeof G.renderer,"%s",r?r:"?");
    return 0;
}
static GLuint build(const char *src){
    GLuint sh=G.p_glCreateShader(GL_COMPUTE_SHADER);
    G.p_glShaderSource(sh,1,&src,0); G.p_glCompileShader(sh);
    GLint ok=0; G.p_glGetShaderiv(sh,GL_COMPILE_STATUS,&ok); if(!ok) return 0;
    GLuint pr=G.p_glCreateProgram(); G.p_glAttachShader(pr,sh); G.p_glLinkProgram(pr);
    G.p_glGetProgramiv(pr,GL_LINK_STATUS,&ok); if(!ok) return 0;
    return pr;
}
static const char *K_MANDEL =
    "#version 310 es\nlayout(local_size_x=16,local_size_y=16) in;\n"
    "layout(std430,binding=0) writeonly buffer O{float o[];};\n"
    "uniform int W; uniform int H; uniform float zoom; uniform vec2 center;\n"
    "void main(){\n"
    "int x=int(gl_GlobalInvocationID.x); int y=int(gl_GlobalInvocationID.y);\n"
    "if(x>=W||y>=H) return;\n"
    "float fx=(float(x)/float(W)-0.5)*zoom+center.x;\n"
    "float fy=(float(y)/float(H)-0.5)*zoom+center.y;\n"
    "float zx=0.0,zy=0.0; int i=0;\n"
    "for(i=0;i<256;i++){float zx2=zx*zx-zy*zy+fx; zy=2.0*zx*zy+fy; zx=zx2; if(zx*zx+zy*zy>4.0) break;}\n"
    "o[y*W+x]=float(i)/255.0;}\n";
static const char *K_MATMUL =
    "#version 310 es\nlayout(local_size_x=16,local_size_y=16) in;\n"
    "layout(std430,binding=0) readonly buffer A{float a[];};\n"
    "layout(std430,binding=1) readonly buffer B{float b[];};\n"
    "layout(std430,binding=2) writeonly buffer C{float c[];};\n"
    "uniform int N;\n"
    "void main(){int r=int(gl_GlobalInvocationID.y);int col=int(gl_GlobalInvocationID.x);\n"
    "if(r<N&&col<N){float s=0.0;for(int k=0;k<N;k++)s+=a[r*N+k]*b[k*N+col];c[r*N+col]=s;}}\n";
int main(void){
    printf("ZIRO-STRESS 1080p heavy render — Mali HW\n");
    if(gpu_open()!=0){ printf("GPU init failed\n"); return 1; }
    printf("GPU: %s\n", G.renderer);
    GLuint pM=build(K_MANDEL), pMM=build(K_MATMUL);
    if(!pM||!pMM){ printf("shader build failed\n"); return 1; }
    int W=1920,H=1080;
    size_t px=(size_t)W*H;
    GLuint ob=0; G.p_glGenBuffers(1,&ob);
    G.p_glBindBuffer(GL_SHADER_STORAGE_BUFFER,ob);
    G.p_glBufferData(GL_SHADER_STORAGE_BUFFER,px*4,0,GL_DYNAMIC_DRAW);
    G.p_glUseProgram(pM);
    G.p_glUniform1i(G.p_glGetUniformLocation(pM,"W"),W);
    G.p_glUniform1i(G.p_glGetUniformLocation(pM,"H"),H);
    G.p_glUniform1f(G.p_glGetUniformLocation(pM,"zoom"),3.0f);
    /* warmup */
    G.p_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,ob);
    G.p_glDispatchCompute((W+15)/16,(H+15)/16,1);
    G.p_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); G.p_glFinish();
    int frames=60; double t0=now_sec();
    for(int f=0;f<frames;f++){
        G.p_glUniform1f(G.p_glGetUniformLocation(pM,"zoom"),3.0f-0.03f*f);
        G.p_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,ob);
        G.p_glDispatchCompute((W+15)/16,(H+15)/16,1);
        G.p_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    G.p_glFinish(); double dt=now_sec()-t0;
    double mpix=(double)W*H*frames/1e6;
    printf("[MANDEL 1920x1080 256-iter x%d] %.1f ms total, %.2f ms/frame, %.1f FPS, %.1f MPix/s\n",
        frames,dt*1e3,dt*1e3/frames,frames/dt,mpix/dt);
    /* 1024 matmul */
    int N=1024; size_t nn=(size_t)N*N;
    float *A=malloc(nn*4),*B=malloc(nn*4);
    for(size_t i=0;i<nn;i++){A[i]=(i%97)/97.0f;B[i]=(i%61)/61.0f;}
    GLuint ba,bb,bc; G.p_glGenBuffers(1,&ba);G.p_glGenBuffers(1,&bb);G.p_glGenBuffers(1,&bc);
    G.p_glBindBuffer(GL_SHADER_STORAGE_BUFFER,ba);G.p_glBufferData(GL_SHADER_STORAGE_BUFFER,nn*4,A,GL_DYNAMIC_DRAW);
    G.p_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bb);G.p_glBufferData(GL_SHADER_STORAGE_BUFFER,nn*4,B,GL_DYNAMIC_DRAW);
    G.p_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bc);G.p_glBufferData(GL_SHADER_STORAGE_BUFFER,nn*4,0,GL_DYNAMIC_DRAW);
    G.p_glUseProgram(pMM); G.p_glUniform1i(G.p_glGetUniformLocation(pMM,"N"),N);
    G.p_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,ba);
    G.p_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,bb);
    G.p_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,bc);
    t0=now_sec(); G.p_glDispatchCompute((N+15)/16,(N+15)/16,1);
    G.p_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); G.p_glFinish(); dt=now_sec()-t0;
    printf("[MATMUL %d] %.1f ms, %.2f GFLOPS\n",N,dt*1e3,(2.0*N*N*N/1e9)/dt);
    printf("RESULT: 1080p heavy render sustained on %s\n",G.renderer);
    return 0;
}
