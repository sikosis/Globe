#ifndef GLOBE_SYNTAX_HIGHLIGHTER_H
#define GLOBE_SYNTAX_HIGHLIGHTER_H

#include <cstddef>
#include <string>
#include <vector>

enum class SyntaxLanguage {
	PlainText,
	Html,
	Php,
	Rdef,
	Cpp
};

enum class SyntaxKind {
	Tag,
	Attribute,
	String,
	Comment,
	Entity,
	Keyword,
	Variable,
	Number
};

struct SyntaxSpan {
	std::size_t start;
	std::size_t length;
	SyntaxKind kind;
};

std::vector<SyntaxSpan> BuildSyntaxSpans(const std::string& text,
	SyntaxLanguage language);

#endif
