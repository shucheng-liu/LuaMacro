// last update: 20160501

#ifndef INIMANAGER_H
#define INIMANAGER_H

#if defined(_WIN32)
	#include <tchar.h>
#else
// Other platform needs something which implements Windows tchar.h...
#endif
#include <memory>
#include <string>

typedef std::basic_string<TCHAR> tstring;

namespace Mylib
{

class IniManager
{
public:
	IniManager();
	~IniManager();

	int LoadFile(const TCHAR *filePath);
	inline int LoadFile(const tstring &filePath) { return LoadFile(filePath.c_str()); }
	int SaveFile(const TCHAR *filePath);
	inline int SaveFile(const tstring &filePath) { return SaveFile(filePath.c_str()); }

	const TCHAR * GetValue(const TCHAR *section, const TCHAR *key, const TCHAR *default = nullptr);
	long GetLongValue(const TCHAR *section, const TCHAR *key, long default = 0);
	double GetDoubleValue(const TCHAR *section, const TCHAR *key, double default = 0);
	bool GetBoolValue(const TCHAR *section, const TCHAR *key, bool default = false);

	int SetValue(const TCHAR *section, const TCHAR *key, const TCHAR *value);
	int SetLongValue(const TCHAR *section, const TCHAR *key, long value, bool isHex = false);
	int SetDoubleValue(const TCHAR *section, const TCHAR *key, double value);
	int SetBoolValue(const TCHAR *section, const TCHAR *key, bool value);

	bool DeleteKey(const TCHAR *section, const TCHAR *key);
	inline bool DeleteSection(const TCHAR *section) { return DeleteKey(section, nullptr); }

private:
	class Impl;
	std::unique_ptr<Impl>	_pimpl;
};

}

#endif INIMANAGER_H
