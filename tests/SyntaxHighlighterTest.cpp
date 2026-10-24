#include "SyntaxHighlighter.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool
HasSpan(const std::string& text, const std::vector<SyntaxSpan>& spans,
	SyntaxKind kind, const std::string& expected) {
	for (const SyntaxSpan& span : spans) {
		if (span.kind == kind && text.substr(span.start, span.length) == expected)
			return true;
	}
	return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
ExpectSpan(const std::string& text, const std::vector<SyntaxSpan>& spans,
	SyntaxKind kind, const std::string& expected) {
	if (HasSpan(text, spans, kind, expected))
		return;
	std::cerr << "Missing highlighted token: " << expected << '\n';
	std::exit(EXIT_FAILURE);
}
//---------------------------------------------------------------------------------------------------------------------------------//


} // namespace


int
main() {
	const std::string html
		= "<div class=\"hero\">Hello &amp;<!-- note --></div>";
	std::vector<SyntaxSpan> htmlSpans
		= BuildSyntaxSpans(html, SyntaxLanguage::Html);
	ExpectSpan(html, htmlSpans, SyntaxKind::Tag, "div");
	ExpectSpan(html, htmlSpans, SyntaxKind::Attribute, "class");
	ExpectSpan(html, htmlSpans, SyntaxKind::String, "\"hero\"");
	ExpectSpan(html, htmlSpans, SyntaxKind::Entity, "&amp;");
	ExpectSpan(html, htmlSpans, SyntaxKind::Comment, "<!-- note -->");

	const std::string php
		= "<?php $name = \"Globe\"; if ($name) { echo 42; } // note\n?>";
	std::vector<SyntaxSpan> phpSpans
		= BuildSyntaxSpans(php, SyntaxLanguage::Php);
	ExpectSpan(php, phpSpans, SyntaxKind::Keyword, "<?php");
	ExpectSpan(php, phpSpans, SyntaxKind::Variable, "$name");
	ExpectSpan(php, phpSpans, SyntaxKind::String, "\"Globe\"");
	ExpectSpan(php, phpSpans, SyntaxKind::Keyword, "if");
	ExpectSpan(php, phpSpans, SyntaxKind::Keyword, "echo");
	ExpectSpan(php, phpSpans, SyntaxKind::Number, "42");
	ExpectSpan(php, phpSpans, SyntaxKind::Comment, "// note");

	const std::string embedded
		= "<main><?PHP /* unfinished comment";
	std::vector<SyntaxSpan> embeddedSpans
		= BuildSyntaxSpans(embedded, SyntaxLanguage::Php);
	ExpectSpan(embedded, embeddedSpans, SyntaxKind::Tag, "main");
	ExpectSpan(embedded, embeddedSpans, SyntaxKind::Keyword, "<?PHP");
	ExpectSpan(embedded, embeddedSpans, SyntaxKind::Comment,
		"/* unfinished comment");

	const std::string unfinishedHtml = "<a title=\"unfinished";
	std::vector<SyntaxSpan> unfinishedHtmlSpans
		= BuildSyntaxSpans(unfinishedHtml, SyntaxLanguage::Html);
	ExpectSpan(unfinishedHtml, unfinishedHtmlSpans, SyntaxKind::Tag, "a");
	ExpectSpan(unfinishedHtml, unfinishedHtmlSpans, SyntaxKind::Attribute,
		"title");
	ExpectSpan(unfinishedHtml, unfinishedHtmlSpans, SyntaxKind::String,
		"\"unfinished");

	if (!BuildSyntaxSpans(html, SyntaxLanguage::PlainText).empty()) {
		std::cerr << "Plain text unexpectedly produced highlighted spans\n";
		return EXIT_FAILURE;
	}

	std::cout << "Syntax highlighter tests passed\n";
	return EXIT_SUCCESS;
}
//---------------------------------------------------------------------------------------------------------------------------------//

