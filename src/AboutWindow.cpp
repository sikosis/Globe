#include "AboutWindow.h"

#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <File.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Roster.h>
#include <SeparatorView.h>
#include <Size.h>
#include <String.h>
#include <StringView.h>
#include <TabView.h>
#include <TranslationUtils.h>
#include <View.h>

namespace {

class ArtworkView : public BView {
public:
	explicit ArtworkView(BBitmap* artwork);
	~ArtworkView() override;

	void Draw(BRect updateRect) override;

private:
	BBitmap* fArtwork;
};


class LinkView : public BStringView {
public:
	LinkView(const char* name, const char* label, const char* url);

	void AttachedToWindow() override;
	void MouseDown(BPoint point) override;

private:
	BString fUrl;
};


BBitmap*
LoadArtwork() {
	return BTranslationUtils::GetBitmap('PNG ', "about-artwork.png");
}
//---------------------------------------------------------------------------------------------------------------------------------//


BString
AppVersion() {
	BString version("Version v0.1");
	app_info appInfo;
	if (be_app->GetAppInfo(&appInfo) != B_OK)
		return version;

	BFile appFile(&appInfo.ref, B_READ_ONLY);
	BAppFileInfo appFileInfo(&appFile);
	version_info versionInfo;
	if (appFileInfo.GetVersionInfo(&versionInfo, B_APP_VERSION_KIND) == B_OK) {
		if (versionInfo.minor == 0) {
			version.SetToFormat("Version v%lu.%lu",
				(unsigned long)versionInfo.major,
				(unsigned long)versionInfo.middle);
		} else {
			version.SetToFormat("Version v%lu.%lu%lu",
				(unsigned long)versionInfo.major,
				(unsigned long)versionInfo.middle,
				(unsigned long)versionInfo.minor);
		}
	}

	return version;
}
//---------------------------------------------------------------------------------------------------------------------------------//


BStringView*
Heading(const char* name, const char* text) {
	BStringView* heading = new BStringView(name, text);
	heading->SetFont(be_bold_font);
	return heading;
}
//---------------------------------------------------------------------------------------------------------------------------------//


BView*
CreateCreditsView() {
	BView* view = new BView("Credits", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BLayoutBuilder::Group<>(view, B_VERTICAL, 5.0f)
		.SetInsets(12.0f)
		.Add(Heading("project heading", "Project"))
		.Add(new BStringView("original copyright",
			"Copyright 2001–2004 Gergely Rózsahegyi"))
		.Add(new BStringView("current copyright",
			"Copyright 2026 The Globe contributors"))
		.AddStrut(8.0f)
		.Add(Heading("current heading", "Current Contributor"))
		.Add(new BStringView("current contributor", "Sikosis"))
		.AddStrut(8.0f)
		.Add(Heading("original heading", "Original Contributors"))
		.Add(new BStringView("original developer",
			"Gergely Rózsahegyi (RoGer) — development"))
		.Add(new BStringView("original designer",
			"Attila Ökrös (kesigomu) — graphics, tags, and design"))
		.AddGlue();

	return view;
}
//---------------------------------------------------------------------------------------------------------------------------------//


BView*
CreateLicencesView() {
	BView* view = new BView("Licences", B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BLayoutBuilder::Group<>(view, B_VERTICAL, 5.0f)
		.SetInsets(12.0f)
		.Add(Heading("globe licence heading", "Globe"))
		.Add(new BStringView("globe licence",
			"GNU General Public License, version 2 or later"))
		.Add(new LinkView("gpl link", "GNU GPL version 2",
			"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html"))
		.AddStrut(8.0f)
		.Add(Heading("heritage heading", "Heritage"))
		.Add(new BStringView("heritage",
			"A modern continuation of the original BeOS Globe web editor."))
		.Add(new BStringView("preserved source",
			"The original source and artwork remain preserved in Globe-Original."))
		.AddGlue();

	return view;
}
//---------------------------------------------------------------------------------------------------------------------------------//


ArtworkView::ArtworkView(BBitmap* artwork)
	:
	BView("application artwork", B_WILL_DRAW),
	fArtwork(artwork) {
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetExplicitMinSize(BSize(100.0f, 200.0f));
	SetExplicitMaxSize(BSize(100.0f, 200.0f));
}
//---------------------------------------------------------------------------------------------------------------------------------//


ArtworkView::~ArtworkView() {
	delete fArtwork;
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
ArtworkView::Draw(BRect updateRect) {
	(void)updateRect;
	if (fArtwork == nullptr)
		return;

	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	DrawBitmap(fArtwork, fArtwork->Bounds(), Bounds(), B_FILTER_BITMAP_BILINEAR);
}
//---------------------------------------------------------------------------------------------------------------------------------//


LinkView::LinkView(const char* name, const char* label, const char* url)
	:
	BStringView(name, label),
	fUrl(url) {
	SetToolTip(url);
	BFont font;
	GetFont(&font);
	font.SetFace(font.Face() | B_UNDERSCORE_FACE);
	SetFont(&font);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
LinkView::AttachedToWindow() {
	BStringView::AttachedToWindow();
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	rgb_color background = ui_color(B_PANEL_BACKGROUND_COLOR);
	int32 luminance = (299 * background.red + 587 * background.green
		+ 114 * background.blue) / 1000;
	if (luminance < 128)
		SetHighColor(105, 190, 255);
	else
		SetHighColor(0, 76, 168);
}
//---------------------------------------------------------------------------------------------------------------------------------//


void
LinkView::MouseDown(BPoint point) {
	(void)point;
	const char* arguments[] = {fUrl.String()};
	be_roster->Launch("text/html", 1, arguments);
}
//---------------------------------------------------------------------------------------------------------------------------------//


} // namespace


AboutWindow::AboutWindow()
	:
	BWindow(BRect(0, 0, 520, 540), "About Globe", B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_CLOSE_ON_ESCAPE) {
	BStringView* title = new BStringView("title", "Globe");
	BFont titleFont(*be_bold_font);
	titleFont.SetSize(be_plain_font->Size() * 2.0f);
	title->SetFont(&titleFont);

	BString versionText = AppVersion();
	BStringView* version = new BStringView("version", versionText.String());
	BStringView* description = new BStringView("description",
		"A Haiku-native editor for HTML and PHP files.");

	BTabView* tabs = new BTabView("about tabs", B_WIDTH_FROM_LABEL);
	tabs->AddTab(CreateCreditsView());
	tabs->AddTab(CreateLicencesView());

	BButton* closeButton = new BButton("close", "Close",
		new BMessage(B_QUIT_REQUESTED));
	closeButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 10.0f)
		.SetInsets(18.0f)
		.AddGroup(B_HORIZONTAL, 16.0f)
			.Add(new ArtworkView(LoadArtwork()))
			.AddGroup(B_VERTICAL, 4.0f)
				.Add(title)
				.Add(version)
				.AddStrut(4.0f)
				.Add(description)
				.Add(new LinkView("source link", "github.com/sikosis/Globe",
					"https://github.com/sikosis/Globe"))
				.AddGlue()
			.End()
		.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(tabs, 1.0f)
		.AddGroup(B_HORIZONTAL, 0.0f)
			.AddGlue()
			.Add(closeButton)
		.End();

	CenterOnScreen();
}
//---------------------------------------------------------------------------------------------------------------------------------//
