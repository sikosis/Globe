#ifndef GLOBE_EDITOR_WINDOW_H
#define GLOBE_EDITOR_WINDOW_H

#include <Entry.h>
#include <Window.h>

class BFilePanel;
class BTextView;

class EditorWindow : public BWindow {
public:
	explicit EditorWindow(const entry_ref* document = nullptr);
	~EditorWindow() override;

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	status_t _LoadDocument(const entry_ref& document);
	status_t _SaveDocument();
	status_t _SaveDocumentAs(const entry_ref& directory, const char* name);
	void _ShowOpenPanel();
	void _ShowSavePanel();
	void _SetDocument(const entry_ref& document);
	void _SetModified(bool modified);
	void _UpdateTitle();

	BTextView* fTextView;
	BFilePanel* fOpenPanel;
	BFilePanel* fSavePanel;
	entry_ref fDocument;
	bool fHasDocument;
	bool fModified;
	bool fCloseAfterSave;
};

#endif
