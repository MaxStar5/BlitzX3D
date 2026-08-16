#ifndef PREFS_H
#define PREFS_H

#include <string>
#include <vector>

class Prefs {
public:
	int rgb_bkgrnd[3] = { 34, 85, 136 };
	int rgb_string[3] = { 0, 255, 102 };
	int rgb_ident[3] = { 255, 255, 255 };
	int rgb_keyword[3] = { 170, 255, 255 };
	int rgb_comment[3] = { 255, 238, 0 };
	int rgb_digit[3] = { 51, 255, 221 };
	int rgb_default[3] = { 238, 238, 238 };

	std::string font_debug = "consolas";
	int font_debug_height = 14;

	std::string homeDir;

	void open();
};

extern Prefs prefs;

#endif
