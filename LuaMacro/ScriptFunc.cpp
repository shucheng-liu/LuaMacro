#include "ScriptFunc.h"

#include <Windows.h>

#include <vector>
#include <string>

#include "lua/lua.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "Mylib.h"

using namespace std;
using namespace Mylib;
using namespace cv;

enum ScriptMode
{
	ScriptMode_ReadSetting,
	ScriptMode_Execute
};

enum CoordMode
{
	CoordMode_Global	= 0,
	CoordMode_Local		= 1
};

enum MouseButton
{
	MouseButton_Left = 0,
	MouseButton_Right = 1
};

enum ButtonAction
{
	ButtonAction_Click	= 0,
	ButtonAction_Down	= 1,
	ButtonAction_Up		= 2
};

struct ImageData
{
	unsigned int	width;
	unsigned int	height;
	BYTE			*buffer;
};

extern Logger	*g_logger;	// main.cpp
extern bool		g_isScriptDebugLogEnabled;	// main.cpp

volatile int g_hotkey;

static const char *AbortMsg = "MyAbort";

static tstring g_scriptDirPath;

static ScriptMode			g_scriptMode;
static volatile ScriptState	g_scriptState = ScriptState_Stopped;
static bool					g_isContinuousRun = false;

static CoordMode	g_coordMode = CoordMode_Global;
static POINT		g_localToGlobalOffset;

static int g_screenWidth;
static int g_screenHeight;

static vector<ImageData *>	g_imgList;
static int	g_imgListMaxIdx = -1;

//------------------------------------------------------------------------------

static void init()
{
	g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
	g_screenHeight = GetSystemMetrics(SM_CYSCREEN);
	g_logger->Write(LogLevel_Debug, __funct__, _T("Screen W=%d H=%d"), g_screenWidth, g_screenHeight);
}

//------------------------------------------------------------------------------

static void releaseAllResources()
{
	// Release all image buffers in image list.
	for (int i = g_imgListMaxIdx; i >= 0; i--)
	{
		if (g_imgList[i])
		{
			delete[] g_imgList[i]->buffer;
			delete g_imgList[i];
			g_imgList[i] = nullptr;
		}
	}
	g_imgListMaxIdx = -1;
}

//------------------------------------------------------------------------------

static inline void checkIfStop(lua_State *L)
{
	if (g_scriptState == ScriptState_Stopping)
		luaL_error(L, AbortMsg);	// a trick to interrupt the running lua script
}

//------------------------------------------------------------------------------

static PBITMAPINFO createBitmapInfoStruct(HBITMAP hBmp)
{
	BITMAP bmp;
	PBITMAPINFO pbmi;
	WORD    cClrBits;

	// Retrieve the bitmap color format, width, and height.  
	if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp))
		throw "GetObject() error in CreateBitmapInfoStruct().";

	// Convert the color format to a count of bits.  
	cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
	if (cClrBits == 1)
		cClrBits = 1;
	else if (cClrBits <= 4)
		cClrBits = 4;
	else if (cClrBits <= 8)
		cClrBits = 8;
	else if (cClrBits <= 16)
		cClrBits = 16;
	else if (cClrBits <= 24)
		cClrBits = 24;
	else cClrBits = 32;

	// Allocate memory for the BITMAPINFO structure. (This structure  
	// contains a BITMAPINFOHEADER structure and an array of RGBQUAD  
	// data structures.)  

	if (cClrBits < 24)
	{
		int tmp = 1 << cClrBits;
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
			sizeof(BITMAPINFOHEADER) +
			sizeof(RGBQUAD) * tmp);
	}
	// There is no RGBQUAD array for these formats: 24-bit-per-pixel or 32-bit-per-pixel 

	else
		pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
			sizeof(BITMAPINFOHEADER));

	// Initialize the fields in the BITMAPINFO structure.  

	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = bmp.bmWidth;
	pbmi->bmiHeader.biHeight = bmp.bmHeight;
	pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
	pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
	if (cClrBits < 24)
		pbmi->bmiHeader.biClrUsed = (1 << cClrBits);

	// If the bitmap is not compressed, set the BI_RGB flag.  
	pbmi->bmiHeader.biCompression = BI_RGB;

	// Compute the number of bytes in the array of color  
	// indices and store the result in biSizeImage.  
	// The width must be DWORD aligned unless the bitmap is RLE 
	// compressed. 
	pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8
		* pbmi->bmiHeader.biHeight;
	// Set biClrImportant to 0, indicating that all of the  
	// device colors are important.  
	pbmi->bmiHeader.biClrImportant = 0;
	return pbmi;
}

//------------------------------------------------------------------------------

static int putImageToList(unsigned int width, unsigned int height, BYTE *buffer)
{
	ImageData *pii = new ImageData;
	pii->width = width;
	pii->height = height;
	pii->buffer = buffer;

	// find null slot to store image info
	for (int i = 0; i < g_imgListMaxIdx; i++)
	{
		if (!g_imgList[i])
		{
			g_imgList[i] = pii;
			return i;
		}
	}
	// cannot find null slot
	g_imgListMaxIdx++;
	if ((int)g_imgList.size() > g_imgListMaxIdx)
	{
		g_imgList[g_imgListMaxIdx] = pii;
		return g_imgListMaxIdx;
	}
	else if (g_imgList.size() == g_imgListMaxIdx)
	{
		g_imgList.push_back(pii);
		return g_imgListMaxIdx;
	}
	else
		return -1;
}

//------------------------------------------------------------------------------

static double searchImageByTemplate(unsigned int searchImageIndex,
	int roiLeft, int roiRight, int roiTop, int roiBottom,
	const TCHAR *templateFilePath, int *ptx, int *pty)
{
	/* Match template and compute mean square error to all patches in search image, and return the minimum value.
	*/

	//// get ROI of search image ////

	ImageData *pii = g_imgList[searchImageIndex];
	Mat sImg(pii->height, pii->width, CV_8UC3, pii->buffer);
	cv::Rect roiRect(roiLeft, roiTop, roiRight - roiLeft, roiBottom - roiTop);
	Mat roi = Mat(sImg, roiRect).clone();

	//// get template image ////

	tstring templateFileFullPath = Path::Combine(g_scriptDirPath.c_str(), templateFilePath);

	// Convert Unicode string to MBCS string if needed.
#if defined(_UNICODE) || defined(UNICODE)
	char path[MAX_PATH * 2];
	if (WideCharToMultiByte(CP_ACP, 0, templateFileFullPath.c_str(), -1, path, MAX_PATH * 2, NULL, NULL) == 0)
	{
		g_logger->Write(LogLevel_Fatal, __funct__, _T("Failed to convert string. err=%d"), GetLastError());
		g_logger->Write(LogLevel_Debug, __funct__, _T("SizeOfSrcStr=%d SizeOfDstStr=%d"), templateFileFullPath.size(), MAX_PATH * 2);
		return DBL_MAX;
	}
#else
	const char *path = templateFileFullPath.c_str();
#endif
	Mat templ = imread(path, CV_LOAD_IMAGE_COLOR);	// the color order is B,G,R

	//// match template ////

	int iw = roi.cols;
	int ih = roi.rows;
	int tw = templ.cols;
	int th = templ.rows;
	double minerr = DBL_MAX;
	int ox = -1, oy = -1;
	for (int sy = 0; sy <= ih - th; sy++)
	{
		for (int sx = 0; sx <= iw - tw; sx++)
		{
			int ss = 0;		// sum of squares
			int pixelCnt = 0;
			for (int y = 0; y < th; y++)
			{
				uchar *pi = roi.ptr<uchar>(sy + y) + sx * 3;
				uchar *pt = templ.ptr<uchar>(y);
				for (int x = 0; x < tw; x++)
				{
					// ignore invalid pixel (black color)
					if (pt[0] != 0 || pt[1] != 0 || pt[2] != 0)
					{
						// b
						int diff = pi[0] - pt[0];
						ss += diff * diff;
						// g
						diff = pi[1] - pt[1];
						ss += diff * diff;
						// r
						diff = pi[2] - pt[2];
						ss += diff * diff;

						pixelCnt++;
					}
					pi += 3;
					pt += 3;
				}
			}
			double mse = (double)ss / pixelCnt;
			if (mse < minerr)
			{
				minerr = mse;
				ox = sx + (tw - 1) / 2;
				oy = sy + (th - 1) / 2;
			}
		}
	}
	if (ptx)
		*ptx = roiLeft + ox + (tw - 1) / 2;
	if (pty)
		*pty = roiTop + oy + (th - 1) / 2;

	roi.release();
	templ.release();
	return minerr;
}

//------------------------------------------------------------------------------

static void EncodingConversionErrorHandler(const TCHAR *callerName, lua_State *L)
{
	g_logger->Write(LogLevel_Fatal, callerName, _T("Failed to convert string."));
	luaL_error(L, "An internal error occurred.");
}

//------------------------------------------------------------------------------

static int l_FinishEnvironmentSetting(lua_State *L)
{
	// Read 'HOTKEY'.
	lua_getglobal(L, "HOTKEY");
	g_hotkey = (int)lua_tointeger(L, -1);

	if (g_scriptMode == ScriptMode_ReadSetting)
		luaL_error(L, AbortMsg);	// a trick to abort the running lua script

	// Read 'IS_CONTINUOUS_RUN'
	lua_getglobal(L, "IS_CONTINUOUS_RUN");
	g_isContinuousRun = (lua_toboolean(L, -1)) != 0;

	// Ensure all resources are released before run script.
	releaseAllResources();

	return 0;
}

//------------------------------------------------------------------------------

static int l_StopScript(lua_State *L)
{
	StopScript();
	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_SetCoordMode(lua_State *L)
{
	HWND hWnd = NULL;
	int argc = lua_gettop(L);
	if (argc == 1)
	{
		g_coordMode = (CoordMode)lua_tointeger(L, 1);
		if (g_coordMode != CoordMode_Global)
			luaL_error(L, "Arguments error.");
	}
	else if (argc == 3)
	{
		g_coordMode = (CoordMode)lua_tointeger(L, 1);
		if (g_coordMode == CoordMode_Local)
		{
			hWnd = FindWindowA(lua_tostring(L, 2), lua_tostring(L, 3));
			if (!hWnd)
				luaL_error(L, "The window is not launched.");

			g_localToGlobalOffset.x = 0;
			g_localToGlobalOffset.y = 0;
			BOOL ret = ClientToScreen(hWnd, &g_localToGlobalOffset);
			if (!ret)
				luaL_error(L, "Failed to convert local coordinates to global.");
		}
		else
			luaL_error(L, "Arguments error.");
	}
	else
		luaL_error(L, "Wrong number of arguments.");

	return 0;
}

//------------------------------------------------------------------------------

static int l_MoveMouseToPoint(lua_State *L)
{
	INPUT in;
	in.type = INPUT_MOUSE;
	in.mi.mouseData = 0;
	in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
	in.mi.time = 0;
	in.mi.dwExtraInfo = NULL;
	// Notice that if MOUSEEVENTF_ABSOLUTE value is specified, 
	// dx and dy contain normalized absolute coordinates between 0 and 65535.
	in.mi.dx = LONG((int64_t)lua_tointeger(L, 1) * 65535 / g_screenWidth);
	in.mi.dy = LONG((int64_t)lua_tointeger(L, 2) * 65535 / g_screenHeight);
	if (g_coordMode == CoordMode_Local)
	{
		in.mi.dx += g_localToGlobalOffset.x;
		in.mi.dy += g_localToGlobalOffset.y;
	}
	SendInput(1, &in, sizeof(INPUT));

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_MouseButton(lua_State *L)
{
	MouseButton button = (MouseButton)lua_tointeger(L, 1);
	ButtonAction action = (ButtonAction)lua_tointeger(L, 2);

	INPUT in;
	in.type = INPUT_MOUSE;
	in.mi.dx = 0;
	in.mi.dy = 0;
	in.mi.mouseData = 0;
	in.mi.time = 0;
	in.mi.dwExtraInfo = NULL;

	DWORD buttonDown = (button == MouseButton_Left) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
	DWORD buttonUp = (button == MouseButton_Left) ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
	if (action == ButtonAction_Click)
	{
		in.mi.dwFlags = buttonDown;
		SendInput(1, &in, sizeof(INPUT));
		Sleep(50);
		in.mi.dwFlags = buttonUp;
		SendInput(1, &in, sizeof(INPUT));
	}
	else if (action == ButtonAction_Down)
	{
		in.mi.dwFlags = buttonDown;
		SendInput(1, &in, sizeof(INPUT));
	}
	else if (action == ButtonAction_Up)
	{
		in.mi.dwFlags = buttonUp;
		SendInput(1, &in, sizeof(INPUT));
	}
	else
		luaL_error(L, "Invalid argument 'action'.");

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_RotateMouseWheel(lua_State *L)
{
	INPUT in;
	in.type = INPUT_MOUSE;
	in.mi.dx = 0;
	in.mi.dy = 0;
	in.mi.mouseData = WHEEL_DELTA * (DWORD)lua_tointeger(L, 1);
	in.mi.dwFlags = MOUSEEVENTF_WHEEL;
	in.mi.time = 0;
	in.mi.dwExtraInfo = NULL;
	SendInput(1, &in, sizeof(INPUT));

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_KeyboardButton(lua_State *L)
{
	WORD virtualKeyCode = (WORD)lua_tointeger(L, 1);
	ButtonAction action = (ButtonAction)lua_tointeger(L, 2);

	INPUT in;
	in.type = INPUT_KEYBOARD;
	in.ki.wVk = virtualKeyCode;
	in.ki.wScan = MapVirtualKey(in.ki.wVk, MAPVK_VK_TO_VSC);
	in.ki.dwFlags = 0;	// KEYEVENTF_KEYUP - If specified, the key is being released. If not specified, the key is being pressed.
	in.ki.time = 0;		// If this parameter is zero, the system will provide its own time stamp.
	in.ki.dwExtraInfo = NULL;

	if (action == ButtonAction_Click)
	{
		in.ki.dwFlags = 0;					// the key is being pressed
		SendInput(1, &in, sizeof(INPUT));
		in.ki.dwFlags = KEYEVENTF_KEYUP;	// the key is being released
		SendInput(1, &in, sizeof(INPUT));
	}
	else if (action == ButtonAction_Down)
	{
		in.ki.dwFlags = 0;					// the key is being pressed
		SendInput(1, &in, sizeof(INPUT));
	}
	else if (action == ButtonAction_Up)
	{
		in.ki.dwFlags = KEYEVENTF_KEYUP;	// the key is being released
		SendInput(1, &in, sizeof(INPUT));
	}
	else
		luaL_error(L, "Invalid argument 'action'.");

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_Delay(lua_State *L)
{
	unsigned int msMin, msMax;

	int argc = lua_gettop(L);
	if (argc == 2)
	{
		msMin = (unsigned int)lua_tointeger(L, 1);
		msMax = (unsigned int)lua_tointeger(L, 2);
	}
	else if (argc == 1)
	{
		msMin = msMax = (unsigned int)lua_tointeger(L, 1);
	}
	else
		luaL_error(L, "Invalid number of arguments.");
	
	if (msMax < 1)
		return 0;
	if (msMin < 1)
		msMin = 1;

	unsigned int msDelay = (rand() % (msMax - msMin + 1)) + msMin;
	int64_t startTime = Mylib::Timer::TickCount();
	int64_t nowTime = startTime;
	unsigned int elapsedTime = (unsigned int)Mylib::Timer::ElapsedTimeInMillisec(nowTime - startTime);
	while (elapsedTime < msDelay)
	{
		Sleep((msDelay - elapsedTime >= 100) ? 100 : msDelay - elapsedTime);
		checkIfStop(L);
		nowTime = Mylib::Timer::TickCount();
		elapsedTime = (unsigned int)Mylib::Timer::ElapsedTimeInMillisec(nowTime - startTime);
	}

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_PlayWavSoundAsync(lua_State *L)
{
	const char *filePath = lua_tostring(L, 1);
#if defined(_UNICODE) || defined(UNICODE)
	wchar_t path[MAX_PATH];
	if (MultiByteToWideChar(CP_ACP, 0, filePath, -1, path, MAX_PATH) == 0)
	{
		DWORD err = GetLastError();
		g_logger->Write(LogLevel_Error, __funct__, _T("Failed to convert string. err:%d"), err);
		_tprintf(_T("Failed to convert string. err:%d\n"), err);
		return 0;
	}
#else
	const char *path = filePath;
#endif
	PlaySound(path, NULL, SND_ASYNC | SND_FILENAME);

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_WriteLog(lua_State *L)
{
	int logLevel = (int)lua_tointeger(L, 1);
	const char *message = lua_tostring(L, 2);
	if (logLevel == LogLevel_Debug && !g_isScriptDebugLogEnabled)
		return 0;
#if defined(_UNICODE) || defined(UNICODE)
	wchar_t msg[MAX_PATH];
	if (MultiByteToWideChar(CP_ACP, 0, message, -1, msg, MAX_PATH) == 0)
	{
		DWORD err = GetLastError();
		g_logger->Write(LogLevel_Error, __funct__, _T("Failed to convert string. err:%d"), err);
		_tprintf(_T("Failed to convert string. err:%d\n"), err);
		return 0;
	}
#else
	const char *msg = message;
#endif
	g_logger->Write((LogLevel)logLevel, __funct__, msg);

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_CaptureWindowImage(lua_State *L)
{
	HWND hWnd = NULL;
	int argc = lua_gettop(L);
	if (argc == 2)
	{
		hWnd = FindWindowA(lua_tostring(L, 1), lua_tostring(L, 2));
		if (!hWnd)
			luaL_error(L, "The window is not launched.");
	}
	else if (argc != 0)
		luaL_error(L, "Wrong number of arguments.");

	long width;
	long height;
	if (hWnd == NULL)
	{
		width = g_screenWidth;
		height = g_screenHeight;
	}
	else
	{
		RECT rect;
		GetClientRect(hWnd, &rect);
		width = rect.right;
		height = rect.bottom;
	}
	HDC hDC = GetDC(hWnd);
	HDC hCaptureDC = CreateCompatibleDC(hDC);
	HBITMAP hCaptureBitmap = CreateCompatibleBitmap(hDC, width, height);
	SelectObject(hCaptureDC, hCaptureBitmap);
	BitBlt(hCaptureDC, 0, 0, width, height, hDC, 0, 0, SRCCOPY | CAPTUREBLT);

	PBITMAPINFO pbi = createBitmapInfoStruct(hCaptureBitmap);
	PBITMAPINFOHEADER pbih = (PBITMAPINFOHEADER)pbi;
	LPBYTE lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);
	// Retrieve the color table (RGBQUAD array) and the bits  
	// (array of palette indices) from the DIB.  
	if (!GetDIBits(hCaptureDC, hCaptureBitmap, 0, (WORD)pbih->biHeight, lpBits, pbi, DIB_RGB_COLORS))
		luaL_error(L, "Error occurred in function 'GetDIBits()'.");

	BYTE *imgBuf = nullptr;
	try
	{
		imgBuf = new BYTE[width * height * 3];
	}
	catch (std::bad_alloc &ba)
	{
		g_logger->Write(LogLevel_Fatal, __funct__, _T("Failed to allocate memory. (Did you forget to delete image?)"));
		throw ba;
	}
	BYTE *in = lpBits;
	BYTE *out;
	for (int y = 0; y < height; y++)
	{
		out = imgBuf + (height - y - 1) * width * 3;
		for (int x = 0; x < width; x++)
		{
			out[0] = in[0];	// b
			out[1] = in[1];	// g
			out[2] = in[2];	// r
			in += 4;
			out += 3;
		}
	}

	GlobalFree((HGLOBAL)lpBits);
	LocalFree((HLOCAL)pbi);
	ReleaseDC(hWnd, hDC);
	DeleteDC(hCaptureDC);
	DeleteObject(hCaptureBitmap);

	checkIfStop(L);

	int imgIdx = putImageToList(width, height, imgBuf);
	if (imgIdx == -1)
		luaL_error(L, "Failed to get image index.");

	lua_pushnumber(L, (lua_Number)imgIdx);
	return 1;
}

//------------------------------------------------------------------------------

static int l_GetImageWidthHeight(lua_State *L)
{
	int imageIndex = (int)lua_tointeger(L, 1);

	lua_pushinteger(L, g_imgList[imageIndex]->width);
	lua_pushinteger(L, g_imgList[imageIndex]->height);
	return 2;
}

//------------------------------------------------------------------------------

static int l_GetRgbOfPointOnImage(lua_State *L)
{
	int imageIndex = (int)lua_tointeger(L, 1);
	int x = (int)lua_tointeger(L, 2);
	int y = (int)lua_tointeger(L, 3);

	ImageData *pid = g_imgList[imageIndex];
	const uchar *p = pid->buffer + (y * pid->width + x) * 3;
	
	lua_pushinteger(L, p[2]);	// r
	lua_pushinteger(L, p[1]);	// g
	lua_pushinteger(L, p[0]);	// b
	return 3;
}

//------------------------------------------------------------------------------

static int l_SaveImage(lua_State *L)
{
	int imageIndex = (int)lua_tointeger(L, 1);
	const char *filePath = lua_tostring(L, 2);

	if (filePath[0] == '\0')
	{
		g_logger->Write(LogLevel_Warn, __funct__, _T("The file path is an empty string."));
		return 0;
	}

	ImageData *pid = g_imgList[imageIndex];
	Mat img(pid->height, pid->width, CV_8UC3, pid->buffer);

	wstring wFilePath = Encoding::Default().GetString(filePath);
	if (wFilePath.empty())
		EncodingConversionErrorHandler(__funct__, L);
	wstring wPath = Path::Combine(g_scriptDirPath, wFilePath);
	string path = Encoding::Default().GetString(wPath);
	if (path.empty())
		EncodingConversionErrorHandler(__funct__, L);

	imwrite(path.c_str(), img);

	return 0;
}

//------------------------------------------------------------------------------

static int l_DeleteImage(lua_State *L)
{
	int imageIndex = (int)lua_tointeger(L, 1);

	if ((int)imageIndex > g_imgListMaxIdx)
		luaL_error(L, "Invalid image index.");

	delete[] g_imgList[imageIndex]->buffer;
	delete g_imgList[imageIndex];
	g_imgList[imageIndex] = NULL;

	if (imageIndex == g_imgListMaxIdx)
	{
		while (g_imgListMaxIdx >= 0 && !g_imgList[g_imgListMaxIdx])
			g_imgListMaxIdx--;
	}

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

static int l_SearchImageByTemplate(lua_State *L)
{
#if defined(_UNICODE) || defined(UNICODE)
	wchar_t templPath[MAX_PATH];
	if (MultiByteToWideChar(CP_ACP, 0, lua_tostring(L, 6), -1, templPath, MAX_PATH) == 0)
	{
		g_logger->Write(LogLevel_Fatal, __funct__, _T("Failed to convert string. err:%d"), GetLastError());
		luaL_error(L, "An internal error occurred.");
		return 0;
	}
#else
	const char *templPath = lua_tostring(L, 6);
#endif

	int x, y;
	double retVal = searchImageByTemplate(
		(unsigned int)lua_tointeger(L, 1),
		(int)lua_tointeger(L, 2),
		(int)lua_tointeger(L, 3),
		(int)lua_tointeger(L, 4),
		(int)lua_tointeger(L, 5),
		templPath,
		&x, &y);
	lua_pushnumber(L, retVal);
	lua_pushinteger(L, x);
	lua_pushinteger(L, y);
	checkIfStop(L);
	return 3;
}

//------------------------------------------------------------------------------

static int l_TestFunc(lua_State *L)
{
	int argc = lua_gettop(L);
	const char *str = lua_tostring(L, 1);
	if (str == NULL)
		printf("argc=%d, I'm NULL.\n", argc);
	else
		printf("argc=%d, I'm %s.\n", argc, str);

	checkIfStop(L);
	return 0;
}

//------------------------------------------------------------------------------

int ReadScriptSetting(const TCHAR *scriptPath)
{
	g_scriptMode = ScriptMode_ReadSetting;
	g_scriptDirPath = Path::GetDirectoryName(scriptPath);
	int retVal = -1;

	// Initialize Lua interpreter.
	lua_State *L = luaL_newstate(); /* opens Lua */
	if (!L)
	{
		_tprintf(_T("Failed to open Lua.\n"));
		goto FuncEnd;
	}
	luaL_openlibs(L); /* opens the standard libraries */

	// Register functions.
	lua_register(L, "FinishEnvironmentSetting", l_FinishEnvironmentSetting);

	// Convert Unicode string to MBCS string if needed.
#if defined(_UNICODE) || defined(UNICODE)
	char path[MAX_PATH * 2];
	if (WideCharToMultiByte(CP_ACP, 0, scriptPath, -1, path, MAX_PATH * 2, NULL, NULL) == 0)
	{
		_tprintf(_T("Failed to convert string. err:%d\n"), GetLastError());
		goto FuncEnd;
	}
#else
	const char *path = scriptPath;
#endif

	// Initialize some stuff.
	init();

	// Run the script.

	g_scriptState = ScriptState_Running;
	if (luaL_dofile(L, path) != 0)
	{
		char errmsg[2048];
		errmsg[0] = '\0';
		errno_t err = strncpy_s(errmsg, lua_tostring(L, -1), _TRUNCATE);
		if (err != 0)
			g_logger->Write(LogLevel_Warn, __funct__, _T("'strncpy_s', errno_t=%d"), err);
		if (strstr(errmsg, AbortMsg))
		{
			retVal = g_hotkey;
		}
		else
		{
			printf("Script error: %s\n", errmsg);
			goto FuncEnd;
		}
	}

FuncEnd:
	g_scriptState = ScriptState_Stopped;
	if (L)
		lua_close(L);
	return retVal;
}

//------------------------------------------------------------------------------

void RunScript(const TCHAR *scriptPath)
{
	g_scriptMode = ScriptMode_Execute;

	// Initialize Lua interpreter.
	g_logger->Write(LogLevel_Info, __funct__, _T("Initialize Lua"));
	lua_State *L = luaL_newstate(); /* opens Lua */
	if (!L)
	{
		_tprintf(_T("Failed to open Lua.\n"));
		g_logger->Write(LogLevel_Fatal, __funct__, _T("Failed to open Lua."));
		goto FuncEnd;
	}
	luaL_openlibs(L); /* opens the standard libraries */

	// Register functions.

	lua_register(L, "FinishEnvironmentSetting", l_FinishEnvironmentSetting);
	lua_register(L, "StopScript", l_StopScript);
	lua_register(L, "SetCoordMode", l_SetCoordMode);
	lua_register(L, "MoveMouseToPoint", l_MoveMouseToPoint);
	lua_register(L, "MouseButton", l_MouseButton);
	lua_register(L, "RotateMouseWheel", l_RotateMouseWheel);
	lua_register(L, "KeyboardButton", l_KeyboardButton);
	lua_register(L, "Delay", l_Delay);
	lua_register(L, "PlayWavSoundAsync", l_PlayWavSoundAsync);
	lua_register(L, "WriteLog", l_WriteLog);
	lua_register(L, "CaptureWindowImage", l_CaptureWindowImage);
	lua_register(L, "GetImageWidthHeight", l_GetImageWidthHeight);
	lua_register(L, "GetRgbOfPointOnImage", l_GetRgbOfPointOnImage);
	lua_register(L, "SaveImage", l_SaveImage);
	lua_register(L, "DeleteImage", l_DeleteImage);
	lua_register(L, "SearchImageByTemplate", l_SearchImageByTemplate);
	lua_register(L, "TestFunc", l_TestFunc);

	// Convert Unicode string to MBCS string if needed.
#if defined(_UNICODE) || defined(UNICODE)
	char path[MAX_PATH];
	if (WideCharToMultiByte(CP_ACP, 0, scriptPath, -1, path, MAX_PATH, NULL, NULL) == 0)
	{
		_tprintf(_T("Failed to convert string. err:%d\n"), GetLastError());
		goto FuncEnd;
	}
#else
	const char *path = scriptPath;
#endif

	// Run the script.

	_tprintf(_T("Script is started.\n"));
	do
	{
		g_scriptState = ScriptState_Running;
		int64_t startTime = Mylib::Timer::TickCount();
		if (luaL_dofile(L, path) != 0)
		{
			char errmsg[2048];
			errmsg[0] = '\0';
			errno_t err = strncpy_s(errmsg, lua_tostring(L, -1), _TRUNCATE);
			if (err != 0)
				g_logger->Write(LogLevel_Warn, __funct__, _T("'strncpy_s', errno_t=%d"), err);
			if (strstr(errmsg, AbortMsg))
				_tprintf(_T("Script is stopped.\n"));
			else
				printf("Script error: %s\n", errmsg);
			break;
		}
		if (!g_isContinuousRun)
			_tprintf(_T("Script is stopped. Running time: %.2lf s\n"), Mylib::Timer::ElapsedTimeInSec(Mylib::Timer::TickCount() - startTime));
	}
	while (g_isContinuousRun);

FuncEnd:
	g_scriptState = ScriptState_Stopped;
	if (L)
		lua_close(L);
	releaseAllResources();
}

//------------------------------------------------------------------------------

void StopScript()
{
	if (g_scriptState == ScriptState_Running)
		g_scriptState = ScriptState_Stopping;
}

//------------------------------------------------------------------------------

ScriptState GetScriptState()
{
	return g_scriptState;
}
