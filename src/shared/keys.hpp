/**
 * This file is a whole mess of constants and definitions.
 *
 * Continue at your own risk.
 */

// #pragma once
// #include <Geode/Geode.hpp>
// #include "imgui.h"

// using namespace geode::prelude;
// using namespace cocos2d;

// ImGuiKey cocosKeyToImgui(enumKeyCodes key) {
//     switch (key) {
//         case enumKeyCodes::KEY_A: return ImGuiKey_A;
//         case enumKeyCodes::KEY_B: return ImGuiKey_B;
//         case enumKeyCodes::KEY_C: return ImGuiKey_C;
//         case enumKeyCodes::KEY_D: return ImGuiKey_D;
//         case enumKeyCodes::KEY_E: return ImGuiKey_E;
//         case enumKeyCodes::KEY_F: return ImGuiKey_F;
//         case enumKeyCodes::KEY_G: return ImGuiKey_G;
//         case enumKeyCodes::KEY_H: return ImGuiKey_H;
//         case enumKeyCodes::KEY_I: return ImGuiKey_I;
//         case enumKeyCodes::KEY_J: return ImGuiKey_J;
//         case enumKeyCodes::KEY_K: return ImGuiKey_K;
//         case enumKeyCodes::KEY_L: return ImGuiKey_L;
//         case enumKeyCodes::KEY_M: return ImGuiKey_M;
//         case enumKeyCodes::KEY_N: return ImGuiKey_N;
//         case enumKeyCodes::KEY_O: return ImGuiKey_O;
//         case enumKeyCodes::KEY_P: return ImGuiKey_P;
//         case enumKeyCodes::KEY_Q: return ImGuiKey_Q;
//         case enumKeyCodes::KEY_R: return ImGuiKey_R;
//         case enumKeyCodes::KEY_S: return ImGuiKey_S;
//         case enumKeyCodes::KEY_T: return ImGuiKey_T;
//         case enumKeyCodes::KEY_U: return ImGuiKey_U;
//         case enumKeyCodes::KEY_V: return ImGuiKey_V;
//         case enumKeyCodes::KEY_W: return ImGuiKey_W;
//         case enumKeyCodes::KEY_X: return ImGuiKey_X;
//         case enumKeyCodes::KEY_Y: return ImGuiKey_Y;
//         case enumKeyCodes::KEY_Z: return ImGuiKey_Z;

//         case enumKeyCodes::KEY_Zero: return ImGuiKey_0;
//         case enumKeyCodes::KEY_One: return ImGuiKey_1;
//         case enumKeyCodes::KEY_Two: return ImGuiKey_2;
//         case enumKeyCodes::KEY_Three: return ImGuiKey_3;
//         case enumKeyCodes::KEY_Four: return ImGuiKey_4;
//         case enumKeyCodes::KEY_Five: return ImGuiKey_5;
//         case enumKeyCodes::KEY_Six: return ImGuiKey_6;
//         case enumKeyCodes::KEY_Seven: return ImGuiKey_7;
//         case enumKeyCodes::KEY_Eight: return ImGuiKey_8;
//         case enumKeyCodes::KEY_Nine: return ImGuiKey_9;

//         case enumKeyCodes::KEY_F1: return ImGuiKey_F1;
//         case enumKeyCodes::KEY_F2: return ImGuiKey_F2;
//         case enumKeyCodes::KEY_F3: return ImGuiKey_F3;
//         case enumKeyCodes::KEY_F4: return ImGuiKey_F4;
//         case enumKeyCodes::KEY_F5: return ImGuiKey_F5;
//         case enumKeyCodes::KEY_F6: return ImGuiKey_F6;
//         case enumKeyCodes::KEY_F7: return ImGuiKey_F7;
//         case enumKeyCodes::KEY_F8: return ImGuiKey_F8;
//         case enumKeyCodes::KEY_F9: return ImGuiKey_F9;
//         case enumKeyCodes::KEY_F10: return ImGuiKey_F10;
//         case enumKeyCodes::KEY_F11: return ImGuiKey_F11;
//         case enumKeyCodes::KEY_F12: return ImGuiKey_F12;

//         case enumKeyCodes::KEY_Control: return ImGuiKey_LeftCtrl;
//         case enumKeyCodes::KEY_Shift: return ImGuiKey_LeftShift;
//         case enumKeyCodes::KEY_Alt: return ImGuiKey_LeftAlt;

//         case enumKeyCodes::KEY_Tab: return ImGuiKey_Tab;
//         case enumKeyCodes::KEY_ArrowLeft: return ImGuiKey_LeftArrow;
//         case enumKeyCodes::KEY_ArrowRight: return ImGuiKey_RightArrow;
//         case enumKeyCodes::KEY_ArrowUp: return ImGuiKey_UpArrow;
//         case enumKeyCodes::KEY_ArrowDown: return ImGuiKey_DownArrow;
//         case enumKeyCodes::KEY_Space: return ImGuiKey_Space;
//         case enumKeyCodes::KEY_Enter: return ImGuiKey_Enter;
//         case enumKeyCodes::KEY_Escape: return ImGuiKey_Escape;
//         case enumKeyCodes::KEY_Delete: return ImGuiKey_Delete;
//         case enumKeyCodes::KEY_Backspace: return ImGuiKey_Backspace;
//         case enumKeyCodes::KEY_Home: return ImGuiKey_Home;
//         case enumKeyCodes::KEY_End: return ImGuiKey_End;
//         case enumKeyCodes::KEY_PageUp: return ImGuiKey_PageUp;
//         case enumKeyCodes::KEY_PageDown: return ImGuiKey_PageDown;
//         case enumKeyCodes::KEY_Insert: return ImGuiKey_Insert;
//         case enumKeyCodes::KEY_Pause: return ImGuiKey_Pause;
//         case enumKeyCodes::KEY_ScrollLock: return ImGuiKey_ScrollLock;
//         case enumKeyCodes::KEY_CapsLock: return ImGuiKey_CapsLock;
//         case enumKeyCodes::KEY_Numlock: return ImGuiKey_NumLock;
//     }

//     return ImGuiKey_COUNT;
// }

// cocos2d::enumKeyCodes imguiKeyToCocos(ImGuiKey key) {
//     switch (key) {
//         case ImGuiKey_A: return cocos2d::enumKeyCodes::KEY_A;
//         case ImGuiKey_B: return cocos2d::enumKeyCodes::KEY_B;
//         case ImGuiKey_C: return cocos2d::enumKeyCodes::KEY_C;
//         case ImGuiKey_D: return cocos2d::enumKeyCodes::KEY_D;
//         case ImGuiKey_E: return cocos2d::enumKeyCodes::KEY_E;
//         case ImGuiKey_F: return cocos2d::enumKeyCodes::KEY_F;
//         case ImGuiKey_G: return cocos2d::enumKeyCodes::KEY_G;
//         case ImGuiKey_H: return cocos2d::enumKeyCodes::KEY_H;
//         case ImGuiKey_I: return cocos2d::enumKeyCodes::KEY_I;
//         case ImGuiKey_J: return cocos2d::enumKeyCodes::KEY_J;
//         case ImGuiKey_K: return cocos2d::enumKeyCodes::KEY_K;
//         case ImGuiKey_L: return cocos2d::enumKeyCodes::KEY_L;
//         case ImGuiKey_M: return cocos2d::enumKeyCodes::KEY_M;
//         case ImGuiKey_N: return cocos2d::enumKeyCodes::KEY_N;
//         case ImGuiKey_O: return cocos2d::enumKeyCodes::KEY_O;
//         case ImGuiKey_P: return cocos2d::enumKeyCodes::KEY_P;
//         case ImGuiKey_Q: return cocos2d::enumKeyCodes::KEY_Q;
//         case ImGuiKey_R: return cocos2d::enumKeyCodes::KEY_R;
//         case ImGuiKey_S: return cocos2d::enumKeyCodes::KEY_S;
//         case ImGuiKey_T: return cocos2d::enumKeyCodes::KEY_T;
//         case ImGuiKey_U: return cocos2d::enumKeyCodes::KEY_U;
//         case ImGuiKey_V: return cocos2d::enumKeyCodes::KEY_V;
//         case ImGuiKey_W: return cocos2d::enumKeyCodes::KEY_W;
//         case ImGuiKey_X: return cocos2d::enumKeyCodes::KEY_X;
//         case ImGuiKey_Y: return cocos2d::enumKeyCodes::KEY_Y;
//         case ImGuiKey_Z: return cocos2d::enumKeyCodes::KEY_Z;

//         case ImGuiKey_0: return cocos2d::enumKeyCodes::KEY_Zero;
//         case ImGuiKey_1: return cocos2d::enumKeyCodes::KEY_One;
//         case ImGuiKey_2: return cocos2d::enumKeyCodes::KEY_Two;
//         case ImGuiKey_3: return cocos2d::enumKeyCodes::KEY_Three;
//         case ImGuiKey_4: return cocos2d::enumKeyCodes::KEY_Four;
//         case ImGuiKey_5: return cocos2d::enumKeyCodes::KEY_Five;
//         case ImGuiKey_6: return cocos2d::enumKeyCodes::KEY_Six;
//         case ImGuiKey_7: return cocos2d::enumKeyCodes::KEY_Seven;
//         case ImGuiKey_8: return cocos2d::enumKeyCodes::KEY_Eight;
//         case ImGuiKey_9: return cocos2d::enumKeyCodes::KEY_Nine;

//         case ImGuiKey_F1: return cocos2d::enumKeyCodes::KEY_F1;
//         case ImGuiKey_F2: return cocos2d::enumKeyCodes::KEY_F2;
//         case ImGuiKey_F3: return cocos2d::enumKeyCodes::KEY_F3;
//         case ImGuiKey_F4: return cocos2d::enumKeyCodes::KEY_F4;
//         case ImGuiKey_F5: return cocos2d::enumKeyCodes::KEY_F5;
//         case ImGuiKey_F6: return cocos2d::enumKeyCodes::KEY_F6;
//         case ImGuiKey_F7: return cocos2d::enumKeyCodes::KEY_F7;
//         case ImGuiKey_F8: return cocos2d::enumKeyCodes::KEY_F8;
//         case ImGuiKey_F9: return cocos2d::enumKeyCodes::KEY_F9;
//         case ImGuiKey_F10: return cocos2d::enumKeyCodes::KEY_F10;
//         case ImGuiKey_F11: return cocos2d::enumKeyCodes::KEY_F11;
//         case ImGuiKey_F12: return cocos2d::enumKeyCodes::KEY_F12;

//         case ImGuiKey_LeftCtrl: return cocos2d::enumKeyCodes::KEY_Control;
//         case ImGuiKey_LeftShift: return cocos2d::enumKeyCodes::KEY_Shift;
//         case ImGuiKey_LeftAlt: return cocos2d::enumKeyCodes::KEY_Alt;

//         case ImGuiKey_Tab: return cocos2d::enumKeyCodes::KEY_Tab;
//         case ImGuiKey_LeftArrow: return cocos2d::enumKeyCodes::KEY_ArrowLeft;
//         case ImGuiKey_RightArrow: return
//         cocos2d::enumKeyCodes::KEY_ArrowRight; case ImGuiKey_UpArrow: return
//         cocos2d::enumKeyCodes::KEY_ArrowUp; case ImGuiKey_DownArrow: return
//         cocos2d::enumKeyCodes::KEY_ArrowDown; case ImGuiKey_Space: return
//         cocos2d::enumKeyCodes::KEY_Space; case ImGuiKey_Enter: return
//         cocos2d::enumKeyCodes::KEY_Enter; case ImGuiKey_Escape: return
//         cocos2d::enumKeyCodes::KEY_Escape; case ImGuiKey_Delete: return
//         cocos2d::enumKeyCodes::KEY_Delete; case ImGuiKey_Backspace: return
//         cocos2d::enumKeyCodes::KEY_Backspace; case ImGuiKey_Home: return
//         cocos2d::enumKeyCodes::KEY_Home; case ImGuiKey_End: return
//         cocos2d::enumKeyCodes::KEY_End; case ImGuiKey_PageUp: return
//         cocos2d::enumKeyCodes::KEY_PageUp; case ImGuiKey_PageDown: return
//         cocos2d::enumKeyCodes::KEY_PageDown; case ImGuiKey_Insert: return
//         cocos2d::enumKeyCodes::KEY_Insert; case ImGuiKey_Pause: return
//         cocos2d::enumKeyCodes::KEY_Pause; case ImGuiKey_ScrollLock: return
//         cocos2d::enumKeyCodes::KEY_ScrollLock; case ImGuiKey_CapsLock: return
//         cocos2d::enumKeyCodes::KEY_CapsLock; case ImGuiKey_NumLock: return
//         cocos2d::enumKeyCodes::KEY_Numlock;
//     }
//     return cocos2d::enumKeyCodes::KEY_None; // Default case for unmapped keys
// }
