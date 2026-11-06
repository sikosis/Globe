#include "TagTools.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void
ExpectInsertion(const TagInsertion& insertion, const std::string& text,
	std::size_t selectionStart, std::size_t selectionEnd) {
	if (insertion.text == text && insertion.selectionStart == selectionStart
		&& insertion.selectionEnd == selectionEnd) {
		return;
	}
	std::cerr << "Unexpected tag insertion: " << insertion.text << '\n';
	std::exit(EXIT_FAILURE);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
ExpectTemplate(const std::string& tagName, const std::string& expected) {
	std::string tagTemplate = BuildHtmlTagTemplate(tagName);
	if (tagTemplate == expected)
		return;
	std::cerr << "Unexpected template for " << tagName << ": "
		<< tagTemplate << '\n';
	std::exit(EXIT_FAILURE);
}
//---------------------------------------------------------------------------------------------------------------------------------//


} // namespace


int
main() {
	ExpectInsertion(ExpandTagTemplate("<strong>%c</strong>", "Globe"),
		"<strong>Globe</strong>", 8, 13);
	ExpectInsertion(ExpandTagTemplate("<p>%c</p>", ""), "<p></p>", 3, 3);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("a"), ""),
		"<a href=\"\"></a>", 9, 9);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("a"), "Globe"),
		"<a href=\"\">Globe</a>", 9, 9);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("br"), ""),
		"<br />", 6, 6);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("hr"), ""),
		"<hr />", 6, 6);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("img"), ""),
		"<img src=\"\" alt=\"\" />", 10, 10);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("section"), ""),
		"<section></section>", 9, 9);
	const char* voidElements[] = {
		"area", "base", "br", "col", "embed", "hr", "input", "link",
		"meta", "param", "source", "track", "wbr"
	};
	for (const char* tagName : voidElements)
		ExpectTemplate(tagName, "<" + std::string(tagName) + " />%c");
	ExpectTemplate("img", "<img src=\"%c\" alt=\"\" />");
	ExpectTemplate("a", "<a href=\"%c\">%s</a>");

	std::cout << "Tag tools tests passed\n";
	return EXIT_SUCCESS;
}
//---------------------------------------------------------------------------------------------------------------------------------//
