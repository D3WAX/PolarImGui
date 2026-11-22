#ifndef JNIHOOKS
#define JNIHOOKS

// Touch routing: a touch that STARTS on the menu goes to ImGui only (the game never sees it);
// any other touch goes to the game only (ImGui never sees it). Decided once, at ACTION_DOWN.
static bool g_menuTouch = false;

jmethodID MotionEvent_getX;
jmethodID MotionEvent_getY;
jmethodID MotionEvent_getAction;
jmethodID KeyEvent_getUnicodeChar;
jmethodID KeyEvent_getMetaState;
jmethodID KeyEvent_getAction;
jmethodID KeyEvent_getKeyCode;
jboolean (*old_nativeInjectEvent)(JNIEnv *, jobject, jobject);
jboolean nativeInjectEvent(JNIEnv *env, jobject thiz, jobject inputEvent)
{
    if (init)
    {
        jclass MotionEventCls = env->FindClass(OBFUSCATE("android/view/MotionEvent"));
        jclass KeyEventCls = env->FindClass(OBFUSCATE("android/view/KeyEvent"));
        if (env->IsInstanceOf(inputEvent, MotionEventCls))
        {
            if (!MotionEvent_getX)
                MotionEvent_getX = env->GetMethodID(MotionEventCls, OBFUSCATE("getX"),
                                                    OBFUSCATE("()F"));
            if (!MotionEvent_getY)
                MotionEvent_getY = env->GetMethodID(MotionEventCls, OBFUSCATE("getY"),
                                                    OBFUSCATE("()F"));
            if (!MotionEvent_getAction)
                MotionEvent_getAction = env->GetMethodID(MotionEventCls, OBFUSCATE("getAction"),
                                                         OBFUSCATE("()I"));
            float x = env->CallFloatMethod(inputEvent, MotionEvent_getX);
            float y = env->CallFloatMethod(inputEvent, MotionEvent_getY);
            int action = env->CallIntMethod(inputEvent, MotionEvent_getAction);
            int masked = action & 0xFF; // 0=DOWN 1=UP 2=MOVE 3=CANCEL 5=POINTER_DOWN 6=POINTER_UP

            if (masked == 0) // first finger down: decide who owns this touch
            {
                const float m = 8.0f; // small margin around the window
                g_menuTouch = (g_menuW > 0.0f &&
                               x >= g_menuX - m && x <= g_menuX + g_menuW + m &&
                               y >= g_menuY - m && y <= g_menuY + g_menuH + m);
            }

            if (g_menuTouch)
            {
                // only the first finger drives ImGui; CANCEL is treated like UP
                if (masked == 0 || masked == 1 || masked == 2 || masked == 3)
                    ImGui_ImplAndroid_HandleInputEvent((int) x, (int) y, masked == 3 ? 1 : masked);
                if (masked == 1 || masked == 3)
                    g_menuTouch = false;
                return true; // swallow: the game must not see touches that belong to the menu
            }
            // touch started outside the menu: fall through to the game untouched
        } else if (env->IsInstanceOf(inputEvent, KeyEventCls))
        {
            if (!KeyEvent_getAction)
                KeyEvent_getAction = env->GetMethodID(KeyEventCls, OBFUSCATE("getAction"),
                                                      OBFUSCATE("()I"));
            if (env->CallIntMethod(inputEvent, KeyEvent_getAction) == 0)
            {
                if (!KeyEvent_getKeyCode)
                    KeyEvent_getKeyCode = env->GetMethodID(KeyEventCls, OBFUSCATE("getKeyCode"),
                                                           OBFUSCATE("()I"));
                if (!KeyEvent_getUnicodeChar)
                    KeyEvent_getUnicodeChar = env->GetMethodID(KeyEventCls,
                                                               OBFUSCATE("getUnicodeChar"),
                                                               OBFUSCATE("(I)I"));
                if (!KeyEvent_getMetaState)
                    KeyEvent_getMetaState = env->GetMethodID(KeyEventCls, OBFUSCATE("getMetaState"),
                                                             OBFUSCATE("()I"));
                ImGuiIO &io = ImGui::GetIO();
                int KeyCode = env->CallIntMethod(inputEvent, KeyEvent_getKeyCode);
                switch (KeyCode)
                {
                    case 19:
                        io.KeysDown[io.KeyMap[ImGuiKey_UpArrow]] = true;
                        break;
                    case 20:
                        io.KeysDown[io.KeyMap[ImGuiKey_DownArrow]] = true;
                        break;
                    case 21:
                        io.KeysDown[io.KeyMap[ImGuiKey_LeftArrow]] = true;
                        break;
                    case 22:
                        io.KeysDown[io.KeyMap[ImGuiKey_RightArrow]] = true;
                        break;
                    case 61:
                        io.KeysDown[io.KeyMap[ImGuiKey_Tab]] = true;
                        break;
                    case 66:
                        io.KeysDown[io.KeyMap[ImGuiKey_Enter]] = true;
                        break;
                    case 67:
                        io.KeysDown[io.KeyMap[ImGuiKey_Backspace]] = true;;
                        break;
                    case 92:
                        io.KeysDown[io.KeyMap[ImGuiKey_PageUp]] = true;
                        break;
                    case 93:
                        io.KeysDown[io.KeyMap[ImGuiKey_PageDown]] = true;
                        break;
                    case 111:
                        io.KeysDown[io.KeyMap[ImGuiKey_Escape]] = true;
                        break;
                    case 112:
                        io.KeysDown[io.KeyMap[ImGuiKey_Delete]] = true;
                        break;
                    case 122:
                        io.KeysDown[io.KeyMap[ImGuiKey_Home]] = true;
                        break;
                    case 123:
                        io.KeysDown[io.KeyMap[ImGuiKey_End]] = true;
                        break;
                    case 124:
                        io.KeysDown[io.KeyMap[ImGuiKey_Insert]] = true;
                        break;
                    default:
                        io.AddInputCharacter(env->CallIntMethod(inputEvent, KeyEvent_getUnicodeChar,
                                                                env->CallIntMethod(inputEvent,
                                                                                   KeyEvent_getMetaState)));
                        break;
                }
            }
        }
    }
    return old_nativeInjectEvent(env, thiz, inputEvent);
}
jint (*old_RegisterNatives)(JNIEnv*, jclass, const JNINativeMethod*, jint);
jint hook_RegisterNatives(JNIEnv* env, jclass klazz, const JNINativeMethod* methods, jint methodcount)
{
    for (int i = 0; i < methodcount; ++i)
    {
        auto method = methods[i];
        if (strcmp(method.name, OBFUSCATE("nativeInjectEvent")) == 0)
        {
            hook((void *) method.fnPtr, (void *) nativeInjectEvent,
                 (void **) &old_nativeInjectEvent);
        }
    }
    return old_RegisterNatives(env, klazz, methods, methodcount);
}

#endif
