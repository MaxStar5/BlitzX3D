#ifndef BLITZLANG_H
#define BLITZLANG_H

#include "TextEditor.h"
#include <set>
#include <string>

TextEditor::LanguageDefinition makeBlitzLangDef(const std::set<std::string>& keywords, const std::set<std::string>& engineFuncs, const std::set<std::string>& customFuncs);
const std::set<std::string>& builtinBlitzKeywords();

#endif
