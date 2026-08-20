#include <cstdio>
#include <string>
#include <windows.h>

int measureCodepoint(char chr) {
	if ((chr & 0x80) == 0x00) return 1;
	int len = 0;
	while (((chr >> (7 - len)) & 0x01) == 0x01) {
		len++;
		if (len > 8) return 8;
	}
	return len;
}

int decodeCharacter(const char* buf, int index) {
	int codepointLen = measureCodepoint(buf[index]);
	if (codepointLen == 1) return buf[index];
	int newChar = buf[index] & (0x7f >> codepointLen);
	for (int j = 1; j < codepointLen; j++)
		newChar = (newChar << 6) | (buf[index + j] & 0x3f);
	return newChar;
}

std::wstring convertToUtf16(const std::string& str) {
	std::wstring result;
	result.reserve(str.size());
	for (int i = 0; i < (int)str.size();) {
		int codepoint = decodeCharacter(str.c_str(), i);
		i += measureCodepoint(str[i]);
		if (codepoint > 0xFFFF) {
			// needs a surrogate pair
			codepoint -= 0x10000;
			result.push_back((wchar_t)(0xD800 + (codepoint >> 10)));
			result.push_back((wchar_t)(0xDC00 + (codepoint & 0x3FF)));
		} else {
			result.push_back((wchar_t)codepoint);
		}
	}
	return result;
}

void test(const char* in, const wchar_t* expected, const char* name) {
	std::wstring out = convertToUtf16(in);
	if (out != expected) {
		printf("FAIL %s\n", name);
		return;
	}
	// a terrible way to do this but one nonetheless
	printf("PASS %s\n", name);
}

int main() {
	test("ABC", L"ABC", "ascii");
	test("\xC3\xA9", L"\x00E9", "latin");
	test("\xE4\xB8\xAD", L"\x4E2D", "cjk");
	test("\xF0\x9F\x98\x80", L"\xD83D\xDE00", "emoji");
	test("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", L"\x041F\x0440\x0438\x0432\x0435\x0442", "russian");
	test("\xD0\x81", L"\x0401", "russian-yo");
	return 0;
}
