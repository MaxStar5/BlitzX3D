#ifndef DEBUGTREE_H
#define DEBUGTREE_H

#include <string>
#include <vector>

#include "dbgipc.h"
#include "../linker/linker.h"
#include "../compiler/environ.h"
#include "../bbruntime/basic.h"

class DebugTree {
public:
	typedef DbgTreeNode Node;

protected:
	struct ArrayInfo {
		bool isVector;
		void* buffer;
		Type* elemType;
		std::vector<int> sizes;
	};

	std::vector<Node> nodes;
	int st_nest;

	static bool readArray(void* var, Type* t, ArrayInfo& out);
	static std::string arrayTag(const ArrayInfo& ai);
	void buildVar(void* var, Decl* d, const std::string& name, std::vector<Node>& out);
	void buildElementNode(void* var, Type* t, const std::string& id, Node& cn);
	void buildArrayChildren(const ArrayInfo& ai, std::vector<Node>& out);
	void buildStructChildren(void* structVar, StructType* st, std::vector<Node>& out);

public:
	DebugTree();
	virtual ~DebugTree();

	void clearNodes() { nodes.clear(); }
	virtual void buildCache() { nodes.clear(); }
	const std::vector<Node>& getNodes()const { return nodes; }
};

class ConstsTree : public DebugTree {
	Environ* envron;

public:
	ConstsTree();
	void reset(Environ* env);
	void buildCache();
};

class GlobalsTree : public DebugTree {
	Module* module;
	Environ* envron;

public:
	GlobalsTree();
	void reset(Module* mod, Environ* env);
	void buildCache();
};

class LocalsTree : public DebugTree {
	struct Frame {
		void* frame;
		Environ* env;
		std::string func;
		Frame(void* f, Environ* e, const char* fn) :frame(f), env(e), func(fn) {}
	};
	std::vector<Frame> frames;

public:
	LocalsTree();
	void reset(Environ* env);
	void buildCache();
	void pushFrame(void* frame, void* env, const char* func);
	void popFrame();
	int size()const { return (int)frames.size(); }
};

#endif
