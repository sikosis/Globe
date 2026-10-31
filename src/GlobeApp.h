#ifndef GLOBE_APP_H
#define GLOBE_APP_H

#include <Application.h>
#include <Entry.h>

class EditorWindow;

class GlobeApp : public BApplication {
public:
	GlobeApp();

	void AboutRequested() override;
	void ArgvReceived(int32 argc, char** argv) override;
	status_t GetSupportedSuites(BMessage* message) override;
	void MessageReceived(BMessage* message) override;
	void ReadyToRun() override;
	void RefsReceived(BMessage* message) override;
	BHandler* ResolveSpecifier(BMessage* message, int32 index,
		BMessage* specifier, int32 form, const char* property) override;

private:
	EditorWindow* _CreateWindow(const entry_ref* document = nullptr,
		bool readOnly = false, bool presentation = false);
	EditorWindow* _DocumentAt(int32 index) const;
	EditorWindow* _DocumentByID(const char* id) const;
	EditorWindow* _DocumentFromSpecifier(const BMessage& specifier,
		int32 form) const;
	bool _HandleScriptingMessage(BMessage* message);
	void _OpenDocument(const entry_ref& document);

	bool fOpenedDocument;
	int32 fEditorWindowCount;
	int32 fNextDocumentID;
};

#endif
