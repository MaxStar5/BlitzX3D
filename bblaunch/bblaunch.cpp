#include <string>
#include <Windows.h>
#include "../MultiLang/MultiLang.h"

static std::string getAppDir() {
	char buffer[MAX_PATH];
	std::string t = std::string(buffer, GetModuleFileName(NULL, buffer, MAX_PATH));
	int n = t.find_last_of('\\');
	if(n != std::string::npos) t = t.substr(0, n); //Remove filename from the path and return it
	return t;
}

static void fail(const char* p) {
	::MessageBox(0, p, MultiLang::bb_err, MB_SETFOREGROUND | MB_TOPMOST | MB_ICONERROR);
	ExitProcess(-1);
}

int WINAPI WinMain(_In_ HINSTANCE inst, _In_opt_ HINSTANCE prev, _In_ char* cmd, _In_ int show) {
	std::string basedir = getAppDir();

	_putenv_s("blitzpath", basedir.c_str());
	SetCurrentDirectory(basedir.c_str());

	std::string idePath = "\\bin\\blitzide_imgui.exe ";
	basedir += idePath + cmd;

	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

	ZeroMemory(&startupInfo, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	ZeroMemory(&processInfo, sizeof(processInfo));

	if(!CreateProcess(NULL, (char*)basedir.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo)) {
		fail(MultiLang::init_err);
	}

	//Wait until BlitzIDE enters an idle state, then close bblaunch.
	WaitForInputIdle(processInfo.hProcess, INFINITE);

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	return 0; //Success!
}