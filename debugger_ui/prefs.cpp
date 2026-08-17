#include "prefs.h"

#include "../theme.h"
#include "../inipp/inipp.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <SDL3/SDL.h>

Prefs prefs;

static std::string getExeDir() {
	const char* base = SDL_GetBasePath();
	if (!base) return "";
	std::string result(base);
	return result + "blitz_debugger.exe";
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

static std::string resolveConfigDir() {
	const char* ad = std::getenv("APPDATA");
	if (ad && *ad) {
		std::string dir = std::string(ad) + "/BlitzX3D";
		std::filesystem::create_directories(dir);
		return dir;
	}
	const char* ud = std::getenv("USERPROFILE");
	if (ud && *ud) {
		std::string dir = std::string(ud) + "/AppData/Roaming/BlitzX3D";
		std::filesystem::create_directories(dir);
		return dir;
	}
	return prefs.homeDir + "/cfg";
}

static void migrateLegacyConfig() {
	if (prefs.homeDir.empty() || prefs.configDir == prefs.homeDir + "/cfg") return;
	for (const char* name : { "blitzide.ini", "imgui.ini", "themes.ini" }) {
		std::filesystem::path src = std::filesystem::path(prefs.homeDir) / "cfg" / name;
		std::filesystem::path dst = std::filesystem::path(prefs.configDir) / name;
		if (!std::filesystem::exists(src)) continue;
		if (std::filesystem::exists(dst)) continue;
		std::error_code ec;
		std::filesystem::copy_file(src, dst, std::filesystem::copy_options::none, ec);
	}
}

static void parseColor(const std::string& s, int* rgb) {
	rgb[0] = rgb[1] = rgb[2] = 0;
	sscanf(s.c_str(), "%d %d %d", &rgb[0], &rgb[1], &rgb[2]);
}

void Prefs::open() {
	homeDir = resolveHomeDir();
	configDir = resolveConfigDir();

	migrateLegacyConfig();

	if (homeDir.empty()) return;

	std::ifstream in((configDir + "/blitzide.ini").c_str(), std::ios::in);
	if (!in.good()) return;
	in.seekg(0, std::ios::end);
	if (in.tellg() == 0) { in.close(); return; }
	in.seekg(0, std::ios::beg);

	inipp::Ini<char> ini;
	ini.parse(in);
	in.close();

	inipp::get_value(ini.sections["FONTS"], "DebugFont", font_debug);
	inipp::get_value(ini.sections["FONTS"], "DebugFontSize", font_debug_height);

	inipp::get_value(ini.sections["UI"], "Theme", theme);

	std::string c;
	inipp::get_value(ini.sections["COLORS"], "Background", c); parseColor(c, rgb_bkgrnd);
	inipp::get_value(ini.sections["COLORS"], "String", c); parseColor(c, rgb_string);
	inipp::get_value(ini.sections["COLORS"], "Ident", c); parseColor(c, rgb_ident);
	inipp::get_value(ini.sections["COLORS"], "Keyword", c); parseColor(c, rgb_keyword);
	inipp::get_value(ini.sections["COLORS"], "Comment", c); parseColor(c, rgb_comment);
	inipp::get_value(ini.sections["COLORS"], "Digit", c); parseColor(c, rgb_digit);
	inipp::get_value(ini.sections["COLORS"], "Default", c); parseColor(c, rgb_default);

	themeLoadUserThemes(configDir + "/themes.ini");
}
