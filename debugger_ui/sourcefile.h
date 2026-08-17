#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include <string>

#include "TextEditor.h"

class SourceFile {
	std::string path, name;
	TextEditor editor;
	bool loaded;

public:
	SourceFile();
	~SourceFile();

	void load(const char* file);
	void highLight(int row, int col);
	void render(const char* tabId);

	const std::string& getPath()const { return path; }
	const std::string& getName()const { return name; }
};

#endif
