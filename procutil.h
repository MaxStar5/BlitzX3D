#ifndef PROCUTIL_H
#define PROCUTIL_H

#include <string>
#include <vector>

inline std::vector<std::string> splitCommandLine(const std::string& s) {
	std::vector<std::string> out;
	std::string cur;
	bool inQuote = false;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (c == '"') { inQuote = !inQuote; continue; }
		if ((c == ' ' || c == '\t') && !inQuote) {
			if (!cur.empty()) { out.push_back(cur); cur.clear(); }
			continue;
		}
		cur += c;
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

#if defined(_WIN32)

inline std::string quoteWindowsArg(const std::string& arg) {
	if (arg.empty()) return "\"\"";
	if (arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
	std::string out = "\"";
	for (size_t i = 0; i < arg.size(); ++i) {
		size_t bs = 0;
		while (i < arg.size() && arg[i] == '\\') { ++bs; ++i; }
		if (i == arg.size()) {
			out.append(bs * 2, '\\');
			break;
		}
		if (arg[i] == '"') {
			out.append(bs * 2 + 1, '\\');
			out += '"';
		}
		else {
			out.append(bs, '\\');
			out += arg[i];
		}
	}
	out += '"';
	return out;
}

inline std::string buildWindowsCommandLine(const std::vector<std::string>& args) {
	std::string cmd;
	for (size_t k = 0; k < args.size(); ++k) {
		if (k) cmd += ' ';
		cmd += quoteWindowsArg(args[k]);
	}
	return cmd;
}

#endif

#endif
