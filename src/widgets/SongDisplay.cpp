#include "SongDisplay.hpp"
#include "Drawing.hpp"


int SongDisplay::slotAt(Vec pos) {
	if (pos.x < labelW())
		return -1;
	int col = clamp((int) ((pos.x - labelW()) / slotW()), 0, SLOTS - 1);
	return page() * SLOTS + col;
}


void SongDisplay::onButton(const ButtonEvent& e) {
	if (!module || e.action != GLFW_PRESS) {
		Widget::onButton(e);
		return;
	}
	int i = slotAt(e.pos);
	if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
		if (i < 0) {
			// Label area flips to the next page of the song
			int next = (page() + 1) * SLOTS;
			module->songCursor = next < module->songLength ? next : 0;
		}
		else if (i < module->songLength) {
			// Select the slot and show its pattern in the grid; dragging then moves it
			module->songCursor = i;
			module->params[GridSequencer::PATTERN_PARAM].setValue(module->song[i] + 1);
			module->saveSong(undoBefore);
			dragging = true;
			dragMoved = false;
			dragX = e.pos.x;
			lastEdgeMove = 0.0;
		}
		e.consume(this);
	}
	else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && i >= module->songLength) {
		// Empty slots: add the edit pattern at the end of the song
		GridSequencer* m = module;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuItem(string::f("Add edit pattern (P%02d) to the end of the song", m->editPattern() + 1), "", [=]() {
			m->songEdit("add song slot", [=]() {
				m->songCursor = std::max(m->songLength - 1, 0);
				m->songInsert(m->editPattern(), true);
			});
		}, m->songLength >= MAX_SONG_LENGTH));
		e.consume(this);
	}
	else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && i >= 0 && i < module->songLength) {
		module->songCursor = i;
		GridSequencer* m = module;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(string::f("Song slot %d (pattern %02d)", i + 1, m->song[i] + 1)));
		menu->addChild(createMenuItem(string::f("Set to edit pattern (P%02d)", m->editPattern() + 1), "", [=]() {
			m->songEdit("set song slot", [=]() {
				m->song[i] = m->editPattern();
			});
		}));
		menu->addChild(createMenuItem("Open this pattern in editor", "", [=]() {
			m->params[GridSequencer::PATTERN_PARAM].setValue(m->song[i] + 1);
		}));
		menu->addChild(createMenuItem("Insert edit pattern before (INS)", "", [=]() {
			m->songEdit("insert song slot", [=]() {
				m->songCursor = i;
				m->songInsert(m->editPattern(), false);
			});
		}, m->songLength >= MAX_SONG_LENGTH));
		menu->addChild(createMenuItem("Add edit pattern after (ADD)", "", [=]() {
			m->songEdit("add song slot", [=]() {
				m->songCursor = i;
				m->songInsert(m->editPattern(), true);
			});
		}, m->songLength >= MAX_SONG_LENGTH));
		menu->addChild(createMenuItem("Duplicate slot", "", [=]() {
			m->songEdit("duplicate song slot", [=]() {
				m->songCursor = i;
				m->songInsert(m->song[i]);
			});
		}, m->songLength >= MAX_SONG_LENGTH));
		menu->addChild(createMenuItem("Delete slot", "", [=]() {
			m->songEdit("delete song slot", [=]() {
				m->songCursor = i;
				m->songDelete();
			});
		}));
		e.consume(this);
	}
	else {
		Widget::onButton(e);
	}
}


void SongDisplay::step() {
	Widget::step();
	if (!module || !dragging)
		return;
	int from = module->songCursor;
	int to = from;
	if (dragX < labelW() || dragX >= box.size.x) {
		// Past an end of the strip: step along a slot at a time
		double now = system::getTime();
		if (now - lastEdgeMove >= 0.25) {
			lastEdgeMove = now;
			to = from + (dragX < labelW() ? -1 : 1);
		}
	}
	else {
		int col = (int) ((dragX - labelW()) / slotW());
		to = page() * SLOTS + clamp(col, 0, SLOTS - 1);
	}
	to = clamp(to, 0, module->songLength - 1);
	if (to != from) {
		module->songMove(from, to);
		dragMoved = true;
	}
}


void SongDisplay::onHoverScroll(const HoverScrollEvent& e) {
	if (!module) {
		e.consume(this);
		return;
	}
	int delta = (e.scrollDelta.y > 0.f) ? 1 : (e.scrollDelta.y < 0.f ? -1 : 0);
	int i = slotAt(e.pos);
	if (i < 0)
		module->songCursor = clamp(module->songCursor + delta, 0, std::max(module->songLength - 1, 0));
	else if (i < module->songLength) {
		GridSequencer* m = module;
		m->songEdit("change song slot", [=]() {
			m->song[i] = clamp(m->song[i] + delta, 0, NUM_PATTERNS - 1);
		});
	}
	e.consume(this);
}


void SongDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		Widget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	float lw = labelW(), sw = slotW();
	int length = module ? module->songLength : 0;
	int pages = std::max((length + SLOTS - 1) / SLOTS, 1);

	fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x0e, 0x10, 0x13));
	setFont(vg, DISPLAY_FONT);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

	fillRect(vg, 0.5f, 0.5f, lw - 1.f, box.size.y - 1.f, nvgRGB(0x20, 0x23, 0x2a));
	nvgFillColor(vg, nvgRGB(0xc8, 0xcc, 0xd4));
	nvgFontSize(vg, 10.5f);
	nvgText(vg, lw / 2, box.size.y * 0.32f, "SONG", NULL);
	std::string pageStr = string::f("%d/%d", page() + 1, pages);
	nvgFillColor(vg, nvgRGB(0xb0, 0xb6, 0xc0));
	nvgText(vg, lw / 2, box.size.y * 0.72f, pageStr.c_str(), NULL);

	nvgFontSize(vg, 12.5f);
	for (int col = 0; col < SLOTS; col++) {
		int i = page() * SLOTS + col;
		float x = lw + col * sw;
		if (i >= length) {
			fillRect(vg, x + 1.f, 1.f, sw - 2.f, box.size.y - 2.f, nvgRGB(0x15, 0x17, 0x1b), 1.5f);
			continue;
		}
		bool playing = module && module->playingSong() && module->step >= 0 && module->songPos == i;
		fillRect(vg, x + 1.f, 1.f, sw - 2.f, box.size.y - 2.f, playing ? nvgRGB(0x3c, 0xa0, 0x58) : nvgRGB(0x26, 0x2a, 0x33), 1.5f);
		if (module && module->songCursor == i) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + 1.5f, 1.5f, sw - 3.f, box.size.y - 3.f, 1.5f);
			// The slot being dragged gets a heavier white outline
			bool moving = dragging && dragMoved;
			nvgStrokeColor(vg, moving ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xf2, 0xdc, 0x4a));
			nvgStrokeWidth(vg, moving ? 2.f : 1.2f);
			nvgStroke(vg);
		}
		int p = module ? module->song[i] : 0;
		nvgFillColor(vg, nvgRGB(0xf0, 0xf2, 0xf6));
		std::string s = string::f("%02d", p + 1);
		nvgText(vg, x + sw / 2, box.size.y / 2, s.c_str(), NULL);
	}
}
