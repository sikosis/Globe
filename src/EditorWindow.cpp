#include "EditorWindow.h"

#include "Messages.h"
#include "SyntaxHighlighter.h"

#include <Alert.h>
#include <Application.h>
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
#include <Roster.h>
#include <ScrollView.h>
#include <Size.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>

#include <climits>
#include <cstring>
#include <strings.h>
#include <vector>

namespace {

const float kLineNumberGutterWidth = 52.0f;
const bigtime_t kHighlightDelay = 150000;


rgb_color
SyntaxColour(SyntaxKind kind) {
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


class EditorTextView : public BTextView {
public:
	EditorTextView(BRect frame, BRect textRect)
		:
		BTextView(frame, "editor", textRect, B_FOLLOW_ALL,
			B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
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


EditorWindow::EditorWindow(const entry_ref* document)
	:
	BWindow(BRect(100, 100, 900, 700), "Untitled — Globe", B_DOCUMENT_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fTextView(nullptr),
	fStatusView(nullptr),
	fHighlightRunner(nullptr),
	fOpenPanel(nullptr),
	fSavePanel(nullptr),
	fHasDocument(false),
	fModified(false),
	fPendingAction(PendingAction::None) {
	BMenuBar* menuBar = new BMenuBar("menu bar");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("New", new BMessage(kMessageNewDocument), 'N'));
	fileMenu->AddItem(new BMenuItem("Open…", new BMessage(kMessageOpenDocument), 'O'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Save", new BMessage(kMessageSaveDocument), 'S'));
	fileMenu->AddItem(new BMenuItem("Save as…",
		new BMessage(kMessageSaveDocumentAs), 'S', B_SHIFT_KEY));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(fileMenu);

	BMenu* editMenu = new BMenu("Edit");
	editMenu->AddItem(new BMenuItem("Undo", new BMessage(B_UNDO), 'Z'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cut", new BMessage(B_CUT), 'X'));
	editMenu->AddItem(new BMenuItem("Copy", new BMessage(B_COPY), 'C'));
	editMenu->AddItem(new BMenuItem("Paste", new BMessage(B_PASTE), 'V'));
	editMenu->AddItem(new BMenuItem("Select all", new BMessage(B_SELECT_ALL), 'A'));
	menuBar->AddItem(editMenu);

	BMenu* previewMenu = new BMenu("Preview");
	previewMenu->AddItem(new BMenuItem("Preferred browser",
		new BMessage(kMessagePreviewBrowser), 'P'));
	previewMenu->AddItem(new BMenuItem("Tororu",
		new BMessage(kMessagePreviewTororu), 'P', B_SHIFT_KEY));
	menuBar->AddItem(previewMenu);

	BRect textFrame(0, 0, 100, 100);
	BRect textRect(textFrame);
	textRect.left += kLineNumberGutterWidth + 8;
	textRect.top += 8;
	textRect.right -= 8;
	textRect.bottom -= 8;
	EditorTextView* editor = new EditorTextView(textFrame, textRect);
	fTextView = editor;
	fTextView->SetStylable(true);
	fTextView->SetFontAndColor(be_fixed_font);
	fTextView->SetDoesUndo(true);
	fTextView->SetWordWrap(false);
	fTextView->MakeFocus(true);
	fStatusView = new BStringView("status", "Line 1, Column 1");
	fStatusView->SetTruncation(B_TRUNCATE_END);
	fStatusView->SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
	fStatusView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	BScrollView* scrollView = new BScrollView("editor scroll", fTextView,
		B_FOLLOW_ALL, 0, true, true);
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.Add(scrollView)
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(B_USE_SMALL_SPACING, 2, B_USE_SMALL_SPACING, 2)
			.Add(fStatusView)
		.End();

	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(be_app), nullptr,
		B_FILE_NODE, true);
	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this), nullptr,
		B_FILE_NODE, false, new BMessage(kMessageSavePanelAccepted));

	if (document != nullptr) {
		OpenDocument(*document);
	}

	CenterOnScreen();
	_UpdateStatus();
	Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow::~EditorWindow() {
	delete fHighlightRunner;
	delete fOpenPanel;
	delete fSavePanel;
	be_app->PostMessage(kMessageWindowClosed);
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
EditorWindow::IsPristine() const {
	return !fHasDocument && !fModified && fTextView->TextLength() == 0;
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
EditorWindow::MessageReceived(BMessage* message) {
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
		case B_UNDO:
			fTextView->Undo(be_clipboard);
			break;
		case B_CUT:
			fTextView->Cut(be_clipboard);
			break;
		case B_COPY:
			fTextView->Copy(be_clipboard);
			break;
		case B_PASTE:
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


bool
EditorWindow::QuitRequested() {
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

