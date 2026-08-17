#ifndef STDAFX_H
#define STDAFX_H

#define _WIN32_WINNT 0x601

#pragma warning(disable:4786)
#pragma warning(disable:4244)
#pragma warning(disable:4267)

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <map>
#include <set>
#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

int atoi(const std::string& s);
double atof(const std::string& s);
std::string itoa(int n);
std::string ftoa(float n);
std::string tolower(const std::string& s);
std::string toupper(const std::string& s);
std::string fullfilename(const std::string& t);
std::string filenamepath(const std::string& t);
std::string filenamefile(const std::string& t);

#endif
