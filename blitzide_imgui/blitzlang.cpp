#include "blitzlang.h"

#include <algorithm>
#include <cctype>
#include <cstring>

static bool isid(int c) {
	return std::isalnum((unsigned char)c) || c == '_';
}

static bool tokenizeBlitzString(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end) {
	const char* p = in_begin;
	if (*p != '"') return false;
	++p;
	while (p < in_end) {
		if (*p == '"') {
			if (p + 1 < in_end && p[1] == '"') { p += 2; continue; }
			out_begin = in_begin;
			out_end = p + 1;
			return true;
		}
		++p;
	}
	out_begin = in_begin;
	out_end = in_end;
	return true;
}

static bool tokenizeBlitzHex(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end) {
	const char* p = in_begin;
	if (*p != '$') return false;
	++p;
	if (p >= in_end || !std::isxdigit((unsigned char)*p)) return false;
	while (p < in_end && std::isxdigit((unsigned char)*p)) ++p;
	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool tokenizeBlitzNumber(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end) {
	const char* p = in_begin;
	bool hasDigit = false;
	while (p < in_end && std::isdigit((unsigned char)*p)) { ++p; hasDigit = true; }
	if (p < in_end && *p == '.') {
		const char* q = p + 1;
		while (q < in_end && std::isdigit((unsigned char)*q)) ++q;
		if (q > p + 1) { p = q; hasDigit = true; }
	}
	if (!hasDigit) return false;
	out_begin = in_begin;
	out_end = p;
	return true;
}

static bool tokenizeBlitzIdent(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end) {
	const char* p = in_begin;
	if (!std::isalpha((unsigned char)*p) && *p != '_') return false;
	++p;
	while (p < in_end && isid(*p)) ++p;
	if (p < in_end && (*p == '$' || *p == '#' || *p == '%')) ++p;
	out_begin = in_begin;
	out_end = p;
	return true;
}

const std::set<std::string>& builtinBlitzKeywords() {
	static const std::set<std::string> kws = {
		"dim", "addressof", "then", "if", "and", "goto", "return", "gosub", "float",
		"read", "exit", "else", "select", "endif", "function", "after", "include",
		"field", "elseif", "while", "wend", "for", "str", "to", "until", "step",
		"insert", "each", "next", "or", "continue", "pi", "type", "int", "default",
		"local", "global", "const", "case", "repeat", "forever", "true", "data",
		"new", "restore", "offsetof", "abs", "mod", "sgn", "false", "enum", "delete",
		"first", "last", "before", "lor", "object", "handle", "xor", "not", "shl",
		"shr", "sar", "null", "powtwo", "infinity"
	};
	return kws;
}

TextEditor::LanguageDefinition makeBlitzLangDef(const std::set<std::string>& keywords, const std::set<std::string>& engineFuncs, const std::set<std::string>& customFuncs, const std::set<std::string>& globals, const std::set<std::string>& consts) {
	TextEditor::LanguageDefinition langDef;

	langDef.mName = "Blitz";
	langDef.mCaseSensitive = false;
	langDef.mAutoIndentation = true;
	langDef.mSingleLineComment = ";";
	langDef.mCommentStart = "\x01";
	langDef.mCommentEnd = "\x02";
	langDef.mPreprocChar = '#';

	std::set<std::string> all = builtinBlitzKeywords();
	for (const auto& k : keywords) all.insert(k);
	for (auto& k : all) {
		std::string t = k;
		std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::toupper(c); });
		langDef.mKeywords.insert(t);
	}

	for (const auto& f : engineFuncs) {
		std::string t = f;
		std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::toupper(c); });
		TextEditor::Identifier id;
		id.mDeclaration = "Function";
		langDef.mIdentifiers.insert(std::make_pair(t, id));
	}

	for (const auto& f : customFuncs) {
		std::string t = f;
		std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::toupper(c); });
		TextEditor::Identifier id;
		id.mDeclaration = "Function";
		langDef.mIdentifiers.insert(std::make_pair(t, id));
	}

	for (const auto& g : globals) {
		std::string t = g;
		std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::toupper(c); });
		langDef.mGlobals.insert(t);
	}

	for (const auto& c : consts) {
		std::string t = c;
		std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return std::toupper(c); });
		langDef.mConsts.insert(t);
	}

	langDef.mTokenize = [](const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, TextEditor::PaletteIndex& paletteIndex) -> bool {
		paletteIndex = TextEditor::PaletteIndex::Max;

		while (in_begin < in_end && std::isblank((unsigned char)*in_begin)) ++in_begin;
		if (in_begin == in_end) {
			out_begin = in_end;
			out_end = in_end;
			paletteIndex = TextEditor::PaletteIndex::Default;
			return true;
		}
		if (*in_begin == ';') {
			out_begin = in_begin;
			out_end = in_end;
			paletteIndex = TextEditor::PaletteIndex::Comment;
			return true;
		}
		else if (tokenizeBlitzString(in_begin, in_end, out_begin, out_end)) {
			paletteIndex = TextEditor::PaletteIndex::String;
		}
		else if (tokenizeBlitzHex(in_begin, in_end, out_begin, out_end)) {
			paletteIndex = TextEditor::PaletteIndex::Number;
		}
		else if (tokenizeBlitzNumber(in_begin, in_end, out_begin, out_end)) {
			paletteIndex = TextEditor::PaletteIndex::Number;
		}
		else if (tokenizeBlitzIdent(in_begin, in_end, out_begin, out_end)) {
			paletteIndex = TextEditor::PaletteIndex::Identifier;
		}
		else {
			out_begin = in_begin;
			out_end = in_begin + 1;
			paletteIndex = TextEditor::PaletteIndex::Default;
		}
		return true;
	};

	return langDef;
}
