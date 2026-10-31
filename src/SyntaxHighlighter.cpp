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
	AddSpan(spans, cursor, cursor + 1, SyntaxKind::Tag);
	++cursor;
	if (cursor < end && (text[cursor] == '/' || text[cursor] == '!')) {
		AddSpan(spans, cursor, cursor + 1, SyntaxKind::Tag);
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
			AddSpan(spans, cursor, cursor + 1, SyntaxKind::Tag);
			++cursor;
			break;
		}
		if (text[cursor] == '/' && cursor + 1 < end
			&& text[cursor + 1] == '>') {
			AddSpan(spans, cursor, cursor + 2, SyntaxKind::Tag);
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

