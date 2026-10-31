#include "GlobeApp.h"

#include "AboutWindow.h"
#include "EditorWindow.h"
#include "Messages.h"

#include <Entry.h>
#include <File.h>
#include <Message.h>
#include <PropertyInfo.h>
#include <String.h>

#include <cstring>

namespace {

property_info kGlobeProperties[] = {
	{ "Document", { B_CREATE_PROPERTY, B_COUNT_PROPERTIES,
		B_DELETE_PROPERTY }, { B_DIRECT_SPECIFIER, B_INDEX_SPECIFIER,
		B_ID_SPECIFIER, B_NAME_SPECIFIER },
		"Create, count, address, or close documents", 0, {}, {}, {} },
	{ nullptr, {}, {}, nullptr, 0, {}, {}, {} }
};


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
SendScriptingReply(BMessage* request, status_t error, int32 result) {
	BMessage reply(B_REPLY);
	reply.AddInt32("result", result);
	reply.AddInt32("error", error);
	request->SendReply(&reply);
}
//---------------------------------------------------------------------------------------------------------------------------------//


} // namespace


int
main() {
	GlobeApp application;
	application.Run();
	return 0;
}
//---------------------------------------------------------------------------------------------------------------------------------//


GlobeApp::GlobeApp()
	:
	BApplication("application/x-vnd.hGlobe"),
	fOpenedDocument(false),
	fEditorWindowCount(0),
	fNextDocumentID(1) {
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


status_t
GlobeApp::GetSupportedSuites(BMessage* message) {
	message->AddString("suites", "suite/vnd.hGlobe");
	BPropertyInfo propertyInfo(kGlobeProperties);
	message->AddFlat("messages", &propertyInfo);
	return BApplication::GetSupportedSuites(message);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
GlobeApp::MessageReceived(BMessage* message) {
	if ((message->what == B_CREATE_PROPERTY
			|| message->what == B_COUNT_PROPERTIES
			|| message->what == B_DELETE_PROPERTY)
		&& _HandleScriptingMessage(message)) {
		return;
	}

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


BHandler*
GlobeApp::ResolveSpecifier(BMessage* message, int32 index,
	BMessage* specifier, int32 form, const char* property) {
	if (strcmp(property, "Document") == 0) {
		if ((form == B_INDEX_SPECIFIER || form == B_ID_SPECIFIER
				|| form == B_NAME_SPECIFIER)
			&& message->what != B_DELETE_PROPERTY) {
			return _DocumentFromSpecifier(*specifier, form);
		}
		return this;
	}

	BPropertyInfo propertyInfo(kGlobeProperties);
	if (propertyInfo.FindMatch(message, 0, specifier, form, property) >= 0)
		return this;

	return BApplication::ResolveSpecifier(message, index, specifier, form,
		property);
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
GlobeApp::_CreateWindow(const entry_ref* document, bool readOnly,
	bool presentation) {
	BString documentID;
	documentID.SetToFormat("document-%ld",
		static_cast<long>(fNextDocumentID++));
	EditorWindow* window = new EditorWindow(documentID.String(), document,
		readOnly, presentation);
	++fEditorWindowCount;
	return window;
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow*
GlobeApp::_DocumentAt(int32 documentIndex) const {
	int32 currentIndex = 0;
	for (int32 index = 0; index < CountWindows(); ++index) {
		EditorWindow* window = dynamic_cast<EditorWindow*>(WindowAt(index));
		if (window == nullptr)
			continue;
		if (currentIndex == documentIndex)
			return window;
		++currentIndex;
	}
	return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow*
GlobeApp::_DocumentByID(const char* id) const {
	if (id == nullptr)
		return nullptr;
	for (int32 index = 0; index < CountWindows(); ++index) {
		EditorWindow* window = dynamic_cast<EditorWindow*>(WindowAt(index));
		if (window != nullptr && strcmp(window->DocumentID(), id) == 0)
			return window;
	}
	return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------------------//


EditorWindow*
GlobeApp::_DocumentFromSpecifier(const BMessage& specifier, int32 form) const {
	if (form == B_INDEX_SPECIFIER) {
		int32 index = -1;
		if (specifier.FindInt32("index", &index) == B_OK && index >= 0)
			return _DocumentAt(index);
		return nullptr;
	}
	if (form == B_ID_SPECIFIER) {
		const char* id = nullptr;
		if (specifier.FindString("id", &id) == B_OK)
			return _DocumentByID(id);
		int32 numericID = 0;
		if (specifier.FindInt32("id", &numericID) == B_OK) {
			BString documentID;
			documentID.SetToFormat("document-%ld",
				static_cast<long>(numericID));
			return _DocumentByID(documentID.String());
		}
	}
	if (form == B_NAME_SPECIFIER) {
		const char* name = nullptr;
		if (specifier.FindString("name", &name) == B_OK)
			return _DocumentByID(name);
	}
	return nullptr;
}
//---------------------------------------------------------------------------------------------------------------------------------//


bool
GlobeApp::_HandleScriptingMessage(BMessage* message) {
	int32 specifierIndex = 0;
	BMessage specifier;
	BString property;
	if (message->GetCurrentSpecifier(&specifierIndex, &specifier) != B_OK
		|| specifier.FindString("property", &property) != B_OK
		|| property != "Document") {
		return false;
	}

	if (message->what == B_COUNT_PROPERTIES) {
		SendScriptingReply(message, B_OK, fEditorWindowCount);
		return true;
	}

	if (message->what == B_CREATE_PROPERTY) {
		const char* path = nullptr;
		bool readOnly = false;
		bool presentation = false;
		message->FindString("Path", &path);
		message->FindBool("ReadOnly", &readOnly);
		message->FindBool("Presentation", &presentation);

		entry_ref document;
		const entry_ref* documentPointer = nullptr;
		if (path != nullptr) {
			status_t error = get_ref_for_path(path, &document);
			if (error != B_OK) {
				SendScriptingReply(message, error);
				return true;
			}
			BFile file(&document, B_READ_ONLY);
			error = file.InitCheck();
			if (error != B_OK) {
				SendScriptingReply(message, error);
				return true;
			}
			documentPointer = &document;
		}

		EditorWindow* window = _CreateWindow(documentPointer, readOnly,
			presentation);
		fOpenedDocument = documentPointer != nullptr || fOpenedDocument;
		SendScriptingReply(message, B_OK, window->DocumentID());
		return true;
	}

	if (message->what == B_DELETE_PROPERTY) {
		EditorWindow* window = _DocumentFromSpecifier(specifier, specifier.what);
		if (window == nullptr) {
			SendScriptingReply(message, B_BAD_INDEX);
			return true;
		}
		status_t error = window->PostMessage(B_QUIT_REQUESTED);
		SendScriptingReply(message, error);
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------------------------------------------------------------//

