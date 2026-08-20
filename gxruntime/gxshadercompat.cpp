#include "std.h"
#include "gxshadercompat.h"

#include "gxutf8.h"
#include <cstring>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string dirName(const std::string& path) {
	size_t pos = path.find_last_of("/\\");
	return pos == std::string::npos ? std::string() : path.substr(0, pos + 1);
}

static std::string joinPath(const std::string& dir, const std::string& name) {
	return dir.empty() ? name : dir + name;
}

static bool readTextFile(const std::string& path, std::string& out) {
	std::ifstream f(UTF8::toWide(path).c_str(), std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF)
		out.erase(0, 3);
	return true;
}

static bool hasWholeWord(const std::string& s, const char* word) {
	size_t n = std::strlen(word);
	for (size_t i = 0; i + n <= s.size(); ++i) {
		if (s.compare(i, n, word) == 0) {
			bool before = (i == 0) || !(std::isalnum((unsigned char)s[i - 1]) || s[i - 1] == '_');
			bool after = (i + n == s.size()) || !(std::isalnum((unsigned char)s[i + n]) || s[i + n] == '_');
			if (before && after) return true;
		}
	}
	return false;
}

static void replaceWholeWord(std::string& s, const char* word, const std::string& repl) {
	size_t n = std::strlen(word);
	std::string out;
	out.reserve(s.size());
	size_t i = 0;
	bool inBlock = false;
	while (i < s.size()) {
		if (s.compare(i, 2, "//") == 0) { out += s.substr(i); break; }
		if (!inBlock && s.compare(i, 2, "/*") == 0) { inBlock = true; out += "/*"; i += 2; continue; }
		if (inBlock) {
			size_t close = s.find("*/", i);
			if (close == std::string::npos) { out += s.substr(i); break; }
			out += s.substr(i, close - i + 2);
			i = close + 2;
			continue;
		}
		if (s.compare(i, n, word) == 0) {
			bool before = (i == 0) || !(std::isalnum((unsigned char)s[i - 1]) || s[i - 1] == '_');
			bool after = (i + n == s.size()) || !(std::isalnum((unsigned char)s[i + n]) || s[i + n] == '_');
			if (before && after) { out += repl; i += n; continue; }
		}
		out += s[i];
		++i;
	}
	s = out;
}

static void processSource(const std::string& src, const std::string& baseDir,
	std::vector<std::string>& visited, std::string& out) {
	std::istringstream in(src);
	std::string line;
	bool inBlock = false;
	while (std::getline(in, line)) {
		size_t i = 0;
		while (i < line.size()) {
			if (!inBlock) {
				if (line.compare(i, 2, "//") == 0) break;
				if (line.compare(i, 2, "/*") == 0) { inBlock = true; i += 2; continue; }
				++i;
			}
			else {
				size_t close = line.find("*/", i);
				if (close == std::string::npos) i = line.size();
				else { inBlock = false; i = close + 2; }
			}
		}

		bool isInclude = false;
		if (!inBlock) {
			std::string t = line;
			size_t p = t.find_first_not_of(" \t");
			if (p != std::string::npos && t.compare(p, 8, "#include") == 0) {
				std::string incName;
				size_t q = t.find('"', p + 8);
				if (q != std::string::npos) {
					size_t r = t.find('"', q + 1);
					if (r != std::string::npos) incName = t.substr(q + 1, r - q - 1);
				}
				else {
					q = t.find('<', p + 8);
					if (q != std::string::npos) {
						size_t r = t.find('>', q + 1);
						if (r != std::string::npos) incName = t.substr(q + 1, r - q - 1);
					}
				}
				if (!incName.empty()) {
					std::string incPath = joinPath(baseDir, incName);
					if (std::find(visited.begin(), visited.end(), incPath) == visited.end()) {
						visited.push_back(incPath);
						std::string incSrc;
						if (readTextFile(incPath, incSrc)) {
							std::string incOut;
							processSource(incSrc, dirName(incPath), visited, incOut);
							out += incOut;
							isInclude = true;
						}
					}
					else {
						isInclude = true;
					}
				}
			}
		}
		if (!isInclude) { out += line; out += "\n"; }
	}
}

static void removeViewportDecl(std::string& s) {
	size_t pos = 0;
	while ((pos = s.find("ViewportSize", pos)) != std::string::npos) {
		size_t ls = s.rfind('\n', pos);
		size_t lineStart = (ls == std::string::npos) ? 0 : ls + 1;
		size_t lineEnd = s.find('\n', pos);
		if (lineEnd == std::string::npos) lineEnd = s.size();
		std::string line = s.substr(lineStart, lineEnd - lineStart);
		size_t p = line.find_first_not_of(" \t\r");
		size_t p2 = line.find_first_of(" \t", p);
		if (p2 != std::string::npos && line.substr(p, p2 - p) == "float4") {
			s.erase(lineStart, lineEnd - lineStart);
			pos = lineStart;
			continue;
		}
		pos += 10;
	}
}

static void injectSamplerTexture(std::string& s) {
	std::string out;
	out.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		size_t sam = s.find("sampler", i);
		if (sam == std::string::npos) { out += s.substr(i); break; }

		{
			bool before = (sam == 0) || !(std::isalnum((unsigned char)s[sam - 1]) || s[sam - 1] == '_');
			size_t after = sam + 7;
			bool afterOk = (after >= s.size()) || !(std::isalnum((unsigned char)s[after]) || s[after] == '_');
			if (!before || !afterOk) { out += s.substr(i, sam + 7 - i); i = sam + 7; continue; }
		}
		{
			std::string prefix = s.substr(0, sam);
			size_t lastOpen = prefix.rfind("/*");
			size_t lastClose = prefix.rfind("*/");
			bool inBlock = (lastOpen != std::string::npos) && (lastClose == std::string::npos || lastOpen > lastClose);
			size_t lastNL = prefix.rfind('\n');
			size_t lastLineComment = prefix.rfind("//");
			bool afterLine = (lastLineComment != std::string::npos) && (lastLineComment > lastNL);
			if (inBlock || afterLine) { out += s.substr(i, sam + 7 - i); i = sam + 7; continue; }
		}
		size_t st = s.find("sampler_state", sam + 7);
		if (st == std::string::npos || st - sam > 64) { out += s.substr(i, sam + 7 - i); i = sam + 7; continue; }
		size_t open = s.find('{', st);
		if (open == std::string::npos) { out += s.substr(i, sam - i); i = sam; continue; }
		int depth = 0;
		size_t k = open;
		for (; k < s.size(); ++k) {
			if (s[k] == '{') ++depth;
			else if (s[k] == '}') { --depth; if (depth == 0) break; }
		}
		if (k >= s.size()) { out += s.substr(i); break; }
		std::string block = s.substr(open, k - open + 1);
		std::string low = block;
		std::transform(low.begin(), low.end(), low.begin(), (int(*)(int))std::tolower);
		bool hasTex = low.find("texture") != std::string::npos;
		if (!hasTex) {
			out += s.substr(i, open - i + 1);
			out += "\nTexture = <SceneTex>;";
			out += block.substr(1);
		}
		else {
			out += s.substr(i, k - i + 1);
		}
		i = k + 1;
	}
	s = out;
}

bool convertShaderSource(IDirect3DDevice9* dev, const std::string& filename, std::string& out) {
	std::string source;
	if (!readTextFile(filename, source)) return false;

	std::vector<std::string> visited;
	visited.push_back(filename);

	std::string inlined;
	processSource(source, dirName(filename), visited, inlined);

	bool newStyle = hasWholeWord(inlined, "VIEWPORT_SIZE");

	if (newStyle) {
		D3DVIEWPORT9 vp = {};
		if (dev && SUCCEEDED(dev->GetViewport(&vp)) && vp.Width > 0 && vp.Height > 0) {
			removeViewportDecl(inlined);
			char buf[128];
			sprintf(buf, "float4(0.0f, 0.0f, %u.0f, %u.0f)", vp.Width, vp.Height);
			replaceWholeWord(inlined, "ViewportSize", buf);
		}

		bool hasSceneTex = hasWholeWord(inlined, "SceneTex");
		injectSamplerTexture(inlined);
		if (!hasSceneTex) inlined = "texture SceneTex;\n" + inlined;

		if (hasWholeWord(inlined, "Gamma")) {
			inlined = "#define Gamma gammaValue\n" + inlined;
		}
	}

	out = inlined;
	return true;
}