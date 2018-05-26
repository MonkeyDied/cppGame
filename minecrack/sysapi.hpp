#pragma once
/*
 * Wraping System API
 */

namespace sysapi {
	
// For a rect on screen
// in pixel coords
struct Rect {
	int left, right, top, bottom;
};

// Some system functions need initialization and cleansing
void auxiliary_init();
void auxiliary_exit();

void mouseLeftClick();
void mouseRightClick();
void mouseMoveTo(int x, int y);
void getMousePosition(int& xPos, int& yPos);

// Get the windows rect of ms_arbiter.exe
bool getWindowRect(Rect& rect);

// First capture the whole screen by captureScreen
// Then read the buffer through getScreenColor
void captureScreen();
unsigned int getScreenColor(int x, int y);

} // namespace sysapi