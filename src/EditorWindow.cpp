#include "EditorWindow.h"

#include "Messages.h"

#include <Alert.h>
#include <Application.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>

#include <climits>
#include <cstring>
#include <strings.h>
#include <vector>

namespace {

const float kLineNumberGutterWidth = 52.0f;


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
	fOpenPanel(nullptr),
	fSavePanel(nullptr),
	fHasDocument(false),
	fModified(false),
	fCloseAfterSave(false) {
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

	BRect textFrame(0, 0, 100, 100);
	BRect textRect(textFrame);
	textRect.left += kLineNumberGutterWidth + 8;
	textRect.top += 8;
	textRect.right -= 8;
	textRect.bottom -= 8;
	EditorTextView* editor = new EditorTextView(textFrame, textRect);
	fTextView = editor;
	fTextView->SetStylable(false);
	fTextView->SetDoesUndo(true);
	fTextView->SetWordWrap(false);
	fTextView->MakeFocus(true);
	editMenu->SetTargetForItems(fTextView);
	fStatusView = new BStringView("status", "Line 1, Column 1");

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
		status_t error = _LoadDocument(*document);
		if (error != B_OK)
			ShowFileError("Opening", document->name, error);
	}

	CenterOnScreen();
	SetPulseRate(100000);
	_UpdateStatus();
	Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow::~EditorWindow() {
	delete fOpenPanel;
	delete fSavePanel;
	be_app->PostMessage(kMessageWindowClosed);
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
					fCloseAfterSave = false;
				} else if (fCloseAfterSave) {
					fCloseAfterSave = false;
					PostMessage(B_QUIT_REQUESTED);
				}
			}
			break;
		}
		case B_CANCEL:
			fCloseAfterSave = false;
			break;
		case kMessageDocumentChanged:
			_SetModified(true);
			break;
		default:
			BWindow::MessageReceived(message);
			break;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
EditorWindow::Pulse() {
	_UpdateStatus();
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
				fCloseAfterSave = true;
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
EditorWindow::_LoadDocument(const entry_ref& document) {
	BFile file(&document, B_READ_ONLY);
	status_t error = file.InitCheck();
	if (error != B_OK)
		return error;

	off_t size = 0;
	error = file.GetSize(&size);
	if (error != B_OK)
		return error;
	if (size < 0 || size > INT32_MAX)
		return B_BAD_VALUE;

	std::vector<char> contents(static_cast<size_t>(size) + 1, '\0');
	int32 totalRead = 0;
	while (totalRead < size) {
		ssize_t bytesRead = file.Read(contents.data() + totalRead,
			static_cast<size_t>(size - totalRead));
		if (bytesRead < 0)
			return static_cast<status_t>(bytesRead);
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
	return B_OK;
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
EditorWindow::_SetDocument(const entry_ref& document) {
	fDocument = document;
	fHasDocument = true;
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

