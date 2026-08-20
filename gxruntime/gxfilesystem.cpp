#include "std.h"
#include "gxfilesystem.h"
#include "gxutf8.h"

static std::set<gxDir*> dir_set;

gxFileSystem::gxFileSystem() {
	dir_set.clear();
}

gxFileSystem::~gxFileSystem() {
	while(dir_set.size()) closeDir(*dir_set.begin());
}

bool gxFileSystem::createDir(const std::string& dir) {
	return CreateDirectoryW(UTF8::toWide(dir).c_str(), 0) ? true : false;
}

bool gxFileSystem::deleteDir(const std::string& dir) {
	return RemoveDirectoryW(UTF8::toWide(dir).c_str()) ? true : false;
}

bool gxFileSystem::createFile(const std::string& file) {
	HANDLE h = CreateFileW(UTF8::toWide(file).c_str(),
		GENERIC_ALL,
		0,
		0,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		0);
	if(h != INVALID_HANDLE_VALUE) {
		CloseHandle(h);
		return true;
	}
	else return false;
}

bool gxFileSystem::deleteFile(const std::string& file) {
	return DeleteFileW(UTF8::toWide(file).c_str()) ? true : false;
}

bool gxFileSystem::copyFile(const std::string& src, const std::string& dest) {
	return CopyFileW(UTF8::toWide(src).c_str(), UTF8::toWide(dest).c_str(), false) ? true : false;
}

bool gxFileSystem::renameFile(const std::string& src, const std::string& dest) {
	return MoveFileW(UTF8::toWide(src).c_str(), UTF8::toWide(dest).c_str()) ? true : false;
}

bool gxFileSystem::setCurrentDir(const std::string& dir) {
	return SetCurrentDirectoryW(UTF8::toWide(dir).c_str()) ? true : false;
}

std::string gxFileSystem::getCurrentDir()const {
	wchar_t buff[MAX_PATH];
	if(!GetCurrentDirectoryW(MAX_PATH, buff)) return "";
	std::string t = UTF8::fromWide(buff); if(t.size() && t[t.size() - 1] != '\\') t += '\\';
	return t;
}

int gxFileSystem::getFileSize(const std::string& name)const {
	std::filesystem::path p = std::filesystem::path(UTF8::toWide(name));
	return std::filesystem::exists(p) ? (int)std::filesystem::file_size(p) : 0;
}

int gxFileSystem::getFileType(const std::string& name)const {
	DWORD t = GetFileAttributesW(UTF8::toWide(name).c_str());
	return t == INVALID_FILE_ATTRIBUTES ? FILE_TYPE_NONE :
		(t & FILE_ATTRIBUTE_DIRECTORY ? FILE_TYPE_DIR : FILE_TYPE_FILE);
}

gxDir* gxFileSystem::openDir(const std::string& name, int flags) {
	std::wstring t = UTF8::toWide(name);
	if(!t.empty() && t.back() == L'\\') t += L"*";
	else t += L"\\*";
	WIN32_FIND_DATAW f;
	HANDLE h = FindFirstFileW(t.c_str(), &f);
	if(h != INVALID_HANDLE_VALUE) {
		gxDir* d = new gxDir(h, f);
		dir_set.insert(d);
		return d;
	}
	return 0;
}
gxDir* gxFileSystem::verifyDir(gxDir* d) {
	return dir_set.count(d) ? d : 0;
}

void gxFileSystem::closeDir(gxDir* d) {
	if(dir_set.erase(d)) delete d;
}