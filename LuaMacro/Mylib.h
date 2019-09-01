#ifndef MYLIB_H
#define MYLIB_H

#if defined(_WIN32)
	#include <Windows.h>
	#include <tchar.h>
#else
// Other platform needs something which implements Windows tchar.h...
#endif
#include <string>
#include <algorithm>
#include <mutex>

#if defined(_UNICODE) || defined(UNICODE)
	#define __funct__	__FUNCTIONW__
#else
	#define __funct__	__FUNCTION__
#endif

typedef std::basic_string<TCHAR> tstring;

//------------------------------------------------------------------------------

void BrPause();

//------------------------------------------------------------------------------

#if defined(_WIN32)
bool BrGetAppDirPath_CM(/*[out]*/TCHAR **appDirPath_cm, /*[out]*/unsigned int *pathLength, /*[out]*/void *nativeErrInfo);
#endif

inline bool BrGetAppDirPath_CM(/*[out]*/TCHAR **appDirPath_cm, /*[out]*/unsigned int *pathLength)
{
#if defined(_WIN32)
	DWORD nil;
	return BrGetAppDirPath_CM(appDirPath_cm, pathLength, &nil);
#endif
}

inline bool BrGetAppDirPath_CM(/*[out]*/TCHAR **appDirPath_cm)
{
#if defined(_WIN32)
	unsigned int nil1;
	DWORD nil2;
	return BrGetAppDirPath_CM(appDirPath_cm, &nil1, &nil2);
#endif
}

//------------------------------------------------------------------------------

namespace Mylib
{

enum LogLevel
{
	LogLevel_Debug = 0,
	LogLevel_Info,
	LogLevel_Warn,
	LogLevel_Error,
	LogLevel_Fatal
};

class Logger
{
public:
	explicit Logger(const TCHAR *dirPath, const TCHAR *logName);
	explicit Logger(const tstring &dirPath, const tstring &logName)
		: Logger(dirPath.c_str(), logName.c_str()) {}

	inline bool IsEnabled() { return _isEnabled; }
	inline void IsEnabled(bool isEnabled) { _isEnabled = isEnabled; }
	inline LogLevel Level() { return _level; }
	inline void Level(LogLevel level) { _level = level; }

	void Write(LogLevel level, const TCHAR *functionName, const TCHAR *msgFormat, ...);

private:
	tstring		_logDirPath;
	tstring		_logPath;
	bool		_isEnabled = false;
	LogLevel	_level = LogLevel_Warn;
	std::mutex	_lockObject;
};

//------------------------------------------------------------------------------

class Timer
{
public:
	inline static int64_t Frequency()
	{
		EnsureInitialized();
		return _frequency;
	}

	inline static bool IsHighResolutionSupported()
	{
		EnsureInitialized();
		return _isHighResolutionSupported;
	}

	inline static int64_t TickCount()
	{
#if defined(_WIN32)
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return counter.QuadPart;
#else
		return (int64_t)clock();
#endif
	}

	inline static double ElapsedTimeInSec(int64_t tickCount)
	{
		EnsureInitialized();
		return (double)tickCount / _frequency;
	}

	inline static int64_t ElapsedTimeInMillisec(int64_t tickCount)
	{
		EnsureInitialized();
		return (_frequency != 1000) ? (tickCount * 1000) / _frequency : tickCount;
	}

	inline static int64_t ElapsedTimeInMicrosec(int64_t tickCount)
	{
		EnsureInitialized();
		return (tickCount * 1000000) / _frequency;
	}

private:
	static int64_t		_frequency;	// = 0
	static bool			_isHighResolutionSupported;
	static std::mutex	_lockObject;

	static void EnsureInitialized();
};

//------------------------------------------------------------------------------

class String
{
public:
	inline static tstring Empty() { return tstring(); }
};

//------------------------------------------------------------------------------

class Path
{
public:
	static tstring Combine(const TCHAR *path1, const TCHAR *path2);
	inline static tstring Combine(const tstring &path1, const tstring &path2)
	{
		return Combine(path1.c_str(), path2.c_str());
	}
	inline static tstring Combine(const TCHAR *path1, const TCHAR *path2, const TCHAR *path3)
	{
		tstring tmp = Combine(path1, path2);
		return Combine(tmp.c_str(), path3);
	}
	inline static tstring Combine(const tstring &path1, const tstring &path2, const tstring &path3)
	{
		tstring tmp = Combine(path1, path2);
		return Combine(tmp, path3);
	}

	inline static tstring GetDirectoryName(const TCHAR *path) { return GetDirectoryName(tstring(path)); }
	static tstring GetDirectoryName(const tstring &path);

private:
#if defined(_WIN32)
	static const TCHAR Delimiter = _T('\\');
#else
	static const TCHAR Delimiter = _T('/');
#endif
};

//------------------------------------------------------------------------------

class Directory
{
public:
	static bool Create(/*[in]*/const TCHAR *path, /*[out]*/void *nativeErrInfo = nullptr);
	inline static bool Create(/*[in]*/const tstring &path, /*[out]*/void *nativeErrInfo = nullptr)
	{
		return Create(path.c_str(), nativeErrInfo);
	}

	static bool Exists(const TCHAR *path);
	inline static bool Exists(const tstring &path) { return Exists(path.c_str()); }
};

//------------------------------------------------------------------------------

class File
{
public:
	static bool Exists(const TCHAR *path);
	inline static bool Exists(const tstring &path) { return Exists(path.c_str()); }
};

//------------------------------------------------------------------------------

class Encoding
{
public:
	inline Encoding(int codePage) { _codePage = (codePage == CP_ACP) ? GetACP() : codePage; }

	inline std::string GetString(/*[in]*/const wchar_t *s, /*[out]*/void *nativeErrInfo = nullptr)
	{
		return GetString(std::wstring(s), nativeErrInfo);
	}
	std::string GetString(/*[in]*/const std::wstring &s, /*[out]*/void *nativeErrInfo = nullptr);
	inline std::wstring GetString(/*[in]*/const char *s, /*[out]*/void *nativeErrInfo = nullptr)
	{
		return GetString(std::string(s), nativeErrInfo);
	}
	std::wstring GetString(/*[in]*/const std::string &s, /*[out]*/void *nativeErrInfo = nullptr);

	inline static Encoding Default() { return _defaultEncoding; }
	inline static Encoding UTF8() { return _utf8Encoding; }

	inline int CodePage() { return _codePage; }

private:
	static Encoding _defaultEncoding;
	static Encoding _utf8Encoding;

	int _codePage;
};

}// namespace Mylib

#endif MYLIB_H
