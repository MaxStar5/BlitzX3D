#include "app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static bool hasArg(int argc, char* argv[], const char* arg) {
	for (int k = 1; k < argc; ++k)
		if (std::strcmp(argv[k], arg) == 0) return true;
	return false;
}

int main(int argc, char* argv[]) {
	bool skipPicker = hasArg(argc, argv, "--imgui") || ([]() {
		const char* e = std::getenv("BLITZ_IMGUI_IDE");
		return e && e[0] == '1';
	})();
	return App::run(argc, argv, skipPicker);
}
