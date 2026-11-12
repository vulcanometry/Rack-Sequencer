#include "ScrollerDisplay.hpp"
#include "Drawing.hpp"


void ScrollerDisplay::onButton(const ButtonEvent& e) {
	if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
		Widget::onButton(e);
		return;
	}
	if (e.action == GLFW_PRESS) {
		dragging = false;
		if (e.pos.x >= labelW()) {
			// Outside the thumb: centre the view on the clicked step first
			if (e.pos.x < thumbX() || e.pos.x > thumbX() + thumbW())
				panThumbTo(e.pos.x - thumbW() / 2);
			grabOffset = e.pos.x - thumbX();
			dragX = e.pos.x;
			dragging = true;
		}
	}
	e.consume(this);
}


void ScrollerDisplay::onDragMove(const DragMoveEvent& e) {
	if (!module || !dragging || e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;
	dragX += e.mouseDelta.x / getAbsoluteZoom();
	panThumbTo(dragX - grabOffset);
}


void ScrollerDisplay::onHoverScroll(const HoverScrollEvent& e) {
	if (module) {
		float delta = (e.scrollDelta.x != 0.f) ? e.scrollDelta.x : e.scrollDelta.y;
		if (delta != 0.f)
			module->setViewStart(module->viewStart + (delta < 0.f ? 1 : -1));
	}
	e.consume(this);
}


void ScrollerDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		Widget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	float lw = labelW(), sw = stepW(), h = box.size.y;
	int viewStart = module ? module->viewStart : 0;
	int pattern = module ? module->viewPattern() : 0;
	int track = module ? module->selectedTrack : 0;
	int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;

	fillRect(vg, 0, 0, box.size.x, h, nvgRGB(0x0e, 0x10, 0x13));

	// Corner: the range of steps in view
	fillRect(vg, 0.5f, 0.5f, lw - 1.f, h - 1.f, nvgRGB(0x20, 0x23, 0x2a));
	setFont(vg, DISPLAY_FONT);
	nvgFontSize(vg, 9.f);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, nvgRGB(0xd0, 0xd4, 0xdc));
	std::string range = string::f("%d-%d", viewStart + 1, viewStart + VIEW_STEPS);
	nvgText(vg, lw / 2, h / 2, range.c_str(), NULL);

	// The edited track's active length, with a divider every 16 steps
	fillRect(vg, lw, 0.5f, trackLen * sw, h - 1.f, nvgRGB(0x24, 0x28, 0x31));
	for (int s = VIEW_STEPS; s < NUM_STEPS; s += VIEW_STEPS)
		fillRect(vg, lw + s * sw - 0.5f, 0, 1.f, h, nvgRGB(0x3a, 0x40, 0x4c));

	if (module) {
		// Notes, drawn at their pitch so the overview shows each line's shape
		int rows = module->numRows();
		auto noteY = [&](int row) {
			return 1.5f + (1.f - (row + 0.5f) / rows) * (h - 3.f);
		};
		for (int pass = 0; pass < 2; pass++) {
			for (int t = 0; t < NUM_TRACKS; t++) {
				// Other tracks first, faintly; then the edited track (all its voices) on top
				bool edited = t == track;
				if ((pass == 0) == edited || module->isVoiceOutput(t))
					continue;
				NVGcolor c = TRACK_COLORS[t];
				c.a = edited ? 1.f : 0.3f;
				int length = module->trackLength(pattern, t);
				for (int v = 0; v < module->voiceCount(t); v++) {
					int l = laneOf(t, v);
					for (int s = 0; s < length; s++) {
						int row = module->notes[pattern][l][s];
						if (row < 0)
							continue;
						float w = std::max(std::min((int) module->lengths[pattern][l][s], length - s) * sw - 1.f, 1.5f);
						fillRect(vg, lw + s * sw, noteY(row) - 1.f, w, 2.f, c);
					}
				}
			}
		}

		// Playhead of the edited track
		int playStep = module->trackStep[track];
		if (module->step >= 0 && playStep >= 0 && module->playingPattern == pattern)
			fillRect(vg, lw + playStep * sw, 0, std::max(sw, 1.5f), h, nvgRGBA(0xff, 0xff, 0xff, 0x80));
	}

	// Thumb: the steps shown in the grid
	float tx = lw + viewStart * sw, tw = VIEW_STEPS * sw;
	fillRect(vg, tx, 0.5f, tw, h - 1.f, nvgRGBA(0xff, 0xff, 0xff, dragging ? 0x30 : 0x1c), 2.f);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, tx + 0.5f, 1.f, tw - 1.f, h - 2.f, 2.f);
	nvgStrokeColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xb0));
	nvgStrokeWidth(vg, 1.f);
	nvgStroke(vg);
}
