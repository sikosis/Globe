#include "EditorWindow.h"

#include "Messages.h"
#include "SyntaxHighlighter.h"
#include "TagTools.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <Control.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <Size.h>
#include <String.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <View.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <climits>
#include <cstring>
#include <strings.h>
#include <vector>

namespace {

const float kLineNumberGutterWidth = 52.0f;
const float kEditorBorderInset = 2.0f;
const bigtime_t kHighlightDelay = 150000;
const bigtime_t kTagCompletionDelay = 300000;
const float kAutomationScreenInset = 24.0f;
const uint32 kMessageTagLauncherFilter = 'gtlf';
const uint32 kMessageTagLauncherMove = 'gtlm';
const uint32 kMessageTagLauncherAccept = 'gtla';
const uint32 kMessageTagLauncherCancel = 'gtlc';
const uint32 kMessageTagLauncherClosed = 'gtlx';
const uint32 kMessageTagLauncherToggleMode = 'gtlt';

property_info kDocumentProperties[] = {
	{ "ID", { B_GET_PROPERTY }, { B_DIRECT_SPECIFIER },
		"Return the stable document identifier", 0, { B_STRING_TYPE }, {}, {} },
	{ "Path", { B_GET_PROPERTY }, { B_DIRECT_SPECIFIER },
		"Return the document path", 0, { B_STRING_TYPE }, {}, {} },
	{ "ReadOnly", { B_GET_PROPERTY }, { B_DIRECT_SPECIFIER },
		"Return whether editing is disabled", 0, { B_BOOL_TYPE }, {}, {} },
	{ "Presentation", { B_GET_PROPERTY }, { B_DIRECT_SPECIFIER },
		"Return whether document chrome is hidden", 0, { B_BOOL_TYPE }, {}, {} },
	{ "ScrollMaximum", { B_GET_PROPERTY }, { B_DIRECT_SPECIFIER },
		"Return the maximum vertical scroll position", 0, { B_FLOAT_TYPE },
		{}, {} },
	{ "ScrollPosition", { B_GET_PROPERTY, B_SET_PROPERTY },
		{ B_DIRECT_SPECIFIER }, "Get or set the vertical scroll position", 0,
		{ B_FLOAT_TYPE }, {}, {} },
	{ nullptr, {}, {}, nullptr, 0, {}, {}, {} }
};

const char* const kHtmlTags[] = {
	"a", "abbr", "address", "article", "aside", "audio", "b", "base",
	"blockquote", "body", "br", "button", "canvas", "caption", "cite",
	"code", "col", "colgroup", "data", "datalist", "dd", "del", "details",
	"dialog", "div", "dl", "dt", "em", "embed", "fieldset", "figcaption",
	"figure", "footer", "form", "h1", "h2", "h3", "h4", "h5", "h6",
	"head", "header", "hr", "html", "i", "iframe", "img", "input", "label",
	"legend", "li", "link", "main", "meta", "nav", "noscript", "object",
	"ol", "optgroup", "option", "p", "picture", "pre", "q", "script",
	"section", "select", "small", "source", "span", "strong", "style", "sub",
	"summary", "sup", "table", "tbody", "td", "template", "textarea", "tfoot",
	"th", "thead", "time", "title", "tr", "track", "u", "ul", "video", "wbr"
};


BRect
AvailableScreenFrame() {
	BScreen screen;
	if (!screen.IsValid())
		return BRect(24, 24, 1000, 700);

	BRect frame = screen.Frame();
	frame.InsetBy(kAutomationScreenInset, kAutomationScreenInset);
	return frame;
}
//---------------------------------------------------------------------------------------------------------------------------------//


BRect
EditorFrame(bool automation) {
	BRect available = AvailableScreenFrame();
	if (automation)
		return available;

	return BRect(0.0f, 0.0f, std::min(800.0f, available.Width()),
		std::min(600.0f, available.Height()));
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
SendScriptingReply(BMessage* request, status_t error) {
	BMessage reply(B_REPLY);
	reply.AddInt32("error", error);
	if (error != B_OK)
		reply.AddString("message", strerror(error));
	request->SendReply(&reply);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
SendScriptingReply(BMessage* request, status_t error, const char* result) {
	BMessage reply(B_REPLY);
	reply.AddString("result", result);
	reply.AddInt32("error", error);
	request->SendReply(&reply);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
SendScriptingReply(BMessage* request, status_t error, bool result) {
	BMessage reply(B_REPLY);
	reply.AddBool("result", result);
	reply.AddInt32("error", error);
	request->SendReply(&reply);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
SendScriptingReply(BMessage* request, status_t error, float result) {
	BMessage reply(B_REPLY);
	reply.AddFloat("result", result);
	reply.AddInt32("error", error);
	request->SendReply(&reply);
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
UsesDarkEditorBackground() {
	rgb_color background = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	int32 luminance = (299 * background.red + 587 * background.green
		+ 114 * background.blue) / 1000;
	return luminance < 128;
}
//---------------------------------------------------------------------------------------------------------------------------------//


rgb_color
SyntaxBaseColour() {
	return UsesDarkEditorBackground()
		? rgb_color{220, 224, 232, 255}
		: rgb_color{35, 40, 48, 255};
}
//---------------------------------------------------------------------------------------------------------------------------------//


rgb_color
SyntaxColour(SyntaxKind kind) {
	if (UsesDarkEditorBackground()) {
		switch (kind) {
			case SyntaxKind::Tag:
				return {255, 126, 156, 255};
			case SyntaxKind::MarkupDelimiter:
			case SyntaxKind::Preprocessor:
				return {120, 180, 255, 255};
			case SyntaxKind::Keyword:
				return {203, 166, 247, 255};
			case SyntaxKind::Attribute:
			case SyntaxKind::Type:
				return {112, 210, 215, 255};
			case SyntaxKind::Function:
				return {255, 213, 128, 255};
			case SyntaxKind::String:
				return {166, 218, 149, 255};
			case SyntaxKind::Number:
			case SyntaxKind::Entity:
				return {229, 192, 123, 255};
			case SyntaxKind::Comment:
				return {145, 155, 170, 255};
			case SyntaxKind::Variable:
				return {220, 150, 210, 255};
		}
	}

	switch (kind) {
		case SyntaxKind::Tag:
			return {173, 35, 71, 255};
		case SyntaxKind::MarkupDelimiter:
		case SyntaxKind::Preprocessor:
			return {0, 92, 185, 255};
		case SyntaxKind::Keyword:
			return {122, 62, 157, 255};
		case SyntaxKind::Attribute:
		case SyntaxKind::Type:
			return {0, 105, 125, 255};
		case SyntaxKind::Function:
			return {121, 94, 0, 255};
		case SyntaxKind::String:
			return {36, 121, 45, 255};
		case SyntaxKind::Number:
			return {176, 74, 0, 255};
		case SyntaxKind::Comment:
			return {104, 115, 125, 255};
		case SyntaxKind::Entity:
			return {138, 82, 0, 255};
		case SyntaxKind::Variable:
			return {145, 60, 125, 255};
	}
	return SyntaxBaseColour();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
TintToolbarIcon(BBitmap* icon, rgb_color colour) {
	if (icon == nullptr || !icon->IsValid())
		return;
	color_space colourSpace = icon->ColorSpace();
	if (colourSpace != B_RGBA32 && colourSpace != B_RGB32)
		return;

	uint8* bits = static_cast<uint8*>(icon->Bits());
	int32 width = icon->Bounds().IntegerWidth() + 1;
	int32 height = icon->Bounds().IntegerHeight() + 1;
	for (int32 y = 0; y < height; ++y) {
		uint8* pixel = bits + y * icon->BytesPerRow();
		for (int32 x = 0; x < width; ++x, pixel += 4) {
			pixel[0] = colour.blue;
			pixel[1] = colour.green;
			pixel[2] = colour.red;
		}
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


class ToolbarButton : public BControl {
public:
	ToolbarButton(const char* name, BMessage* message, const char* toolTip,
		BBitmap* icon)
		:
		BControl(name, "", message, B_WILL_DRAW),
		fIcon(icon),
		fHover(false) {
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(34.0f, 34.0f));
		SetExplicitMaxSize(BSize(34.0f, 34.0f));
		SetToolTip(toolTip);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	~ToolbarButton() override {
		delete fIcon;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void Draw(BRect updateRect) override {
		SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		FillRect(updateRect);

		BRect buttonBounds(Bounds());
		buttonBounds.InsetBy(2.0f, 2.0f);
		if (Value() == B_CONTROL_ON) {
			SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
				B_DARKEN_2_TINT));
			FillRoundRect(buttonBounds, 6.0f, 6.0f);
		} else if (fHover) {
			SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
				B_DARKEN_1_TINT));
			FillRoundRect(buttonBounds, 6.0f, 6.0f);
		}

		if (fIcon != nullptr) {
			BRect iconBounds(fIcon->Bounds());
			BPoint location(std::floor((Bounds().Width() - iconBounds.Width())
				/ 2.0f), std::floor((Bounds().Height() - iconBounds.Height())
				/ 2.0f));
			SetDrawingMode(B_OP_ALPHA);
			DrawBitmap(fIcon, location);
			SetDrawingMode(B_OP_COPY);
		}
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void MouseDown(BPoint point) override {
		if (!IsEnabled())
			return;
		SetValue(B_CONTROL_ON);
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		Invalidate();
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void MouseUp(BPoint point) override {
		bool invoke = Value() == B_CONTROL_ON && Bounds().Contains(point);
		SetValue(B_CONTROL_OFF);
		Invalidate();
		if (invoke)
			Invoke();
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void MouseMoved(BPoint point, uint32 transit,
		const BMessage* message) override {
		bool hover = transit == B_ENTERED_VIEW || transit == B_INSIDE_VIEW;
		if (hover != fHover) {
			fHover = hover;
			Invalidate();
		}
		BControl::MouseMoved(point, transit, message);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


private:
	BBitmap* fIcon;
	bool fHover;
};


ToolbarButton*
TagButton(const char* label, const char* tagTemplate, const char* toolTip,
	const char* iconResource) {
	BMessage* message = new BMessage(kMessageInsertTag);
	message->AddString("template", tagTemplate);
	message->AddBool("expand_template", true);
	BBitmap* icon = BTranslationUtils::GetBitmap('PNG ', iconResource);
	TintToolbarIcon(icon, ui_color(B_CONTROL_TEXT_COLOR));
	return new ToolbarButton(label, message, toolTip, icon);
}
//---------------------------------------------------------------------------------------------------------------------------------//


class EditorTextView : public BTextView {
public:
	EditorTextView(bool presentation)
		:
		BTextView("editor", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE
			| B_SCROLL_VIEW_AWARE),
		fNotifyChanges(true),
		fPresentation(presentation),
		fLineNumberView(nullptr),
		fLastScrollTop(-1.0f) {
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void SetNotifyChanges(bool notifyChanges) {
		fNotifyChanges = notifyChanges;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void SetLineNumberView(BView* lineNumberView) {
		fLineNumberView = lineNumberView;
		InvalidateLineNumbers();
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void InvalidateLineNumbers() {
		if (fLineNumberView != nullptr)
			fLineNumberView->Invalidate();
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void Draw(BRect updateRect) override {
		BTextView::Draw(updateRect);
		if (Bounds().top != fLastScrollTop) {
			fLastScrollTop = Bounds().top;
			InvalidateLineNumbers();
		}
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void Pulse() override {
		BTextView::Pulse();
		if (Window() != nullptr)
			Window()->PostMessage(kMessageUpdateStatus);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void MessageReceived(BMessage* message) override {
		if ((message->what == B_SIMPLE_DATA || message->what == B_REFS_RECEIVED)
			&& message->HasRef("refs") && Window() != nullptr) {
			Window()->PostMessage(message);
			return;
		}
		BTextView::MessageReceived(message);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void KeyDown(const char* bytes, int32 numBytes) override {
		if (fPresentation && numBytes == 1 && bytes[0] == B_ESCAPE) {
			if (Window() != nullptr)
				Window()->PostMessage(kMessageShowreelStop);
			return;
		}

		BTextView::KeyDown(bytes, numBytes);
		if (Window() == nullptr)
			return;
		if (numBytes == 1 && bytes[0] == '<')
			Window()->PostMessage(kMessageScheduleTagCompletion);
		else
			Window()->PostMessage(kMessageCancelTagCompletion);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void InsertText(const char* text, int32 length, int32 offset,
		const text_run_array* runs) override {
		bool addsLine = length > 0
			&& std::memchr(text, '\n', static_cast<std::size_t>(length)) != nullptr;
		BTextView::InsertText(text, length, offset, runs);
		if (addsLine && Window() != nullptr)
			Window()->PostMessage(kMessageUpdateLineNumbers);
		if (fNotifyChanges && Window() != nullptr)
			Window()->PostMessage(kMessageDocumentChanged);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void DeleteText(int32 fromOffset, int32 toOffset) override {
		bool removesLine = fromOffset >= 0 && toOffset > fromOffset
			&& toOffset <= TextLength()
			&& std::memchr(Text() + fromOffset, '\n',
				static_cast<std::size_t>(toOffset - fromOffset)) != nullptr;
		BTextView::DeleteText(fromOffset, toOffset);
		if (removesLine && Window() != nullptr)
			Window()->PostMessage(kMessageUpdateLineNumbers);
		if (fNotifyChanges && Window() != nullptr)
			Window()->PostMessage(kMessageDocumentChanged);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


private:
	bool fNotifyChanges;
	bool fPresentation;
	BView* fLineNumberView;
	float fLastScrollTop;
};


class LineNumberView : public BView {
public:
	LineNumberView(EditorTextView* textView)
		:
		BView("line numbers", B_WILL_DRAW),
		fTextView(textView) {
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(kLineNumberGutterWidth, 0.0f));
		SetExplicitMaxSize(BSize(kLineNumberGutterWidth, B_SIZE_UNLIMITED));
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void Draw(BRect updateRect) override {
		SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		FillRect(updateRect);

		BRect bounds(Bounds());
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		StrokeLine(BPoint(bounds.right, updateRect.top),
			BPoint(bounds.right, updateRect.bottom));

		BFont font;
		fTextView->GetFont(&font);
		SetFont(&font);
		font_height fontHeight;
		GetFontHeight(&fontHeight);
		SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_DISABLED_LABEL_TINT));

		BRect textBounds(fTextView->Bounds());
		float lineHeight = fTextView->LineHeight(0);
		float firstLineTop = fTextView->PointAt(0).y;
		int32 firstLine = lineHeight > 0.0f
			? static_cast<int32>(std::floor(
				(textBounds.top - firstLineTop) / lineHeight)) : 0;
		firstLine = std::max(0, firstLine);
		int32 visibleLineCount = lineHeight > 0.0f
			? static_cast<int32>(std::ceil(textBounds.Height() / lineHeight)) + 2
			: 1;
		int32 lastLine = std::min(firstLine + visibleLineCount,
			fTextView->CountLines() - 1);

		for (int32 line = firstLine; line <= lastLine; ++line) {
			BPoint linePosition = fTextView->PointAt(fTextView->OffsetAt(line));
			BString lineNumber;
			lineNumber << line + 1;
			float numberWidth = StringWidth(lineNumber.String());
			DrawString(lineNumber.String(), BPoint(bounds.right - numberWidth - 7,
				linePosition.y - textBounds.top + fontHeight.ascent
					+ kEditorBorderInset));
		}
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


private:
	EditorTextView* fTextView;
};


class TagLauncherField : public BTextView {
public:
	TagLauncherField()
		:
		BTextView("tag search", B_WILL_DRAW | B_NAVIGABLE) {
		SetWordWrap(false);
		SetInsets(8.0f, 7.0f, 8.0f, 7.0f);
		SetExplicitMinSize(BSize(320.0f, 34.0f));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 34.0f));
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void KeyDown(const char* bytes, int32 numBytes) override {
		if (numBytes == 1 && bytes[0] == '`') {
			Window()->PostMessage(kMessageTagLauncherToggleMode);
			return;
		}
		if (numBytes == 1 && bytes[0] == B_ESCAPE) {
			Window()->PostMessage(kMessageTagLauncherCancel);
			return;
		}
		if (numBytes == 1 && bytes[0] == B_ENTER) {
			Window()->PostMessage(kMessageTagLauncherAccept);
			return;
		}
		if (numBytes == 1 && (bytes[0] == B_UP_ARROW
			|| bytes[0] == B_DOWN_ARROW)) {
			BMessage message(kMessageTagLauncherMove);
			message.AddInt32("delta", bytes[0] == B_UP_ARROW ? -1 : 1);
			Window()->PostMessage(&message);
			return;
		}

		BTextView::KeyDown(bytes, numBytes);
		Window()->PostMessage(kMessageTagLauncherFilter);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//
};


class TagLauncherItem : public BStringItem {
public:
	TagLauncherItem(const char* label, const std::string& content,
		bool expandTemplate)
		:
		BStringItem(label),
		fContent(content),
		fExpandTemplate(expandTemplate) {
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	const char* Content() const {
		return fContent.c_str();
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	bool ExpandsTemplate() const {
		return fExpandTemplate;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


private:
	std::string fContent;
	bool fExpandTemplate;
};


struct LauncherEntry {
	std::string label;
	std::string content;
	bool expandTemplate;
};


class TagLauncherWindow : public BWindow {
public:
	TagLauncherWindow(EditorWindow* owner, BPoint position)
		:
		BWindow(BRect(position.x, position.y, position.x + 420.0f,
				position.y + 340.0f), "Insert HTML",
			B_FLOATING_WINDOW_LOOK, B_FLOATING_SUBSET_WINDOW_FEEL,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
				| B_ASYNCHRONOUS_CONTROLS),
		fTarget(owner),
		fField(new TagLauncherField()),
		fListView(new BListView("tag matches", B_SINGLE_SELECTION_LIST)),
		fHeading(new BStringView("tag launcher heading", "Insert HTML tag")),
		fSnippetMode(false),
		fNotifyTarget(true) {
		BScrollView* fieldBorder = new BScrollView("tag search border", fField,
			B_WILL_DRAW, false, false, B_FANCY_BORDER);
		BScrollView* results = new BScrollView("tag results", fListView,
			B_WILL_DRAW, false, true, B_FANCY_BORDER);
		BStringView* help = new BStringView("tag launcher help",
			"Type to filter  •  ` tags/snippets  •  ↑↓ select  •  Enter insert");
		help->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_DISABLED_LABEL_TINT));

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fHeading)
			.Add(fieldBorder)
			.Add(results)
			.Add(help);
		fListView->SetInvocationMessage(new BMessage(kMessageTagLauncherAccept));
		fListView->SetTarget(this);
		AddToSubset(owner);
		_ConstrainToScreen();
		_LoadEntries();
		_Filter();
		fField->MakeFocus(true);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void SuppressCloseNotification() {
		fNotifyTarget = false;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void MessageReceived(BMessage* message) override {
		switch (message->what) {
			case kMessageTagLauncherFilter:
				_Filter();
				break;
			case kMessageTagLauncherMove:
			{
				int32 delta = 0;
				message->FindInt32("delta", &delta);
				_MoveSelection(delta);
				break;
			}
			case kMessageTagLauncherAccept:
				_Accept();
				break;
			case kMessageTagLauncherCancel:
				PostMessage(B_QUIT_REQUESTED);
				break;
			case kMessageTagLauncherToggleMode:
				_ToggleMode();
				break;
			default:
				BWindow::MessageReceived(message);
				break;
		}
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	bool QuitRequested() override {
		if (fNotifyTarget)
			fTarget.SendMessage(kMessageTagLauncherClosed);
		return true;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


private:
	void _LoadEntries() {
		fEntries.clear();
		if (!fSnippetMode) {
			for (const char* tag : kHtmlTags) {
				fEntries.push_back({tag, BuildHtmlTagTemplate(tag), true});
			}
			return;
		}

		BPath path;
		if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) != B_OK)
			return;
		if (path.Append("Globe") != B_OK)
			return;
		status_t error = create_directory(path.Path(), 0755);
		if (error != B_OK && error != B_FILE_EXISTS)
			return;
		if (path.Append("snippets") != B_OK)
			return;
		error = create_directory(path.Path(), 0755);
		if (error != B_OK && error != B_FILE_EXISTS)
			return;

		BDirectory directory(path.Path());
		BEntry entry;
		while (directory.GetNextEntry(&entry, true) == B_OK) {
			if (!entry.IsFile())
				continue;
			char name[B_FILE_NAME_LENGTH];
			if (entry.GetName(name) != B_OK)
				continue;
			std::string content;
			if (_ReadSnippet(entry, content) == B_OK)
				fEntries.push_back({name, content, false});
		}
		std::sort(fEntries.begin(), fEntries.end(),
			[](const LauncherEntry& left, const LauncherEntry& right) {
				return strcasecmp(left.label.c_str(), right.label.c_str()) < 0;
			});
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	status_t _ReadSnippet(BEntry& entry, std::string& content) {
		BFile file(&entry, B_READ_ONLY);
		status_t error = file.InitCheck();
		if (error != B_OK)
			return error;

		off_t size = 0;
		error = file.GetSize(&size);
		if (error != B_OK)
			return error;
		if (size < 0 || size > INT32_MAX)
			return B_BAD_VALUE;

		content.assign(static_cast<std::size_t>(size), '\0');
		std::size_t totalRead = 0;
		while (totalRead < static_cast<std::size_t>(size)) {
			ssize_t bytesRead = file.Read(content.data() + totalRead,
				static_cast<std::size_t>(size) - totalRead);
			if (bytesRead < 0)
				return static_cast<status_t>(bytesRead);
			if (bytesRead == 0)
				break;
			totalRead += static_cast<std::size_t>(bytesRead);
		}
		content.resize(totalRead);
		return B_OK;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void _ToggleMode() {
		fSnippetMode = !fSnippetMode;
		SetTitle(fSnippetMode ? "Insert snippet" : "Insert HTML");
		fHeading->SetText(fSnippetMode
			? "Code snippets — settings/Globe/snippets"
			: "Insert HTML tag");
		fField->SetText("");
		fField->Select(0, 0);
		_LoadEntries();
		_Filter();
		fField->MakeFocus(true);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void _Filter() {
		while (BListItem* item = fListView->RemoveItem(int32(0)))
			delete item;

		std::string query(fField->Text());
		std::transform(query.begin(), query.end(), query.begin(),
			[](unsigned char character) { return std::tolower(character); });
		for (const LauncherEntry& entry : fEntries) {
			std::string candidate(entry.label);
			std::transform(candidate.begin(), candidate.end(), candidate.begin(),
				[](unsigned char character) { return std::tolower(character); });
			if (query.empty() || candidate.compare(0, query.size(), query) == 0) {
				fListView->AddItem(new TagLauncherItem(entry.label.c_str(),
					entry.content, entry.expandTemplate));
			}
		}
		if (fListView->CountItems() == 0 && !query.empty()) {
			for (const LauncherEntry& entry : fEntries) {
				std::string candidate(entry.label);
				std::transform(candidate.begin(), candidate.end(), candidate.begin(),
					[](unsigned char character) { return std::tolower(character); });
				if (candidate.find(query) != std::string::npos) {
					fListView->AddItem(new TagLauncherItem(entry.label.c_str(),
						entry.content, entry.expandTemplate));
				}
			}
		}
		if (fListView->CountItems() == 0 && fSnippetMode)
			fListView->AddItem(new BStringItem("No snippet files found"));
		if (fListView->CountItems() > 0) {
			fListView->Select(0);
			fListView->ScrollToSelection();
		}
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void _MoveSelection(int32 delta) {
		int32 count = fListView->CountItems();
		if (count == 0)
			return;
		int32 selection = fListView->CurrentSelection();
		selection = std::max(0, std::min(count - 1, selection + delta));
		fListView->Select(selection);
		fListView->ScrollToSelection();
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void _Accept() {
		TagLauncherItem* item = dynamic_cast<TagLauncherItem*>(
			fListView->ItemAt(fListView->CurrentSelection()));
		if (item == nullptr)
			return;

		BMessage message(kMessageInsertTag);
		message.AddString("template", item->Content());
		message.AddBool("replace_opener", true);
		message.AddBool("expand_template", item->ExpandsTemplate());
		fTarget.SendMessage(&message);
		PostMessage(B_QUIT_REQUESTED);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void _ConstrainToScreen() {
		BScreen screen(this);
		if (!screen.IsValid())
			return;
		BRect screenFrame = screen.Frame();
		BRect frame = Frame();
		float x = std::max(screenFrame.left,
			std::min(frame.left, screenFrame.right - frame.Width()));
		float y = std::max(screenFrame.top,
			std::min(frame.top, screenFrame.bottom - frame.Height()));
		MoveTo(x, y);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	BMessenger fTarget;
	TagLauncherField* fField;
	BListView* fListView;
	BStringView* fHeading;
	std::vector<LauncherEntry> fEntries;
	bool fSnippetMode;
	bool fNotifyTarget;
};


void
ShowFileError(const char* operation, const char* path, status_t error) {
	BString message(operation);
	message << " “" << path << "” failed: " << strerror(error);
	(new BAlert("File error", message.String(), "OK", nullptr, nullptr,
		B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
}
//---------------------------------------------------------------------------------------------------------------------------------//


} // namespace


EditorWindow::EditorWindow(const char* documentID, const entry_ref* document,
	bool readOnly, bool presentation, int64 automationSession,
	float presentationFontSize)
	:
	BWindow(EditorFrame(automationSession != 0), "Untitled — Globe",
		B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS),
	fMenuBar(nullptr),
	fTagToolbar(nullptr),
	fLineNumberView(nullptr),
	fTextView(nullptr),
	fScrollView(nullptr),
	fStatusView(nullptr),
	fHighlightRunner(nullptr),
	fTagCompletionRunner(nullptr),
	fTagLauncher(nullptr),
	fOpenPanel(nullptr),
	fSavePanel(nullptr),
	fDocumentID(documentID),
	fInitialDocumentStatus(B_OK),
	fHasDocument(false),
	fModified(false),
	fReadOnly(readOnly || automationSession != 0),
	fPresentation(presentation || automationSession != 0),
	fAutomationSession(automationSession),
	fPresentationMode(automationSession != 0),
	fPendingAction(PendingAction::None) {
	fMenuBar = new BMenuBar("menu bar");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("New", new BMessage(kMessageNewDocument), 'N'));
	fileMenu->AddItem(new BMenuItem("Open…", new BMessage(kMessageOpenDocument), 'O'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Save", new BMessage(kMessageSaveDocument), 'S'));
	fileMenu->AddItem(new BMenuItem("Save as…",
		new BMessage(kMessageSaveDocumentAs), 'S', B_SHIFT_KEY));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("About Globe…",
		new BMessage(B_ABOUT_REQUESTED)));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Quit",
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(fileMenu);

	BMenu* editMenu = new BMenu("Edit");
	editMenu->AddItem(new BMenuItem("Undo", new BMessage(B_UNDO), 'Z'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cut", new BMessage(B_CUT), 'X'));
	editMenu->AddItem(new BMenuItem("Copy", new BMessage(B_COPY), 'C'));
	editMenu->AddItem(new BMenuItem("Paste", new BMessage(B_PASTE), 'V'));
	editMenu->AddItem(new BMenuItem("Select all", new BMessage(B_SELECT_ALL), 'A'));
	fMenuBar->AddItem(editMenu);

	BMenu* previewMenu = new BMenu("Preview");
	previewMenu->AddItem(new BMenuItem("Preferred browser",
		new BMessage(kMessagePreviewBrowser), 'P'));
	previewMenu->AddSeparatorItem();
	previewMenu->AddItem(new BMenuItem("Tororu",
		new BMessage(kMessagePreviewTororu), 'P', B_SHIFT_KEY));
	fMenuBar->AddItem(previewMenu);

	fTagToolbar = new BView("tag toolbar", B_WILL_DRAW);
	fTagToolbar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(fTagToolbar, B_HORIZONTAL, 3.0f)
		.SetInsets(B_USE_SMALL_SPACING, 3.0f, B_USE_SMALL_SPACING, 3.0f)
		.Add(TagButton("P", "<p>%c</p>", "Paragraph", "toolbar-p.png"))
		.Add(TagButton("B", "<strong>%c</strong>", "Strong text",
			"toolbar-bold.png"))
		.Add(TagButton("I", "<em>%c</em>", "Emphasised text",
			"toolbar-italic.png"))
		.Add(TagButton("H1", "<h1>%c</h1>", "Heading 1", "toolbar-h1.png"))
		.Add(TagButton("A", "<a href=\"\">%c</a>", "Link", "toolbar-anchor.png"))
		.Add(TagButton("IMG", "<img src=\"%c\" alt=\"\" />", "Image",
			"toolbar-image.png"))
		.Add(TagButton("DIV", "<div>%c</div>", "Division", "toolbar-div.png"))
		.Add(TagButton("SPAN", "<span>%c</span>", "Inline span",
			"toolbar-span.png"))
		.Add(TagButton("UL", "<ul>\n%c\n</ul>", "Unordered list",
			"toolbar-ul.png"))
		.Add(TagButton("LI", "<li>%c</li>", "List item", "toolbar-li.png"))
		.Add(TagButton("BR", "<br />%c", "Line break", "toolbar-br.png"))
		.AddGlue();

	EditorTextView* editor = new EditorTextView(fPresentationMode);
	fTextView = editor;
	fTextView->SetStylable(true);
	fTextView->SetFontAndColor(be_fixed_font);
	fTextView->SetDoesUndo(true);
	fTextView->SetWordWrap(false);
	fTextView->SetInsets(8.0f, 8.0f, 8.0f, 8.0f);
	fTextView->SetExplicitMinSize(BSize(0.0f, 0.0f));
	fTextView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	fTextView->MakeEditable(!fReadOnly);
	fTextView->MakeFocus(true);
	fLineNumberView = new LineNumberView(editor);
	editor->SetLineNumberView(fLineNumberView);
	fStatusView = new BStringView("status", "Line 1, Column 1");
	fStatusView->SetTruncation(B_TRUNCATE_END);
	fStatusView->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
	fStatusView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	fScrollView = new BScrollView("editor scroll", fTextView,
		B_WILL_DRAW | B_FRAME_EVENTS, true, true, B_FANCY_BORDER);
	fScrollView->SetExplicitMinSize(BSize(0.0f, 0.0f));
	fScrollView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,
		B_SIZE_UNLIMITED));
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fTagToolbar)
		.AddGroup(B_HORIZONTAL, 0)
			.Add(fLineNumberView)
			.Add(fScrollView)
			.End()
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(B_USE_SMALL_SPACING, 2, B_USE_SMALL_SPACING, 2)
			.Add(fStatusView)
		.End();
	if (fPresentation) {
		fMenuBar->Hide();
		fTagToolbar->Hide();
		fStatusView->Hide();
	}

	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(be_app), nullptr,
		B_FILE_NODE, true);
	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this), nullptr,
		B_FILE_NODE, false, new BMessage(kMessageSavePanelAccepted));

	if (document != nullptr)
		fInitialDocumentStatus = OpenDocument(*document);

	BRect availableFrame = AvailableScreenFrame();
	float maximumWidth = std::max(1.0f, availableFrame.Width());
	float maximumHeight = std::max(1.0f, availableFrame.Height());
	SetSizeLimits(std::min(240.0f, maximumWidth), maximumWidth,
		std::min(160.0f, maximumHeight), maximumHeight);
	ResizeTo(std::min(Frame().Width(), maximumWidth),
		std::min(Frame().Height(), maximumHeight));
	if (!fPresentationMode)
		CenterOnScreen();
	else
		AddShortcut(B_ESCAPE, 0, new BMessage(kMessageShowreelStop));
	_UpdateStatus();
	Layout(true);
	if (fPresentationMode) {
		BFont presentationFont(*be_fixed_font);
		presentationFont.SetSize(presentationFontSize);
		int32 textLength = fTextView->TextLength();
		if (textLength > 0) {
			fTextView->SetFontAndColor(0, textLength, &presentationFont,
				B_FONT_SIZE);
		} else
			fTextView->SetFontAndColor(&presentationFont);
		editor->InvalidateLineNumbers();
	}
	Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow::~EditorWindow() {
	if (fTagLauncher != nullptr && fTagLauncher->Lock()) {
		static_cast<TagLauncherWindow*>(fTagLauncher)
			->SuppressCloseNotification();
		fTagLauncher->Quit();
	}
	delete fHighlightRunner;
	delete fTagCompletionRunner;
	delete fOpenPanel;
	delete fSavePanel;
	be_app->PostMessage(kMessageWindowClosed);
}
//---------------------------------------------------------------------------------------------------------------------------------//


const char*
EditorWindow::DocumentID() const {
	return fDocumentID.String();
}
//---------------------------------------------------------------------------------------------------------------------------------//


status_t
EditorWindow::GetSupportedSuites(BMessage* message) {
	message->AddString("suites", "suite/vnd.hGlobe-Document");
	BPropertyInfo propertyInfo(kDocumentProperties);
	message->AddFlat("messages", &propertyInfo);
	return BWindow::GetSupportedSuites(message);
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
EditorWindow::IsPristine() const {
	return !fHasDocument && !fModified && fTextView->TextLength() == 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//


status_t
EditorWindow::InitialDocumentStatus() const {
	return fInitialDocumentStatus;
}
//---------------------------------------------------------------------------------------------------------------------------------//


status_t
EditorWindow::OpenDocument(const entry_ref& document) {
	BFile file(&document, B_READ_ONLY);
	status_t error = file.InitCheck();
	if (error != B_OK) {
		ShowFileError("Opening", document.name, error);
		return error;
	}

	off_t size = 0;
	error = file.GetSize(&size);
	if (error != B_OK) {
		ShowFileError("Opening", document.name, error);
		return error;
	}
	if (size < 0 || size > INT32_MAX) {
		ShowFileError("Opening", document.name, B_BAD_VALUE);
		return B_BAD_VALUE;
	}

	std::vector<char> contents(static_cast<size_t>(size) + 1, '\0');
	int32 totalRead = 0;
	while (totalRead < size) {
		ssize_t bytesRead = file.Read(contents.data() + totalRead,
			static_cast<size_t>(size - totalRead));
		if (bytesRead < 0) {
			ShowFileError("Opening", document.name,
				static_cast<status_t>(bytesRead));
			return static_cast<status_t>(bytesRead);
		}
		if (bytesRead == 0)
			break;
		totalRead += static_cast<int32>(bytesRead);
	}

	EditorTextView* editor = static_cast<EditorTextView*>(fTextView);
	editor->SetNotifyChanges(false);
	fTextView->SetText(contents.data(), totalRead);
	editor->SetNotifyChanges(true);
	_SetDocument(document);
	_SetModified(false);
	_ApplySyntaxHighlighting();
	_ResetScrollPosition();
	_UpdateStatus();
	return B_OK;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::SetAutomationSession(int64 session) {
	fAutomationSession = session;
	fPresentationMode = session != 0;
	fReadOnly = fPresentationMode;

	fTextView->MakeEditable(!fPresentationMode);
	fTextView->MakeSelectable(true);
	if (fPresentationMode) {
		fPresentation = true;
		fMenuBar->Hide();
		fTagToolbar->Hide();
		fStatusView->Hide();
		BRect availableFrame = AvailableScreenFrame();
		float maximumWidth = std::max(1.0f, availableFrame.Width());
		float maximumHeight = std::max(1.0f, availableFrame.Height());
		SetSizeLimits(std::min(240.0f, maximumWidth), maximumWidth,
			std::min(160.0f, maximumHeight), maximumHeight);
		MoveTo(availableFrame.LeftTop());
		ResizeTo(maximumWidth, maximumHeight);
		Layout(true);
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


int64
EditorWindow::AutomationSession() const {
	return fAutomationSession;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::MessageReceived(BMessage* message) {
	if ((message->what == B_GET_PROPERTY || message->what == B_SET_PROPERTY)
		&& _HandleScriptingMessage(message)) {
		return;
	}

	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_REFS_RECEIVED:
		{
			BMessage refs(B_REFS_RECEIVED);
			entry_ref document;
			int32 index = 0;
			while (message->FindRef("refs", index++, &document) == B_OK)
				refs.AddRef("refs", &document);
			if (refs.HasRef("refs"))
				be_app->PostMessage(&refs);
			break;
		}
		case kMessageNewDocument:
			be_app->PostMessage(kMessageNewDocument);
			break;
		case kMessageOpenDocument:
			_ShowOpenPanel();
			break;
		case kMessageSaveDocument:
			if (fHasDocument) {
				status_t error = _SaveDocument();
				if (error != B_OK)
					ShowFileError("Saving", fDocument.name, error);
			} else
				_ShowSavePanel();
			break;
		case kMessageSaveDocumentAs:
			_ShowSavePanel();
			break;
		case kMessageSavePanelAccepted:
		{
			entry_ref directory;
			const char* name = nullptr;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				status_t error = _SaveDocumentAs(directory, name);
				if (error != B_OK) {
					ShowFileError("Saving", name, error);
					fPendingAction = PendingAction::None;
				} else {
					PendingAction action = fPendingAction;
					fPendingAction = PendingAction::None;
					if (action == PendingAction::Close)
						PostMessage(B_QUIT_REQUESTED);
					else if (action != PendingAction::None) {
						error = _LaunchPreview(action);
						if (error != B_OK) {
							const char* operation
								= action == PendingAction::PreviewTororu
									? "Previewing in Tororu"
									: "Previewing in the preferred browser";
							ShowFileError(operation, fDocument.name, error);
						}
					}
				}
			}
			break;
		}
		case B_CANCEL:
			fPendingAction = PendingAction::None;
			break;
		case kMessageDocumentChanged:
			_SetModified(true);
			_ScheduleSyntaxHighlighting();
			_UpdateStatus();
			break;
		case kMessageUpdateStatus:
			_UpdateStatus();
			break;
		case kMessageUpdateLineNumbers:
			fLineNumberView->Invalidate();
			break;
		case kMessageHighlightDocument:
			delete fHighlightRunner;
			fHighlightRunner = nullptr;
			_ApplySyntaxHighlighting();
			break;
		case kMessagePreviewBrowser:
			_Preview(PendingAction::PreviewBrowser);
			break;
		case kMessagePreviewTororu:
			_Preview(PendingAction::PreviewTororu);
			break;
		case kMessageInsertTag:
		{
			const char* tagTemplate = nullptr;
			if (!fReadOnly
				&& message->FindString("template", &tagTemplate) == B_OK) {
				bool replaceOpener = false;
				message->FindBool("replace_opener", &replaceOpener);
				if (replaceOpener) {
					int32 selectionStart = 0;
					int32 selectionEnd = 0;
					fTextView->GetSelection(&selectionStart, &selectionEnd);
					if (selectionStart == selectionEnd && selectionStart > 0
						&& fTextView->Text()[selectionStart - 1] == '<') {
						fTextView->Delete(selectionStart - 1, selectionStart);
						fTextView->Select(selectionStart - 1,
							selectionStart - 1);
					}
				}
				bool expandTemplate = true;
				bool requestedExpansion = true;
				if (message->FindBool("expand_template",
						&requestedExpansion) == B_OK) {
					expandTemplate = requestedExpansion;
				}
				if (expandTemplate)
					_InsertTagTemplate(tagTemplate);
				else
					_InsertSnippet(tagTemplate);
			}
			break;
		}
		case kMessageScheduleTagCompletion:
			_ScheduleTagCompletion();
			break;
		case kMessageShowTagCompletion:
			delete fTagCompletionRunner;
			fTagCompletionRunner = nullptr;
			_ShowTagCompletion();
			break;
		case kMessageCancelTagCompletion:
			delete fTagCompletionRunner;
			fTagCompletionRunner = nullptr;
			break;
		case kMessageTagLauncherClosed:
			fTagLauncher = nullptr;
			Activate(true);
			fTextView->MakeFocus(true);
			break;
		case kMessageAutomationGetScroll:
			_ReplyWithScrollState(message);
			break;
		case kMessageAutomationSetScroll:
		{
			int64 session = 0;
			float value = 0.0f;
			if (message->FindInt64("session", &session) != B_OK
				|| message->FindFloat("value", &value) != B_OK
				|| session == 0 || session != fAutomationSession) {
				BMessage reply;
				reply.AddInt32("status", B_BAD_VALUE);
				message->SendReply(&reply);
				break;
			}

			BScrollBar* scrollBar = fScrollView->ScrollBar(B_VERTICAL);
			if (scrollBar == nullptr) {
				BMessage reply;
				reply.AddInt32("status", B_NO_INIT);
				message->SendReply(&reply);
				break;
			}

			value = std::max(0.0f, std::min(value,
				_VerticalScrollMaximum()));
			fTextView->ScrollTo(fTextView->Bounds().left, value);
			scrollBar->SetValue(value);
			_ReplyWithScrollState(message);
			break;
		}
		case kMessageAutomationClose:
		{
			int64 session = 0;
			BMessage reply;
			if (message->FindInt64("session", &session) != B_OK
				|| session == 0 || session != fAutomationSession) {
				reply.AddInt32("status", B_BAD_VALUE);
				message->SendReply(&reply);
				break;
			}

			reply.AddInt32("status", B_OK);
			message->SendReply(&reply);
			PostMessage(B_QUIT_REQUESTED);
			break;
		}
		case kMessageShowreelStop:
		{
			BMessenger showreel("application/x-vnd.ShowReel");
			if (fAutomationSession != 0 && showreel.IsValid())
				showreel.SendMessage(kMessageShowreelStop);
			break;
		}
		case B_ABOUT_REQUESTED:
			be_app->AboutRequested();
			break;
		case B_UNDO:
			if (!fReadOnly)
				fTextView->Undo(be_clipboard);
			break;
		case B_CUT:
			if (!fReadOnly)
				fTextView->Cut(be_clipboard);
			break;
		case B_COPY:
			fTextView->Copy(be_clipboard);
			break;
		case B_PASTE:
			if (!fReadOnly)
				fTextView->Paste(be_clipboard);
			break;
		case B_SELECT_ALL:
			fTextView->SelectAll();
			break;
		default:
			BWindow::MessageReceived(message);
			break;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


BHandler*
EditorWindow::ResolveSpecifier(BMessage* message, int32 index,
	BMessage* specifier, int32 form, const char* property) {
	BPropertyInfo propertyInfo(kDocumentProperties);
	if (propertyInfo.FindMatch(message, 0, specifier, form, property) >= 0)
		return this;

	return BWindow::ResolveSpecifier(message, index, specifier, form, property);
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
EditorWindow::QuitRequested() {
	if (fPresentationMode)
		return true;

	if (fModified) {
		BString message("Save changes to “");
		message << (fHasDocument ? fDocument.name : "Untitled") << "”?";
		BAlert* alert = new BAlert("Unsaved changes", message.String(), "Cancel",
			"Discard", "Save", B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		int32 choice = alert->Go();
		if (choice == 0)
			return false;
		if (choice == 2) {
			if (!fHasDocument) {
				fPendingAction = PendingAction::Close;
				_ShowSavePanel();
				return false;
			}
			status_t error = _SaveDocument();
			if (error != B_OK) {
				ShowFileError("Saving", fDocument.name, error);
				return false;
			}
		}
	}

	return true;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ReplyWithScrollState(BMessage* message) {
	BMessage reply;
	BScrollBar* scrollBar = fScrollView->ScrollBar(B_VERTICAL);
	if (scrollBar == nullptr) {
		reply.AddInt32("status", B_NO_INIT);
		message->SendReply(&reply);
		return;
	}

	float minimum = 0.0f;
	float maximum = _VerticalScrollMaximum();

	font_height fontHeight;
	fTextView->GetFontHeight(&fontHeight);

	reply.AddInt32("status", B_OK);
	reply.AddBool("ready", true);
	reply.AddInt64("session", fAutomationSession);
	reply.AddFloat("minimum", minimum);
	reply.AddFloat("maximum", maximum);
	reply.AddFloat("value", fTextView->Bounds().top);
	reply.AddFloat("line_height",
		fontHeight.ascent + fontHeight.descent + fontHeight.leading);
	message->SendReply(&reply);
}
//---------------------------------------------------------------------------------------------------------------------------------//


float
EditorWindow::_VerticalScrollMaximum() const {
	if (fTextView->TextLength() == 0)
		return 0.0f;

	float topInset = 0.0f;
	float bottomInset = 0.0f;
	fTextView->GetInsets(nullptr, &topInset, nullptr, &bottomInset);
	float contentHeight = fTextView->TextHeight(0,
		fTextView->CountLines() - 1);
	return std::max(0.0f, topInset + contentHeight + bottomInset
		- fTextView->Bounds().Height());
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
EditorWindow::_HandleScriptingMessage(BMessage* message) {
	int32 specifierIndex = 0;
	BMessage specifier;
	BString property;
	if (message->GetCurrentSpecifier(&specifierIndex, &specifier) != B_OK
		|| specifier.FindString("property", &property) != B_OK) {
		return false;
	}

	if (message->what == B_GET_PROPERTY) {
		if (property == "ID") {
			SendScriptingReply(message, B_OK, fDocumentID.String());
			return true;
		}
		if (property == "Path") {
			BPath path;
			if (fHasDocument && path.SetTo(&fDocument) == B_OK)
				SendScriptingReply(message, B_OK, path.Path());
			else
				SendScriptingReply(message, B_OK, "");
			return true;
		}
		if (property == "ReadOnly") {
			SendScriptingReply(message, B_OK, fReadOnly);
			return true;
		}
		if (property == "Presentation") {
			SendScriptingReply(message, B_OK, fPresentation);
			return true;
		}

		BScrollBar* scrollBar = fScrollView->ScrollBar(B_VERTICAL);
		if (scrollBar == nullptr) {
			SendScriptingReply(message, B_NO_INIT);
			return true;
		}
		if (property == "ScrollMaximum") {
			SendScriptingReply(message, B_OK, _VerticalScrollMaximum());
			return true;
		}
		if (property == "ScrollPosition") {
			SendScriptingReply(message, B_OK, fTextView->Bounds().top);
			return true;
		}
	}

	if (message->what == B_SET_PROPERTY && property == "ScrollPosition") {
		float position = 0.0f;
		if (message->FindFloat("data", &position) != B_OK) {
			int32 integerPosition = 0;
			if (message->FindInt32("data", &integerPosition) != B_OK) {
				SendScriptingReply(message, B_BAD_VALUE);
				return true;
			}
			position = static_cast<float>(integerPosition);
		}

		BScrollBar* scrollBar = fScrollView->ScrollBar(B_VERTICAL);
		if (scrollBar == nullptr) {
			SendScriptingReply(message, B_NO_INIT);
			return true;
		}
		position = std::max(0.0f, std::min(position,
			_VerticalScrollMaximum()));
		fTextView->ScrollTo(fTextView->Bounds().left, position);
		scrollBar->SetValue(position);
		SendScriptingReply(message, B_OK, fTextView->Bounds().top);
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//


status_t
EditorWindow::_SaveDocument() {
	BFile file(&fDocument, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status_t error = file.InitCheck();
	if (error != B_OK)
		return error;

	ssize_t expected = fTextView->TextLength();
	ssize_t written = file.Write(fTextView->Text(), expected);
	if (written < 0)
		return static_cast<status_t>(written);
	if (written != expected)
		return B_IO_ERROR;

	_SetModified(false);
	return B_OK;
}
//---------------------------------------------------------------------------------------------------------------------------------//


status_t
EditorWindow::_SaveDocumentAs(const entry_ref& directory, const char* name) {
	BPath path(&directory);
	status_t error = path.InitCheck();
	if (error != B_OK)
		return error;
	error = path.Append(name);
	if (error != B_OK)
		return error;

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	error = file.InitCheck();
	if (error != B_OK)
		return error;

	ssize_t expected = fTextView->TextLength();
	ssize_t written = file.Write(fTextView->Text(), expected);
	if (written < 0)
		return static_cast<status_t>(written);
	if (written != expected)
		return B_IO_ERROR;

	entry_ref document;
	error = get_ref_for_path(path.Path(), &document);
	if (error != B_OK)
		return error;
	_SetDocument(document);
	_SetModified(false);
	_ApplySyntaxHighlighting();
	return B_OK;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ShowOpenPanel() {
	if (!fOpenPanel->IsShowing())
		fOpenPanel->Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ShowSavePanel() {
	if (fHasDocument)
		fSavePanel->SetSaveText(fDocument.name);
	if (!fSavePanel->IsShowing())
		fSavePanel->Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ApplySyntaxHighlighting() {
	int32 textLength = fTextView->TextLength();
	rgb_color baseColour = SyntaxBaseColour();
	if (textLength == 0) {
		fTextView->SetFontAndColor(nullptr, 0, &baseColour);
		return;
	}

	// Colour changes must not reset font metrics or disturb the editor's scroll
	// position while the user is typing.
	fTextView->SetFontAndColor(0, textLength, nullptr, 0, &baseColour);

	SyntaxLanguage language = SyntaxLanguage::PlainText;
	if (strcmp(_FileTypeLabel(), "HTML") == 0)
		language = SyntaxLanguage::Html;
	else if (strcmp(_FileTypeLabel(), "PHP") == 0)
		language = SyntaxLanguage::Php;
	else if (strcmp(_FileTypeLabel(), "Resource definition") == 0)
		language = SyntaxLanguage::Rdef;
	else if (strcmp(_FileTypeLabel(), "C/C++") == 0)
		language = SyntaxLanguage::Cpp;
	else if (strcmp(_FileTypeLabel(), "Makefile") == 0)
		language = SyntaxLanguage::Makefile;
	else if (strcmp(_FileTypeLabel(), "Markdown") == 0)
		language = SyntaxLanguage::Markdown;
	if (language == SyntaxLanguage::PlainText)
		return;

	std::string text(fTextView->Text(), static_cast<std::size_t>(textLength));
	std::vector<SyntaxSpan> spans = BuildSyntaxSpans(text, language);
	for (const SyntaxSpan& span : spans) {
		if (span.start >= static_cast<std::size_t>(textLength))
			continue;
		std::size_t end = span.start + span.length;
		if (end > static_cast<std::size_t>(textLength))
			end = static_cast<std::size_t>(textLength);
		rgb_color colour = SyntaxColour(span.kind);
		fTextView->SetFontAndColor(static_cast<int32>(span.start),
			static_cast<int32>(end), nullptr, 0, &colour);
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ScheduleSyntaxHighlighting() {
	delete fHighlightRunner;
	BMessage message(kMessageHighlightDocument);
	fHighlightRunner = new BMessageRunner(BMessenger(this), &message,
		kHighlightDelay, 1);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_InsertTagTemplate(const char* tagTemplate) {
	int32 selectionStart = 0;
	int32 selectionEnd = 0;
	fTextView->GetSelection(&selectionStart, &selectionEnd);
	std::string selectedText(fTextView->Text() + selectionStart,
		static_cast<std::size_t>(selectionEnd - selectionStart));
	TagInsertion insertion = ExpandTagTemplate(tagTemplate, selectedText);

	fTextView->Delete(selectionStart, selectionEnd);
	fTextView->Select(selectionStart, selectionStart);
	fTextView->Insert(insertion.text.c_str());
	fTextView->Select(selectionStart + static_cast<int32>(insertion.selectionStart),
		selectionStart + static_cast<int32>(insertion.selectionEnd));
	fTextView->ScrollToSelection();
	fTextView->MakeFocus(true);
	_UpdateStatus();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_InsertSnippet(const char* snippet) {
	int32 selectionStart = 0;
	int32 selectionEnd = 0;
	fTextView->GetSelection(&selectionStart, &selectionEnd);
	fTextView->Delete(selectionStart, selectionEnd);
	fTextView->Select(selectionStart, selectionStart);
	fTextView->Insert(snippet);
	int32 insertionEnd = selectionStart + static_cast<int32>(strlen(snippet));
	fTextView->Select(insertionEnd, insertionEnd);
	fTextView->MakeFocus(true);
	_UpdateStatus();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ScheduleTagCompletion() {
	delete fTagCompletionRunner;
	BMessage message(kMessageShowTagCompletion);
	fTagCompletionRunner = new BMessageRunner(BMessenger(this), &message,
		kTagCompletionDelay, 1);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ShowTagCompletion() {
	int32 selectionStart = 0;
	int32 selectionEnd = 0;
	fTextView->GetSelection(&selectionStart, &selectionEnd);
	if (selectionStart != selectionEnd || selectionStart == 0
		|| fTextView->Text()[selectionStart - 1] != '<') {
		return;
	}

	if (fTagLauncher != nullptr) {
		fTagLauncher->Activate(true);
		return;
	}

	BRect documentFrame = Frame();
	const float launcherWidth = 420.0f;
	const float launcherHeight = 340.0f;
	BPoint point(documentFrame.left
		+ (documentFrame.Width() - launcherWidth) / 2.0f,
		documentFrame.top
			+ (documentFrame.Height() - launcherHeight) / 2.0f);
	fTagLauncher = new TagLauncherWindow(this, point);
	fTagLauncher->Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_Preview(PendingAction action) {
	if (!fHasDocument) {
		fPendingAction = action;
		_ShowSavePanel();
		return;
	}

	if (fModified) {
		status_t error = _SaveDocument();
		if (error != B_OK) {
			ShowFileError("Saving", fDocument.name, error);
			return;
		}
	}

	status_t error = _LaunchPreview(action);
	if (error != B_OK) {
		const char* operation = action == PendingAction::PreviewTororu
			? "Previewing in Tororu"
			: "Previewing in the preferred browser";
		ShowFileError(operation, fDocument.name, error);
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


status_t
EditorWindow::_LaunchPreview(PendingAction action) {
	BMessage message(B_REFS_RECEIVED);
	status_t error = message.AddRef("refs", &fDocument);
	if (error != B_OK)
		return error;

	if (action == PendingAction::PreviewBrowser)
		return be_roster->Launch("text/html", &message);
	if (action == PendingAction::PreviewTororu) {
		return be_roster->Launch("application/x-vnd.sikosis-Tororu",
			&message);
	}
	return B_BAD_VALUE;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_ResetScrollPosition() {
	fTextView->Select(0, 0);

	BScrollBar* horizontal = fScrollView->ScrollBar(B_HORIZONTAL);
	if (horizontal != nullptr)
		horizontal->SetValue(0.0f);
	BScrollBar* vertical = fScrollView->ScrollBar(B_VERTICAL);
	if (vertical != nullptr)
		vertical->SetValue(0.0f);

	fTextView->ScrollTo(B_ORIGIN);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_SetDocument(const entry_ref& document) {
	fDocument = document;
	fHasDocument = true;

	BEntry entry(&document);
	BEntry parent;
	if (entry.GetParent(&parent) == B_OK)
		fSavePanel->SetPanelDirectory(&parent);

	_UpdateTitle();
	_UpdateStatus();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_SetModified(bool modified) {
	if (fModified == modified)
		return;
	fModified = modified;
	_UpdateTitle();
	_UpdateStatus();
}
//---------------------------------------------------------------------------------------------------------------------------------//


const char*
EditorWindow::_FileTypeLabel() const {
	if (!fHasDocument)
		return "Plain text";

	if (strcasecmp(fDocument.name, "Makefile") == 0
		|| strcasecmp(fDocument.name, "GNUmakefile") == 0
		|| strcasecmp(fDocument.name, "BSDmakefile") == 0
		|| strncasecmp(fDocument.name, "Makefile.", 9) == 0) {
		return "Makefile";
	}
	const char* extension = strrchr(fDocument.name, '.');
	if (extension == nullptr)
		return "Plain text";
	if (strcasecmp(extension, ".mk") == 0
		|| strcasecmp(extension, ".mak") == 0
		|| strcasecmp(extension, ".make") == 0) {
		return "Makefile";
	}
	if (strcasecmp(extension, ".html") == 0
		|| strcasecmp(extension, ".htm") == 0) {
		return "HTML";
	}
	if (strcasecmp(extension, ".php") == 0
		|| strcasecmp(extension, ".phtml") == 0) {
		return "PHP";
	}
	if (strcasecmp(extension, ".md") == 0
		|| strcasecmp(extension, ".markdown") == 0
		|| strcasecmp(extension, ".mdown") == 0
		|| strcasecmp(extension, ".mkd") == 0) {
		return "Markdown";
	}
	if (strcasecmp(extension, ".rdef") == 0)
		return "Resource definition";
	if (strcasecmp(extension, ".h") == 0
		|| strcasecmp(extension, ".hh") == 0
		|| strcasecmp(extension, ".hpp") == 0
		|| strcasecmp(extension, ".hxx") == 0
		|| strcasecmp(extension, ".c") == 0
		|| strcasecmp(extension, ".cc") == 0
		|| strcasecmp(extension, ".cpp") == 0
		|| strcasecmp(extension, ".cxx") == 0) {
		return "C/C++";
	}
	return "Plain text";
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_UpdateStatus() {
	if (fStatusView == nullptr || fTextView == nullptr)
		return;

	int32 selectionStart = 0;
	int32 selectionEnd = 0;
	fTextView->GetSelection(&selectionStart, &selectionEnd);
	int32 line = fTextView->LineAt(selectionEnd);
	int32 lineOffset = fTextView->OffsetAt(line);
	BString linePrefix;
	linePrefix.SetTo(fTextView->Text() + lineOffset, selectionEnd - lineOffset);
	int32 column = linePrefix.CountChars() + 1;

	BString status;
	status << "Line " << line + 1 << " of " << fTextView->CountLines()
		<< ", Column " << column << "   •   "
		<< _FileTypeLabel() << "   •   "
		<< (fModified ? "Modified" : "Saved");
	if (selectionStart != selectionEnd) {
		BString selection;
		selection.SetTo(fTextView->Text() + selectionStart,
			selectionEnd - selectionStart);
		status << "   •   " << selection.CountChars() << " selected";
	}
	if (status != fStatusView->Text())
		fStatusView->SetText(status.String());
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::_UpdateTitle() {
	BString title(fHasDocument ? fDocument.name : "Untitled");
	if (fModified)
		title << " •";
	title << " — Globe";
	SetTitle(title.String());
}
//---------------------------------------------------------------------------------------------------------------------------------//

