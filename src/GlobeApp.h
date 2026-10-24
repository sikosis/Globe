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
	void MessageReceived(BMessage* message) override;
	void ReadyToRun() override;
	void RefsReceived(BMessage* message) override;

private:
	EditorWindow* _CreateWindow(const entry_ref* document = nullptr);
	void _OpenDocument(const entry_ref& document);

	bool fOpenedDocument;
	int32 fEditorWindowCount;
};

#endif
