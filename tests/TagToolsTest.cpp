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


} // namespace


int
main() {
	ExpectInsertion(ExpandTagTemplate("<strong>%c</strong>", "Globe"),
		"<strong>Globe</strong>", 8, 13);
	ExpectInsertion(ExpandTagTemplate("<p>%c</p>", ""), "<p></p>", 3, 3);
	ExpectInsertion(ExpandTagTemplate("<br>%c", ""), "<br>", 4, 4);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("img"), ""),
		"<img src=\"\" alt=\"\">", 10, 10);
	ExpectInsertion(ExpandTagTemplate(BuildHtmlTagTemplate("section"), ""),
		"<section></section>", 9, 9);

	std::cout << "Tag tools tests passed\n";
	return EXIT_SUCCESS;
}
//---------------------------------------------------------------------------------------------------------------------------------//
