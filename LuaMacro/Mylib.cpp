#include "Mylib.h"

#if defined(_WIN32)
	#include <Shlobj.h>
#endif

#include <sys/timeb.h>

using namespace Mylib;

//------------------------------------------------------------------------------

void BrPause()
{
	_tprintf(_T("Please press enter to continue..."));
	wint_t ch;
	while ((ch = _gettchar()) != _T('\n'));
	_puttchar(_T('\n'));
}

//------------------------------------------------------------------------------

#if defined(_WIN32)
bool BrGetAppDirPath(/*[in_out]*/TCHAR *buffer, unsigned int bufferSize, /*[out]*/void *nativeErrInfo)
{
	DWORD copySize = GetModuleFileName(NULL, buffer, bufferSize);
	if (copySize == bufferSize || copySize == 0)
	{
		*((DWORD *)nativeErrInfo) = GetLastError();
		return false;
	}
	TCHAR *p = _tcsrchr(buffer, _T('\\'));
	*(++p) = _T('\0');

	*((DWORD *)nativeErrInfo) = ERROR_SUCCESS;
	return true;
}

//------------------------------------------------------------------------------

bool BrGetAppDirPath_CM(/*[out]*/TCHAR **appDirPath_cm, /*[out]*/unsigned int *pathLength, /*[out]*/void *nativeErrInfo)
{
	DWORD bufSize = 256;
	TCHAR *buffer = new TCHAR[bufSize];
	while (true)
	{
		DWORD copySize = GetModuleFileName(NULL, buffer, bufSize);
		if (copySize == bufSize)
		{
			bufSize *= 2;
			delete[] buffer;
			buffer = new TCHAR[bufSize];
		}
		else if (copySize == 0)
		{
			*((DWORD *)nativeErrInfo) = GetLastError();
			return false;
		}
		else
		{
			TCHAR *p = _tcsrchr(buffer, _T('\\'));
			*(++p) = _T('\0');

			*pathLength = (unsigned int)_tcslen(buffer);
			*appDirPath_cm = new TCHAR[*pathLength + 1];
			_tcscpy_s(*appDirPath_cm, *pathLength + 1, buffer);

			delete[] buffer;
			*((DWORD *)nativeErrInfo) = ERROR_SUCCESS;
			return true;
		}
	}
}
#endif

//------------------------------------------------------------------------------

Logger::Logger(const TCHAR *dirPath, const TCHAR *logName)
{
	_logDirPath = dirPath;
	_logPath = Path::Combine(dirPath, logName);

	if (!Directory::Exists(_logDirPath))
		Directory::Create(_logDirPath);
}

//------------------------------------------------------------------------------

void Logger::Write(LogLevel logLevel, const TCHAR *functionName, const TCHAR *msgFormat, ...)
{
	if (!_isEnabled || _level > logLevel)
		return;

	TCHAR timeStr[64];
	time_t logTime;
	tm log_tm;
	_timeb timeBuffer;

	time(&logTime);
	_localtime64_s(&log_tm, &logTime);
	_tcsftime(timeStr, 64, _T("%Y%m%d_%H:%M:%S"), &log_tm);

	_ftime_s(&timeBuffer);
	_stprintf_s(timeStr, _T("%s.%03d"), timeStr, timeBuffer.millitm);

	TCHAR levelStr[8];
	switch (logLevel)
	{
	case LogLevel_Debug:
		_stprintf_s(levelStr, _T("[Debug]"));
		break;
	case LogLevel_Info:
		_stprintf_s(levelStr, _T("[Info] "));
		break;
	case LogLevel_Warn:
		_stprintf_s(levelStr, _T("[Warn] "));
		break;
	case LogLevel_Error:
		_stprintf_s(levelStr, _T("[Error]"));
		break;
	case LogLevel_Fatal:
		_stprintf_s(levelStr, _T("[Fatal]"));
		break;
	default:
		return;
	}

	{
		std::lock_guard<std::mutex> lock(_lockObject);

		FILE *fp = nullptr;
		for (int tryCnt = 0; tryCnt <= 1; tryCnt++)
		{
			errno_t err = _tfopen_s(&fp, _logPath.c_str(), _T("a"));
			if (err == ENOENT)	// if the directory path does not exist
			{
				if (tryCnt == 0)
				{
					if (!Directory::Exists(_logDirPath))
						Directory::Create(_logDirPath);
				}
				else
					return;
			}
			else if (err != 0)	// if failed to open the log file
				return;
			else
				break;
		}

		_ftprintf_s(fp, _T("%s %s | %s | "), levelStr, timeStr, functionName);
		va_list argp;
		va_start(argp, msgFormat);
		_vftprintf(fp, msgFormat, argp);
		va_end(argp);
#if defined(_WIN32)
		_fputtc(_T('\r'), fp);
#endif
		_fputtc(_T('\n'), fp);

		fclose(fp);
	}
}

//------------------------------------------------------------------------------

int64_t Timer::_frequency = 0;
bool Timer::_isHighResolutionSupported;
std::mutex Timer::_lockObject;

//------------------------------------------------------------------------------

void Timer::EnsureInitialized()
{
	// Singleton initialization.
	if (_frequency == 0)
	{
		{
			std::lock_guard<std::mutex> lock(_lockObject);

			if (_frequency == 0)
			{
#if defined(_WIN32)
				LARGE_INTEGER freq;
				BOOL retVal = QueryPerformanceFrequency(&freq);
				_frequency = freq.QuadPart;
				_isHighResolutionSupported = (retVal != FALSE);
#else
				_frequency = CLOCKS_PER_SEC;
				_isHighResolutionSupported = (_frequency > 1000);
#endif
			}
		}
	}
}

//------------------------------------------------------------------------------

tstring Path::Combine(const TCHAR *path1, const TCHAR *path2)
{
	tstring retVal;
	if (path1 != nullptr)
		retVal = path1;
	else if (path2 != nullptr)
	{
		retVal = path2;
		return retVal;
	}
	else
		return retVal;

	if (path2 != nullptr && path2[0] != _T('\0'))
	{
		if (!retVal.empty())
		{
			tstring path(path2);
			TCHAR lastCh_retVal = retVal.back();
			TCHAR firstCh_path = path.front();
			if ((lastCh_retVal == _T('\\') || lastCh_retVal == _T('/')) &&
				(firstCh_path == _T('\\') || firstCh_path == _T('/')))			// ...\ + \...
			{
				retVal.pop_back();		// ... + \...
				retVal.append(path);	// ...\...
			}
			else if ((lastCh_retVal == _T('\\') || lastCh_retVal == _T('/')) ||
				(firstCh_path == _T('\\') || firstCh_path == _T('/')))		// ...\ + ... or ... + \...
			{
				retVal.append(path);	// ...\...
			}
			else																				// ... + ...
			{
				retVal.push_back(Delimiter);	// ...\ + ...
				retVal.append(path);			// ...\...
			}
			// Update delimiter to current OS style.
#if defined(_WIN32)
			std::replace(retVal.begin(), retVal.end(), _T('/'), Delimiter);
#else
			std::replace(retVal.begin(), retVal.end(), _T('\\'), delimiter);
#endif
		}
		else
			retVal = path2;
	}

	return retVal;
}

//------------------------------------------------------------------------------

tstring Path::GetDirectoryName(const tstring &path)
{
	// The output of this function basically follows .NET System.IO.Path.GetDirectoryName().
	// Test some arguments with .NET System.IO.Path.GetDirectoryName(), 
	// the following table shows the return value of .NET and this function.
	// (Both '/' and '\\' are valid delimiter here)
	// Input	.NET Output				This Func Output
	// ""			ArgumentException		""
	// C:\123		C:\						C:\
	// C:\\123		C:\						C:\
	// C::\123		ArgumentException		""
	// C:\			null					""
	// C:			null					""
	// C			""						""
	// CC:\			CC:						CC:
	// C:123		C:						""	***
	// \			null					""
	// :			ArgumentException		""
	// :\			ArgumentException		""
	// 123			""						""
	// \123			\						\
	// 123\456		123						123
	// 123\456\		123\456					123\456

	// Find the last delimiter. ('/' or '\\')
	size_t lastDelIdx = path.find_last_of(_T("/\\"));
	if (lastDelIdx == tstring::npos)
		return String::Empty();

	tstring retVal;
	TCHAR theLastDelimiter = path[lastDelIdx];
	// Chars exist before the last delimiter.
	if (lastDelIdx != 0)
	{
		retVal = path.substr(0, lastDelIdx);
		// Several continuous delimiters are considered as only one delimiter.
		while (!retVal.empty() && (retVal.back() == _T('/') || retVal.back() == _T('\\')))
			retVal.pop_back();
		// Continuous colons are not allowed in path.
		if (retVal.find(_T("::")) != tstring::npos)
			return String::Empty();

		if (retVal.compare(_T(":")) == 0)
			return String::Empty();
		if (retVal.length() == 2 && retVal[1] == _T(':'))
		{
			if (path.length() > (lastDelIdx + 1))	// "C:\..." => "C:\"
			{
				retVal.push_back(theLastDelimiter);
				return retVal;
			}
			else									// "C:\" => ""
				return String::Empty();
		}
		return retVal;
	}
	else
	{
		if (path.length() != 1)		// "\..." => "\"
		{
			retVal = theLastDelimiter;
			return retVal;
		}
		else						// "\" => ""
			return String::Empty();
	}
}

//------------------------------------------------------------------------------

bool Directory::Create(/*[in]*/const TCHAR *path, /*[out]*/void *nativeErrInfo)
{
#if defined(_WIN32)
	int errCode = SHCreateDirectoryEx(NULL, path, NULL);
	if (nativeErrInfo != nullptr)
		*((int *)nativeErrInfo) = errCode;
	if (errCode == ERROR_SUCCESS || errCode == ERROR_FILE_EXISTS || errCode == ERROR_ALREADY_EXISTS)
		return true;
	else
		return false;
#else
	throw "NotImplemented";
#endif
}

//------------------------------------------------------------------------------

bool Directory::Exists(const TCHAR *path)
{
#if defined(_WIN32)
	struct _stat info;
	if (_tstat(path, &info) != 0)
		return false;
#else
	struct stat info;
	if (stat(path, &info) != 0)
		return false;
#endif
	return (info.st_mode & S_IFDIR) != 0;
}

//------------------------------------------------------------------------------

bool File::Exists(const TCHAR *path)
{
#if defined(_WIN32)
	struct _stat info;
	if (_tstat(path, &info) != 0)
		return false;
#else
	struct stat info;
	if (stat(path, &info) != 0)
		return false;
#endif
	return (info.st_mode & _S_IFREG) != 0;
}

//------------------------------------------------------------------------------

Encoding Encoding::_defaultEncoding(CP_ACP);
Encoding Encoding::_utf8Encoding(CP_UTF8);

//------------------------------------------------------------------------------

std::string Encoding::GetString(/*[in]*/const std::wstring &s, /*[out]*/void *nativeErrInfo)
{
#if defined(_WIN32)
	if (nativeErrInfo != nullptr)
		*((DWORD *)nativeErrInfo) = ERROR_SUCCESS;
	if (s.empty())
		return std::string();
	int requiredSize = WideCharToMultiByte(_codePage, 0, &s[0], (int)s.size(), NULL, 0, NULL, NULL);
	if (requiredSize == 0)
	{
		if (nativeErrInfo != nullptr)
			*((DWORD *)nativeErrInfo) = GetLastError();
		return std::string();
	}
	std::string retVal(requiredSize, 0);
	if (WideCharToMultiByte(_codePage, 0, &s[0], (int)s.size(), &retVal[0], requiredSize, NULL, NULL) == 0)
	{
		if (nativeErrInfo != nullptr)
			*((DWORD *)nativeErrInfo) = GetLastError();
		return std::string();
	}
	return retVal;
#else
	throw "NotImplemented";
#endif
}

//------------------------------------------------------------------------------

std::wstring Encoding::GetString(/*[in]*/const std::string &s, /*[out]*/void *nativeErrInfo)
{
#if defined(_WIN32)
	if (nativeErrInfo != nullptr)
		*((DWORD *)nativeErrInfo) = ERROR_SUCCESS;
	if (s.empty())
		return std::wstring();
	int requiredSize = MultiByteToWideChar(_codePage, 0, &s[0], (int)s.size(), NULL, 0);
	if (requiredSize == 0)
	{
		if (nativeErrInfo != nullptr)
			*((DWORD *)nativeErrInfo) = GetLastError();
		return std::wstring();
	}
	std::wstring retVal(requiredSize, 0);
	if (MultiByteToWideChar(_codePage, 0, &s[0], (int)s.size(), &retVal[0], requiredSize) == 0)
	{
		if (nativeErrInfo != nullptr)
			*((DWORD *)nativeErrInfo) = GetLastError();
		return std::wstring();
	}
	return retVal;
#else
	throw "NotImplemented";
#endif
}
