#include "theme.h"

#include "inipp/inipp.h"

#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>

static ImVec4 colf(int r, int g, int b, int a = 255) {
	return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

static ImVec4 col3(const int* rgb) {
	return colf(rgb[0], rgb[1], rgb[2]);
}

static ImVec4 mixc(const ImVec4& a, const ImVec4& b, float t) {
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static ImVec4 scalec(const ImVec4& c, float f) {
	return ImVec4(c.x * f, c.y * f, c.z * f, c.w * f);
}

static void applyStyleBase(const ImVec4& bg, const ImVec4& panel, const ImVec4& surface, const ImVec4& text, const ImVec4& accent, float rounding, float alpha) {
	ImGuiStyle& s = ImGui::GetStyle();
	s.WindowRounding = rounding;
	s.FrameRounding = rounding;
	s.ChildRounding = rounding;
	s.PopupRounding = rounding;
	s.ScrollbarRounding = rounding;
	s.TabRounding = rounding;
	s.GrabRounding = rounding;
	s.WindowBorderSize = 1.0f;
	s.FrameBorderSize = 0.0f;
	s.TabBorderSize = 0.0f;
	s.WindowPadding = ImVec2(6, 6);
	s.FramePadding = ImVec2(6, 4);
	s.ItemSpacing = ImVec2(6, 5);
	s.ItemInnerSpacing = ImVec2(6, 5);
	s.IndentSpacing = 20.0f;
	s.ScrollbarSize = 14.0f;
	s.Alpha = alpha;

	ImVec4* c = s.Colors;
	const ImVec4 textDisabled = scalec(text, 0.55f);
	const ImVec4 hover = mixc(surface, text, 0.10f);
	const ImVec4 active = mixc(surface, text, 0.18f);
	const ImVec4 border = mixc(surface, text, 0.17f);
	c[ImGuiCol_Text] = text;
	c[ImGuiCol_TextDisabled] = textDisabled;
	c[ImGuiCol_WindowBg] = bg;
	c[ImGuiCol_ChildBg] = panel;
	c[ImGuiCol_PopupBg] = bg;
	c[ImGuiCol_Border] = border;
	c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_FrameBg] = surface;
	c[ImGuiCol_FrameBgHovered] = hover;
	c[ImGuiCol_FrameBgActive] = active;
	c[ImGuiCol_TitleBg] = surface;
	c[ImGuiCol_TitleBgActive] = mixc(surface, text, 0.06f);
	c[ImGuiCol_TitleBgCollapsed] = surface;
	c[ImGuiCol_MenuBarBg] = panel;
	c[ImGuiCol_ScrollbarBg] = surface;
	c[ImGuiCol_ScrollbarGrab] = mixc(surface, text, 0.21f);
	c[ImGuiCol_ScrollbarGrabHovered] = mixc(surface, text, 0.30f);
	c[ImGuiCol_ScrollbarGrabActive] = mixc(surface, text, 0.38f);
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = mixc(surface, text, 0.38f);
	c[ImGuiCol_SliderGrabActive] = mixc(surface, text, 0.46f);
	c[ImGuiCol_Button] = mixc(surface, text, 0.11f);
	c[ImGuiCol_ButtonHovered] = mixc(surface, text, 0.21f);
	c[ImGuiCol_ButtonActive] = mixc(surface, text, 0.28f);
	c[ImGuiCol_Header] = mixc(panel, text, 0.09f);
	c[ImGuiCol_HeaderHovered] = mixc(panel, text, 0.14f);
	c[ImGuiCol_HeaderActive] = mixc(panel, text, 0.20f);
	c[ImGuiCol_Separator] = border;
	c[ImGuiCol_SeparatorHovered] = mixc(border, text, 0.2f);
	c[ImGuiCol_SeparatorActive] = mixc(border, text, 0.35f);
	c[ImGuiCol_ResizeGrip] = border;
	c[ImGuiCol_ResizeGripHovered] = mixc(border, accent, 0.5f);
	c[ImGuiCol_ResizeGripActive] = accent;
	c[ImGuiCol_Tab] = mixc(surface, text, 0.06f);
	c[ImGuiCol_TabHovered] = mixc(surface, text, 0.16f);
	c[ImGuiCol_TabSelected] = mixc(surface, text, 0.11f);
	c[ImGuiCol_TabDimmed] = mixc(surface, text, 0.03f);
	c[ImGuiCol_TabDimmedSelected] = mixc(surface, text, 0.09f);
	c[ImGuiCol_TabSelectedOverline] = accent;
	c[ImGuiCol_TextSelectedBg] = mixc(bg, accent, 0.3f);
	c[ImGuiCol_NavHighlight] = accent;
	c[ImGuiCol_NavWindowingHighlight] = text;
}

static const ThemeSpec gThemes[] = {
	{ "Classic Dark", colf(32, 32, 36), colf(28, 28, 31), colf(24, 24, 27), colf(220, 220, 220), colf(120, 190, 240),
		{ { 34, 85, 136 }, { 0, 255, 102 }, { 255, 255, 255 }, { 170, 255, 255 }, { 255, 238, 0 }, { 51, 255, 221 }, { 238, 238, 238 } } },
	{ "Classic Light", colf(238, 238, 242), colf(250, 250, 252), colf(255, 255, 255), colf(40, 40, 40), colf(0, 120, 215),
		{ { 248, 248, 248 }, { 0, 128, 64 }, { 40, 40, 40 }, { 0, 0, 200 }, { 0, 128, 0 }, { 160, 64, 0 }, { 20, 20, 20 } } },
	{ "Classic", colf(31, 31, 31), colf(42, 42, 42), colf(48, 48, 48), colf(255, 255, 255), colf(97, 139, 191),
		{ { 30, 30, 30 }, { 106, 255, 132 }, { 220, 220, 220 }, { 86, 156, 214 }, { 106, 153, 85 }, { 181, 206, 168 }, { 212, 212, 212 } } },
	{ "BlitzPro", colf(32, 32, 34), colf(24, 24, 26), colf(20, 20, 22), colf(220, 220, 214), colf(230, 125, 163),
		{ { 30, 30, 30 }, { 240, 155, 135 }, { 220, 220, 204 }, { 230, 125, 163 }, { 87, 166, 74 }, { 50, 235, 135 }, { 238, 238, 238 } } },
	{ "One Dark", colf(40, 44, 52), colf(33, 37, 43), colf(48, 53, 63), colf(171, 178, 191), colf(97, 175, 239),
		{ { 40, 44, 52 }, { 152, 195, 121 }, { 224, 228, 235 }, { 198, 120, 221 }, { 92, 99, 112 }, { 209, 154, 102 }, { 171, 178, 191 } } },
	{ "Gruvbox Dark", colf(40, 40, 40), colf(29, 32, 33), colf(50, 48, 47), colf(235, 219, 178), colf(215, 153, 33),
		{ { 40, 40, 40 }, { 184, 187, 38 }, { 235, 219, 178 }, { 204, 36, 29 }, { 146, 131, 116 }, { 214, 93, 14 }, { 235, 219, 178 } } },
	{ "Solarized Dark", colf(0, 43, 54), colf(7, 54, 66), colf(0, 58, 73), colf(147, 161, 161), colf(38, 139, 210),
		{ { 0, 43, 54 }, { 133, 153, 0 }, { 211, 54, 130 }, { 181, 137, 0 }, { 108, 113, 196 }, { 220, 50, 47 }, { 147, 161, 161 } } },
	{ "Dracula", colf(40, 42, 54), colf(33, 34, 44), colf(55, 57, 71), colf(248, 248, 242), colf(189, 147, 249),
		{ { 40, 42, 54 }, { 241, 250, 140 }, { 248, 248, 242 }, { 255, 121, 198 }, { 98, 114, 164 }, { 139, 233, 253 }, { 248, 248, 242 } } },
	{ "Nord", colf(46, 52, 64), colf(59, 66, 82), colf(67, 76, 94), colf(216, 222, 233), colf(136, 192, 208),
		{ { 46, 52, 64 }, { 163, 190, 140 }, { 216, 222, 233 }, { 129, 161, 193 }, { 76, 86, 106 }, { 208, 135, 112 }, { 216, 222, 233 } } },
	{ "Catppuccin", colf(30, 30, 46), colf(24, 24, 37), colf(49, 49, 68), colf(205, 214, 244), colf(180, 190, 254),
		{ { 30, 30, 46 }, { 166, 227, 161 }, { 205, 214, 244 }, { 203, 166, 247 }, { 108, 112, 134 }, { 250, 179, 135 }, { 205, 214, 244 } } },
};

static const int gThemeBuiltinCount = (int)(sizeof(gThemes) / sizeof(gThemes[0]));

static std::vector<UserTheme> gUserThemes;
static std::string gUserThemesPath;

static void parseColor(const std::string& s, int* rgb) {
	rgb[0] = rgb[1] = rgb[2] = 0;
	sscanf(s.c_str(), "%d %d %d", &rgb[0], &rgb[1], &rgb[2]);
}

static std::string colorToString(const int* rgb) {
	char b[64];
	sprintf(b, "%d %d %d", rgb[0], rgb[1], rgb[2]);
	return b;
}

int themeBuiltinCount() { return gThemeBuiltinCount; }

const ThemeSpec* themeBuiltin(int i) {
	return (i >= 0 && i < gThemeBuiltinCount) ? &gThemes[i] : &gThemes[0];
}

int themeUserCount() { return (int)gUserThemes.size(); }

const UserTheme* themeUser(int i) {
	return (i >= 0 && i < (int)gUserThemes.size()) ? &gUserThemes[i] : nullptr;
}

bool themeFind(const std::string& name, bool* outIsUser, int* outIndex) {
	for (int i = 0; i < gThemeBuiltinCount; ++i) {
		if (name == gThemes[i].name) {
			*outIsUser = false;
			*outIndex = i;
			return true;
		}
	}
	for (int i = 0; i < (int)gUserThemes.size(); ++i) {
		if (name == gUserThemes[i].name) {
			*outIsUser = true;
			*outIndex = i;
			return true;
		}
	}
	return false;
}

void themeLoadUserThemes(const std::string& path) {
	gUserThemesPath = path;
	gUserThemes.clear();
	if (path.empty()) return;
	std::ifstream in(path.c_str(), std::ios::in);
	if (!in.good()) return;
	inipp::Ini<char> ini;
	ini.parse(in);
	in.close();

	for (auto& sec : ini.sections) {
		if (sec.first.rfind("theme_", 0) != 0) continue;
		UserTheme t;
		if (!inipp::get_value(sec.second, "Name", t.name)) continue;
		std::string v;
		if (inipp::get_value(sec.second, "Bg", v)) parseColor(v, t.bg);
		if (inipp::get_value(sec.second, "Panel", v)) parseColor(v, t.panel);
		if (inipp::get_value(sec.second, "Surface", v)) parseColor(v, t.surface);
		if (inipp::get_value(sec.second, "Text", v)) parseColor(v, t.text);
		if (inipp::get_value(sec.second, "Accent", v)) parseColor(v, t.accent);
		if (inipp::get_value(sec.second, "Background", v)) parseColor(v, t.editor[0]);
		if (inipp::get_value(sec.second, "String", v)) parseColor(v, t.editor[1]);
		if (inipp::get_value(sec.second, "Ident", v)) parseColor(v, t.editor[2]);
		if (inipp::get_value(sec.second, "Keyword", v)) parseColor(v, t.editor[3]);
		if (inipp::get_value(sec.second, "Comment", v)) parseColor(v, t.editor[4]);
		if (inipp::get_value(sec.second, "Digit", v)) parseColor(v, t.editor[5]);
		if (inipp::get_value(sec.second, "Default", v)) parseColor(v, t.editor[6]);
		gUserThemes.push_back(t);
	}
}

void themeSaveUserThemes(const std::string& path) {
	if (path.empty()) return;
	inipp::Ini<char> ini;
	for (size_t i = 0; i < gUserThemes.size(); ++i) {
		const UserTheme& t = gUserThemes[i];
		auto& sec = ini.sections["theme_" + std::to_string(i + 1)];
		sec.insert(std::make_pair("Name", t.name));
		sec.insert(std::make_pair("Bg", colorToString(t.bg)));
		sec.insert(std::make_pair("Panel", colorToString(t.panel)));
		sec.insert(std::make_pair("Surface", colorToString(t.surface)));
		sec.insert(std::make_pair("Text", colorToString(t.text)));
		sec.insert(std::make_pair("Accent", colorToString(t.accent)));
		sec.insert(std::make_pair("Background", colorToString(t.editor[0])));
		sec.insert(std::make_pair("String", colorToString(t.editor[1])));
		sec.insert(std::make_pair("Ident", colorToString(t.editor[2])));
		sec.insert(std::make_pair("Keyword", colorToString(t.editor[3])));
		sec.insert(std::make_pair("Comment", colorToString(t.editor[4])));
		sec.insert(std::make_pair("Digit", colorToString(t.editor[5])));
		sec.insert(std::make_pair("Default", colorToString(t.editor[6])));
	}
	std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
	if (!out.good()) return;
	ini.generate(out);
}

void themeAddUserTheme(const UserTheme& t) {
	for (auto& e : gUserThemes) {
		if (e.name == t.name) {
			e = t;
			themeSaveUserThemes(gUserThemesPath);
			return;
		}
	}
	gUserThemes.push_back(t);
	themeSaveUserThemes(gUserThemesPath);
}

void themeRemoveUserTheme(const std::string& name) {
	for (auto it = gUserThemes.begin(); it != gUserThemes.end(); ++it) {
		if (it->name == name) {
			gUserThemes.erase(it);
			themeSaveUserThemes(gUserThemesPath);
			return;
		}
	}
}

void themeApplyStyle(const std::string& name, float rounding, float alpha) {
	bool isUser = false;
	int idx = 0;
	if (themeFind(name, &isUser, &idx)) {
		if (isUser) {
			const UserTheme& t = gUserThemes[idx];
			applyStyleBase(col3(t.bg), col3(t.panel), col3(t.surface), col3(t.text), col3(t.accent), rounding, alpha);
		}
		else {
			const ThemeSpec& t = gThemes[idx];
			applyStyleBase(t.bg, t.panel, t.surface, t.text, t.accent, rounding, alpha);
		}
		return;
	}
	const ThemeSpec& t = gThemes[0];
	applyStyleBase(t.bg, t.panel, t.surface, t.text, t.accent, rounding, alpha);
}

bool themeEditorColors(const std::string& name, int out[7][3]) {
	bool isUser = false;
	int idx = 0;
	if (!themeFind(name, &isUser, &idx)) return false;
	if (isUser) {
		memcpy(out, gUserThemes[idx].editor, sizeof(gUserThemes[idx].editor));
	}
	else {
		memcpy(out, gThemes[idx].editor, sizeof(gThemes[idx].editor));
	}
	return true;
}

void themeCapture(UserTheme& t, const int editor[7][3]) {
	ImGuiStyle& s = ImGui::GetStyle();
	auto to3 = [](const ImVec4& c, int* rgb) {
		rgb[0] = (int)(c.x * 255.0f + 0.5f);
		rgb[1] = (int)(c.y * 255.0f + 0.5f);
		rgb[2] = (int)(c.z * 255.0f + 0.5f);
	};
	to3(s.Colors[ImGuiCol_WindowBg], t.bg);
	to3(s.Colors[ImGuiCol_ChildBg], t.panel);
	to3(s.Colors[ImGuiCol_FrameBg], t.surface);
	to3(s.Colors[ImGuiCol_Text], t.text);
	to3(s.Colors[ImGuiCol_CheckMark], t.accent);
	memcpy(t.editor, editor, sizeof(t.editor));
}