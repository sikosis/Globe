#include "GlobeApp.h"

#include "AboutWindow.h"
#include "EditorWindow.h"
#include "Messages.h"

#include <Entry.h>
#include <Message.h>


int
main() {
	GlobeApp application;
	application.Run();
	return 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//


GlobeApp::GlobeApp()
	:
	BApplication("application/x-vnd.Globe"),
	fOpenedDocument(false),
	fEditorWindowCount(0) {
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::AboutRequested() {
	(new AboutWindow())->Show();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::ArgvReceived(int32 argc, char** argv) {
	for (int32 index = 1; index < argc; ++index) {
		entry_ref document;
		if (get_ref_for_path(argv[index], &document) == B_OK)
			_OpenDocument(document);
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::MessageReceived(BMessage* message) {
	switch (message->what) {
		case kMessageNewDocument:
			_CreateWindow()->Activate();
			break;
		case kMessageWindowClosed:
			if (fEditorWindowCount > 0)
				--fEditorWindowCount;
			if (fEditorWindowCount == 0)
				PostMessage(B_QUIT_REQUESTED);
			break;
		default:
			BApplication::MessageReceived(message);
			break;
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::ReadyToRun() {
	if (!fOpenedDocument)
		_CreateWindow();
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::RefsReceived(BMessage* message) {
	entry_ref document;
	for (int32 index = 0;
		message->FindRef("refs", index, &document) == B_OK; ++index) {
		_OpenDocument(document);
	}
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::_OpenDocument(const entry_ref& document) {
	for (int32 index = 0; index < CountWindows(); ++index) {
		EditorWindow* window = dynamic_cast<EditorWindow*>(WindowAt(index));
		if (window == nullptr || !window->Lock())
			continue;

		bool reuseWindow = window->IsPristine();
		if (reuseWindow) {
			status_t error = window->OpenDocument(document);
			window->Activate();
			window->Unlock();
			if (error == B_OK) {
				fOpenedDocument = true;
				return;
			}
			return;
		}
		window->Unlock();
	}

	_CreateWindow(&document);
	fOpenedDocument = true;
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow*
GlobeApp::_CreateWindow(const entry_ref* document) {
	EditorWindow* window = new EditorWindow(document);
	++fEditorWindowCount;
	return window;
}
//---------------------------------------------------------------------------------------------------------------------------------//

