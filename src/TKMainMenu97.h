#pragma once
// F1: ten rows must fit entirely inside the emulated 640x200 VGA frame.
// Keep the 5x7 font, 13-pixel row pitch and the existing footer text.
struct TKMainMenu97 {
  static const int rows=10;
  static const int x=125, y=7, width=390, height=186;
  static const int firstRowOffset=25, rowPitch=13;
  static const int separatorOffset=162, footerOffset=170;
  static int rowY(int index) { return y+firstRowOffset+index*rowPitch; }
};
