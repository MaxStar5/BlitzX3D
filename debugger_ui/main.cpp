#include "stdafx.h"
#include "app.h"

int main(int argc, char* argv[]) {
	int pid = 0;
	if (argc > 1) pid = atoi(argv[1]);

	App app;
	g_app = &app;
	if (!app.init(pid)) return 1;
	app.run();
	app.shutdown();
	g_app = nullptr;
	return 0;
}
