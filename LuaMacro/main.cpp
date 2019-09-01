#include <Windows.h>
#include <tchar.h>
#include <process.h>

#include "Mylib.h"
#include "IniManager.h"

#include "ScriptFunc.h"

using namespace Mylib;

extern volatile int g_hotkey;	// ScriptFunc.cpp

IniManager	*g_ini = nullptr;
Logger		*g_logger = nullptr;
TCHAR		*g_appDirPath = nullptr;

static HHOOK g_hKeyboard = nullptr;
static const TCHAR *g_scriptPath = nullptr;

static bool	g_isHookRawLogEnabled = false;
bool g_isScriptDebugLogEnabled = false;

//------------------------------------------------------------------------------

unsigned int __stdcall RunScript(void *param)
{
	RunScript((TCHAR *)param);
	return 0;
}

//------------------------------------------------------------------------------

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	static bool s_isCtrlPressed = false;
	static bool s_isAltPressed = false;

	if (nCode >= HC_ACTION)
	{
		KBDLLHOOKSTRUCT *kbhs = (KBDLLHOOKSTRUCT *)lParam;
		if (g_isHookRawLogEnabled)
			g_logger->Write(LogLevel_Debug, __funct__, _T("wParam=0x%X vkCode=0x%X flags=0x%X"), wParam, kbhs->vkCode, kbhs->flags);
		if ((kbhs->vkCode == VK_LCONTROL || kbhs->vkCode == VK_RCONTROL) && !(kbhs->flags & LLKHF_UP))
			s_isCtrlPressed = true;
		else if ((kbhs->vkCode == VK_LCONTROL || kbhs->vkCode == VK_RCONTROL) && (kbhs->flags & LLKHF_UP))
			s_isCtrlPressed = false;
		else if ((kbhs->vkCode == VK_LMENU || kbhs->vkCode == VK_RMENU) && !(kbhs->flags & LLKHF_UP))
			s_isAltPressed = true;
		else if ((kbhs->vkCode == VK_LMENU || kbhs->vkCode == VK_RMENU) && (kbhs->flags & LLKHF_UP))
			s_isAltPressed = false;
		else if (kbhs->vkCode == g_hotkey && !(kbhs->flags & LLKHF_UP) && !s_isCtrlPressed && !s_isAltPressed)
		{
			if (GetScriptState() == ScriptState_Stopped)
			{
				// Run the script.
				uintptr_t hThread = _beginthreadex(NULL, 0, RunScript, (void *)g_scriptPath, 0, NULL);
				if (hThread > 0)	// success
					CloseHandle((HANDLE)hThread);		// we don't need the handle of the thread anymore
				else
					_tprintf(_T("Failed to create thread. err:%d\n"), errno);
			}
			else if (GetScriptState() == ScriptState_Running)
			{
				StopScript();
			}
		}
	}
	return CallNextHookEx(g_hKeyboard, nCode, wParam, lParam);
}

//------------------------------------------------------------------------------

int _tmain(int argc, TCHAR *argv[])
{
	if (argc <= 1)
	{
		_tprintf(_T("Please specify a script.\n"));
		BrPause();
		return 1;
	}
	else if (!File::Exists(argv[1]))
	{
		_tprintf(_T("The script does not exist.\n"));
		BrPause();
		return 2;
	}
	g_scriptPath = argv[1];

	if (!BrGetAppDirPath_CM(&g_appDirPath))
	{
		_tprintf(_T("Failed to get directory path of application.\n"));
		BrPause();
		return 3;
	}

	if (!File::Exists(Path::Combine(g_appDirPath, _T("config.ini"))))
	{
		_tprintf(_T("Config file does not exist.\n"));
		BrPause();
		return 4;
	}

	g_ini = new IniManager();
	if (g_ini->LoadFile(Path::Combine(g_appDirPath, _T("config.ini"))) != 0)
	{
		_tprintf(_T("Cannot load ini file.\n"));
		BrPause();
		return 5;
	}

	g_isHookRawLogEnabled = g_ini->GetBoolValue(_T("Log"), _T("HookRawData"), false);
	g_isScriptDebugLogEnabled = g_ini->GetBoolValue(_T("Log"), _T("ScriptDebugLog"), false);
	g_logger = new Logger(g_appDirPath, _T("app.log"));
	g_logger->IsEnabled(g_ini->GetBoolValue(_T("Log"), _T("Enabled"), false));
	g_logger->Level((LogLevel)g_ini->GetLongValue(_T("Log"), _T("Level"), LogLevel_Warn));
	g_logger->Write(LogLevel_Info, __funct__, _T("App start"));

	// Register keyboard hook.
	g_hKeyboard = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
	if (!g_hKeyboard)
	{
		_tprintf(_T("Hook error: %d\n"), GetLastError());
		BrPause();
		return 6;
	}

	// Read script setting.
	g_logger->Write(LogLevel_Info, __funct__, _T("Read script setting"));
	g_hotkey = ReadScriptSetting(g_scriptPath);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Free resources.
	if (g_hKeyboard)
		UnhookWindowsHookEx(g_hKeyboard);
	delete[] g_appDirPath;
	delete g_logger;
	delete g_ini;

	return 0;
}
