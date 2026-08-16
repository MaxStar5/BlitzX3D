#include "app.h"

#include "blitzlang.h"
#include "filedialog.h"
#include "publish.h"
#include "spawn.h"
#include "update.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl3.h"
#include "../imgui/backends/imgui_impl_opengl3_loader.h"

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

static App* g_app = nullptr;

static std::string toLower(const std::string& s) {
	std::string t = s;
	std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return t;
}

static bool startsWithWord(const std::string& s, const std::string& w) {
	if (s.size() < w.size()) return false;
	if (s.compare(0, w.size(), w) != 0) return false;
	if (s.size() == w.size()) return true;
	return std::isspace((unsigned char)s[w.size()]) != 0;
}

static void parseBlitzDecl(const std::string& text, std::set<std::string>& names) {
	std::vector<std::string> parts;
	int depth = 0;
	bool inStr = false;
	std::string cur;
	for (char c : text) {
		if (inStr) { if (c == '"') inStr = false; cur += c; continue; }
		if (c == '"') { inStr = true; cur += c; continue; }
		if (c == '(' || c == '[' || c == '{') { ++depth; cur += c; continue; }
		if (c == ')' || c == ']' || c == '}') { if (depth > 0) --depth; cur += c; continue; }
		if (c == ',' && depth == 0) { parts.push_back(cur); cur.clear(); continue; }
		cur += c;
	}
	if (!cur.empty()) parts.push_back(cur);

	for (const auto& part : parts) {
		size_t i = 0;
		while (i < part.size() && (std::isspace((unsigned char)part[i]) || part[i] == ':')) ++i;
		if (i >= part.size() || !(std::isalpha((unsigned char)part[i]) || part[i] == '_')) continue;
		size_t start = i;
		while (i < part.size() && (std::isalnum((unsigned char)part[i]) || part[i] == '_')) ++i;
		if (i > start) names.insert(part.substr(start, i - start));
	}
}

static std::string stripDeclSuffix(const std::string& s) {
	if (s.empty()) return s;
	char last = s.back();
	if (last == '$' || last == '#' || last == '%') return s.substr(0, s.size() - 1);
	return s;
}

static std::string stripBOM(const std::string& s) {
	if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
		return s.substr(3);
	return s;
}

static std::string normalizePath(const std::string& p) {
	try { return fs::weakly_canonical(p).string(); }
	catch (...) { return p; }
}

static bool samePath(const std::string& a, const std::string& b) {
#if defined(_WIN32)
	return toLower(normalizePath(a)) == toLower(normalizePath(b));
#else
	return normalizePath(a) == normalizePath(b);
#endif
}

static bool fileDefinesFunction(const std::string& path, const std::string& name, int& outLine) {
	std::ifstream in(path, std::ios::binary);
	if (!in.good()) return false;
	std::stringstream ss;
	ss << in.rdbuf();
	std::stringstream lines(stripBOM(ss.str()));
	std::string line;
	int ln = 0;
	while (std::getline(lines, line, '\n')) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t lead = line.find_first_not_of(" \t");
		std::string t = lead == std::string::npos ? "" : toLower(line.substr(lead));
		if (startsWithWord(t, "function")) {
			size_t p = line.find_first_of(" \t", lead);
			std::string fname = p == std::string::npos ? "" : line.substr(p + 1);
			fname = fname.substr(0, fname.find_first_of(" ("));
			if (!fname.empty() && stripDeclSuffix(toLower(fname)) == name) {
				outLine = ln;
				return true;
			}
		}
		++ln;
	}
	return false;
}

static std::vector<std::string> getIncludePaths(const std::string& path) {
	std::vector<std::string> result;
	if (path.empty()) return result;
	std::ifstream in(path, std::ios::binary);
	if (!in.good()) return result;
	std::stringstream ss;
	ss << in.rdbuf();
	std::stringstream lines(stripBOM(ss.str()));
	std::string line;
	while (std::getline(lines, line, '\n')) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t lead = line.find_first_not_of(" \t");
		std::string t = lead == std::string::npos ? "" : toLower(line.substr(lead));
		if (startsWithWord(t, "include")) {
			size_t q1 = line.find('"');
			size_t q2 = q1 == std::string::npos ? std::string::npos : line.find('"', q1 + 1);
			if (q1 != std::string::npos && q2 != std::string::npos) {
				std::string rel = line.substr(q1 + 1, q2 - q1 - 1);
				fs::path target(rel);
				if (target.is_relative())
					target = fs::path(path).parent_path() / target;
				if (fs::exists(target)) result.push_back(target.string());
				else if (fs::exists(rel)) result.push_back(fs::absolute(rel).string());
			}
		}
	}
	return result;
}

static std::string findFunctionInIncludes(const std::string& startFile, const std::string& name, std::vector<std::string>& visited, int& outLine) {
	if (startFile.empty()) return "";
	std::string key = normalizePath(startFile);
	if (std::find(visited.begin(), visited.end(), key) != visited.end()) return "";
	visited.push_back(key);

	if (fileDefinesFunction(startFile, name, outLine)) return startFile;

	for (const auto& inc : getIncludePaths(startFile)) {
		std::string found = findFunctionInIncludes(inc, name, visited, outLine);
		if (!found.empty()) return found;
	}
	return "";
}

static void openUrlImpl(const std::string& url) {
#if defined(_WIN32)
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		execl("/usr/bin/xdg-open", "xdg-open", url.c_str(), (char*)nullptr);
		execl("/usr/bin/open", "open", url.c_str(), (char*)nullptr);
		_exit(1);
	}
#endif
}

void App::openUrl(const std::string& url) { openUrlImpl(url); }

static void glfw_error_callback(int error, const char* description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void applyDarkStyle() {
	ImGuiStyle& s = ImGui::GetStyle();
	s.WindowRounding = 0.0f;
	s.FrameRounding = 0.0f;
	s.ChildRounding = 0.0f;
	s.PopupRounding = 0.0f;
	s.ScrollbarRounding = 0.0f;
	s.TabRounding = 0.0f;
	s.GrabRounding = 0.0f;
	s.WindowBorderSize = 1.0f;
	s.FrameBorderSize = 0.0f;
	s.TabBorderSize = 0.0f;
	s.WindowPadding = ImVec2(6, 6);
	s.FramePadding = ImVec2(6, 4);
	s.ItemSpacing = ImVec2(6, 5);
	s.ItemInnerSpacing = ImVec2(6, 5);
	s.IndentSpacing = 20.0f;
	s.ScrollbarSize = 14.0f;

	ImVec4* c = s.Colors;
	auto col = [](float r, float g, float b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); };
	c[ImGuiCol_Text] = col(220, 220, 220);
	c[ImGuiCol_TextDisabled] = col(120, 120, 120);
	c[ImGuiCol_WindowBg] = col(32, 32, 36);
	c[ImGuiCol_ChildBg] = col(28, 28, 31);
	c[ImGuiCol_PopupBg] = col(32, 32, 36);
	c[ImGuiCol_Border] = col(58, 58, 64);
	c[ImGuiCol_BorderShadow] = col(0, 0, 0, 0);
	c[ImGuiCol_FrameBg] = col(24, 24, 27);
	c[ImGuiCol_FrameBgHovered] = col(44, 44, 50);
	c[ImGuiCol_FrameBgActive] = col(54, 54, 60);
	c[ImGuiCol_TitleBg] = col(24, 24, 27);
	c[ImGuiCol_TitleBgActive] = col(38, 38, 43);
	c[ImGuiCol_TitleBgCollapsed] = col(24, 24, 27);
	c[ImGuiCol_MenuBarBg] = col(28, 28, 31);
	c[ImGuiCol_ScrollbarBg] = col(24, 24, 27);
	c[ImGuiCol_ScrollbarGrab] = col(64, 64, 70);
	c[ImGuiCol_ScrollbarGrabHovered] = col(80, 80, 88);
	c[ImGuiCol_ScrollbarGrabActive] = col(95, 95, 104);
	c[ImGuiCol_CheckMark] = col(120, 190, 240);
	c[ImGuiCol_SliderGrab] = col(100, 100, 108);
	c[ImGuiCol_SliderGrabActive] = col(120, 120, 130);
	c[ImGuiCol_Button] = col(46, 46, 52);
	c[ImGuiCol_ButtonHovered] = col(66, 66, 74);
	c[ImGuiCol_ButtonActive] = col(80, 80, 90);
	c[ImGuiCol_Header] = col(44, 44, 50);
	c[ImGuiCol_HeaderHovered] = col(56, 56, 64);
	c[ImGuiCol_HeaderActive] = col(66, 66, 74);
	c[ImGuiCol_Separator] = col(58, 58, 64);
	c[ImGuiCol_SeparatorHovered] = col(80, 80, 90);
	c[ImGuiCol_SeparatorActive] = col(100, 100, 112);
	c[ImGuiCol_ResizeGrip] = col(58, 58, 64);
	c[ImGuiCol_ResizeGripHovered] = col(80, 80, 90);
	c[ImGuiCol_ResizeGripActive] = col(100, 100, 112);
	c[ImGuiCol_Tab] = col(36, 36, 40);
	c[ImGuiCol_TabHovered] = col(56, 56, 64);
	c[ImGuiCol_TabSelected] = col(46, 46, 52);
	c[ImGuiCol_TabDimmed] = col(30, 30, 34);
	c[ImGuiCol_TabDimmedSelected] = col(40, 40, 45);
	c[ImGuiCol_TabSelectedOverline] = col(120, 190, 240);
	c[ImGuiCol_TextSelectedBg] = col(60, 90, 120);
	c[ImGuiCol_NavHighlight] = col(120, 190, 240);
	c[ImGuiCol_NavWindowingHighlight] = col(200, 200, 220);
}

App::App() {}
App::~App() {}

int App::run(int argc, char* argv[], bool skipPicker) {
	App app;
	g_app = &app;
	app.skipPicker = skipPicker;
	if (!app.init(argc, argv)) return 1;
	app.mainloop();
	app.shutdown();
	g_app = nullptr;
	return 0;
}

bool App::init(int argc, char* argv[]) {
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) return false;

	prefs.open();

	windowW = prefs.win_w; windowH = prefs.win_h;
	if (!skipPicker) { windowW = 320; windowH = 140; }

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	window = glfwCreateWindow(windowW, windowH, "BlitzX3D IDE", nullptr, nullptr);
	if (!window) { glfwTerminate(); return false; }
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

#if defined(_WIN32)
	{
		HICON hBig = (HICON)LoadImageA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1), IMAGE_ICON, 32, 32, LR_SHARED);
		HICON hSmall = (HICON)LoadImageA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1), IMAGE_ICON, 16, 16, LR_SHARED);
		HWND hwnd = glfwGetWin32Window(window);
		if (hBig) SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
		if (hSmall) SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
	}
#endif

	{
		GLFWmonitor* mon = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = mon ? glfwGetVideoMode(mon) : nullptr;
		if (mode) {
			int x = (mode->width - windowW) / 2;
			int y = (mode->height - windowH) / 2;
			glfwSetWindowPos(window, x, y);
			if (skipPicker) {
				glfwMaximizeWindow(window);
			}
		}
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.Fonts->AddFontDefaultBitmap();
	if (!prefs.homeDir.empty()) {
		io.IniFilename = strdup((prefs.homeDir + "/cfg/imgui.ini").c_str());
	}

	applyDarkStyle();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	glfwSetDropCallback(window, [](GLFWwindow*, int count, const char** paths) {
		if (g_app) {
			for (int k = 0; k < count; ++k) {
				g_app->openPath(paths[k]);
			}
		}
	});

	initKeywords();
	startUpdateCheck(this);

	for (int k = 1; k < argc; ++k) {
		std::string a = argv[k];
		if (a == "--imgui") continue;
		if (a.size() && a[0] == '-') continue;
		openPath(a);
	}
	if (docs.empty()) fileNew();

	return true;
}

void App::shutdown() {
	if (compileThread.joinable()) compileThread.join();
	if (keywordThread.joinable()) keywordThread.join();

	if (currentIndex >= 0 && pickerDone) {
		glfwGetWindowSize(window, &windowW, &windowH);
		prefs.win_w = windowW;
		prefs.win_h = windowH;
	}
	prefs.close();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
}

void App::mainloop() {
	while (!glfwWindowShouldClose(window) && !quitting) {
		glfwPollEvents();
		frame();
	}
}

void App::frame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (skipPicker || pickerDone) {
		ImGuiIO& kio = ImGui::GetIO();
		bool ctrl = kio.KeyCtrl;
		bool shift = kio.KeyShift;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F)) editFind();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_H)) editReplace();
		if (ImGui::IsKeyPressed(ImGuiKey_F3)) editFindNext();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) { if (Doc* d = currentDoc()) d->editor.Undo(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) { if (Doc* d = currentDoc()) d->editor.Redo(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) editCut();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) editCopy();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) editPaste();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) editSelectAll();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) { if (currentIndex >= 0) fileSave(currentIndex); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) fileNew();
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) fileOpen();
		(void)shift;
	}

	if (!skipPicker && !pickerDone) {
		drawPicker();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		ImGuiIO& pio = ImGui::GetIO();
		if (pio.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* pbackup = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(pbackup);
		}
		glfwSwapBuffers(window);
		return;
	}

	if (keywordsLoaded) {
		keywordsLoaded = false;
		for (auto& d : docs) {
			std::set<std::string> custom;
			for (const auto& f : d.funcs) {
				if (f.kind == 0) custom.insert(f.label);
			}
			d.editor.SetLanguageDefinition(makeBlitzLangDef(keywords, funcs, custom, d.globals, d.consts));
		}
	}

	menuBar();

	drawEditorPane();

	if (showFuncList) drawFuncList();

	if (showOutput) drawOutput();

	drawFindReplace();

	drawCommandLine();

	drawUpdate();
	drawUpdateDialog();

	if (aboutOpen) {
		ImGui::Begin("About BlitzX3D", &aboutOpen);
		ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
		ImGui::Text("BlitzX3D IDE");
		ImGui::Text("Version V1.3.5");
		ImGui::Separator();
		ImGui::Text("blitzpath: %s", prefs.homeDir.c_str());
		ImGui::End();
	}

	ImGui::Render();

	int fbw = 0, fbh = 0;
	glfwGetFramebufferSize(window, &fbw, &fbh);
	glViewport(0, 0, fbw, fbh);
	glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backup = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup);
	}
	glfwSwapBuffers(window);
}

void App::menuBar() {
	if (!ImGui::BeginMainMenuBar()) return;

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("New", "Ctrl+N")) fileNew();
		if (ImGui::MenuItem("Open...", "Ctrl+O")) fileOpen();
		ImGui::Separator();
		if (ImGui::MenuItem("Close", "Ctrl+W")) if (currentIndex >= 0) fileClose(currentIndex);
		if (ImGui::MenuItem("Close All")) while (!docs.empty()) fileClose((int)docs.size() - 1);
		ImGui::Separator();
		if (ImGui::MenuItem("Save", "Ctrl+S")) if (currentIndex >= 0) fileSave(currentIndex);
		if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) if (currentIndex >= 0) fileSaveAs(currentIndex);
		if (ImGui::MenuItem("Save All")) fileSaveAll();
		ImGui::Separator();
		if (ImGui::BeginMenu("Recent Files")) {
			for (size_t k = 0; k < prefs.recentFiles.size(); ++k) {
				if (ImGui::MenuItem(prefs.recentFiles[k].c_str())) {
					openPath(prefs.recentFiles[k]);
				}
			}
			if (prefs.recentFiles.empty()) ImGui::TextDisabled("(none)");
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Exit", "Alt+F4")) quitting = true;
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Edit")) {
		if (ImGui::MenuItem("Undo", "Ctrl+Z")) if (Doc* d = currentDoc()) d->editor.Undo();
		if (ImGui::MenuItem("Redo", "Ctrl+Y")) if (Doc* d = currentDoc()) d->editor.Redo();
		ImGui::Separator();
		if (ImGui::MenuItem("Cut", "Ctrl+X")) editCut();
		if (ImGui::MenuItem("Copy", "Ctrl+C")) editCopy();
		if (ImGui::MenuItem("Paste", "Ctrl+V")) editPaste();
		if (ImGui::MenuItem("Select All", "Ctrl+A")) editSelectAll();
		ImGui::Separator();
		if (ImGui::MenuItem("Find / Replace...", "Ctrl+F")) editFind();
		if (ImGui::MenuItem("Find Next", "F3")) editFindNext();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Program")) {
		if (ImGui::MenuItem("Run", "F5")) programExecute();
		if (ImGui::MenuItem("Compile", "Ctrl+F5")) programCompile();
		if (ImGui::MenuItem("Publish...")) programPublish();
		if (ImGui::MenuItem("Command Line...")) showCommandLine = true;
		ImGui::Separator();
		if (ImGui::MenuItem("Preprocess", nullptr, &prefs.prg_preprocess)) {}
		if (ImGui::MenuItem("Debug", nullptr, &prefs.prg_debug)) {}
		if (ImGui::MenuItem("No LAA", nullptr, &prefs.prg_nolaa)) {}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Help")) {
		if (ImGui::MenuItem("Help Home")) helpHome();
		ImGui::Separator();
		if (ImGui::MenuItem("About BlitzX3D")) aboutOpen = true;
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

void App::drawTabs() {
	if (docs.empty()) return;
	if (currentIndex < 0) currentIndex = 0;
	int closeIdx = -1;
	if (ImGui::BeginTabBar("##doctabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
		for (int k = 0; k < (int)docs.size(); ++k) {
			Doc& d = docs[k];
			std::string label = d.name;
			if (d.modified) label += "*";
			bool open = true;
			if (ImGui::BeginTabItem(label.c_str(), &open)) {
				currentIndex = k;
				ImGui::EndTabItem();
			}
			if (!open) closeIdx = k;
		}
		ImGui::EndTabBar();
	}
	if (closeIdx >= 0) fileClose(closeIdx);
}

void App::drawEditorPane() {
	ImGuiViewport* vp = ImGui::GetMainViewport();
	float menuH = ImGui::GetFrameHeight();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.22f, vp->WorkPos.y + menuH), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.78f, vp->WorkSize.y - menuH - vp->WorkSize.y * 0.30f), ImGuiCond_Always);
	ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	drawTabs();

	Doc* d = currentDoc();
	if (!d) { ImGui::End(); return; }

	ImGuiIO& io = ImGui::GetIO();
	bool ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	if (ctrl && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
		float delta = 0.0f;
		if (io.MouseWheel != 0.0f) delta = io.MouseWheel;
		if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) delta = -1.0f;
		if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) delta = 1.0f;
		if (delta != 0.0f) {
			const float base = ImGui::GetStyle().FontSizeBase;
			int size = (int)std::lround(base * editorFontScale * (delta > 0.0f ? 1.1f : 1.0f / 1.1f));
			size = std::clamp(size, 7, 48);
			editorFontScale = (float)size / base;
			io.InputQueueCharacters.resize(0);
		}
	}

	applyPalette(*d);

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * editorFontScale);
	d->editor.Render(d->name.c_str(), avail, false);
	ImGui::PopFont();

	std::string word;
	int cline, ccol;
	if (d->editor.TakeCtrlClick(word, cline, ccol))
		handleCtrlClick(*d, word, cline, ccol);

	int rcline, rccol;
	if (d->editor.TakeRightClick(rcline, rccol))
		ImGui::OpenPopup("editor_context");

	if (ImGui::BeginPopup("editor_context")) {
		TextEditor& ed = d->editor;
		bool didEdit = false;
		if (ImGui::MenuItem("Cut", "Ctrl+X")) { ed.Cut(); didEdit = true; }
		if (ImGui::MenuItem("Copy", "Ctrl+C")) ed.Copy();
		if (ImGui::MenuItem("Paste", "Ctrl+V")) { ed.Paste(); didEdit = true; }
		if (ImGui::MenuItem("Select All", "Ctrl+A")) ed.SelectAll();
		ImGui::Separator();
		if (ImGui::MenuItem("Duplicate Line")) { ed.DuplicateLine(); didEdit = true; }
		if (ImGui::MenuItem("Delete Line")) { ed.DeleteLine(); didEdit = true; }
		ImGui::Separator();
		if (ImGui::MenuItem("Toggle Comment", ";")) { ed.ToggleComment(); didEdit = true; }
		if (ImGui::MenuItem("Indent", "Tab")) { ed.Indent(); didEdit = true; }
		if (ImGui::MenuItem("Outdent", "Shift+Tab")) { ed.Outdent(); didEdit = true; }
		ImGui::Separator();
		if (ImGui::MenuItem("Undo", "Ctrl+Z")) ed.Undo();
		if (ImGui::MenuItem("Redo", "Ctrl+Y")) ed.Redo();
		if (didEdit) d->modified = true;
		ImGui::EndPopup();
	}

	ImGui::End();
}

void App::applyPalette(Doc& d) {
	TextEditor::Palette pal = d.editor.GetPalette();
	auto col = [](const int* rgb) -> ImU32 {
		return IM_COL32(rgb[0], rgb[1], rgb[2], 255);
	};
	pal[(int)TextEditor::PaletteIndex::Background] = col(prefs.rgb_bkgrnd);
	pal[(int)TextEditor::PaletteIndex::String] = col(prefs.rgb_string);
	pal[(int)TextEditor::PaletteIndex::Identifier] = col(prefs.rgb_ident);
	pal[(int)TextEditor::PaletteIndex::KnownIdentifier] = IM_COL32(150, 255, 200, 255);
	pal[(int)TextEditor::PaletteIndex::PreprocIdentifier] = IM_COL32(255, 200, 120, 255);
	pal[(int)TextEditor::PaletteIndex::Global] = IM_COL32(196, 160, 255, 255);
	pal[(int)TextEditor::PaletteIndex::Const] = IM_COL32(235, 205, 255, 255);
	pal[(int)TextEditor::PaletteIndex::Keyword] = col(prefs.rgb_keyword);
	pal[(int)TextEditor::PaletteIndex::Comment] = col(prefs.rgb_comment);
	pal[(int)TextEditor::PaletteIndex::MultiLineComment] = col(prefs.rgb_comment);
	pal[(int)TextEditor::PaletteIndex::Number] = col(prefs.rgb_digit);
	pal[(int)TextEditor::PaletteIndex::Default] = col(prefs.rgb_default);
	pal[(int)TextEditor::PaletteIndex::Selection] = IM_COL32(255, 200, 80, 170);
	pal[(int)TextEditor::PaletteIndex::LineNumber] = IM_COL32(120, 120, 120, 200);
	d.editor.SetPalette(pal);
}

void App::drawFuncList() {
	ImGuiViewport* vp = ImGui::GetMainViewport();
	float menuH = ImGui::GetFrameHeight();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + menuH), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.22f, vp->WorkSize.y - menuH), ImGuiCond_Always);
	ImGui::Begin("Functions", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	Doc* d = currentDoc();
	if (d) {
		for (size_t k = 0; k < d->funcs.size(); ++k) {
			const Doc::FuncItem& f = d->funcs[k];
			const char* prefix = f.kind == 0 ? "F " : f.kind == 1 ? "T " : ". ";
			std::string label = prefix + f.label;
			ImGui::PushID((int)k);
			if (ImGui::Selectable(label.c_str())) {
				d->editor.SetCursorPosition(TextEditor::Coordinates(f.line, 0));
				d->editor.SetSelection(TextEditor::Coordinates(f.line, 0),
					TextEditor::Coordinates(f.line, 0));
			}
			ImGui::PopID();
		}
		if (d->funcs.empty()) ImGui::TextDisabled("(no functions)");
	}
	ImGui::End();
}

void App::drawOutput() {
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.22f, vp->WorkPos.y + vp->WorkSize.y * 0.70f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.78f, vp->WorkSize.y * 0.30f), ImGuiCond_Always);
	ImGui::Begin("Output", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	ImGui::BeginChild("##outlines", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4), false);
	{
		std::lock_guard<std::mutex> lock(outputMutex);
		ImGui::PushStyleColor(ImGuiCol_Text, compileOK ? IM_COL32(200, 255, 200, 255) : IM_COL32(255, 200, 200, 255));
		for (const auto& line : outputLines) {
			ImGui::TextWrapped("%s", line.c_str());
		}
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();

	if (ImGui::Button(compiling ? "Compiling..." : "Clear")) {
		if (!compiling) {
			std::lock_guard<std::mutex> lock(outputMutex);
			output.clear();
			outputLines.clear();
		}
	}
	ImGui::End();
}

void App::drawFindReplace() {
	if (!showFind && !showReplace) return;
	int flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
	ImGui::SetNextWindowPos(ImVec2(windowW / 2.0f - 200, 40), ImGuiCond_Appearing);
	if (ImGui::Begin("Find / Replace", &showFind, flags)) {
		ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
		bool doFind = false, doReplace = false, doReplaceAll = false;static char findBuf[512], replaceBuf[512];
		strcpy(findBuf, findStr.c_str());
		ImGui::SetNextItemWidth(300);
		ImGui::InputText("Find text", findBuf, sizeof(findBuf));
		ImGui::SameLine();
		if (ImGui::Button("Find")) doFind = true;
		ImGui::Checkbox("Match case", &matchCase);
		strcpy(replaceBuf, replaceStr.c_str());
		ImGui::SetNextItemWidth(300);
		ImGui::InputText("Replace text", replaceBuf, sizeof(replaceBuf));
		ImGui::SameLine();
		if (ImGui::Button("Replace")) doReplace = true;
		if (ImGui::Button("Replace All")) doReplaceAll = true;
		findStr = findBuf;
		replaceStr = replaceBuf;
		if (doFind || doReplace || doReplaceAll) {
			Doc* d = currentDoc();
			if (d && !findStr.empty()) {
				if (doReplaceAll) {
					std::string text = d->editor.GetText();
					size_t pos = 0;
					while ((pos = text.find(findStr, pos)) != std::string::npos) {
						text.replace(pos, findStr.size(), replaceStr);
						pos += replaceStr.size();
					}
					d->editor.SetText(text);
					d->modified = true;
				}
				else {
					if (doReplace && d->editor.HasSelection()) {
						std::string sel = d->editor.GetSelectedText();
						if (sel == findStr) {
							d->editor.InsertText(replaceStr);
							d->modified = true;
						}
					}
					if (doFind) editFindNext();
				}
			}
			showFind = showReplace = false;
		}
	}
	ImGui::End();
	if (!showFind) showReplace = false;
}

int App::addDoc(const std::string& path) {
	Doc d;
	if (path.empty()) {
		d.name = "untitled";
	}
	else {
		d.path = path;
		fs::path p(path);
		d.name = p.filename().string();
		std::ifstream in(path, std::ios::binary);
		if (in.good()) {
			std::stringstream ss;
			ss << in.rdbuf();
			d.editor.SetText(ss.str());
		}
	}
	d.editor.SetLanguageDefinition(makeBlitzLangDef(keywords, funcs, {}));
	rebuildFuncList(d);
	std::set<std::string> custom;
	for (const auto& f : d.funcs) {
		if (f.kind == 0) custom.insert(f.label);
	}
	d.editor.SetLanguageDefinition(makeBlitzLangDef(keywords, funcs, custom, d.globals, d.consts));
	d.modified = false;
	docs.push_back(std::move(d));
	currentIndex = (int)docs.size() - 1;
	return currentIndex;
}

bool App::openFile(const std::string& path, bool recent) {
	for (int k = 0; k < (int)docs.size(); ++k) {
		if (samePath(docs[k].path, path)) { currentIndex = k; return true; }
	}
	fs::path p(path);
	if (!fs::exists(p)) return false;
	addDoc(path);
	if (recent) addRecent(path);
	return true;
}

bool App::openProject(const std::string& path) {
	fs::path p(path);
	if (!fs::exists(p)) return false;
	fs::path dir = p.parent_path();

	std::ifstream in(path, std::ios::binary);
	if (!in.good()) return false;
	std::stringstream ss;
	ss << in.rdbuf();
	std::string text = ss.str();

	std::string mainFile;
	std::vector<std::string> absPaths;

	auto getAttr = [](const std::string& s, const std::string& key) -> std::string {
		size_t k = s.find(key + "=");
		if (k == std::string::npos) return "";
		size_t q1 = s.find('"', k);
		if (q1 == std::string::npos) return "";
		size_t q2 = s.find('"', q1 + 1);
		if (q2 == std::string::npos) return "";
		return s.substr(q1 + 1, q2 - q1 - 1);
	};

	size_t pos = 0;
	while ((pos = text.find("AbsPath=", pos)) != std::string::npos) {
		std::string v = getAttr(text.substr(pos), "AbsPath");
		if (!v.empty()) absPaths.push_back(v);
		pos += 8;
	}
	mainFile = getAttr(text, "MainFile");

	if (absPaths.empty()) return false;

	std::string mainRel;
	if (!mainFile.empty()) {
		for (const auto& rel : absPaths) {
			std::string r = rel;
			if (!r.empty() && (r[0] == '\\' || r[0] == '/')) r = r.substr(1);
			std::string base = r;
			size_t slash = base.find_last_of('/');
			if (slash != std::string::npos) base = base.substr(slash + 1);
			if (base == mainFile) { mainRel = rel; break; }
		}
	}

	auto openRel = [&](const std::string& rel) {
		std::string r = rel;
		if (!r.empty() && (r[0] == '\\' || r[0] == '/')) r = r.substr(1);
		std::replace(r.begin(), r.end(), '\\', '/');
		fs::path f = dir / r;
		if (fs::exists(f)) openFile(f.string(), false);
	};

	if (!mainRel.empty()) openRel(mainRel);
	for (const auto& rel : absPaths) {
		if (rel == mainRel) continue;
		openRel(rel);
	}

	currentIndex = 0;
	addRecent(path);
	return true;
}

bool App::openPath(const std::string& path) {
	if (fs::path(path).extension().string() == ".ipf") return openProject(path);
	return openFile(path);
}

void App::fileNew() { addDoc(""); }
void App::fileOpen() {
	std::string path;
	if (fileOpenDialog(path)) openPath(path);
}
void App::addRecent(const std::string& path) {
	if (path.empty()) return;
	for (auto it = prefs.recentFiles.begin(); it != prefs.recentFiles.end(); ++it) {
		if (samePath(*it, path)) { prefs.recentFiles.erase(it); break; }
	}
	prefs.recentFiles.insert(prefs.recentFiles.begin(), path);
	if (prefs.recentFiles.size() > 10) prefs.recentFiles.pop_back();
}
void App::fileRecent(const std::string& path) { openPath(path); }

bool App::fileSave(int idx) {
	Doc* d = doc(idx);
	if (!d) return false;
	if (d->path.empty()) return fileSaveAs(idx);
	std::ofstream out(d->path, std::ios::binary | std::ios::trunc);
	if (!out.good()) return false;
	out << d->editor.GetText();
	out.close();
	d->modified = false;
	return true;
}
bool App::fileSaveAs(int idx) {
	Doc* d = doc(idx);
	if (!d) return false;
	std::string defaultName = d->name;
	if (defaultName == "untitled") defaultName = "untitled.bb";
	std::string path;
	if (!fileSaveDialog(path, defaultName.c_str())) return false;
	d->path = path;
	fs::path p(path);
	d->name = p.filename().string();
	rebuildFuncList(*d);
	addRecent(path);
	return fileSave(idx);
}
bool App::fileSaveAll() {
	bool ok = true;
	for (int k = 0; k < (int)docs.size(); ++k) {
		if (docs[k].modified) { if (!fileSave(k)) ok = false; }
	}
	return ok;
}
void App::fileClose(int idx) {
	if (docs[idx].modified) {
		if (!docs[idx].path.empty()) fileSave(idx);
	}
	docs.erase(docs.begin() + idx);
	if (currentIndex >= (int)docs.size()) currentIndex = (int)docs.size() - 1;
	if (docs.empty()) currentIndex = -1;
}
void App::fileExit() { quitting = true; }

void App::editCut() { if (Doc* d = currentDoc()) d->editor.Cut(); }
void App::editCopy() { if (Doc* d = currentDoc()) d->editor.Copy(); }
void App::editPaste() { if (Doc* d = currentDoc()) d->editor.Paste(); }
void App::editSelectAll() { if (Doc* d = currentDoc()) d->editor.SelectAll(); }
void App::editFind() { showFind = true; showReplace = true; }
void App::editReplace() { showReplace = true; showFind = true; }

void App::editFindNext() {
	Doc* d = currentDoc();
	if (!d || findStr.empty()) { editFind(); return; }
	TextEditor::Coordinates cur = d->editor.GetCursorPosition();
	std::string full = d->editor.GetText();
	std::vector<std::string> lines;
	{
		std::stringstream ss(full);
		std::string ln;
		while (std::getline(ss, ln, '\n')) lines.push_back(ln);
	}
	int total = (int)lines.size();
	std::string needle = findStr;
	bool ic = !matchCase;
	auto lower = [](std::string t) { std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); }); return t; };
	if (ic) needle = lower(needle);
	auto findFrom = [&](int startLine, int startCol) -> bool {
		for (int line = startLine; line < total; ++line) {
			std::string hay = ic ? lower(lines[line]) : lines[line];
			int sc = (line == startLine) ? startCol : 0;
			size_t pos = hay.find(needle, sc);
			if (pos != std::string::npos) {
				d->editor.SetCursorPosition(TextEditor::Coordinates(line, (int)pos + (int)needle.size()));
				d->editor.SetSelection(TextEditor::Coordinates(line, (int)pos),
					TextEditor::Coordinates(line, (int)pos + (int)needle.size()));
				return true;
			}
		}
		return false;
	};
	if (!findFrom(cur.mLine, cur.mColumn)) {
		findFrom(0, 0);
	}
}

void App::programExecute() { build(true, false); }
void App::programCompile() { build(false, false); }

void App::programPublish() {
	Doc* e = currentDoc();
	if (!e) return;
	if (prefs.prg_debug) {
		appendOutput("Warning: Debug is enabled; publish will produce a slower executable.\n");
	}
	std::string defaultName = e->name;
	if (defaultName.empty() || defaultName == "untitled") defaultName = "untitled.exe";
	if (!fileSaveDialog(publishExePath, defaultName.c_str(),
		"Executable files (*.exe)|*.exe|All files (*.*)|*.*")) return;

	publishIconPath.clear();
	std::string iconPath;
	if (fileOpenDialog(iconPath, "Icon files (*.ico)|*.ico|All files (*.*)|*.*")) {
		publishIconPath = iconPath;
	}
	build(true, true);
}void App::programPreprocess() { prefs.prg_preprocess = !prefs.prg_preprocess; }
void App::programDebug() { prefs.prg_debug = !prefs.prg_debug; }
void App::programNoLAA() { prefs.prg_nolaa = !prefs.prg_nolaa; }

void App::helpHome() {
	App::openUrl("https://kippykip.com/b3ddocs/commands/index.htm");
}
void App::helpAbout() { aboutOpen = true; }

void App::launchLegacyIDE() {
#if defined(_WIN32)
	const char* bp = std::getenv("blitzpath");
	if (bp && *bp) {
		std::string ide = "\"" + std::string(bp) + "\\bin\\ide.exe\"";
		STARTUPINFOA si = { sizeof(si) };
		PROCESS_INFORMATION pi = { 0 };
		char* cmdline = _strdup(ide.c_str());
		if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		free(cmdline);
	}
#endif
	quitting = true;
}

void App::drawPicker() {
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImVec2 center = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	if (ImGui::Begin("BlitzX3D IDE", nullptr,
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::TextWrapped("Choose your IDE:");
		ImGui::Spacing();
		ImGui::Spacing();
		if (ImGui::Button("New BlitzX3D IDE", ImVec2(-1, 0))) {
			glfwMaximizeWindow(window);
			windowW = prefs.win_w; windowH = prefs.win_h;
			pickerDone = true;
		}
		ImGui::Spacing();
		if (ImGui::Button("Legacy IDE", ImVec2(-1, 0))) {
			launchLegacyIDE();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void App::drawCommandLine() {
	if (!showCommandLine) return;
	int flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
	ImGui::SetNextWindowPos(ImVec2(windowW / 2.0f - 220, 60), ImGuiCond_Appearing);
	if (ImGui::Begin("Command Line", &showCommandLine, flags)) {
		ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
		static char buf[512];
		strcpy(buf, prefs.cmd_line.c_str());
		ImGui::SetNextItemWidth(400);
		ImGui::InputText("Arguments", buf, sizeof(buf));
		if (ImGui::Button("OK")) {
			prefs.cmd_line = buf;
			showCommandLine = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) showCommandLine = false;
	}
	ImGui::End();
}

void App::build(bool exec, bool publish) {
	if (compiling) return;
	Doc* e = currentDoc();
	if (!e) return;

	if (!fileSaveAll()) {
		appendOutput("Save failed; compile aborted.\n");
		return;
	}

	std::string src_file = e->path;
	std::string opts = " ";
	if (prefs.prg_preprocess) opts += "-p ";
	if (prefs.prg_debug) opts += "-d ";
	if (prefs.prg_nolaa) opts += "-nlaa ";

	if (publish) {
		std::string exe = publishExePath.empty() ? src_file : publishExePath;
		if (exe.empty()) exe = "untitled.exe";
		opts += "-o \"" + exe + "\" ";
	}
	else if (!exec) {
		opts += "-c ";
	}

	std::string src = src_file;
	if (src.empty()) {
		src = prefs.homeDir + "/temp/tmp.bb";
		std::ofstream out(src, std::ios::binary | std::ios::trunc);
		if (!out.good()) {
			appendOutput("Error writing temporary file.\n");
			return;
		}
		out << e->editor.GetText();
		out.close();
		e->path = src;
		e->name = "tmp.bb";
		rebuildFuncList(*e);
	}
	else {
		prefs.prg_lastbuild = e->path;
	}

	std::string cmd = prefs.homeDir + "/bin/blitzcc -q " + opts + " \"" + src + "\" " + prefs.cmd_line;
	compile(cmd);
}

void App::compile(const std::string& cmd) {
	if (compiling) return;
	if (compileThread.joinable()) compileThread.join();
	compiling = true;
	compileOK = true;
	appendOutput(">>> " + cmd + "\n");

	compileThread = std::thread([this, cmd]() {
		std::string output;
		int code = runProcess(cmd, output);
		{
			std::lock_guard<std::mutex> lock(outputMutex);
			this->output += output;
			std::string line;
			std::stringstream ss(output);
			while (std::getline(ss, line, '\n')) {
				if (!line.empty() && line.back() == '\r') line.pop_back();
				parseOutputLine(line);
				this->outputLines.push_back(line);
			}
		}
		if (code != 0) compileOK = false;
		else if (!publishIconPath.empty() && !publishExePath.empty()) {
			if (applyIconToExe(publishExePath, publishIconPath)) {
				appendOutput("Icon applied to executable.\n");
			}
			else {
				appendOutput("Warning: could not apply icon to executable.\n");
			}
		}
		compiling = false;
	});
}

void App::appendOutput(const std::string& text) {
	std::lock_guard<std::mutex> lock(outputMutex);
	output += text;
	std::stringstream ss(text);
	std::string line;
	while (std::getline(ss, line, '\n')) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		outputLines.push_back(line);
	}
}

void App::parseOutputLine(const std::string& line) {
	if (line.empty() || line[0] != '"') return;
	size_t n = line.find('"', 1);
	if (n == std::string::npos) return;
	if (n + 1 >= line.size() || line[n + 1] != ':') return;
	std::string file = line.substr(1, n - 1);
	std::string rest = line.substr(n + 2);
	int row1 = 0, col1 = 0, row2 = 0, col2 = 0;
	if (sscanf(rest.c_str(), "%d:%d:%d:%d", &row1, &col1, &row2, &col2) == 4) {
		openFile(file);
		Doc* d = currentDoc();
		if (d && row1 >= 1) {
			d->editor.SetCursorPosition(TextEditor::Coordinates(row1 - 1, col1 - 1));
		}
	}
}

void App::rebuildFuncList(Doc& d) {
	d.funcs.clear();
	d.globals.clear();
	d.consts.clear();
	std::string text = stripBOM(d.editor.GetText());
	std::stringstream ss(text);
	std::string line;
	int ln = 0;
	while (std::getline(ss, line, '\n')) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t lead = line.find_first_not_of(" \t");
		std::string t = lead == std::string::npos ? "" : toLower(line.substr(lead));
		if (startsWithWord(t, "function")) {
			size_t p = line.find_first_of(" \t", lead);
			std::string name = p == std::string::npos ? "" : line.substr(p + 1);
			name = name.substr(0, name.find_first_of(" ("));
			name = stripDeclSuffix(name);
			if (name.size()) d.funcs.push_back({ name, ln, 0 });
		}
		else if (startsWithWord(t, "type")) {
			size_t p = line.find_first_of(" \t", lead);
			std::string name = p == std::string::npos ? "" : line.substr(p + 1);
			if (name.size()) d.funcs.push_back({ name, ln, 1 });
		}
		else if (startsWithWord(t, "global")) {
			parseBlitzDecl(line.substr(lead + 6), d.globals);
		}
		else if (startsWithWord(t, "const")) {
			parseBlitzDecl(line.substr(lead + 5), d.consts);
		}
		else if (t.size() && t[0] == '.') {
			size_t p = line.find_first_of(" \t", lead);
			std::string name = p == std::string::npos ? line.substr(lead + 1) : line.substr(lead + 1, p - lead - 1);
			if (name.size()) d.funcs.push_back({ name, ln, 2 });
		}
		++ln;
	}
}

void App::handleCtrlClick(Doc& d, const std::string& word, int line, int column) {
	(void)column;
	std::string ln = d.editor.GetLineText(line);
	size_t lead = ln.find_first_not_of(" \t");
	std::string t = lead == std::string::npos ? "" : toLower(ln.substr(lead));

	if (startsWithWord(t, "include")) {
		size_t q1 = ln.find('"');
		size_t q2 = q1 == std::string::npos ? std::string::npos : ln.find('"', q1 + 1);
		if (q1 != std::string::npos && q2 != std::string::npos) {
			std::string rel = ln.substr(q1 + 1, q2 - q1 - 1);
			fs::path target(rel);
			if (target.is_relative() && !d.path.empty())
				target = fs::path(d.path).parent_path() / target;
			if (fs::exists(target)) { openFile(target.string()); return; }
			if (fs::exists(rel)) { openFile(rel); return; }
		}
	}

	if (word.empty()) return;

	std::string lw = stripDeclSuffix(toLower(word));
	for (size_t k = 0; k < docs.size(); ++k) {
		Doc& t = docs[k];
		for (const auto& f : t.funcs) {
			if (stripDeclSuffix(toLower(f.label)) == lw) {
				currentIndex = (int)k;
				t.editor.SetCursorPosition(TextEditor::Coordinates(f.line, 0));
				t.editor.SetSelection(TextEditor::Coordinates(f.line, 0),
					TextEditor::Coordinates(f.line, 0));
				return;
			}
		}
	}

	int foundLine = 0;
	std::vector<std::string> visited;
	std::string foundFile = findFunctionInIncludes(d.path, lw, visited, foundLine);
	if (!foundFile.empty()) {
		openFile(foundFile);
		Doc* nd = currentDoc();
		if (nd) {
			nd->editor.SetCursorPosition(TextEditor::Coordinates(foundLine, 0));
			nd->editor.SetSelection(TextEditor::Coordinates(foundLine, 0),
				TextEditor::Coordinates(foundLine, 0));
		}
	}
}

void App::initKeywords() {
	keywords = builtinBlitzKeywords();
	if (prefs.homeDir.empty()) return;
	keywordThread = std::thread([this]() {
		std::string kws;
		int code = 0;
		runProcess(prefs.homeDir + "/bin/blitzcc +k", kws, &code);
		std::set<std::string> loaded;
		std::set<std::string> loadedFuncs;
		std::stringstream ss(kws);
		std::string line;
		const std::set<std::string> builtin = builtinBlitzKeywords();
		while (std::getline(ss, line, '\n')) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty()) continue;
			std::string kw = line.substr(0, line.find(' '));
			if (kw.find('(') != std::string::npos) kw = kw.substr(0, kw.find('('));
			if (kw.empty()) continue;
			if (!isalnum((unsigned char)kw.back())) kw.pop_back();
			if (kw.find("Blitz_") == 0) kw = kw.substr(6);
			std::string kwLow = kw;
			std::transform(kwLow.begin(), kwLow.end(), kwLow.begin(), ::tolower);
			if (builtin.find(kwLow) != builtin.end()) loaded.insert(kw);
			else loadedFuncs.insert(kw);
		}
		{
			std::lock_guard<std::mutex> lock(keywordMutex);
			if (!loaded.empty()) {
				for (const auto& kw : loaded) keywords.insert(kw);
				for (const auto& f : loadedFuncs) funcs.insert(f);
				keywordsLoaded = true;
			}
		}
	});
}


