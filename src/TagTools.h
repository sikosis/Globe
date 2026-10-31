#ifndef GLOBE_TAG_TOOLS_H
#define GLOBE_TAG_TOOLS_H

#include <cstddef>
#include <string>

struct TagInsertion {
	std::string text;
	std::size_t selectionStart;
	std::size_t selectionEnd;
};

TagInsertion ExpandTagTemplate(const std::string& tagTemplate,
	const std::string& selectedText);
std::string BuildHtmlTagTemplate(const std::string& tagName);

#endif
