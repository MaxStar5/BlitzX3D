#include "update.h"
#include "app.h"
#include "prefs.h"

#include "../imgui/imgui.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <wininet.h>
#endif

static const char* VERSION_URL = "https://krimbopple.xyz/BlitzX3D/version.txt";
static const char* RELEASES_URL = "https://github.com/krimbopple/BlitzX3D/releases";

static std::thread updateThread;
static std::atomic<bool> updateRunning{ false };
static std::atomic<bool> updateFound{ false };
static std::string remoteVersion;

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

static bool fetchRemoteVersion(std::string& out) {
	out.clear();
#if defined(_WIN32)
	HINTERNET hInet = InternetOpenA("BlitzX3D-UpdateCheck", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInet) return false;
	DWORD timeout = 4000;
	InternetSetOptionA(hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOptionA(hInet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOptionA(hInet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
	DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE;
	HINTERNET hUrl = InternetOpenUrlA(hInet, VERSION_URL, NULL, 0, flags, 0);
	if (!hUrl) { InternetCloseHandle(hInet); return false; }
	char buff[256];
	DWORD read = 0;
	while (InternetReadFile(hUrl, buff, sizeof(buff) - 1, &read) && read) {
		buff[read] = 0;
		out += buff;
		if (out.size() > 4096) break;
	}
	InternetCloseHandle(hUrl);
	InternetCloseHandle(hInet);
#else
	FILE* p = popen("curl -s --max-time 4 '" VERSION_URL "' 2>/dev/null || wget -q -O - -T 4 '" VERSION_URL "' 2>/dev/null", "r");
	if (!p) return false;
	char buff[256];
	size_t n;
	while ((n = fread(buff, 1, sizeof(buff) - 1, p)) > 0) {
		buff[n] = 0;
		out += buff;
		if (out.size() > 4096) break;
	}
	pclose(p);
#endif
	while (out.size() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ' || out.back() == '\t')) out.pop_back();
	size_t start = out.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) { out.clear(); return false; }
	out = out.substr(start);
	return !out.empty();
}

void startUpdateCheck(App* app) {
	if (updateRunning) return;
	updateRunning = true;
	updateThread = std::thread([app]() {
		std::string remote;
		if (fetchRemoteVersion(remote) && isNewer(remote, BLITZIDE_VERSION) && prefs.ignore_version_update != remote) {
			remoteVersion = remote;
			updateFound = true;
		}
		updateRunning = false;
	});
	updateThread.detach();
	(void)app;
}

void App::drawUpdate() {
	if (!updateFound) return;
	updateFound = false;
	updateOpen = true;
	ImGui::OpenPopup("BlitzX3D Update");
}

void App::drawUpdateDialog() {
	if (!updateOpen) return;
	ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Always);
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImVec2 center = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("BlitzX3D Update", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Text("A new version of BlitzX3D is available!");
		ImGui::Spacing();
		ImGui::TextWrapped("You are running %s. Version %s is available on GitHub.", BLITZIDE_VERSION, remoteVersion.c_str());
		ImGui::Spacing();
		bool ignore = false;
		ImGui::Checkbox("Don't remind me again for this version", &ignore);
		ImGui::Spacing();
		if (ImGui::Button("View Releases", ImVec2(-1, 0))) {
			App::openUrl(RELEASES_URL);
			if (ignore) { prefs.ignore_version_update = remoteVersion; prefs.close(); }
			updateOpen = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::Spacing();
		if (ImGui::Button("Later", ImVec2(-1, 0))) {
			if (ignore) { prefs.ignore_version_update = remoteVersion; prefs.close(); }
			updateOpen = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	else {
		updateOpen = false;
	}
}
