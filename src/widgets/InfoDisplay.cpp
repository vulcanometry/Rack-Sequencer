#include "InfoDisplay.hpp"
#include "Drawing.hpp"
#include "../History.hpp"


NameField::~NameField() {
	GridSequencer* module = getModule();
	if (module && module->nameOf(track) != oldName && APP->history) {
		NameAction* h = new NameAction;
		h->moduleId = module->id;
		h->name = (track < 0) ? "rename song" : "rename track";
		h->track = track;
		h->before = oldName;
		h->after = module->nameOf(track);
		APP->history->push(h);
	}
}


void NameField::onSelectKey(const SelectKeyEvent& e) {
	if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER || e.key == GLFW_KEY_ESCAPE)) {
		if (ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>())
			overlay->requestDelete();
		e.consume(this);
		return;
	}
	ui::TextField::onSelectKey(e);
}


void InfoDisplay::openNameEditor(int track) {
	ui::Menu* menu = createMenu();
	menu->addChild(createMenuLabel(track < 0 ? "Song name" : string::f("Track %d name", track + 1)));
	NameField* field = new NameField;
	field->box.size.x = 260.f;
	field->placeholder = (track < 0) ? "Untitled" : string::f("Track %d", track + 1);
	field->start(module, track);
	menu->addChild(field);
	APP->event->setSelectedWidget(field);
	field->selectAll();
}


void InfoDisplay::onButton(const ButtonEvent& e) {
	if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
		// The track name's area reaches back to the separator, so the two never overlap
		int track = -2;
		if (e.pos.x < nameEnd + 4.f)
			track = -1;
		else if (e.pos.x < trackNameEnd + 8.f)
			track = module->selectedTrack;
		if (track >= -1) {
			openNameEditor(track);
			e.consume(this);
			return;
		}
	}
	Widget::onButton(e);
}


void InfoDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		Widget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x12, 0x14, 0x18), 2.f);
	float cy = box.size.y / 2;

	// Right half: transport (and song slot), playing pattern, bar.beat within the pattern
	bool running = module && module->running;
	std::string transport = "STOP";
	if (module && running)
		transport = module->playingSong()
			? string::f("%s %02d/%02d", module->songOnce() ? "ONCE" : "SONG", module->songPos + 1, module->songLength)
			: "LOOP";
	int playing = module ? module->playingPattern : 0;
	int step = module ? module->step : -1;
	std::string bar = (running && step >= 0) ? string::f("BAR %d.%d", step / 16 + 1, step % 16 / 4 + 1) : "BAR -.-";
	setFont(vg, DISPLAY_FONT);
	nvgFontSize(vg, 13.f);
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	float x = box.size.x - 6.f;
	nvgFillColor(vg, running ? nvgRGB(0xf0, 0xf2, 0xf6) : nvgRGB(0x8a, 0x90, 0x9a));
	x = x - nvgTextBounds(vg, 0, 0, bar.c_str(), NULL, NULL);
	nvgText(vg, box.size.x - 6.f, cy, bar.c_str(), NULL);
	std::string pattern = string::f("P%02d   ", playing + 1);
	// Green while the grid follows the song (then it can differ from SEQ)
	nvgFillColor(vg, (module && module->following()) ? nvgRGB(0x8c, 0xf0, 0x9c) : nvgRGB(0x9c, 0xdc, 0xff));
	nvgText(vg, x, cy, pattern.c_str(), NULL);
	x -= nvgTextBounds(vg, 0, 0, pattern.c_str(), NULL, NULL);
	nvgFillColor(vg, running ? nvgRGB(0x8c, 0xf0, 0x9c) : nvgRGB(0xff, 0x80, 0x70));
	nvgText(vg, x, cy, (transport + "   ").c_str(), NULL);

	// Left half: the song's name, then the selected track's, each shortened to fit. The track's
	// name gets up to 45% of the room, the song's the rest.
	setFont(vg, LABEL_FONT);
	nvgFontSize(vg, 15.f);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	auto fit = [&](std::string s, float room) {
		while (s.size() > 4 && nvgTextBounds(vg, 0, 0, s.c_str(), NULL, NULL) > room)
			s = s.substr(0, s.size() - 4) + "...";
		return s;
	};
	float room = box.size.x / 2 - 12.f;
	int track = module ? module->selectedTrack : 0;
	std::string trackName = fit(module ? module->trackDisplayName(track) : "Track 1", room * 0.45f);
	const char* separator = "  \xc2\xb7  ";  // middle dot
	float trackW = nvgTextBounds(vg, 0, 0, trackName.c_str(), NULL, NULL);
	float separatorW = nvgTextBounds(vg, 0, 0, separator, NULL, NULL);

	bool named = module && !module->songName.empty();
	std::string name = fit(module ? module->displayName() : "Untitled", room - trackW - separatorW);
	nvgFillColor(vg, named ? nvgRGB(0xf6, 0xe7, 0xb0) : nvgRGB(0x8a, 0x90, 0x9a));
	nameEnd = nvgText(vg, 6.f, cy, name.c_str(), NULL);
	nvgFillColor(vg, nvgRGB(0x5a, 0x60, 0x6a));
	trackNameStart = nvgText(vg, nameEnd, cy, separator, NULL);
	// In the track's colour; dimmed while it has no name of its own
	NVGcolor trackColor = TRACK_COLORS[track];
	if (!module || module->trackNames[track].empty())
		trackColor.a = 0.55f;
	nvgFillColor(vg, trackColor);
	trackNameEnd = nvgText(vg, trackNameStart, cy, trackName.c_str(), NULL);
}


void TempoDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		Widget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x0e, 0x10, 0x13), 1.5f);

	bool ext = module && module->inputs[GridSequencer::CLOCK_INPUT].isConnected();
	float bpm = module ? 60.f / (module->stepPeriod * 4.f) : 120.f;
	std::string text = string::f("%.0f", bpm);
	setFont(vg, DISPLAY_FONT);
	nvgFontSize(vg, 14.f);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	// Amber while following an external clock
	nvgFillColor(vg, ext ? nvgRGB(0xff, 0xc8, 0x60) : nvgRGB(0x9c, 0xdc, 0xff));
	nvgText(vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
}
