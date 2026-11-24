#ifndef MENU
#define MENU

#include "Misc/ESP.h"
#include "Unity/Screen.h"
#include <math.h>

// Screen size, filled from the EGL surface in new_eglSwapBuffers (native-lib.cpp).
// Using EGL avoids calling il2cpp/BNM from the render thread.
static int g_screenW = 0;
static int g_screenH = 0;

// Menu window rectangle in screen pixels (updated every frame in DrawMenu, read by the touch hook)
static volatile float g_menuX = 0.0f, g_menuY = 0.0f, g_menuW = 0.0f, g_menuH = 0.0f;

namespace Menu {

    ImVec4 color = ImVec4(1, 1, 1, 1);

    void DrawMenu() {
        ImGuiIO &io = ImGui::GetIO();

        // Aim FOV circle (drawn behind the menu, at the screen center). Also converts px -> degrees
        // for the aim code: tan(angle) = (r / (H/2)) * tan(vfov / 2)
        if (g_screenH > 0)
        {
            float halfH = g_screenH * 0.5f;
            float tv = tanf(Hooks::camVFov * 0.5f * 0.0174533f);
            Hooks::aimFovDeg = atanf((Hooks::aimFovPx / halfH) * tv) * 57.29578f;
            if (Hooks::aimOn)
                ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(g_screenW * 0.5f, halfH),
                                                          Hooks::aimFovPx, IM_COL32(255, 255, 255, 170), 64, 2.0f);
        }

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.03f, io.DisplaySize.y * 0.05f),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.40f, 0.0f), ImGuiCond_FirstUseEver);

        // Tap the arrow in the title bar to collapse the window
        ImGui::Begin("LAC Mod", nullptr, ImGuiWindowFlags_NoSavedSettings);

        // publish the window rect so the touch hook can tell menu touches from game touches
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        g_menuX = wp.x; g_menuY = wp.y; g_menuW = ws.x; g_menuH = ws.y;

        ImGui::TextDisabled("Private server / local use");

        if (ImGui::CollapsingHeader("Weapon"))
        {
            ImGui::SliderFloat("Spread x", &Hooks::spreadMult, 0.0f, 3.0f, "%.2f");
            ImGui::Checkbox("Invert spread", &Hooks::spreadInvert);
            ImGui::SliderFloat("Cam recoil x", &Hooks::recoilMult, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Cam shake x", &Hooks::shakeMult, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Cam anim speed", &Hooks::animSpeed, 0.0f, 2.0f, "%.2f");

            float steadiness = 0.0f, kick = 0.0f;
            if (Hooks::g_localWeapon) steadiness = *(float *) ((uintptr_t) Hooks::g_localWeapon + 0x80);
            if (Hooks::g_cam) kick = *(float *) ((uintptr_t) Hooks::g_cam + 0xDC);
            float shake = Hooks::g_shakePtr ? *Hooks::g_shakePtr : 0.0f;
            ImGui::TextDisabled("steadiness %.3f  kick %.3f  shake %.3f", steadiness, kick, shake);
            ImGui::TextDisabled("anim icall: %s", Hooks::p_animSetSpeed ? "ok" : "not resolved");
        }

        if (ImGui::CollapsingHeader("Aim assist"))
        {
            ImGui::Checkbox("Aim assist", &Hooks::aimOn);
            ImGui::SliderFloat("Strength", &Hooks::aimStrength, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("FOV circle (px)", &Hooks::aimFovPx, 30.0f, 700.0f, "%.0f");
            ImGui::SliderFloat("Max distance (m)", &Hooks::aimMaxDist, 10.0f, 300.0f, "%.0f");
            ImGui::Checkbox("Only while shooting", &Hooks::aimOnlyShooting);
            ImGui::Checkbox("Ignore my team", &Hooks::aimIgnoreTeam);
            ImGui::SliderFloat("Target height", &Hooks::aimTargetH, 0.2f, 1.9f, "%.2f");
            ImGui::SliderFloat("Cam VFOV (calib)", &Hooks::camVFov, 30.0f, 100.0f, "%.0f");
            ImGui::TextDisabled("players %d  in FOV %d  dist %.1f  ang %.1f",
                                Hooks::g_aimSeen, Hooks::g_aimInFov, Hooks::g_aimDist, Hooks::g_aimAng);
            ImGui::TextDisabled("pos via icall: %s", Hooks::g_aimUsedIcall ? "yes" : "no");
        }

        if (ImGui::CollapsingHeader("Misc"))
        {
            ImGui::Checkbox("God Mode (NPC damage)", &Hooks::godMode);
        }

        ImGui::End();
    }

    void DrawImGui() {
        if (init && g_screenW > 0 && g_screenH > 0) {

            ImGuiIO &io = ImGui::GetIO();

            static bool WantTextInputLast = false;
//            if (io.WantTextInput && !WantTextInputLast)
//                displayKeyboard(true);
//            WantTextInputLast = io.WantTextInput;

            ImGui_ImplOpenGL3_NewFrame();

            ImGui_ImplAndroid_NewFrame(g_screenW, g_screenH);

            ImGui::NewFrame();
            DrawMenu();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            /*
             * Don't uncomment this after updating imgui this part crash xD
             * I have to update imgui else InputTextWithHint bork on touch.
             */
//            io.KeysDown[io.KeyMap[ImGuiKey_UpArrow]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_DownArrow]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_LeftArrow]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_RightArrow]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Tab]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Enter]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Backspace]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_PageUp]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_PageDown]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Escape]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Delete]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Home]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_End]] = false;
//            io.KeysDown[io.KeyMap[ImGuiKey_Insert]] = false;
            ImGui::EndFrame();

        }
    }
}
#endif
