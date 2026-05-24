/* Win32 runtime dynamic link lib */

#ifndef BBRUNTIME_DLL_H
#define BBRUNTIME_DLL_H

#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define POINTER_64 __ptr64

#include <Windows.h>

#include "../stdutil/stdutil.h"

class Debugger;

class Runtime {
public:
	virtual int version();
	virtual const char* nextSym();
	virtual int symValue(const char* sym);
	virtual void startup(HINSTANCE hinst);
	virtual void shutdown();
	virtual void asyncStop();
	virtual void asyncRun();
	virtual void asyncEnd();
	virtual void checkmem(std::streambuf* buf);

	virtual void execute(void (*pc)(), const char* args, Debugger* dbg);
};

extern "C" _declspec(dllexport) Runtime * _cdecl runtimeGetRuntime();

#endif