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
	kMessageUpdateStatus = 'gust',
	kMessageHighlightDocument = 'ghlt',
	kMessagePreviewBrowser = 'gpwb',
	kMessagePreviewTororu = 'gpwt',
	kMessageInsertTag = 'gitg',
	kMessageScheduleTagCompletion = 'gstc',
	kMessageShowTagCompletion = 'gshc',
	kMessageCancelTagCompletion = 'gctc',
	kMessageWindowClosed = 'gwcl',
	kMessageAutomationOpen = 'gaop',
	kMessageAutomationGetScroll = 'gags',
	kMessageAutomationSetScroll = 'gass',
	kMessageAutomationClose = 'gacl'
};

#endif
