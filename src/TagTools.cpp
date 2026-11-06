#include "TagTools.h"

#include <unordered_set>

TagInsertion
ExpandTagTemplate(const std::string& tagTemplate,
	const std::string& selectedText) {
	bool hasSeparateSelection = tagTemplate.find("%s") != std::string::npos;
	std::string text;
	std::size_t selectionStart = std::string::npos;
	std::size_t selectionEnd = std::string::npos;
	for (std::size_t offset = 0; offset < tagTemplate.size();) {
		if (tagTemplate.compare(offset, 2, "%c") == 0) {
			selectionStart = text.size();
			if (!hasSeparateSelection)
				text += selectedText;
			selectionEnd = text.size();
			offset += 2;
			continue;
		}
		if (tagTemplate.compare(offset, 2, "%s") == 0) {
			text += selectedText;
			offset += 2;
			continue;
		}
		text += tagTemplate[offset++];
	}
	if (selectionStart == std::string::npos)
		selectionStart = selectionEnd = text.size();
	return {text, selectionStart, selectionEnd};
}
//---------------------------------------------------------------------------------------------------------------------------------//


std::string
BuildHtmlTagTemplate(const std::string& tagName) {
	static const std::unordered_set<std::string> voidElements = {
		"area", "base", "br", "col", "embed", "hr", "input", "link",
		"meta", "param", "source", "track", "wbr"
	};

	if (tagName == "a")
		return "<a href=\"%c\">%s</a>";
	if (tagName == "img")
		return "<img src=\"%c\" alt=\"\" />";
	if (voidElements.find(tagName) != voidElements.end())
		return "<" + tagName + " />%c";
	return "<" + tagName + ">%c</" + tagName + ">";
}
//---------------------------------------------------------------------------------------------------------------------------------//
