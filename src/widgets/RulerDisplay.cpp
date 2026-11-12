#include "RulerDisplay.hpp"
#include "Drawing.hpp"


void RulerDisplay::onButton(const ButtonEvent& e) {
	if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
		Widget::onButton(e);
		return;
	}
	if (e.action == GLFW_PRESS) {
		dragging = false;
		if (e.pos.x >= labelW()) {
			dragPattern = module->viewPattern();
			dragTrack = module->selectedTrack;
			module->savePattern(dragPattern, undoBefore);
			setLengthAt(e.pos.x);
			dragging = true;
			dragPos = e.pos;
		}
	}
	e.consume(this);
}


void RulerDisplay::onDragMove(const DragMoveEvent& e) {
	if (!module || !dragging || e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;
	dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
	setLengthAt(dragPos.x);
}


void RulerDisplay::onHoverScroll(const HoverScrollEvent& e) {
	if (module && e.scrollDelta.y != 0.f) {
		int delta = e.scrollDelta.y > 0.f ? 1 : -1;
		int pattern = module->viewPattern();
		int track = module->selectedTrack;
		module->patternEdit("change track length", pattern, [&]() {
			module->setTrackLength(pattern, track, module->trackLength(pattern, track) + delta);
		});
	}
	e.consume(this);
}


void RulerDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		Widget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	float lw = labelW(), cw = cellW(), h = box.size.y;
	int pageStart = module ? module->viewStart : 0;
	int pattern = module ? module->viewPattern() : 0;
	int track = module ? module->selectedTrack : 0;
	int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;
	NVGcolor trackColor = TRACK_COLORS[track];

	fillRect(vg, 0, 0, box.size.x, h, nvgRGB(0x0e, 0x10, 0x13));

	// Corner label
	fillRect(vg, 0.5f, 0.5f, lw - 1.f, h - 1.f, nvgRGB(0x20, 0x23, 0x2a));
	setFont(vg, DISPLAY_FONT);
	nvgFontSize(vg, 9.5f);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, nvgRGB(0xb0, 0xb6, 0xc0));
	nvgText(vg, lw / 2, h / 2, "STEP", NULL);

	// Steps: active ones shaded by beat, step numbers on each beat
	nvgFontSize(vg, 9.5f);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	for (int col = 0; col < VIEW_STEPS; col++) {
		int s = pageStart + col;
		bool active = s < trackLen;
		bool beatGroup = (s / 4) % 2 == 0;
		NVGcolor c = active ? (beatGroup ? nvgRGB(0x2c, 0x31, 0x3b) : nvgRGB(0x24, 0x28, 0x31)) : nvgRGB(0x12, 0x14, 0x17);
		fillRect(vg, lw + col * cw + 0.5f, 0.5f, cw - 1.f, h - 1.f, c);
		if (s % 4 == 0) {
			nvgFillColor(vg, active ? nvgRGB(0xd0, 0xd4, 0xdc) : nvgRGB(0x50, 0x56, 0x60));
			std::string num = string::f("%d", s + 1);
			nvgText(vg, lw + col * cw + 2.f, h / 2, num.c_str(), NULL);
		}
	}

	// Playhead of the edited track
	if (module && module->step >= 0 && module->trackPattern(track) == pattern) {
		int playStep = module->trackStep[track];
		if (playStep >= pageStart && playStep < pageStart + VIEW_STEPS)
			fillRect(vg, lw + (playStep - pageStart) * cw + 0.5f, h - 2.f, cw - 1.f, 2.f, trackColor);
	}

	// End marker: a bar in the track colour after the last step, with a handle
	int endCol = trackLen - pageStart;
	if (endCol >= 1 && endCol <= VIEW_STEPS) {
		float x = lw + endCol * cw;
		fillRect(vg, x - 2.5f, 0, 2.5f, h, trackColor);
		nvgBeginPath(vg);
		nvgMoveTo(vg, x - 2.5f, 0);
		nvgLineTo(vg, x - 7.f, 0);
		nvgLineTo(vg, x - 2.5f, h / 2);
		nvgClosePath(vg);
		nvgFillColor(vg, trackColor);
		nvgFill(vg);
	}
}
