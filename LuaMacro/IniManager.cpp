// last update: 20160501

#include "IniManager.h"

#include "simpleini/SimpleIni.h"

using namespace Mylib;

//------------------------------------------------------------------------------

class IniManager::Impl	// wrapper of SimpleIni library
{
public:
	Impl() : _ini() {}	// initialize SimpleIni to treating file encoding as system locale, not allow same name of keys, not multi-line in value

	inline int LoadFile(const TCHAR *filePath) { return _ini.LoadFile(filePath); }
	inline int SaveFile(const TCHAR *filePath) { return _ini.SaveFile(filePath); }

	inline const TCHAR * GetValue(const TCHAR *section, const TCHAR *key, const TCHAR *default) { return _ini.GetValue(section, key, default); }
	inline long GetLongValue(const TCHAR *section, const TCHAR *key, long default) { return _ini.GetLongValue(section, key, default); }
	inline double GetDoubleValue(const TCHAR *section, const TCHAR *key, double default) { return _ini.GetDoubleValue(section, key, default); }
	inline bool GetBoolValue(const TCHAR *section, const TCHAR *key, bool default) { return _ini.GetBoolValue(section, key, default); }

	inline int SetValue(const TCHAR *section, const TCHAR *key, const TCHAR *value) { return _ini.SetValue(section, key, value); }
	inline int SetLongValue(const TCHAR *section, const TCHAR *key, long value, bool isHex) { return _ini.SetLongValue(section, key, value, nullptr, isHex); }
	inline int SetDoubleValue(const TCHAR *section, const TCHAR *key, double value) { return _ini.SetDoubleValue(section, key, value); }
	inline int SetBoolValue(const TCHAR *section, const TCHAR *key, bool value) { return _ini.SetBoolValue(section, key, value); }

	inline bool Delete(const TCHAR *section, const TCHAR *key) { return _ini.Delete(section, key); }

private:
	CSimpleIniCase _ini;	// case sensitive to section and key names
};

// Both constructor and destructor should be defined after the body of class Impl, or you would get compiler warnings and errors.
IniManager::IniManager() : _pimpl{ std::make_unique<Impl>() } {}
IniManager::~IniManager() {}

int IniManager::LoadFile(const TCHAR *filePath) { return _pimpl->LoadFile(filePath); }
int IniManager::SaveFile(const TCHAR *filePath) { return _pimpl->SaveFile(filePath); }

const TCHAR * IniManager::GetValue(const TCHAR *section, const TCHAR *key, const TCHAR *default) { return _pimpl->GetValue(section, key, default); }
long IniManager::GetLongValue(const TCHAR *section, const TCHAR *key, long default) { return _pimpl->GetLongValue(section, key, default); }
double IniManager::GetDoubleValue(const TCHAR *section, const TCHAR *key, double default) { return _pimpl->GetDoubleValue(section, key, default); }
bool IniManager::GetBoolValue(const TCHAR *section, const TCHAR *key, bool default) { return _pimpl->GetBoolValue(section, key, default); }

int IniManager::SetValue(const TCHAR *section, const TCHAR *key, const TCHAR *value) { return _pimpl->SetValue(section, key, value); }
int IniManager::SetLongValue(const TCHAR *section, const TCHAR *key, long value, bool isHex) { return _pimpl->SetLongValue(section, key, value, isHex); }
int IniManager::SetDoubleValue(const TCHAR *section, const TCHAR *key, double value) { return _pimpl->SetDoubleValue(section, key, value); }
int IniManager::SetBoolValue(const TCHAR *section, const TCHAR *key, bool value) { return _pimpl->SetBoolValue(section, key, value); }

bool IniManager::DeleteKey(const TCHAR *section, const TCHAR *key) { return _pimpl->Delete(section, key); }
