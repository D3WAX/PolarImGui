#ifndef HOOKS
#define HOOKS
#include <math.h>
#include <string.h>
#include <dlfcn.h>
namespace Hooks // place your hooks in this file to keep everything neat and tidy
{
    // ---- values controlled from the ImGui menu (Menu.h) ----
    bool godMode = false;
    float spreadMult = 1.0f;     // 1 = original
    bool spreadInvert = false;   // tick if "steadiness" turns out to mean accuracy (bigger = better)
    float recoilMult = 1.0f;     // 1 = original (camera recoil: the upward kick)
    float shakeMult = 1.0f;      // 1 = original (camera shake, PlayerControl.camShake)
    float animSpeed = 1.0f;      // 1 = original (speed of the camera Animator: bob / sway / shot shake)

    // ---- aim assist settings (Menu.h) ----
    bool aimOn = false;
    float aimStrength = 5.0f;    // 0..10, 10 = instant
    float aimFovPx = 220.0f;     // radius of the FOV circle in pixels (drawn by the menu)
    float aimFovDeg = 10.0f;     // same radius in degrees (computed by the menu from aimFovPx + camVFov)
    float aimMaxDist = 150.0f;   // meters
    float aimTargetH = 1.2f;     // aim point above the target's feet (chest)
    float aimEyeH = 1.5f;        // our eye height above our feet
    float camVFov = 60.0f;       // assumed vertical camera FOV in degrees (calibration for the circle)
    bool aimOnlyShooting = false;
    bool aimIgnoreTeam = true;

    // ---- aim assist readouts ----
    int g_aimSeen = 0;           // players tracked this frame
    int g_aimInFov = 0;          // players inside the FOV circle
    float g_aimDist = 0.0f;      // distance to the chosen target (m)
    float g_aimAng = 0.0f;       // angle to the chosen target (deg)
    bool g_aimUsedIcall = false; // position came from Transform icall instead of _lastPosition

    // ---- live values for the on-screen readout (helps tuning without a PC) ----
    void *g_localWeapon = nullptr; // local player's PlayerWeapon
    void *g_cam = nullptr;         // PlayerOrbitCamera
    float *g_shakePtr = nullptr;   // &PlayerControl.camShake

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

    // Scales only the NEW part of a value that the game raises and then lets decay:
    // when |value| grows between frames, that growth is a fresh impulse -> multiply just that part.
    static inline void ScaleImpulse(float *value, float lastAfter, float mult)
    {
        float v = *value;
        if (fabsf(mult - 1.0f) > 0.001f && fabsf(v) > fabsf(lastAfter) + 0.0001f)
            *value = lastAfter + (v - lastAfter) * mult;
    }

    // ---- Camera animator speed (UnityEngine.Animator.set_speed via icall, resolved lazily) ----
    typedef void *(*il2cpp_resolve_icall_t)(const char *);
    typedef void (*Animator_set_speed_t)(void *animator, float speed);
    Animator_set_speed_t p_animSetSpeed = nullptr;
    bool animResolveTried = false;
    float animApplied = 1.0f;

    static inline void ApplyAnimSpeed(void *weapon)
    {
        if (!weapon) return;
        if (!animResolveTried)
        {
            animResolveTried = true;
            void *h = dlopen(OBFUSCATE("libil2cpp.so"), RTLD_NOW);
            il2cpp_resolve_icall_t resolve = h ? (il2cpp_resolve_icall_t) dlsym(h, "il2cpp_resolve_icall") : nullptr;
            if (resolve) p_animSetSpeed = (Animator_set_speed_t) resolve("UnityEngine.Animator::set_speed");
        }
        if (!p_animSetSpeed) return;
        if (fabsf(animSpeed - animApplied) < 0.001f && fabsf(animSpeed - 1.0f) < 0.001f) return; // nothing to do
        void *anim = *(void **) ((uintptr_t) weapon + 0x198); // PlayerWeapon.camAnim
        if (anim)
        {
            p_animSetSpeed(anim, animSpeed);
            animApplied = animSpeed;
        }
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
    int g_frame = 0;            // counts camera LateUpdates (one per rendered frame)
    int g_lastShootFrame = -1000;

    static inline void ShootScaled(void *self, void *method, void (*orig)(void *, void *))
    {
        if (IsLocalWeapon(self)) g_lastShootFrame = g_frame;
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
        if (self != nullptr && IsLocalWeapon(self))
        {
            g_localWeapon = self;
            ApplyAnimSpeed(self);
        }
        old_WeaponUpdate(self, method);
    }

    // ---- Tracking other players (PlayerControl$$Update, RVA 0xFAC2D4, runs for every player object) ----
    struct Tgt { void *pc; int seen; };
    Tgt g_tgts[64];

    void (*old_PCUpdate)(void *self, void *method) = nullptr;
    void PCUpdate(void *self, void *method)
    {
        if (self != nullptr)
        {
            int freeSlot = -1;
            bool found = false;
            for (int i = 0; i < 64; i++)
            {
                if (g_tgts[i].pc == self) { g_tgts[i].seen = g_frame; found = true; break; }
                if (freeSlot < 0 && (g_tgts[i].pc == nullptr || g_frame - g_tgts[i].seen > 120)) freeSlot = i;
            }
            if (!found && freeSlot >= 0) { g_tgts[freeSlot].pc = self; g_tgts[freeSlot].seen = g_frame; }
        }
        old_PCUpdate(self, method);
    }

    // il2cpp string equality (UTF-16: length @0x10, chars @0x14)
    static inline bool SameStr(void *a, void *b)
    {
        if (!a || !b) return false;
        int la = *(int *) ((uintptr_t) a + 0x10);
        int lb = *(int *) ((uintptr_t) b + 0x10);
        if (la != lb || la <= 0 || la > 64) return false;
        return memcmp((void *) ((uintptr_t) a + 0x14), (void *) ((uintptr_t) b + 0x14), (size_t) la * 2) == 0;
    }

    // Transform.get_position_Injected(Transform, out Vector3) icall, resolved lazily (fallback only)
    typedef void (*Transform_getPos_t)(void *transform, float *out3);
    Transform_getPos_t p_getPosInj = nullptr;
    bool posResolveTried = false;

    // World position of a PlayerControl: its _lastPosition (Vector3 @0x2E0); falls back to chestBone's transform
    static inline bool PlayerPos(void *pc, float *out)
    {
        float *lp = (float *) ((uintptr_t) pc + 0x2E0);
        if (fabsf(lp[0]) + fabsf(lp[1]) + fabsf(lp[2]) > 0.01f)
        {
            out[0] = lp[0]; out[1] = lp[1]; out[2] = lp[2];
            return true;
        }
        if (!posResolveTried)
        {
            posResolveTried = true;
            void *hd = dlopen(OBFUSCATE("libil2cpp.so"), RTLD_NOW);
            il2cpp_resolve_icall_t resolve = hd ? (il2cpp_resolve_icall_t) dlsym(hd, "il2cpp_resolve_icall") : nullptr;
            if (resolve) p_getPosInj = (Transform_getPos_t) resolve("UnityEngine.Transform::get_position_Injected");
        }
        void *chest = *(void **) ((uintptr_t) pc + 0x130); // PlayerControl.chestBone
        if (p_getPosInj && chest)
        {
            p_getPosInj(chest, out);
            out[1] -= 1.2f; // chest -> approx. feet, so the same height offset applies
            g_aimUsedIcall = true;
            return true;
        }
        return false;
    }

    static inline float Wrap180(float a)
    {
        a = fmodf(a, 360.0f);
        if (a > 180.0f) a -= 360.0f;
        if (a < -180.0f) a += 360.0f;
        return a;
    }

    // Aim assist: rotate the orbit camera (rotationYAxis @0x6C = yaw, rotationXAxis @0x70 = pitch, degrees)
    // toward the best target inside the FOV circle. strength/10 = fraction of the remaining angle applied per frame.
    static inline void AimAssist(void *cam)
    {
        g_aimSeen = 0; g_aimInFov = 0;
        if (!aimOn) return;
        if (aimOnlyShooting && (g_frame - g_lastShootFrame) > 15) return;
        void *me = LocalPlayer();
        if (!me) return;

        float o[3];
        if (!PlayerPos(me, o)) return;
        o[1] += aimEyeH;
        void *myTeam = *(void **) ((uintptr_t) me + 0x70);

        float *ry = (float *) ((uintptr_t) cam + 0x6C);
        float *rx = (float *) ((uintptr_t) cam + 0x70);
        float curYaw = *ry;
        float curPitch = Wrap180(*rx);

        float bestAng = 1e9f, bestDyaw = 0, bestDpitch = 0, bestDist = 0;
        for (int i = 0; i < 64; i++)
        {
            void *pc = g_tgts[i].pc;
            if (!pc || pc == me || g_frame - g_tgts[i].seen > 5) continue;
            g_aimSeen++;

            void *ph = *(void **) ((uintptr_t) pc + 0x270);            // PlayerControl.playerHealth
            if (ph && (*(bool *) ((uintptr_t) ph + 0x75))) continue;   // PlayerHealth.dead
            if (aimIgnoreTeam && SameStr(*(void **) ((uintptr_t) pc + 0x70), myTeam)) continue;

            float t[3];
            if (!PlayerPos(pc, t)) continue;
            float dx = t[0] - o[0], dy = (t[1] + aimTargetH) - o[1], dz = t[2] - o[2];
            float hd = sqrtf(dx * dx + dz * dz);
            float dist = sqrtf(hd * hd + dy * dy);
            if (dist < 0.5f || dist > aimMaxDist) continue;

            float yaw = atan2f(dx, dz) * 57.29578f;
            float pitch = -atan2f(dy, hd) * 57.29578f; // Euler X: positive = looking down
            float dyaw = Wrap180(yaw - curYaw);
            float dpitch = pitch - curPitch;
            float ang = sqrtf(dyaw * dyaw + dpitch * dpitch);
            if (ang > aimFovDeg) continue;
            g_aimInFov++;
            if (ang < bestAng) { bestAng = ang; bestDyaw = dyaw; bestDpitch = dpitch; bestDist = dist; }
        }
        if (bestAng > 1e8f) return;

        g_aimDist = bestDist;
        g_aimAng = bestAng;
        float f = aimStrength / 10.0f;
        if (f <= 0.0f) return;
        if (f > 0.999f)
        {
            f = 1.0f;
            *(float *) ((uintptr_t) cam + 0x74) = 0.0f; // velocityX / velocityY: stop manual drag overshoot
            *(float *) ((uintptr_t) cam + 0x78) = 0.0f;
        }
        *ry = curYaw + bestDyaw * f;
        *rx = curPitch + bestDpitch * f;
    }

    // ---- Camera recoil ----
    // PlayerOrbitCamera.weaponKick (float @0xDC) is set/increased by WeaponKick(float) and decays in LateUpdate.
    // WeaponKick itself is only 8 bytes (too small to hook safely), so we watch the field instead:
    // when its magnitude grows between frames, that growth is a new kick -> scale just that part.
    float lastKickAfter = 0.0f;
    float lastShakeAfter = 0.0f;

    // PlayerOrbitCamera$$LateUpdate, RVA 0xFA30FC
    void (*old_CamLateUpdate)(void *self, void *method) = nullptr;

    void CamLateUpdate(void *self, void *method)
    {
        if (self == nullptr)
        {
            old_CamLateUpdate(self, method);
            return;
        }
        g_cam = self;
        g_frame++;

        float *kick = (float *) ((uintptr_t) self + 0xDC);          // PlayerOrbitCamera.weaponKick
        void *pc = *(void **) ((uintptr_t) self + 0x80);            // PlayerOrbitCamera.playerControl
        float *shake = pc ? (float *) ((uintptr_t) pc + 0x17C) : nullptr; // PlayerControl.camShake
        g_shakePtr = shake;

        ScaleImpulse(kick, lastKickAfter, recoilMult);
        if (shake) ScaleImpulse(shake, lastShakeAfter, shakeMult);
        AimAssist(self);

        old_CamLateUpdate(self, method);

        lastKickAfter = *kick;
        if (shake) lastShakeAfter = *shake;
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

        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0xFAC2D4);
        if (a) hook((void *) a, (void *) PCUpdate, (void **) &old_PCUpdate);

        a = getAbsoluteAddress(OBFUSCATE("libil2cpp.so"), 0xFA30FC);
        if (a) hook((void *) a, (void *) CamLateUpdate, (void **) &old_CamLateUpdate);
    }
}
#endif
