#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#include "prefs.h"
#include "TextEditor.h"

struct SDL_Window;

struct Doc {
	std::string path;
	std::string name;
	TextEditor editor;
	bool modified = false;

	struct FuncItem { std::string label; int line; int kind; };
	std::vector<FuncItem> funcs;
	std::set<std::string> globals;
	std::set<std::string> consts;

	std::string getText() const { return editor.GetText(); }
};

class App {
public:
	static int run(int argc, char* argv[], bool skipPicker);
	static void openUrl(const std::string& url);

private:
	App();
	~App();

	bool init(int argc, char* argv[]);
	void shutdown();
	void frame();
	void mainloop();
	void launchLegacyIDE();
	void drawPicker();

	void fileNew();
	void fileOpen();
	bool fileSave(int idx);
	bool fileSaveAs(int idx);
	bool fileSaveAll();
	void fileClose(int idx);
	void fileExit();
	void fileRecent(const std::string& path);
	void addRecent(const std::string& path);
	void removeRecent(const std::string& path);

	void editCut();
	void editCopy();
	void editPaste();
	void editSelectAll();
	void editFind();
	void editFindNext();
	void editReplace();

	void programExecute();
	void programCompile();
	void programPublish();
	void programPreprocess();
	void programDebug();
	void programNoLAA();

	void helpHome();
	void helpAbout();
	void drawUpdate();
	void drawUpdateDialog();

	void build(bool exec, bool publish);
	void compile(const std::vector<std::string>& args);
	void appendOutput(const std::string& text);
	void parseOutputLine(const std::string& line);

	int current() const { return currentIndex; }
	Doc* doc(int idx) { return idx >= 0 && idx < (int)docs.size() ? &docs[idx] : nullptr; }
	Doc* currentDoc() { return doc(currentIndex); }
	int addDoc(const std::string& path);
	bool openFile(const std::string& path, bool recent = true);
	bool openProject(const std::string& path);
	bool openBlitzProject(const std::string& path);
	bool openPath(const std::string& path);
	void rebuildFuncList(Doc& d);
	void refreshProjectSymbols();
	void openProjectWindow();
	bool convertIpfToBxp(const std::string& path);
	bool saveProjectFile(const std::string& path);
	void handleCtrlClick(Doc& d, const std::string& word, int line, int column);
	void applyPalette(Doc& d);

	void menuBar();
	void drawTabs();
	void drawEditorPane();
	void drawFuncList();
	void drawOutput();
	void drawFindReplace();
	void drawMenus();
	void drawCommandLine();
	void drawStylize();
	void drawProjectWindow();
	void drawProjectNavigator();
	void setupDockLayout();
	void drawPaneBackground();

	void initKeywords();

	SDL_Window* window = nullptr;
	int windowW = 800, windowH = 600;

	std::vector<Doc> docs;
	int currentIndex = -1;

	bool showFuncList = true;
	bool showOutput = true;
	bool showFind = false;
	bool showReplace = false;
	std::string findStr, replaceStr;
	char findBuf[512] = {};
	char replaceBuf[512] = {};
	bool findFocusPending = false;
	bool findAllFiles = false;
	std::string findStatus;
	int requestedIndex = -1;
	int findFlags = 0;
	bool matchCase = false;

	std::string output;
	std::vector<std::string> outputLines;
	std::thread compileThread;
	std::atomic<bool> compiling{ false };
	std::mutex outputMutex;
	bool compileOK = false;

	std::string publishExePath;
	std::string publishIconPath;

	std::set<std::string> keywords;
	std::set<std::string> funcs;
	std::thread keywordThread;
	std::mutex keywordMutex;
	std::atomic<bool> keywordsLoaded = false;

	bool skipPicker = false;
	bool pickerDone = false;
	bool pickLegacy = false;

	float editorFontScale = 1.0f;

	bool showCommandLine = false;
	bool updateOpen = false;

	bool showStylize = false;
	bool showProjectWindow = false;
	bool showProjectNavigator = true;
	std::string projectDraftMainPath;
	std::string projectStatus;
	char projectSavePathBuf[1024] = {};
	char projectFilterBuf[256] = {};
	char projectSymbolFilterBuf[256] = {};

	bool projectOpen = false;
	std::string projectPath;
	std::string projectMainPath;
	std::vector<std::string> projectFiles;
	std::vector<int> projectIncludedDocs;
	std::vector<int> projectNavigatorDocs;

	bool aboutOpen = false;
	bool quitting = false;
};

#endif
