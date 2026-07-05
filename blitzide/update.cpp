#include "stdafx.h"
#include "update.h"
#include "prefs.h"

#include <wininet.h>
#include <commctrl.h>
#include <shellapi.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")

static const char* VERSION_URL = "https://krimbopple.xyz/BlitzX3D/version.txt";
static const char* RELEASES_URL = "https://github.com/krimbopple/BlitzX3D/releases";

static bool fetchRemoteVersion(std::string& out) {
	HINTERNET hInet = InternetOpenA("BlitzX3D-UpdateCheck", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInet) return false;

	DWORD timeout = 4000;
	InternetSetOptionA(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOptionA(hInet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOptionA(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

	DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE;

	HINTERNET hUrl = InternetOpenUrlA(hInet, VERSION_URL, NULL, 0, flags, 0);
	if (!hUrl) {
		InternetCloseHandle(hInet);
		return false;
	}

	char buff[256];
	DWORD read = 0;
	out.clear();
	while (InternetReadFile(hUrl, buff, sizeof(buff) - 1, &read) && read) {
		buff[read] = 0;
		out += buff;
		if (out.size() > 4096) break;
	}

	InternetCloseHandle(hUrl);
	InternetCloseHandle(hInet);

	while (out.size() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ' || out.back() == '\t')) out.pop_back();
	size_t start = out.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) { out.clear(); return false; }
	out = out.substr(start);

	return !out.empty();
}

static std::vector<int> parseVersion(const std::string& v) {
	std::vector<int> parts;
	std::string s = v;
	if (!s.empty() && (s[0] == 'V' || s[0] == 'v')) s = s.substr(1);

	int cur = 0;
	bool any = false;
	for (size_t i = 0; i <= s.size(); ++i) {
		if (i < s.size() && isdigit((unsigned char)s[i])) {
			cur = cur * 10 + (s[i] - '0');
			any = true;
		}
		else {
			if (any) parts.push_back(cur);
			cur = 0; any = false;
		}
	}
	return parts;
}

static bool isNewer(const std::string& remote, const std::string& local) {
	std::vector<int> r = parseVersion(remote);
	std::vector<int> l = parseVersion(local);
	size_t n = r.size() > l.size() ? r.size() : l.size();
	for (size_t i = 0; i < n; ++i) {
		int rv = i < r.size() ? r[i] : 0;
		int lv = i < l.size() ? l[i] : 0;
		if (rv != lv) return rv > lv;
	}
	return false;
}

void checkForUpdate() {
	std::string remote;
	if (!fetchRemoteVersion(remote)) return;
	if (!isNewer(remote, BLITZIDE_VERSION)) return;
	if (prefs.ignore_version_update == remote) return;

	std::wstring wRemote(remote.begin(), remote.end());
	std::wstring wCurrent(BLITZIDE_VERSION, BLITZIDE_VERSION + strlen(BLITZIDE_VERSION));

	std::wstring content = L"You are running " + wCurrent + L". Version " + wRemote + L" is available on github.";

	TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
	HWND parentWnd = AfxGetMainWnd() ? AfxGetMainWnd()->GetSafeHwnd() : NULL;
	tdc.hwndParent = parentWnd;
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
	tdc.pszWindowTitle = L"BlitzX3D Update";
	tdc.pszMainInstruction = L"A new version of BlitzX3D is available!";
	tdc.pszContent = content.c_str();
	tdc.pszVerificationText = L"Don't remind me again for this version";
	tdc.pszMainIcon = TD_INFORMATION_ICON;

	static const TASKDIALOG_BUTTON buttons[] = {
		{ 1001, L"View Releases" },
		{ IDCANCEL, L"Later" },
	};
	tdc.pButtons = buttons;
	tdc.cButtons = 2;
	tdc.nDefaultButton = 1001;

	int pressedBtn = 0;
	BOOL checked = FALSE;
	HRESULT hr = TaskDialogIndirect(&tdc, &pressedBtn, NULL, &checked);

	if (checked) {
		prefs.ignore_version_update = remote;
		prefs.close();
	}

	if (SUCCEEDED(hr) && pressedBtn == 1001) {
		ShellExecuteA(NULL, "open", RELEASES_URL, NULL, NULL, SW_SHOWNORMAL);
	}
}