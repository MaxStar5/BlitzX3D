#include "prefs.h"

#include "../inipp/inipp.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <windows.h>

Prefs prefs;

static std::string getExeDir() {
	char buf[MAX_PATH];
	DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
	if (!n) return "";
	return std::string(buf, n);
}

static std::string resolveHomeDir() {
	const char* p = std::getenv("blitzpath");
	if (p && *p) return p;

	std::string exe = getExeDir();
	if (exe.empty()) return "";
	std::filesystem::path ep(exe);
	std::filesystem::path home = ep.parent_path().parent_path();
	return home.string();
}

static void parseColor(const std::string& s, int* rgb) {
	rgb[0] = rgb[1] = rgb[2] = 0;
	sscanf(s.c_str(), "%d %d %d", &rgb[0], &rgb[1], &rgb[2]);
}

void Prefs::open() {
	homeDir = resolveHomeDir();
	if (homeDir.empty()) return;

	std::ifstream in((homeDir + "/cfg/blitzide.ini").c_str(), std::ios::in);
	if (!in.good()) return;
	in.seekg(0, std::ios::end);
	if (in.tellg() == 0) { in.close(); return; }
	in.seekg(0, std::ios::beg);

	inipp::Ini<char> ini;
	ini.parse(in);
	in.close();

	inipp::get_value(ini.sections["FONTS"], "DebugFont", font_debug);
	inipp::get_value(ini.sections["FONTS"], "DebugFontSize", font_debug_height);

	std::string c;
	inipp::get_value(ini.sections["COLORS"], "Background", c); parseColor(c, rgb_bkgrnd);
	inipp::get_value(ini.sections["COLORS"], "String", c); parseColor(c, rgb_string);
	inipp::get_value(ini.sections["COLORS"], "Ident", c); parseColor(c, rgb_ident);
	inipp::get_value(ini.sections["COLORS"], "Keyword", c); parseColor(c, rgb_keyword);
	inipp::get_value(ini.sections["COLORS"], "Comment", c); parseColor(c, rgb_comment);
	inipp::get_value(ini.sections["COLORS"], "Digit", c); parseColor(c, rgb_digit);
	inipp::get_value(ini.sections["COLORS"], "Default", c); parseColor(c, rgb_default);
}
