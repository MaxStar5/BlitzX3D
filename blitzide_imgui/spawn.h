#ifndef SPAWN_H
#define SPAWN_H

#include <string>
#include <vector>

int runProcess(const std::vector<std::string>& args, std::string& output, int* exitCode = nullptr);

#endif
