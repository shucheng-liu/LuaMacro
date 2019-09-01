#ifndef SCRIPT_FUNC_H
#define SCRIPT_FUNC_H

#include <tchar.h>

enum ScriptState
{
	ScriptState_Running,
	ScriptState_Stopping,
	ScriptState_Stopped
};

int ReadScriptSetting(const TCHAR *scriptPath);

void RunScript(const TCHAR *scriptPath);

void StopScript();

ScriptState GetScriptState();

#endif SCRIPT_FUNC_H
