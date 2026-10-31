#ifndef GLOBE_EDITOR_WINDOW_H
#define GLOBE_EDITOR_WINDOW_H

#include <Entry.h>
#include <Window.h>

class BFilePanel;
class BMessageRunner;
class BStringView;
class BTextView;

class EditorWindow : public BWindow {
public:
	explicit EditorWindow(const entry_ref* document = nullptr);
	~EditorWindow() override;

	bool IsPristine() const;
	status_t OpenDocument(const entry_ref& document);
	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	enum class PendingAction {
		None,
		Close,
		PreviewBrowser,
		PreviewTororu
	};

	status_t _SaveDocument();
	status_t _SaveDocumentAs(const entry_ref& directory, const char* name);
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

	BTextView* fTextView;
	BStringView* fStatusView;
	BMessageRunner* fHighlightRunner;
	BMessageRunner* fTagCompletionRunner;
	BFilePanel* fOpenPanel;
	BFilePanel* fSavePanel;
	entry_ref fDocument;
	bool fHasDocument;
	bool fModified;
	PendingAction fPendingAction;
};

#endif
