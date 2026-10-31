#ifndef GLOBE_EDITOR_WINDOW_H
#define GLOBE_EDITOR_WINDOW_H

#include <Entry.h>
#include <String.h>
#include <Window.h>

class BFilePanel;
class BMenuBar;
class BMessageRunner;
class BScrollView;
class BStringView;
class BTextView;
class BView;

class EditorWindow : public BWindow {
public:
	EditorWindow(const char* documentID, const entry_ref* document = nullptr,
		bool readOnly = false, bool presentation = false);
	~EditorWindow() override;

	const char* DocumentID() const;
	status_t GetSupportedSuites(BMessage* message) override;
	bool IsPristine() const;
	status_t OpenDocument(const entry_ref& document);
	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;
	BHandler* ResolveSpecifier(BMessage* message, int32 index,
		BMessage* specifier, int32 form, const char* property) override;

private:
	enum class PendingAction {
		None,
		Close,
		PreviewBrowser,
		PreviewTororu
	};

	status_t _SaveDocument();
	status_t _SaveDocumentAs(const entry_ref& directory, const char* name);
	bool _HandleScriptingMessage(BMessage* message);
	void _ShowOpenPanel();
	void _ShowSavePanel();
	void _ApplySyntaxHighlighting();
	void _ScheduleSyntaxHighlighting();
	void _InsertTagTemplate(const char* tagTemplate);
	void _ScheduleTagCompletion();
	void _ShowTagCompletion();
	void _Preview(PendingAction action);
	status_t _LaunchPreview(PendingAction action);
	void _SetDocument(const entry_ref& document);
	void _SetModified(bool modified);
	const char* _FileTypeLabel() const;
	void _UpdateStatus();
	void _UpdateTitle();

	BMenuBar* fMenuBar;
	BView* fTagToolbar;
	BTextView* fTextView;
	BScrollView* fScrollView;
	BStringView* fStatusView;
	BMessageRunner* fHighlightRunner;
	BMessageRunner* fTagCompletionRunner;
	BFilePanel* fOpenPanel;
	BFilePanel* fSavePanel;
	entry_ref fDocument;
	BString fDocumentID;
	bool fHasDocument;
	bool fModified;
	bool fReadOnly;
	bool fPresentation;
	PendingAction fPendingAction;
};

#endif
