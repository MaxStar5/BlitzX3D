#ifndef B3D_THEME_H
#define B3D_THEME_H

#include <string>
#include <vector>

#include "imgui/imgui.h"

struct ThemeSpec {
	std::string name;
	ImVec4 bg, panel, surface, text, accent;
	int editor[13][3];
};

struct UserTheme {
	std::string name;
	int bg[3], panel[3], surface[3], text[3], accent[3];
	int editor[13][3];
};

static const int ThemeEditorColorCount = 13;

int themeBuiltinCount();
const ThemeSpec* themeBuiltin(int i);
int themeUserCount();
const UserTheme* themeUser(int i);

bool themeFind(const std::string& name, bool* outIsUser, int* outIndex);

void themeLoadUserThemes(const std::string& path);
void themeSaveUserThemes(const std::string& path);
void themeAddUserTheme(const UserTheme& t);
void themeRemoveUserTheme(const std::string& name);

void themeApplyStyle(const std::string& name, float rounding, float alpha);

bool themeEditorColors(const std::string& name, int out[13][3]);

void themeCapture(UserTheme& t, const int editor[13][3]);
void themeSetUserEditorColors(const std::string& name, const int editor[13][3]);

#endif