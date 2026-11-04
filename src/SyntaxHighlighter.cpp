#include "SyntaxHighlighter.h"

#include <cctype>
#include <cstring>
#include <unordered_set>

namespace {

bool
IsIdentifierStart(char character) {
	unsigned char value = static_cast<unsigned char>(character);
	return std::isalpha(value) || character == '_';
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsIdentifierCharacter(char character) {
	unsigned char value = static_cast<unsigned char>(character);
	return std::isalnum(value) || character == '_';
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsHtmlNameCharacter(char character) {
	return IsIdentifierCharacter(character) || character == '-'
		|| character == ':';
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
StartsWith(const std::string& text, std::size_t offset, const char* value) {
	std::size_t length = std::strlen(value);
	return offset + length <= text.size()
		&& text.compare(offset, length, value) == 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
AddSpan(std::vector<SyntaxSpan>& spans, std::size_t start,
	std::size_t end, SyntaxKind kind) {
	if (end > start)
		spans.push_back({start, end - start, kind});
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsPhpKeyword(const std::string& word) {
	static const std::unordered_set<std::string> keywords = {
		"abstract", "and", "array", "as", "break", "callable", "case",
		"catch", "class", "clone", "const", "continue", "declare",
		"default", "do", "echo", "else", "elseif", "empty", "enddeclare",
		"endfor", "endforeach", "endif", "endswitch", "endwhile", "enum",
		"eval", "exit", "extends", "final", "finally", "fn", "for",
		"foreach", "function", "global", "goto", "if", "implements",
		"include", "include_once", "instanceof", "insteadof", "interface",
		"isset", "list", "match", "namespace", "new", "or", "print",
		"private", "protected", "public", "readonly", "require",
		"require_once", "return", "static", "switch", "throw", "trait",
		"try", "unset", "use", "var", "while", "xor", "yield", "true",
		"false", "null"
	};
	return keywords.find(word) != keywords.end();
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsRdefKeyword(const std::string& word) {
	static const std::unordered_set<std::string> keywords = {
		"archive", "array", "enum", "false", "import", "message",
		"resource", "true"
	};
	return keywords.find(word) != keywords.end()
		|| (word.size() > 2 && word[0] == 'B' && word[1] == '_');
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsCppKeyword(const std::string& word) {
	static const std::unordered_set<std::string> keywords = {
		"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
		"bitor", "bool", "break", "case", "catch", "char", "char8_t",
		"char16_t", "char32_t", "class", "compl", "concept", "const",
		"consteval", "constexpr", "constinit", "const_cast", "continue",
		"co_await", "co_return", "co_yield", "decltype", "default",
		"delete", "do", "double", "dynamic_cast", "else", "enum",
		"explicit", "export", "extern", "false", "float", "for", "friend",
		"goto", "if", "inline", "int", "long", "mutable", "namespace",
		"new", "noexcept", "not", "not_eq", "nullptr", "operator", "or",
		"or_eq", "private", "protected", "public", "register",
		"reinterpret_cast", "requires", "return", "short", "signed",
		"sizeof", "static", "static_assert", "static_cast", "struct",
		"switch", "template", "this", "thread_local", "throw", "true",
		"try", "typedef", "typeid", "typename", "union", "unsigned",
		"using", "virtual", "void", "volatile", "wchar_t", "while", "xor",
		"xor_eq"
	};
	return keywords.find(word) != keywords.end();
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsCppType(const std::string& word) {
	static const std::unordered_set<std::string> types = {
		"bool", "char", "char8_t", "char16_t", "char32_t", "double",
		"float", "int", "long", "short", "signed", "unsigned", "void",
		"wchar_t", "auto", "size_t", "ssize_t", "ptrdiff_t", "int8",
		"int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
		"off_t", "status_t", "bigtime_t"
	};
	return types.find(word) != types.end();
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsCppConstant(const std::string& word) {
	bool hasLetter = false;
	for (char character : word) {
		if (std::islower(static_cast<unsigned char>(character)))
			return false;
		if (std::isupper(static_cast<unsigned char>(character)))
			hasLetter = true;
	}
	return hasLetter && word.find('_') != std::string::npos;
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
IsMakeDirective(const std::string& word) {
	static const std::unordered_set<std::string> directives = {
		"define", "else", "endef", "endif", "export", "ifdef", "ifeq",
		"ifndef", "ifneq", "include", "-include", "override", "private",
		"sinclude", "undefine", "unexport", "vpath"
	};
	return directives.find(word) != directives.end();
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::size_t
FirstNonWhitespace(const std::string& text, std::size_t start,
	std::size_t end) {
	while (start < end
		&& std::isspace(static_cast<unsigned char>(text[start]))) {
		++start;
	}
	return start;
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::size_t
LastNonWhitespace(const std::string& text, std::size_t start,
	std::size_t end) {
	while (end > start
		&& std::isspace(static_cast<unsigned char>(text[end - 1]))) {
		--end;
	}
	return end;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
HighlightMakefileRange(const std::string& text, std::size_t start,
	std::size_t end, std::vector<SyntaxSpan>& spans) {
	std::size_t lineStart = start;
	while (lineStart < end) {
		std::size_t lineEnd = text.find('\n', lineStart);
		if (lineEnd == std::string::npos || lineEnd > end)
			lineEnd = end;
		std::size_t contentStart = FirstNonWhitespace(text, lineStart, lineEnd);

		std::size_t wordEnd = contentStart;
		while (wordEnd < lineEnd
			&& !std::isspace(static_cast<unsigned char>(text[wordEnd]))) {
			++wordEnd;
		}
		std::string firstWord = text.substr(contentStart, wordEnd - contentStart);
		if (IsMakeDirective(firstWord))
			AddSpan(spans, contentStart, wordEnd, SyntaxKind::Keyword);

		if (contentStart < lineEnd && text[lineStart] != '\t') {
			std::size_t assignment = std::string::npos;
			const char* operators[] = {"::=", ":=", "?=", "+=", "!=", "="};
			for (const char* assignmentOperator : operators) {
				std::size_t found = text.find(assignmentOperator, contentStart);
				if (found < lineEnd
					&& (assignment == std::string::npos || found < assignment)) {
					assignment = found;
				}
			}
			std::size_t colon = text.find(':', contentStart);
			if (assignment != std::string::npos
				&& (colon == std::string::npos || colon >= assignment)) {
				std::size_t variableStart = contentStart;
				if (IsMakeDirective(firstWord))
					variableStart = FirstNonWhitespace(text, wordEnd, assignment);
				std::size_t variableEnd = LastNonWhitespace(text, variableStart,
					assignment);
				AddSpan(spans, variableStart, variableEnd, SyntaxKind::Variable);
				std::size_t operatorEnd = assignment + 1;
				while (operatorEnd < lineEnd
					&& (text[operatorEnd] == ':' || text[operatorEnd] == '?'
						|| text[operatorEnd] == '+' || text[operatorEnd] == '!'
						|| text[operatorEnd] == '=')) {
					++operatorEnd;
				}
				AddSpan(spans, assignment, operatorEnd, SyntaxKind::Keyword);
			} else if (colon < lineEnd) {
				std::size_t targetEnd = LastNonWhitespace(text, contentStart, colon);
				bool isSpecialTarget = text.compare(contentStart,
					targetEnd - contentStart, ".PHONY") == 0;
				SyntaxKind targetKind = isSpecialTarget
					? SyntaxKind::Preprocessor : SyntaxKind::Function;
				AddSpan(spans, contentStart, targetEnd, targetKind);
			}
		}

		std::size_t offset = contentStart;
		while (offset < lineEnd) {
			if (text[offset] == '#'
				&& (offset == contentStart || text[offset - 1] != '\\')) {
				AddSpan(spans, offset, lineEnd, SyntaxKind::Comment);
				break;
			}
			if (text[offset] == '$' && offset + 1 < lineEnd) {
				std::size_t variableEnd = offset + 2;
				if (text[offset + 1] == '(' || text[offset + 1] == '{') {
					char closing = text[offset + 1] == '(' ? ')' : '}';
					std::size_t closingOffset = text.find(closing, offset + 2);
					variableEnd = closingOffset < lineEnd
						? closingOffset + 1 : lineEnd;
				}
				AddSpan(spans, offset, variableEnd, SyntaxKind::Variable);
				offset = variableEnd;
				continue;
			}
			if (text[offset] == '\'' || text[offset] == '"') {
				char quote = text[offset];
				std::size_t stringEnd = offset + 1;
				while (stringEnd < lineEnd) {
					if (text[stringEnd] == '\\' && stringEnd + 1 < lineEnd) {
						stringEnd += 2;
						continue;
					}
					if (text[stringEnd++] == quote)
						break;
				}
				AddSpan(spans, offset, stringEnd, SyntaxKind::String);
				offset = stringEnd;
				continue;
			}
			if (std::isdigit(static_cast<unsigned char>(text[offset]))) {
				std::size_t numberEnd = offset + 1;
				while (numberEnd < lineEnd
					&& std::isdigit(static_cast<unsigned char>(text[numberEnd]))) {
					++numberEnd;
				}
				AddSpan(spans, offset, numberEnd, SyntaxKind::Number);
				offset = numberEnd;
				continue;
			}
			++offset;
		}
		lineStart = lineEnd < end ? lineEnd + 1 : end;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
HighlightCppRange(const std::string& text, std::size_t start,
	std::size_t end, std::vector<SyntaxSpan>& spans) {
	std::size_t offset = start;
	while (offset < end) {
		if (offset + 1 < end && text[offset] == '/' && text[offset + 1] == '/') {
			std::size_t commentEnd = text.find('\n', offset + 2);
			if (commentEnd == std::string::npos || commentEnd > end)
				commentEnd = end;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (offset + 1 < end && text[offset] == '/' && text[offset + 1] == '*') {
			std::size_t commentEnd = text.find("*/", offset + 2);
			if (commentEnd == std::string::npos || commentEnd + 2 > end)
				commentEnd = end;
			else
				commentEnd += 2;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (text[offset] == '#') {
			std::size_t directiveEnd = offset + 1;
			while (directiveEnd < end
				&& std::isspace(static_cast<unsigned char>(text[directiveEnd]))) {
				++directiveEnd;
			}
			while (directiveEnd < end
				&& IsIdentifierCharacter(text[directiveEnd])) {
				++directiveEnd;
			}
			AddSpan(spans, offset, directiveEnd, SyntaxKind::Preprocessor);
			offset = directiveEnd;
			continue;
		}
		if (text[offset] == '<') {
			std::size_t lineStart = text.rfind('\n', offset);
			std::size_t includeStart = lineStart == std::string::npos
				? 0 : lineStart + 1;
			if (text.find("#include", includeStart) < offset) {
				std::size_t includeEnd = text.find('>', offset + 1);
				if (includeEnd == std::string::npos || includeEnd >= end)
					includeEnd = end;
				else
					++includeEnd;
				AddSpan(spans, offset, includeEnd, SyntaxKind::String);
				offset = includeEnd;
				continue;
			}
		}
		if (text[offset] == '\'' || text[offset] == '"') {
			char quote = text[offset];
			std::size_t stringEnd = offset + 1;
			while (stringEnd < end) {
				if (text[stringEnd] == '\\' && stringEnd + 1 < end) {
					stringEnd += 2;
					continue;
				}
				if (text[stringEnd++] == quote)
					break;
			}
			AddSpan(spans, offset, stringEnd, SyntaxKind::String);
			offset = stringEnd;
			continue;
		}
		if (IsIdentifierStart(text[offset])) {
			std::size_t wordEnd = offset + 1;
			while (wordEnd < end && IsIdentifierCharacter(text[wordEnd]))
				++wordEnd;
			std::string word = text.substr(offset, wordEnd - offset);
			std::size_t next = wordEnd;
			while (next < end
				&& std::isspace(static_cast<unsigned char>(text[next]))) {
				++next;
			}
			if (IsCppType(word))
				AddSpan(spans, offset, wordEnd, SyntaxKind::Type);
			else if (IsCppKeyword(word))
				AddSpan(spans, offset, wordEnd, SyntaxKind::Keyword);
			else if (IsCppConstant(word))
				AddSpan(spans, offset, wordEnd, SyntaxKind::Entity);
			else if (next < end && text[next] == '(')
				AddSpan(spans, offset, wordEnd, SyntaxKind::Function);
			else if ((word.size() > 1 && word[0] == 'B'
				&& std::isupper(static_cast<unsigned char>(word[1])))
				|| std::isupper(static_cast<unsigned char>(word[0])))
				AddSpan(spans, offset, wordEnd, SyntaxKind::Type);
			offset = wordEnd;
			continue;
		}
		if (std::isdigit(static_cast<unsigned char>(text[offset]))) {
			std::size_t numberEnd = offset + 1;
			while (numberEnd < end
				&& (std::isalnum(static_cast<unsigned char>(text[numberEnd]))
					|| text[numberEnd] == '.' || text[numberEnd] == '_')) {
				++numberEnd;
			}
			AddSpan(spans, offset, numberEnd, SyntaxKind::Number);
			offset = numberEnd;
			continue;
		}
		++offset;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
HighlightRdefRange(const std::string& text, std::size_t start,
	std::size_t end, std::vector<SyntaxSpan>& spans) {
	std::size_t offset = start;
	bool expectResourceName = false;
	while (offset < end) {
		if (offset + 1 < end && text[offset] == '/' && text[offset + 1] == '/') {
			std::size_t commentEnd = text.find('\n', offset + 2);
			if (commentEnd == std::string::npos || commentEnd > end)
				commentEnd = end;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (offset + 1 < end && text[offset] == '/' && text[offset + 1] == '*') {
			std::size_t commentEnd = text.find("*/", offset + 2);
			if (commentEnd == std::string::npos || commentEnd + 2 > end)
				commentEnd = end;
			else
				commentEnd += 2;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (text[offset] == '\'' || text[offset] == '"') {
			char quote = text[offset];
			std::size_t stringEnd = offset + 1;
			while (stringEnd < end) {
				if (text[stringEnd] == '\\' && stringEnd + 1 < end) {
					stringEnd += 2;
					continue;
				}
				if (text[stringEnd++] == quote)
					break;
			}
			AddSpan(spans, offset, stringEnd, SyntaxKind::String);
			offset = stringEnd;
			continue;
		}
		if (text[offset] == '#') {
			std::size_t directiveEnd = offset + 1;
			while (directiveEnd < end
				&& IsIdentifierCharacter(text[directiveEnd])) {
				++directiveEnd;
			}
			if (directiveEnd > offset + 1) {
				AddSpan(spans, offset, directiveEnd, SyntaxKind::Keyword);
				offset = directiveEnd;
				continue;
			}
		}
		if (expectResourceName && text[offset] == '(')
			expectResourceName = false;
		if (IsIdentifierStart(text[offset])) {
			std::size_t wordEnd = offset + 1;
			while (wordEnd < end && IsIdentifierCharacter(text[wordEnd]))
				++wordEnd;
			std::string word = text.substr(offset, wordEnd - offset);
			std::size_t next = wordEnd;
			while (next < end
				&& std::isspace(static_cast<unsigned char>(text[next]))) {
				++next;
			}
			if (IsRdefKeyword(word)) {
				AddSpan(spans, offset, wordEnd, SyntaxKind::Keyword);
				expectResourceName = word == "resource";
			} else if (expectResourceName || (next < end && text[next] == '=')) {
				AddSpan(spans, offset, wordEnd, SyntaxKind::Attribute);
				expectResourceName = false;
			}
			offset = wordEnd;
			continue;
		}
		if (std::isdigit(static_cast<unsigned char>(text[offset]))) {
			std::size_t numberEnd = offset + 1;
			while (numberEnd < end
				&& (std::isalnum(static_cast<unsigned char>(text[numberEnd]))
					|| text[numberEnd] == '.' || text[numberEnd] == '_')) {
				++numberEnd;
			}
			AddSpan(spans, offset, numberEnd, SyntaxKind::Number);
			offset = numberEnd;
			continue;
		}
		++offset;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
HighlightPhpRange(const std::string& text, std::size_t start,
	std::size_t end, std::vector<SyntaxSpan>& spans) {
	std::size_t offset = start;
	while (offset < end) {
		if (offset + 1 < end && text[offset] == '/' && text[offset + 1] == '/') {
			std::size_t commentEnd = text.find('\n', offset + 2);
			if (commentEnd == std::string::npos || commentEnd > end)
				commentEnd = end;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (text[offset] == '#') {
			std::size_t commentEnd = text.find('\n', offset + 1);
			if (commentEnd == std::string::npos || commentEnd > end)
				commentEnd = end;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (offset + 1 < end && text[offset] == '/' && text[offset + 1] == '*') {
			std::size_t commentEnd = text.find("*/", offset + 2);
			if (commentEnd == std::string::npos || commentEnd + 2 > end)
				commentEnd = end;
			else
				commentEnd += 2;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (text[offset] == '\'' || text[offset] == '"') {
			char quote = text[offset];
			std::size_t stringEnd = offset + 1;
			while (stringEnd < end) {
				if (text[stringEnd] == '\\' && stringEnd + 1 < end) {
					stringEnd += 2;
					continue;
				}
				if (text[stringEnd++] == quote)
					break;
			}
			AddSpan(spans, offset, stringEnd, SyntaxKind::String);
			offset = stringEnd;
			continue;
		}
		if (text[offset] == '$' && offset + 1 < end
			&& IsIdentifierStart(text[offset + 1])) {
			std::size_t variableEnd = offset + 2;
			while (variableEnd < end && IsIdentifierCharacter(text[variableEnd]))
				++variableEnd;
			AddSpan(spans, offset, variableEnd, SyntaxKind::Variable);
			offset = variableEnd;
			continue;
		}
		if (IsIdentifierStart(text[offset])) {
			std::size_t wordEnd = offset + 1;
			while (wordEnd < end && IsIdentifierCharacter(text[wordEnd]))
				++wordEnd;
			std::string word = text.substr(offset, wordEnd - offset);
			for (char& character : word) {
				character = static_cast<char>(std::tolower(
					static_cast<unsigned char>(character)));
			}
			if (IsPhpKeyword(word))
				AddSpan(spans, offset, wordEnd, SyntaxKind::Keyword);
			offset = wordEnd;
			continue;
		}
		if (std::isdigit(static_cast<unsigned char>(text[offset]))) {
			std::size_t numberEnd = offset + 1;
			while (numberEnd < end
				&& (std::isalnum(static_cast<unsigned char>(text[numberEnd]))
					|| text[numberEnd] == '.' || text[numberEnd] == '_')) {
				++numberEnd;
			}
			AddSpan(spans, offset, numberEnd, SyntaxKind::Number);
			offset = numberEnd;
			continue;
		}
		++offset;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
HighlightHtmlTag(const std::string& text, std::size_t& offset,
	std::vector<SyntaxSpan>& spans) {
	std::size_t end = text.find('>', offset + 1);
	if (end == std::string::npos)
		end = text.size();
	else
		++end;

	std::size_t cursor = offset;
	AddSpan(spans, cursor, cursor + 1, SyntaxKind::MarkupDelimiter);
	++cursor;
	if (cursor < end && (text[cursor] == '/' || text[cursor] == '!')) {
		AddSpan(spans, cursor, cursor + 1, SyntaxKind::MarkupDelimiter);
		++cursor;
	}
	while (cursor < end && std::isspace(static_cast<unsigned char>(text[cursor])))
		++cursor;
	std::size_t tagNameEnd = cursor;
	while (tagNameEnd < end && IsHtmlNameCharacter(text[tagNameEnd]))
		++tagNameEnd;
	AddSpan(spans, cursor, tagNameEnd, SyntaxKind::Tag);
	cursor = tagNameEnd;

	while (cursor < end) {
		if (text[cursor] == '>') {
			AddSpan(spans, cursor, cursor + 1, SyntaxKind::MarkupDelimiter);
			++cursor;
			break;
		}
		if (text[cursor] == '/' && cursor + 1 < end
			&& text[cursor + 1] == '>') {
			AddSpan(spans, cursor, cursor + 2, SyntaxKind::MarkupDelimiter);
			cursor += 2;
			break;
		}
		if (std::isspace(static_cast<unsigned char>(text[cursor]))) {
			++cursor;
			continue;
		}
		if (text[cursor] == '\'' || text[cursor] == '"') {
			char quote = text[cursor];
			std::size_t stringEnd = cursor + 1;
			while (stringEnd < end && text[stringEnd] != quote)
				++stringEnd;
			if (stringEnd < end)
				++stringEnd;
			AddSpan(spans, cursor, stringEnd, SyntaxKind::String);
			cursor = stringEnd;
			continue;
		}
		if (IsHtmlNameCharacter(text[cursor])) {
			std::size_t attributeEnd = cursor + 1;
			while (attributeEnd < end && IsHtmlNameCharacter(text[attributeEnd]))
				++attributeEnd;
			AddSpan(spans, cursor, attributeEnd, SyntaxKind::Attribute);
			cursor = attributeEnd;
			continue;
		}
		++cursor;
	}
	offset = end;
}
//---------------------------------------------------------------------------------------------------------------------------------//


} // namespace


std::vector<SyntaxSpan>
BuildSyntaxSpans(const std::string& text, SyntaxLanguage language) {
	std::vector<SyntaxSpan> spans;
	if (language == SyntaxLanguage::PlainText)
		return spans;
	if (language == SyntaxLanguage::Rdef) {
		HighlightRdefRange(text, 0, text.size(), spans);
		return spans;
	}
	if (language == SyntaxLanguage::Cpp) {
		HighlightCppRange(text, 0, text.size(), spans);
		return spans;
	}
	if (language == SyntaxLanguage::Makefile) {
		HighlightMakefileRange(text, 0, text.size(), spans);
		return spans;
	}

	std::size_t offset = 0;
	while (offset < text.size()) {
		if (StartsWith(text, offset, "<!--")) {
			std::size_t commentEnd = text.find("-->", offset + 4);
			if (commentEnd == std::string::npos)
				commentEnd = text.size();
			else
				commentEnd += 3;
			AddSpan(spans, offset, commentEnd, SyntaxKind::Comment);
			offset = commentEnd;
			continue;
		}
		if (StartsWith(text, offset, "<?")) {
			std::size_t openerEnd = offset + 2;
			while (openerEnd < text.size()
				&& IsIdentifierCharacter(text[openerEnd])) {
				++openerEnd;
			}
			AddSpan(spans, offset, openerEnd, SyntaxKind::Keyword);
			std::size_t close = text.find("?>", openerEnd);
			std::size_t phpEnd = close == std::string::npos ? text.size() : close;
			HighlightPhpRange(text, openerEnd, phpEnd, spans);
			if (close == std::string::npos) {
				offset = text.size();
			} else {
				AddSpan(spans, close, close + 2, SyntaxKind::Keyword);
				offset = close + 2;
			}
			continue;
		}
		if (text[offset] == '<') {
			HighlightHtmlTag(text, offset, spans);
			continue;
		}
		if (text[offset] == '&') {
			std::size_t entityEnd = text.find(';', offset + 1);
			if (entityEnd != std::string::npos && entityEnd - offset <= 32) {
				AddSpan(spans, offset, entityEnd + 1, SyntaxKind::Entity);
				offset = entityEnd + 1;
				continue;
			}
		}
		++offset;
	}
	return spans;
}
//---------------------------------------------------------------------------------------------------------------------------------//

