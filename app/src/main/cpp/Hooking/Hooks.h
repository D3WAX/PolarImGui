#ifndef HOOKS
#define HOOKS
#include <math.h>
namespace Hooks // place your hooks in this file to keep everything neat and tidy
{
    // ---- values controlled from the ImGui menu (Menu.h) ----
    bool godMode = false;
    float spreadMult = 1.0f;     // 1 = original
    bool spreadInvert = false;   // tick if "steadiness" turns out to mean accuracy (bigger = better)
    float recoilMult = 1.0f;     // 1 = original (camera recoil)

    // ---- live values for the on-screen readout (helps tuning without a PC) ----
    void *g_localWeapon = nullptr; // local player's PlayerWeapon
    void *g_cam = nullptr;         // PlayerOrbitCamera

    // ---- helpers ----
    // PlayerControl$$get_Local (static), RVA 0xFAABEC: the local player's PlayerControl
    void *(*PlayerControl_get_Local)(void *method) = nullptr;

    static inline void *LocalPlayer()
    {
        return PlayerControl_get_Local ? PlayerControl_get_Local(nullptr) : nullptr;
    }

    // PlayerWeapon.playerControl is at 0x20
    static inline bool IsLocalWeapon(void *weapon)
    {
        if (weapon == nullptr) return false;
        void *pc = *(void **) ((uintptr_t) weapon + 0x20);
        return pc != nullptr && pc == LocalPlayer();
    }

    // ---- God Mode: PlayerHealth$$NpcDamage, RVA 0x113B130 ----
    void (*old_NpcDamage)(void *instance, int damage, void *killFeedType, void *method) = nullptr;

    void NpcDamage(void *instance, int damage, void *killFeedType, void *method)
    {
        if (instance != nullptr && godMode)
        {
            return; // ignore damage coming from NPCs
        }
        return old_NpcDamage(instance, damage, killFeedType, method);
    }

    // ---- Bullet spread ----
    // The dump has no field literally called "spread". PlayerWeapon.steadiness (float @0x80) is the best
    // candidate, so we scale it only while the game fires a shot, then restore it.
    // Shoot() and LocalShoot() may call each other, so a guard makes sure we scale only once.
    bool inShoot = false;

    static inline void ShootScaled(void *self, void *method, void (*orig)(void *, void *))
    {
        bool tweak = !inShoot && self != nullptr && fabsf(spreadMult - 1.0f) > 0.001f && IsLocalWeapon(self);
        if (!tweak)
        {
            orig(self, method);
            return;
        }
        float *st = (float *) ((uintptr_t) self + 0x80);
        float saved = *st;
        float m = spreadInvert ? (spreadMult > 0.01f ? 1.0f / spreadMult : 100.0f) : spreadMult;
        inShoot = true;
        *st = saved * m;
        orig(self, method);
        *st = saved;
        inShoot = false;
    }

    // PlayerWeapon$$Shoot, RVA 0x114AE54
    void (*old_Shoot)(void *self, void *method) = nullptr;
    void Shoot(void *self, void *method) { ShootScaled(self, method, old_Shoot); }

    // PlayerWeapon$$LocalShoot, RVA 0x114B74C
    void (*old_LocalShoot)(void *self, void *method) = nullptr;
    void LocalShoot(void *self, void *method) { ShootScaled(self, method, old_LocalShoot); }

    // PlayerWeapon$$Update, RVA 0x1145658 (only used to remember the local weapon for the readout)
    void (*old_WeaponUpdate)(void *self, void *method) = nullptr;
    void WeaponUpdate(void *self, void *method)
    {
        if (self != nullptr && IsLocalWeapon(self)) g_localWeapon = self;
        old_WeaponUpdate(self, method);
    }

    // ---- Camera recoil ----
    // PlayerOrbitCamera.weaponKick (float @0xDC) is set/increased by WeaponKick(float) and decays in LateUpdate.
    // WeaponKick itself is only 8 bytes (too small to hook safely), so we watch the field instead:
    // when its magnitude grows between frames, that growth is a new kick -> scale just that part.
    float lastKickAfter = 0.0f;

    // PlayerOrbitCamera$$LateUpdate, RVA 0xFA30FC
    void (*old_CamLateUpdate)(void *self, void *method) = nullptr;

    void CamLateUpdate(void *self, void *method)
    {
        if (self != nullptr)
        {
            g_cam = self;
            float *kick = (float *) ((uintptr_t) self + 0xDC);
            float k = *kick;
            if (fabsf(recoilMult - 1.0f) > 0.001f && fabsf(k) > fabsf(lastKickAfter) + 0.0001f)
            {
                float impulse = k - lastKickAfter;
                *kick = lastKickAfter + impulse * recoilMult;
            }
            old_CamLateUpdate(self, method);
            lastKickAfter = *kick;
            return;
        }
        old_CamLateUpdate(self, method);
    }

    // Called once from hack_thread, after libil2cpp.so is loaded
    void Install()
    {
        PlayerControl_get_Local = (void *(*)(void *)) getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0xFAABEC);

        DWORD a;
        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0x113B130);
        if (a) hook((void *) a, (void *) NpcDamage, (void **) &old_NpcDamage);

        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0x114AE54);
        if (a) hook((void *) a, (void *) Shoot, (void **) &old_Shoot);

        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0x114B74C);
        if (a) hook((void *) a, (void *) LocalShoot, (void **) &old_LocalShoot);

        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0x1145658);
        if (a) hook((void *) a, (void *) WeaponUpdate, (void **) &old_WeaponUpdate);

        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0xFA30FC);
        if (a) hook((void *) a, (void *) CamLateUpdate, (void **) &old_CamLateUpdate);
    }
}
#endif
