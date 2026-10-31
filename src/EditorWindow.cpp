#include "EditorWindow.h"

#include "Messages.h"
#include "SyntaxHighlighter.h"
#include "TagTools.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Clipboard.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <PropertyInfo.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <Size.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <View.h>

#include <climits>
#include <cstring>
#include <strings.h>
#include <vector>

namespace {

const float kLineNumberGutterWidth = 52.0f;
const bigtime_t kHighlightDelay = 150000;
const bigtime_t kTagCompletionDelay = 300000;
const float kAutomationScreenInset = 24.0f;

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
EditorFrame(bool automation) {
	if (!automation)
		return BRect(100, 100, 900, 700);

	BScreen screen;
	if (!screen.IsValid())
		return BRect(24, 24, 1000, 700);

	BRect frame = screen.Frame();
	frame.InsetBy(kAutomationScreenInset, kAutomationScreenInset);
	return frame;
}
//---------------------------------------------------------------------------------------------------------------------------------//


uint32
EditorWindowFlags(bool automation) {
	uint32 flags = B_ASYNCHRONOUS_CONTROLS;
	if (!automation)
		flags |= B_AUTO_UPDATE_SIZE_LIMITS;
	return flags;
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


rgb_color
SyntaxColour(SyntaxKind kind) {
	rgb_color background = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	int32 luminance = (299 * background.red + 587 * background.green
		+ 114 * background.blue) / 1000;
	if (luminance < 128) {
		switch (kind) {
			case SyntaxKind::Tag:
				return {120, 180, 255, 255};
			case SyntaxKind::Keyword:
				return {203, 166, 247, 255};
			case SyntaxKind::Attribute:
				return {112, 210, 215, 255};
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
		case SyntaxKind::Keyword:
			return ui_color(B_LINK_TEXT_COLOR);
		case SyntaxKind::Attribute:
			return ui_color(B_NAVIGATION_BASE_COLOR);
		case SyntaxKind::String:
		case SyntaxKind::Number:
			return ui_color(B_SUCCESS_COLOR);
		case SyntaxKind::Comment:
			return tint_color(ui_color(B_DOCUMENT_TEXT_COLOR),
				B_DISABLED_LABEL_TINT);
		case SyntaxKind::Entity:
			return ui_color(B_CONTROL_MARK_COLOR);
		case SyntaxKind::Variable:
			return ui_color(B_LINK_VISITED_COLOR);
	}
	return ui_color(B_DOCUMENT_TEXT_COLOR);
}
//---------------------------------------------------------------------------------------------------------------------------------//


BButton*
TagButton(const char* label, const char* tagTemplate, const char* toolTip,
	const char* iconResource) {
	BMessage* message = new BMessage(kMessageInsertTag);
	message->AddString("template", tagTemplate);
	BButton* button = new BButton(label, message);
	BBitmap* icon = BTranslationUtils::GetBitmap('PNG ', iconResource);
	if (icon != nullptr) {
		button->SetIcon(icon, B_TRIM_ICON_BITMAP_KEEP_ASPECT);
		button->SetLabel("");
		button->SetExplicitMinSize(BSize(34.0f, 34.0f));
		button->SetExplicitMaxSize(BSize(34.0f, 34.0f));
		delete icon;
	}
	button->SetToolTip(toolTip);
	return button;
}
//---------------------------------------------------------------------------------------------------------------------------------//


class EditorTextView : public BTextView {
public:
	EditorTextView()
		:
		BTextView("editor", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
		fNotifyChanges(true) {
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void SetNotifyChanges(bool notifyChanges) {
		fNotifyChanges = notifyChanges;
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void Draw(BRect updateRect) override {
		BTextView::Draw(updateRect);

		BRect gutter(Bounds());
		gutter.right = gutter.left + kLineNumberGutterWidth;
		SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		FillRect(gutter & updateRect);

		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		StrokeLine(BPoint(gutter.right, gutter.top),
			BPoint(gutter.right, gutter.bottom));

		font_height fontHeight;
		GetFontHeight(&fontHeight);
		SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_DISABLED_LABEL_TINT));

		int32 firstLine = LineAt(BPoint(TextRect().left, updateRect.top));
		int32 lastLine = LineAt(BPoint(TextRect().left, updateRect.bottom));
		if (firstLine < 0)
			firstLine = 0;
		if (lastLine >= CountLines())
			lastLine = CountLines() - 1;

		for (int32 line = firstLine; line <= lastLine; ++line) {
			BPoint linePosition = PointAt(OffsetAt(line));
			BString lineNumber;
			lineNumber << line + 1;
			float numberWidth = StringWidth(lineNumber.String());
			DrawString(lineNumber.String(), BPoint(gutter.right - numberWidth - 7,
				linePosition.y + fontHeight.ascent));
		}
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void Pulse() override {
		BTextView::Pulse();
		if (Window() != nullptr)
			Window()->PostMessage(kMessageUpdateStatus);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void KeyDown(const char* bytes, int32 numBytes) override {
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
		BTextView::InsertText(text, length, offset, runs);
		Invalidate(BRect(Bounds().left, Bounds().top,
			Bounds().left + kLineNumberGutterWidth, Bounds().bottom));
		if (fNotifyChanges && Window() != nullptr)
			Window()->PostMessage(kMessageDocumentChanged);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


	void DeleteText(int32 fromOffset, int32 toOffset) override {
		BTextView::DeleteText(fromOffset, toOffset);
		Invalidate(BRect(Bounds().left, Bounds().top,
			Bounds().left + kLineNumberGutterWidth, Bounds().bottom));
		if (fNotifyChanges && Window() != nullptr)
			Window()->PostMessage(kMessageDocumentChanged);
	}
	//---------------------------------------------------------------------------------------------------------------------------------//


private:
	bool fNotifyChanges;
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
	bool readOnly, bool presentation, int64 automationSession)
	:
	BWindow(EditorFrame(automationSession != 0), "Untitled — Globe",
		B_DOCUMENT_WINDOW, EditorWindowFlags(automationSession != 0)),
	fMenuBar(nullptr),
	fTagToolbar(nullptr),
	fTextView(nullptr),
	fScrollView(nullptr),
	fStatusView(nullptr),
	fHighlightRunner(nullptr),
	fTagCompletionRunner(nullptr),
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
		.Add(TagButton("IMG", "<img src=\"%c\" alt=\"\">", "Image",
			"toolbar-image.png"))
		.Add(TagButton("DIV", "<div>%c</div>", "Division", "toolbar-div.png"))
		.Add(TagButton("SPAN", "<span>%c</span>", "Inline span",
			"toolbar-span.png"))
		.Add(TagButton("UL", "<ul>\n%c\n</ul>", "Unordered list",
			"toolbar-ul.png"))
		.Add(TagButton("LI", "<li>%c</li>", "List item", "toolbar-li.png"))
		.Add(TagButton("BR", "<br>%c", "Line break", "toolbar-br.png"))
		.AddGlue();

	EditorTextView* editor = new EditorTextView();
	fTextView = editor;
	fTextView->SetStylable(true);
	fTextView->SetFontAndColor(be_fixed_font);
	fTextView->SetDoesUndo(true);
	fTextView->SetWordWrap(false);
	fTextView->MakeEditable(!fReadOnly);
	fTextView->MakeFocus(true);
	fStatusView = new BStringView("status", "Line 1, Column 1");
	fStatusView->SetTruncation(B_TRUNCATE_END);
	fStatusView->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
	fStatusView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	fScrollView = new BScrollView("editor scroll", fTextView,
		0, 0, true, true);
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fTagToolbar)
		.Add(fScrollView)
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

	if (fPresentationMode) {
		SetSizeLimits(240.0f, B_SIZE_UNLIMITED, 160.0f, B_SIZE_UNLIMITED);
	} else {
		CenterOnScreen();
	}
	_UpdateStatus();
	Layout(true);
	Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow::~EditorWindow() {
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
		SetSizeLimits(240.0f, B_SIZE_UNLIMITED, 160.0f,
			B_SIZE_UNLIMITED);
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
			break;
		case kMessageUpdateStatus:
			_UpdateStatus();
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
				_InsertTagTemplate(tagTemplate);
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
	float maximum = 0.0f;
	scrollBar->GetRange(&minimum, &maximum);

	font_height fontHeight;
	fTextView->GetFontHeight(&fontHeight);

	reply.AddInt32("status", B_OK);
	reply.AddBool("ready", true);
	reply.AddInt64("session", fAutomationSession);
	reply.AddFloat("minimum", minimum);
	reply.AddFloat("maximum", maximum);
	reply.AddFloat("value", scrollBar->Value());
	reply.AddFloat("line_height",
		fontHeight.ascent + fontHeight.descent + fontHeight.leading);
	message->SendReply(&reply);
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
			float minimum = 0.0f;
			float maximum = 0.0f;
			scrollBar->GetRange(&minimum, &maximum);
			SendScriptingReply(message, B_OK, maximum);
			return true;
		}
		if (property == "ScrollPosition") {
			SendScriptingReply(message, B_OK, scrollBar->Value());
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
		scrollBar->SetValue(position);
		SendScriptingReply(message, B_OK, scrollBar->Value());
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
	rgb_color baseColour = ui_color(B_DOCUMENT_TEXT_COLOR);
	BFont font(*be_fixed_font);
	if (textLength == 0) {
		fTextView->SetFontAndColor(&font, B_FONT_ALL, &baseColour);
		return;
	}

	fTextView->SetFontAndColor(0, textLength, &font, B_FONT_ALL, &baseColour);

	SyntaxLanguage language = SyntaxLanguage::PlainText;
	if (strcmp(_FileTypeLabel(), "HTML") == 0)
		language = SyntaxLanguage::Html;
	else if (strcmp(_FileTypeLabel(), "PHP") == 0)
		language = SyntaxLanguage::Php;
	else if (strcmp(_FileTypeLabel(), "Resource definition") == 0)
		language = SyntaxLanguage::Rdef;
	else if (strcmp(_FileTypeLabel(), "C/C++") == 0)
		language = SyntaxLanguage::Cpp;
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

	BPopUpMenu* menu = new BPopUpMenu("HTML tags", false, false);
	for (const char* tag : kHtmlTags)
		menu->AddItem(new BMenuItem(tag, nullptr));

	font_height fontHeight;
	fTextView->GetFontHeight(&fontHeight);
	BPoint point = fTextView->PointAt(selectionStart);
	point.y += fontHeight.ascent + fontHeight.descent + fontHeight.leading + 3.0f;
	point = fTextView->ConvertToScreen(point);
	BMenuItem* selected = menu->Go(point, false, true);
	if (selected != nullptr) {
		std::string tagTemplate = BuildHtmlTagTemplate(selected->Label());
		fTextView->Select(selectionStart - 1, selectionStart);
		_InsertTagTemplate(tagTemplate.c_str());
	}
	delete menu;
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

	const char* extension = strrchr(fDocument.name, '.');
	if (extension == nullptr)
		return "Plain text";
	if (strcasecmp(extension, ".html") == 0
		|| strcasecmp(extension, ".htm") == 0) {
		return "HTML";
	}
	if (strcasecmp(extension, ".php") == 0
		|| strcasecmp(extension, ".phtml") == 0) {
		return "PHP";
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

