#include "TagTools.h"

#include <unordered_set>

TagInsertion
ExpandTagTemplate(const std::string& tagTemplate,
	const std::string& selectedText) {
	std::size_t cursor = tagTemplate.find("%c");
	if (cursor == std::string::npos) {
		return {tagTemplate, tagTemplate.size(), tagTemplate.size()};
	}

	std::string text = tagTemplate.substr(0, cursor);
	text += selectedText;
	text += tagTemplate.substr(cursor + 2);
	std::size_t selectionEnd = cursor + selectedText.size();
	return {text, cursor, selectionEnd};
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::string
BuildHtmlTagTemplate(const std::string& tagName) {
	static const std::unordered_set<std::string> voidElements = {
		"area", "base", "br", "col", "embed", "hr", "input", "link",
		"meta", "param", "source", "track", "wbr"
	};

	if (tagName == "img")
		return "<img src=\"%c\" alt=\"\">";
	if (voidElements.find(tagName) != voidElements.end())
		return "<" + tagName + ">%c";
	return "<" + tagName + ">%c</" + tagName + ">";
}
//---------------------------------------------------------------------------------------------------------------------------------//
