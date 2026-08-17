#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <string>

bool fileOpenDialog(std::string& path, const char* filter = "Blitz files (*.bb;*.ipf)|*.bb;*.ipf|All files (*.*)|*.*");
bool fileSaveDialog(std::string& path, const char* defaultName = "untitled.bb",
	const char* filter = "Blitz source (*.bb)|*.bb|All files (*.*)|*.*");

#endif
