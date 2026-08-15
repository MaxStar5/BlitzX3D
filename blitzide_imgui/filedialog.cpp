#include "filedialog.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#else
#include <unistd.h>
#endif

bool fileOpenDialog(std::string& path, const char* filter) {
#if defined(_WIN32)
	OPENFILENAMEA ofn = { sizeof(ofn) };
	char buf[MAX_PATH] = { 0 };
	ofn.lpstrFilter = "Blitz source (*.bb)\0*.bb\0IDEal project (*.ipf)\0*.ipf\0All files (*.*)\0*.*\0";
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if (!GetOpenFileNameA(&ofn)) return false;
	path = buf;
	return true;
#else
	std::string cmd = "zenity --file-selection --file-filter='Blitz source (*.bb)' --file-filter='IDEal project (*.ipf)' --file-filter='All files (*)' 2>/dev/null";
	std::string out;
	FILE* p = popen(cmd.c_str(), "r");
	if (!p) return false;
	char buf[4096];
	size_t n = fread(buf, 1, sizeof(buf) - 1, p);
	buf[n] = 0;
	int rc = pclose(p);
	if (rc != 0 || n == 0) return false;
	std::string s(buf);
	while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
	if (s.empty()) return false;
	path = s;
	return true;
#endif
}

bool fileSaveDialog(std::string& path, const char* defaultName, const char* filter) {
#if defined(_WIN32)
	OPENFILENAMEA ofn = { sizeof(ofn) };
	char buf[MAX_PATH] = { 0 };
	if (defaultName) strncpy(buf, defaultName, MAX_PATH - 1);
	ofn.lpstrFilter = "Blitz source (*.bb)\0*.bb\0All files (*.*)\0*.*\0";
	ofn.lpstrDefExt = "bb";
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	if (!GetSaveFileNameA(&ofn)) return false;
	path = buf;
	return true;
#else
	std::string cmd = "zenity --file-selection --save --confirm-overwrite --file-filter='Blitz source (*.bb)' --file-filter='All files (*)' 2>/dev/null";
	if (defaultName && *defaultName) {
		cmd += " --filename='" + std::string(defaultName) + "'";
	}
	std::string out;
	FILE* p = popen(cmd.c_str(), "r");
	if (!p) return false;
	char buf[4096];
	size_t n = fread(buf, 1, sizeof(buf) - 1, p);
	buf[n] = 0;
	int rc = pclose(p);
	if (rc != 0 || n == 0) return false;
	std::string s(buf);
	while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
	if (s.empty()) return false;
	path = s;
	return true;
#endif
}
