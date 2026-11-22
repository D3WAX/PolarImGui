#include <EGL/egl.h> // need to make a common.h that contains all these headers cuz this is nasty
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include <fstream>
#include <sstream>
#include "nlohmann/json.hpp"
#include "Misc/Logging.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_android.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "Obfuscation/Obfuscate.h"
#include <stdio.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include "Misc/JNIStuff.h"
#include "Misc/FileWrapper.h"
#include "Misc/Utils.h"
#include "ByNameModding/BNM.hpp"
#include "Obfuscation/Custom_Obfuscate.h"
#include "Unity/Unity.h"
#include "Misc/FunctionPointers.h"
#include "Hooking/Hooks.h"
#include "Misc/ImGuiStuff.h"
#include "Menu.h"
#include "Hooking/JNIHooks.h"
#include "Unity/Screen.h"
#include "Unity/Input.h"
// the private version held by certain polarmodders has image loading and a lot more

#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>

// ---- debugging helpers: breadcrumb log + native crash logger (no PC needed) ----
// Log file: /sdcard/Android/data/com.MA.LAC/files/imgui_log.txt
// Optional flag file: create emulator.txt in the same folder to force emulator=true without rebuilding.
#define MOD_DIR "/sdcard/Android/data/com.MA.LAC/files/"

static void FLog(const char *msg)
{
    FILE *f = fopen(MOD_DIR "imgui_log.txt", "a");
    if (f)
    {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

static struct sigaction g_oldSegv, g_oldBus, g_oldAbrt;

static void CrashHandler(int sig, siginfo_t *info, void *ctx)
{
    uintptr_t pc = 0, lr = 0;
#if defined(__aarch64__)
    ucontext_t *uc = (ucontext_t *) ctx;
    pc = (uintptr_t) uc->uc_mcontext.pc;
    lr = (uintptr_t) uc->uc_mcontext.regs[30];
#endif
    Dl_info d1, d2;
    memset(&d1, 0, sizeof d1);
    memset(&d2, 0, sizeof d2);
    dladdr((void *) pc, &d1);
    dladdr((void *) lr, &d2);
    char buf[512];
    snprintf(buf, sizeof buf, "CRASH sig=%d fault_addr=%p pc=%p (%s +0x%lx) lr=%p (%s +0x%lx)",
             sig, info ? info->si_addr : nullptr,
             (void *) pc, d1.dli_fname ? d1.dli_fname : "?",
             d1.dli_fbase ? (unsigned long) (pc - (uintptr_t) d1.dli_fbase) : 0UL,
             (void *) lr, d2.dli_fname ? d2.dli_fname : "?",
             d2.dli_fbase ? (unsigned long) (lr - (uintptr_t) d2.dli_fbase) : 0UL);
    FLog(buf);

    struct sigaction *old = (sig == SIGSEGV) ? &g_oldSegv : (sig == SIGBUS ? &g_oldBus : &g_oldAbrt);
    if ((old->sa_flags & SA_SIGINFO) && old->sa_sigaction)
    {
        old->sa_sigaction(sig, info, ctx);
    } else if (!(old->sa_flags & SA_SIGINFO) && old->sa_handler != SIG_DFL &&
               old->sa_handler != SIG_IGN && old->sa_handler)
    {
        old->sa_handler(sig);
    } else
    {
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

// Safe to call more than once (Unity installs its own handlers later, so we re-install after il2cpp loads)
static void InstallCrashHandler()
{
    const int sigs[3] = {SIGSEGV, SIGBUS, SIGABRT};
    struct sigaction *olds[3] = {&g_oldSegv, &g_oldBus, &g_oldAbrt};
    for (int i = 0; i < 3; i++)
    {
        struct sigaction cur;
        memset(&cur, 0, sizeof cur);
        sigaction(sigs[i], nullptr, &cur);
        if ((cur.sa_flags & SA_SIGINFO) && cur.sa_sigaction == CrashHandler)
            continue; // already ours
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = CrashHandler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sigaction(sigs[i], &sa, olds[i]);
    }
}

EGLBoolean (*old_eglSwapBuffers)(...);
EGLBoolean new_eglSwapBuffers(EGLDisplay _display, EGLSurface _surface) {
    static int stage = 0;
    if (stage == 0) { stage = 1; FLog("eglSwapBuffers: first call"); }

    EGLint w = 0, h = 0;
    eglQuerySurface(_display, _surface, EGL_WIDTH, &w);
    eglQuerySurface(_display, _surface, EGL_HEIGHT, &h);
    g_screenW = w;
    g_screenH = h;

    SetupImGui();
    if (stage == 1 && init) { stage = 2; FLog("ImGui initialised"); }
    if (stage == 2 && g_screenW > 0) { stage = 3; FLog("first DrawImGui: begin"); }
    Menu::DrawImGui();
    if (stage == 3) { stage = 4; FLog("first DrawImGui: end"); }

    return old_eglSwapBuffers(_display, _surface);
}
bool emulator = false; // false = real phone (touch via nativeInjectEvent). If touch/launch misbehaves, try true.
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * reserved)
{
    FLog("JNI_OnLoad");
    if(!emulator){
        jvm = vm;
        JNIEnv *env = nullptr;
        vm->GetEnv((void **) &env, JNI_VERSION_1_6);
        jclass cls = env ? env->FindClass(OBFUSCATE("com/unity3d/player/UnityPlayer")) : nullptr;
        if (env && env->ExceptionCheck())
            env->ExceptionClear();
        if (cls)
        {
            UnityPlayer_cls = (jclass) env->NewGlobalRef(cls); // local refs die after JNI_OnLoad returns
            UnityPlayer_CurrentActivity_fid = env->GetStaticFieldID(UnityPlayer_cls,
                                                                    OBFUSCATE("currentActivity"),
                                                                    OBFUSCATE("Landroid/app/Activity;"));
            if (env->ExceptionCheck())
                env->ExceptionClear();
            hook((void *) env->functions->RegisterNatives, (void *) hook_RegisterNatives,
                 (void **) &old_RegisterNatives);
            FLog("RegisterNatives hooked (touch via nativeInjectEvent)");
        } else
        {
            FLog("UnityPlayer class not found: touch hook skipped");
        }
    }
    return JNI_VERSION_1_6;
}

void *hack_thread(void *)
{
    using namespace BNM;
    FLog("hack_thread: start");
    do {
        sleep(1);
    } while (!Il2cppLoaded());
    FLog("hack_thread: il2cpp loaded");
    InstallCrashHandler(); // Unity has installed its own handlers by now; chain on top of them
    Hooks::Install(); // game hooks (Hooking/Hooks.h), RVA based, no il2cpp calls needed
    FLog("hack_thread: hooks installed");
    if (emulator)
    {
        // Only the emulator input path needs BNM (Unity Input hook)
        AttachIl2Cpp();
        Unity::Input::Setup();
        DetachIl2Cpp();
        FLog("hack_thread: emulator input set up");
    }
    FLog("hack_thread: done");
    return NULL;
}
__attribute__((constructor))
void lib_main()
{
    FLog("---- lib_main");
    if (access(MOD_DIR "emulator.txt", F_OK) == 0)
        emulator = true; // flag file overrides the default without rebuilding
    InstallCrashHandler();
    FLog(emulator ? "mode: emulator=true (Unity Input)" : "mode: emulator=false (nativeInjectEvent)");
    auto eglhandle = dlopen(OBFUSCATE("libEGL.so"), RTLD_LAZY);
    const char *dlopen_error = dlerror();
    if (dlopen_error)
    {
        eglhandle = dlopen(OBFUSCATE("libunity.so"), RTLD_LAZY); // I have no idea if this works it was just to me that it would fix crashes so I did it really quickly
    }
    auto eglSwapBuffers = dlsym(eglhandle, OBFUSCATE("eglSwapBuffers"));
    const char *dlsym_error = dlerror();
    if (dlsym_error)
    {
        LOGE(OBFUSCATE("Cannot load symbol 'eglSwapBuffers': %s"), dlsym_error);
    } else
    {
        hook(eglSwapBuffers, (void *) new_eglSwapBuffers, (void **) &old_eglSwapBuffers);
    }
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}
