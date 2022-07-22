#include "pch.h"
#include "utility.h"

std::string ToUTF8(const std::string& _strAnsi)
{
	std::wstring strUni;
	strUni.resize(_strAnsi.size());
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _strAnsi.c_str(), _strAnsi.size(), strUni.data(), strUni.size());

	std::string strUtf8;
	int nLen = WideCharToMultiByte(CP_UTF8, 0, strUni.c_str(), lstrlenW(strUni.c_str()), NULL, 0, NULL, NULL);
	strUtf8.resize(nLen);
	WideCharToMultiByte(CP_UTF8, 0, strUni.c_str(), lstrlenW(strUni.c_str()), strUtf8.data(), nLen, NULL, NULL);

	return strUtf8;
}