#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#include "debugger.h"
#include "dbgipc.h"
#include "debugtree.h"
#include "profiler.h"

enum ELogSeverity
{
	LOG_INFO = 0,
	LOG_WARNING,
	LOG_ERROR
};

struct LogEntry {
	ELogSeverity severity;
	std::string text;
};

class MainFrame : public Debugger {

	ConstsTree consts_tree;
	GlobalsTree globals_tree;
	LocalsTree locals_tree;
	Profiler profiler;

	std::vector<LogEntry> m_logEntries;

	std::mutex dataMutex;

	std::atomic<int> state;
	std::atomic<int> step_level;
	int cur_pos;
	const char* cur_file;
	std::vector<std::string> call_stack;
	int last_obj_cnt, last_unrel_cnt, last_str_cnt;
	__int64 last_working_set_bytes;

	std::vector<DbgTreeNode> constsNodes, globalsNodes, localsNodes;
	std::vector<DbgFlameNode> flameNodes;
	std::vector<DbgProfilerRow> profilerRows;
	std::string profilerSummary;

	DWORD gameThreadId;
	HANDLE cmdThread;
	std::atomic<bool> cmdRunning;

	HANDLE shmFile;
	LPVOID shmView;
	DbgShm* shm;
	HANDLE cmdShmFile;
	LPVOID cmdShmView;
	DbgCmdShm* cmdShm;
	HANDLE snapEvent;
	HANDLE cmdEvent;

	DWORD lastProfilerSnap;
	DWORD lastFlameSnap;
	DWORD lastSnapshot;
	bool running;

	bool shouldRun()const { return step_level.load() < locals_tree.size(); }
	std::string buildCrashReport(const char* msg)const;

	void handleCommand(int cmd);
	void writeSnapshot();
	void snapshotProfiler();
	void snapshotFlame();
	static void buildTreesGuarded();
	void snapshotTrees();

public:
	MainFrame();
	~MainFrame();

	void start(void* mod, void* env);
	void cmdThreadLoop();

	void debugRun();
	void debugStop();
	bool debugStmt(int srcpos, const char* file);
	void debugEnter(void* frame, void* env, const char* func);
	void debugLeave();
	void debugLog(const char* msg);
	void debugMsg(const char* msg, bool serious);
	void debugSys(void* msg);

	void setRuntime(void* mod, void* env);

	void cmdStop();
	void cmdRun();
	void cmdStepOver();
	void cmdStepInto();
	void cmdStepOut();
	void cmdEnd();
};

extern MainFrame* g_mainFrame;

#endif
