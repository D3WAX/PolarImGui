#ifndef MENU
#define MENU

#include "Misc/ESP.h"
#include "Unity/Screen.h"

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
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.03f, io.DisplaySize.y * 0.05f),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.38f, 0.0f), ImGuiCond_FirstUseEver);

        // Tap the arrow in the title bar to collapse the window
        ImGui::Begin("LAC Mod", nullptr, ImGuiWindowFlags_NoSavedSettings);

        // publish the window rect so the touch hook can tell menu touches from game touches
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        g_menuX = wp.x; g_menuY = wp.y; g_menuW = ws.x; g_menuH = ws.y;

        ImGui::TextDisabled("Offline / local use only");
        ImGui::Separator();

        ImGui::Checkbox("God Mode (NPC damage)", &Hooks::godMode);

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
