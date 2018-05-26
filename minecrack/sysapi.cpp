#include "sysapi.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <vector>

// Some Staff From Internet
void getMousePosition(int& xPos, int& yPos)
{
	// create POINT object named "cursorPos" for "cursor position"
	POINT cursorPos;
	// call the function "GetCursorPos" and pass it the a pointer
	// to our POINT cursorPos
	GetCursorPos(&cursorPos);
	// read the x property from cursorPos and assign it to the
	// integer variable xPos (a pointer, technically)
	xPos = cursorPos.x;
	// do the same thing for the y position
	yPos = cursorPos.y;
}


// click the mouse using "SendInput"
void mouseRightClick()
{
	// create an INPUT object named "Input"
	INPUT Input = { 0 };

	// simulate the right mouse button being pressed
	// specify the type of input as mouse
	Input.type = INPUT_MOUSE;
	// specify the action that was performed, "right down"
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	// send the input to the system
	::SendInput(1, &Input, sizeof(INPUT));

	// simulate release of the right mouse button
	// clear the the "Input" object rather than assign a more memory
	::ZeroMemory(&Input, sizeof(INPUT));
	// specify type of input
	Input.type = INPUT_MOUSE;
	// indicate that the action "right up" was performed
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	// send the input to the system
	::SendInput(1, &Input, sizeof(INPUT));
}

void mouseLeftClick()
{
	INPUT Input = { 0 };

	// left down
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	::SendInput(1, &Input, sizeof(INPUT));

	// left up
	::ZeroMemory(&Input, sizeof(INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	::SendInput(1, &Input, sizeof(INPUT));
}


// finds out information about the user's current screen configuration
// then moves the cursor to the requested coordinates
void mouseMoveTo(int toX, int toY)
{
	// get system information regarding screen size / resolution
	double screenRes_width = ::GetSystemMetrics(SM_CXSCREEN) - 1;
	double screenRes_height = ::GetSystemMetrics(SM_CYSCREEN) - 1;
	// scale the function inputs 'toX and 'toY' appropriately by a
	// factor of 65535
	double dx = toX*(65535.0f / screenRes_width);
	double dy = toY*(65535.0f / screenRes_height);
	// we now have variables 'dx' and 'dy' with the user-desired
	// coordinates in a form that the SendInput function can understand

	// set up INPUT Input, assign values, and move the cursor
	INPUT Input = { 0 };
	// we want to be sending a MOUSE-type input
	Input.type = INPUT_MOUSE;
	// the mouse input is of the 'MOVE ABSOLUTE' variety
	Input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	Input.mi.dx = LONG(dx);
	Input.mi.dy = LONG(dy);
	// we have given information about the magnitude of the new
	// cursor position; here we will send it
	::SendInput(1, &Input, sizeof(INPUT));
}




typedef struct EnumHWndsArg
{
	std::vector<HWND> *vecHWnds;
	DWORD dwProcessId;
}EnumHWndsArg, *LPEnumHWndsArg;

HANDLE GetProcessHandleByID(int nID)//通过进程ID获取进程句柄
{
	return OpenProcess(PROCESS_ALL_ACCESS, FALSE, nID);
}

DWORD GetProcessIDByName(const char* pName)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hSnapshot) {
		return NULL;
	}
	PROCESSENTRY32 pe = { sizeof(pe) };
	for (BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {
		if (strcmp(pe.szExeFile, pName) == 0) {
			CloseHandle(hSnapshot);
			return pe.th32ProcessID;
		}
		//printf("%-6d %s\n", pe.th32ProcessID, pe.szExeFile);
	}
	CloseHandle(hSnapshot);
	return 0;
}

BOOL CALLBACK lpEnumFunc(HWND hwnd, LPARAM lParam)
{
	EnumHWndsArg *pArg = (LPEnumHWndsArg)lParam;
	DWORD  processId;
	GetWindowThreadProcessId(hwnd, &processId);
	if (processId == pArg->dwProcessId)
	{
		pArg->vecHWnds->push_back(hwnd);
		//printf("%p\n", hwnd);
	}
	return TRUE;
}

void GetHWndsByProcessID(DWORD processID, std::vector<HWND> &vecHWnds)
{
	EnumHWndsArg wi;
	wi.dwProcessId = processID;
	wi.vecHWnds = &vecHWnds;
	EnumWindows(lpEnumFunc, (LPARAM)&wi);
}


bool getWindowRect(Rect& rect) {
	DWORD pid = GetProcessIDByName("ms_arbiter.exe");
	if (pid != 0)
	{
		std::vector<HWND> vecHWnds;
		GetHWndsByProcessID(pid, vecHWnds);
		for (const HWND &h : vecHWnds)
		{
			RECT test;
			if (GetWindowRect(h, &test)) {
				if (test.right - test.left == 510 &&
					test.bottom - test.top == 400)
				{
					rect.left   = test.left;
					rect.right  = test.right;
					rect.top    = test.top;
					rect.bottom = test.bottom;
					return true;
				}
			}
			else {
				puts("--> Failed to get Rect Info");
			}
		}
	}
	else {
		puts("Failed to get pid of ms_arbiter.exe");
	}
	return false;
}

HDC dc;
void auxiliary_init() {
	dc = GetDC(NULL);
}

void auxiliary_exit() {
	ReleaseDC(NULL, dc);
}

unsigned int getScreenColor2(int x, int y) {
	COLORREF color = GetPixel(dc, x, y);
	return (unsigned int)color;
}

#include <iostream>
using std::cout;
using std::endl;
std::vector<BYTE> pixels;
BITMAPINFO MyBMInfo = { 0 };

int nScreenWidth;
int nScreenHeight;
size_t pixelSize;
size_t scanlineSize;
size_t bitmapSize;

void captureScreen() {
	HDC hdc = GetDC(NULL);	// Entire Screen

	// Get screen dimensions
	nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Create compatible DC, create a compatible bitmap and copy the screen using BitBlt()
	HDC hCaptureDC = CreateCompatibleDC(hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, nScreenWidth, nScreenHeight);
	HGDIOBJ hOld = SelectObject(hCaptureDC, hBitmap);
	BOOL bOK = BitBlt(hCaptureDC, 0, 0, nScreenWidth, nScreenHeight, hdc, 0, 0, SRCCOPY | CAPTUREBLT);

	SelectObject(hCaptureDC, hOld); // always select the previously selected object once done
	DeleteDC(hCaptureDC);
	
	
	MyBMInfo.bmiHeader.biSize = sizeof(MyBMInfo.bmiHeader);

	// Get the BITMAPINFO structure from the bitmap
	if (0 == GetDIBits(hdc, hBitmap, 0, 0, NULL, &MyBMInfo, DIB_RGB_COLORS)) {
		cout << "error" << endl;
	}

	// create the bitmap buffer
	pixels.clear();
	pixels.resize(MyBMInfo.bmiHeader.biSizeImage);
	std::fill(pixels.begin(), pixels.end(), 0);
	BYTE* lpPixels = pixels.data();//new BYTE[MyBMInfo.bmiHeader.biSizeImage];

	// Better do this here - the original bitmap might have BI_BITFILEDS, which makes it
	// necessary to read the color table - you might not want this.
	MyBMInfo.bmiHeader.biCompression = BI_RGB;
	MyBMInfo.bmiHeader.biHeight = -nScreenHeight;

	// get the actual bitmap buffer
	if (0 == GetDIBits(hdc, hBitmap, 0, nScreenHeight, (LPVOID)lpPixels, &MyBMInfo, DIB_RGB_COLORS)) {
		cout << "error2" << endl;
	}

	//for (int i = 0; i < 100; i++) {
	//	cout << (int)lpPixels[i];
	//}

	DeleteObject(hBitmap);
	ReleaseDC(NULL, hdc);
	//delete[] lpPixels;

	// The following items contain some auxiliary items to be computed
	// the following calculations work for 16/24/32 bits bitmaps 
	// but assume a byte pixel array
	pixelSize = MyBMInfo.bmiHeader.biBitCount / 8;
	// the + 3 ) & ~3 part is there to ensure that each
	// scan line is 4 byte aligned
	scanlineSize = (pixelSize * MyBMInfo.bmiHeader.biWidth + 3) & ~3;
	bitmapSize = std::abs(MyBMInfo.bmiHeader.biHeight) * scanlineSize;
}


unsigned int getScreenColor(int x, int y) {
	size_t pixelOffset = y * scanlineSize + x * pixelSize;
	return RGB(pixels[pixelOffset + 2],
			   pixels[pixelOffset + 1],
			   pixels[pixelOffset + 0]);
}


