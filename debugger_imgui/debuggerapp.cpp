#include "stdafx.h"
#include "debugger.h"
#include "mainframe.h"

Debugger* _cdecl debuggerGetDebugger(void* mod, void* env) {
	if (!g_mainFrame) {
		g_mainFrame = new MainFrame();
	}
	g_mainFrame->start(mod, env);
	return g_mainFrame;
}
