#pragma once
#include "fabgl.h"

// Layout for a physical Brazilian ABNT2 PS/2 keyboard, FabGL 1.0.9.
// This structure is passed directly to Keyboard::setLayout(). It inherits
// letters, navigation, function keys, modifiers and numpad from the FabGL US
// table, and overrides only the PS/2 set-2 scan codes that differ in ABNT2.
// The original TK-2000 matrix has no accented-character keys; the caller must
// ignore the cedilla key; the TK matrix has no such input.
static const fabgl::KeyboardLayout TK2000ABNT2Layout = {
  "BR", "Portuguese (Brazil ABNT2)", &fabgl::USLayout,
  {
    { 0x0E, fabgl::VK_QUOTE },       // ' " (US: ` ~)
    { 0x54, fabgl::VK_ACUTEACCENT }, // acute / grave, physically dead
    { 0x5B, fabgl::VK_LEFTBRACKET }, // [ {
    { 0x5D, fabgl::VK_RIGHTBRACKET },// ] }
    { 0x4C, fabgl::VK_CEDILLA_c },   // ç Ç (no matching TK hardware key)
    { 0x52, fabgl::VK_TILDE },       // tilde / circumflex, physically dead
    { 0x4A, fabgl::VK_SEMICOLON },   // ; : (US: / ?)
    { 0x61, fabgl::VK_BACKSLASH },   // physical ABNT2 \ | next to left Shift
    { 0x51, fabgl::VK_SLASH },       // physical ABNT2 / ? next to right Shift
  },
  {}, // extended scancodes inherited unchanged (arrows, KP Enter, etc.)
  {
    { fabgl::VK_6,           { 0, 0, 0, 1 }, fabgl::VK_DIAERESIS }, // Shift+6: ¨, not ^
    { fabgl::VK_ACUTEACCENT, { 0, 0, 0, 1 }, fabgl::VK_GRAVEACCENT },
    { fabgl::VK_TILDE,       { 0, 0, 0, 1 }, fabgl::VK_CARET },
    { fabgl::VK_CEDILLA_c,   { 0, 0, 0, 1 }, fabgl::VK_CEDILLA_C },
    { fabgl::VK_BACKSLASH,   { 0, 0, 0, 1 }, fabgl::VK_VERTICALBAR },
    { fabgl::VK_q,           { 0, 0, 1, 0 }, fabgl::VK_SLASH },    // AltGr+Q
    { fabgl::VK_w,           { 0, 0, 1, 0 }, fabgl::VK_QUESTION }, // AltGr+W
  },
  {}, // no dead-key composition: the original TK cannot display accents
  {},
};
