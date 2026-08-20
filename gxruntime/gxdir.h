#ifndef GXDIR_H
#define GXDIR_H

#include <string>
#include <Windows.h>

class gxDir {
public:
	gxDir(HANDLE h, const WIN32_FIND_DATAW& f);
	~gxDir();

private:
	HANDLE handle;
	WIN32_FIND_DATAW findData;

	/***** GX INTERFACE *****/
public:
	std::string getNextFile();
};

#endif