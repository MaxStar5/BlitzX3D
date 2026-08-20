#include "std.h"
#include "gxdir.h"
#include "gxutf8.h"

gxDir::gxDir(HANDLE h, const WIN32_FIND_DATAW& f) :handle(h), findData(f) {
}

gxDir::~gxDir() {
	if(handle != INVALID_HANDLE_VALUE) FindClose(handle);
}

std::string gxDir::getNextFile() {
	if(handle == INVALID_HANDLE_VALUE) return "";
	std::string t = UTF8::fromWide(findData.cFileName);
	if(!FindNextFileW(handle, &findData)) {
		FindClose(handle);
		handle = INVALID_HANDLE_VALUE;
	}
	return t;
}