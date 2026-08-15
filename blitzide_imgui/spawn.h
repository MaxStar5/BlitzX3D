#ifndef SPAWN_H
#define SPAWN_H

#include <string>

int runProcess(const std::string& cmd, std::string& output, int* exitCode = nullptr);

#endif
