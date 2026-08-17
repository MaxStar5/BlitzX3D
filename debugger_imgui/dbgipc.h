#ifndef DBGIPC_H
#define DBGIPC_H

#include <string>
#include <vector>
#include <cstring>
#include <cstdint>

enum {
	DBG_MSG_STATE = 1,
	DBG_MSG_STMT = 2,
	DBG_MSG_LOG = 3,
	DBG_MSG_TREES = 4,
	DBG_MSG_PROFILER = 5,
	DBG_MSG_FLAME = 6,
	DBG_MSG_MEM = 7,
	DBG_MSG_META = 8
};

enum {
	DBG_CMD_RUN = 1,
	DBG_CMD_STOP = 2,
	DBG_CMD_STEPOVER = 3,
	DBG_CMD_STEPINTO = 4,
	DBG_CMD_STEPOUT = 5,
	DBG_CMD_END = 6,
	DBG_REQ_TREES = 7,
	DBG_REQ_PROFILER = 8,
	DBG_REQ_FLAME = 9
};

enum {
	DBG_STATE_STARTING = 0,
	DBG_STATE_RUNNING = 1,
	DBG_STATE_STOPPED = 2,
	DBG_STATE_ENDING = 3
};

static const int DBG_FRAME_HEADER = 5;

class DbgSerializer {
	std::vector<unsigned char> buf;

public:
	DbgSerializer() {}
	DbgSerializer(const unsigned char* data, int size) :buf(data, data + size) { pos = 0; }

	int pos = 0;

	void writeByte(unsigned char v) { buf.push_back(v); }
	void writeInt(int v) {
		unsigned char b[4];
		std::memcpy(b, &v, 4);
		for (int k = 0; k < 4; ++k) buf.push_back(b[k]);
	}
	void writeInt64(std::int64_t v) {
		unsigned char b[8];
		std::memcpy(b, &v, 8);
		for (int k = 0; k < 8; ++k) buf.push_back(b[k]);
	}
	void writeDouble(double v) {
		unsigned char b[8];
		std::memcpy(b, &v, 8);
		for (int k = 0; k < 8; ++k) buf.push_back(b[k]);
	}
	void writeStr(const std::string& s) {
		writeInt((int)s.size());
		for (size_t k = 0; k < s.size(); ++k) buf.push_back((unsigned char)s[k]);
	}

	bool readByte(unsigned char& v) {
		if (pos + 1 > (int)buf.size()) return false;
		v = buf[pos++];
		return true;
	}
	bool readInt(int& v) {
		if (pos + 4 > (int)buf.size()) return false;
		std::memcpy(&v, &buf[pos], 4);
		pos += 4;
		return true;
	}
	bool readInt64(std::int64_t& v) {
		if (pos + 8 > (int)buf.size()) return false;
		std::memcpy(&v, &buf[pos], 8);
		pos += 8;
		return true;
	}
	bool readDouble(double& v) {
		if (pos + 8 > (int)buf.size()) return false;
		std::memcpy(&v, &buf[pos], 8);
		pos += 8;
		return true;
	}
	bool readStr(std::string& s) {
		int n = 0;
		if (!readInt(n)) return false;
		if (n < 0 || pos + n > (int)buf.size()) return false;
		s.assign((const char*)&buf[pos], n);
		pos += n;
		return true;
	}

	const unsigned char* data() const { return buf.data(); }
	int size() const { return (int)buf.size(); }
	bool readOk() const { return pos <= (int)buf.size(); }
};


struct DbgTreeNode {
	std::string id;
	std::string label;
	bool expandable = false;
	std::vector<DbgTreeNode> children;
};

static void writeTree(DbgSerializer& s, const DbgTreeNode& n) {
	s.writeByte(n.expandable ? 1 : 0);
	s.writeStr(n.id);
	s.writeStr(n.label);
	s.writeInt((int)n.children.size());
	for (const DbgTreeNode& c : n.children) writeTree(s, c);
}

static bool readTree(DbgSerializer& s, DbgTreeNode& n) {
	unsigned char ex = 0;
	if (!s.readByte(ex)) return false;
	n.expandable = ex != 0;
	if (!s.readStr(n.id)) return false;
	if (!s.readStr(n.label)) return false;
	int cnt = 0;
	if (!s.readInt(cnt)) return false;
	if (cnt < 0 || cnt > 100000) return false;
	n.children.resize(cnt);
	for (int k = 0; k < cnt; ++k) {
		if (!readTree(s, n.children[k])) return false;
	}
	return true;
}


struct DbgFlameNode {
	std::string name;
	int samples;
	int selfSamples;
	std::vector<DbgFlameNode> children;
};

static void writeFlameNode(DbgSerializer& s, const DbgFlameNode& n) {
	s.writeStr(n.name);
	s.writeInt(n.samples);
	s.writeInt(n.selfSamples);
	s.writeInt((int)n.children.size());
	for (const DbgFlameNode& c : n.children) writeFlameNode(s, c);
}

static bool readFlameNode(DbgSerializer& s, DbgFlameNode& n) {
	if (!s.readStr(n.name)) return false;
	if (!s.readInt(n.samples)) return false;
	if (!s.readInt(n.selfSamples)) return false;
	int cnt = 0;
	if (!s.readInt(cnt)) return false;
	if (cnt < 0 || cnt > 100000) return false;
	n.children.resize(cnt);
	for (int k = 0; k < cnt; ++k) {
		if (!readFlameNode(s, n.children[k])) return false;
	}
	return true;
}


struct DbgProfilerRow {
	std::string func;
	double selfMs;
	double totalMs;
	double maxMs;
	int callCount;
	int netObjDelta;
	int netStrDelta;
};

static std::string pipeNameForPid(int pid) {
	char buf[64];
	sprintf(buf, "\\\\.\\pipe\\blitzdbg_%d", pid);
	return std::string(buf);
}

static const int DBG_SHM_SIZE = 32 * 1024 * 1024;

struct DbgShm {
	volatile LONG snapSeq;
	int payloadSize;
	volatile LONG cmdSeq;
	int cmd;
	unsigned char payload[1];
};

struct DbgCmdShm {
	volatile LONG seq;
	int cmd;
};

static std::string shmNameForPid(int pid) {
	char buf[64];
	sprintf(buf, "Local\\blitzdbg_shm_%d", pid);
	return std::string(buf);
}

static std::string cmdShmNameForPid(int pid) {
	char buf[64];
	sprintf(buf, "Local\\blitzdbg_cmdshm_%d", pid);
	return std::string(buf);
}

static std::string snapEventForPid(int pid) {
	char buf[64];
	sprintf(buf, "Local\\blitzdbg_snap_%d", pid);
	return std::string(buf);
}

static std::string cmdEventForPid(int pid) {
	char buf[64];
	sprintf(buf, "Local\\blitzdbg_cmd_%d", pid);
	return std::string(buf);
}

#endif
