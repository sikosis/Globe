#ifndef GLOBE_APP_H
#define GLOBE_APP_H

#include <Application.h>
#include <Entry.h>

class GlobeApp : public BApplication {
public:
	GlobeApp();

	void ArgvReceived(int32 argc, char** argv) override;
	void MessageReceived(BMessage* message) override;
	void ReadyToRun() override;
	void RefsReceived(BMessage* message) override;

private:
	void _OpenDocument(const entry_ref& document);

	bool fOpenedDocument;
};

#endif
