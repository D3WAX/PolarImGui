#ifndef HOOKS
#define HOOKS
namespace Hooks // place your hooks in this file to keep everything neat and tidy
{
    // Toggle controlled from the ImGui menu (Menu.h)
    bool godMode = false;

    // PlayerHealth$$NpcDamage, RVA 0x113B130 (from dump.cs / script.json)
    // void NpcDamage(PlayerHealth *this, int32_t _damage, String *_killFeedType, MethodInfo *method)
    void (*old_NpcDamage)(void *instance, int damage, void *killFeedType, void *method) = nullptr;

    void NpcDamage(void *instance, int damage, void *killFeedType, void *method)
    {
        if (instance != nullptr && godMode)
        {
            return; // ignore damage coming from NPCs
        }
        return old_NpcDamage(instance, damage, killFeedType, method);
    }

    // Called once from hack_thread, after libil2cpp.so is loaded
    void Install()
    {
        DWORD addr = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0x113B130);
        if (addr != 0)
        {
            hook((void *) addr, (void *) NpcDamage, (void **) &old_NpcDamage);
        }
    }
}
#endif
