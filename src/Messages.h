#ifndef GLOBE_MESSAGES_H
#define GLOBE_MESSAGES_H

#include <SupportDefs.h>

enum {
	kMessageNewDocument = 'gnew',
	kMessageOpenDocument = 'gopn',
	kMessageSaveDocument = 'gsav',
	kMessageSaveDocumentAs = 'gsva',
	kMessageSavePanelAccepted = 'gspa',
	kMessageDocumentChanged = 'gchg',
	kMessageWindowClosed = 'gwcl'
};

#endif
