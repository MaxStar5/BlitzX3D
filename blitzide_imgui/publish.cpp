#include "publish.h"

#if defined(_WIN32)
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

#pragma pack(push, 2)
typedef struct {
	WORD idReserved;
	WORD idType;
	WORD idCount;
} ICONDIR;

typedef struct {
	BYTE bWidth;
	BYTE bHeight;
	BYTE bColorCount;
	BYTE bReserved;
	WORD wPlanes;
	WORD wBitCount;
	DWORD dwBytesInRes;
	DWORD dwImageOffset;
} ICONDIRENTRY;

typedef struct {
	WORD idReserved;
	WORD idType;
	WORD idCount;
} GRPICONDIR;

typedef struct {
	BYTE  bWidth;
	BYTE  bHeight;
	BYTE  bColorCount;
	BYTE  bReserved;
	WORD  wPlanes;
	WORD  wBitCount;
	DWORD dwBytesInRes;
	WORD  nID;
} GRPICONDIRENTRY;
#pragma pack(pop)

static BOOL CALLBACK EnumLangDeleteCb(HMODULE, LPCSTR aType, LPCSTR aName, LANGID aLang, LONG_PTR aParam) {
	HANDLE hUpdate = (HANDLE)aParam;
	UpdateResourceA(hUpdate, aType, aName, aLang, NULL, 0);
	return TRUE;
}

static BOOL CALLBACK EnumNameDeleteCb(HMODULE aModule, LPCSTR aType, LPSTR aName, LONG_PTR aParam) {
	EnumResourceLanguagesA(aModule, aType, aName, EnumLangDeleteCb, aParam);
	return TRUE;
}

static void RemoveExistingIcons(HANDLE aUpdate, const std::string& aExePath) {
	HMODULE hMod = LoadLibraryExA(aExePath.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (!hMod) return;
	EnumResourceNamesA(hMod, RT_ICON, EnumNameDeleteCb, (LONG_PTR)aUpdate);
	EnumResourceNamesA(hMod, RT_GROUP_ICON, EnumNameDeleteCb, (LONG_PTR)aUpdate);
	FreeLibrary(hMod);
}

bool applyIconToExe(const std::string& exePath, const std::string& icoPath) {
	HANDLE hFile = CreateFileA(icoPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	ICONDIR dir;
	DWORD bytesRead;
	if (!ReadFile(hFile, &dir, sizeof(dir), &bytesRead, NULL) || bytesRead != sizeof(dir)) {
		CloseHandle(hFile);
		return false;
	}
	if (dir.idReserved != 0 || dir.idType != 1 || dir.idCount == 0) {
		CloseHandle(hFile);
		return false;
	}

	std::vector<ICONDIRENTRY> entries(dir.idCount);
	DWORD entrySize = sizeof(ICONDIRENTRY) * dir.idCount;
	if (!ReadFile(hFile, entries.data(), entrySize, &bytesRead, NULL) || bytesRead != entrySize) {
		CloseHandle(hFile);
		return false;
	}

	std::vector<std::vector<BYTE>> images(dir.idCount);
	for (WORD i = 0; i < dir.idCount; ++i) {
		if (SetFilePointer(hFile, (LONG)entries[i].dwImageOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
			CloseHandle(hFile);
			return false;
		}
		images[i].resize(entries[i].dwBytesInRes);
		if (!ReadFile(hFile, images[i].data(), entries[i].dwBytesInRes, &bytesRead, NULL) || bytesRead != entries[i].dwBytesInRes) {
			CloseHandle(hFile);
			return false;
		}
	}
	CloseHandle(hFile);

	HANDLE hUpdate = BeginUpdateResourceA(exePath.c_str(), FALSE);
	if (!hUpdate) return false;

	RemoveExistingIcons(hUpdate, exePath);

	const LANGID lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
	bool success = true;

	for (WORD i = 0; i < dir.idCount; ++i) {
		WORD iconId = i + 1;
		if (!UpdateResourceA(hUpdate, RT_ICON, MAKEINTRESOURCEA(iconId), lang,
			images[i].data(), (DWORD)images[i].size())) {
			success = false;
		}
	}

	GRPICONDIR grpDir;
	grpDir.idReserved = 0;
	grpDir.idType = 1;
	grpDir.idCount = dir.idCount;

	std::vector<BYTE> groupData(sizeof(GRPICONDIR) + sizeof(GRPICONDIRENTRY) * dir.idCount);
	memcpy(groupData.data(), &grpDir, sizeof(GRPICONDIR));

	GRPICONDIRENTRY* grpEntries = (GRPICONDIRENTRY*)(groupData.data() + sizeof(GRPICONDIR));
	for (WORD i = 0; i < dir.idCount; ++i) {
		grpEntries[i].bWidth = entries[i].bWidth;
		grpEntries[i].bHeight = entries[i].bHeight;
		grpEntries[i].bColorCount = entries[i].bColorCount;
		grpEntries[i].bReserved = entries[i].bReserved;
		grpEntries[i].wPlanes = entries[i].wPlanes;
		grpEntries[i].wBitCount = entries[i].wBitCount;
		grpEntries[i].dwBytesInRes = entries[i].dwBytesInRes;
		grpEntries[i].nID = i + 1;
	}

	if (!UpdateResourceA(hUpdate, RT_GROUP_ICON, MAKEINTRESOURCEA(1), lang,
		groupData.data(), (DWORD)groupData.size())) {
		success = false;
	}

	if (!EndUpdateResourceA(hUpdate, !success)) return false;

	return success;
}
#else
bool applyIconToExe(const std::string&, const std::string&) {
	return true;
}
#endif
