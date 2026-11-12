#include "ModDisplay.hpp"
#include "Drawing.hpp"
#include "../History.hpp"


int ModDisplay::voiceAt(float x, int lead) {
	int voices = module->voiceCount(lead);
	float inCell = (x - labelW()) / cellW();
	inCell -= std::floor(inCell);
	return clamp((int) (inCell * voices), 0, voices - 1);
}


int ModDisplay::setAt(int step, float value) {
	int target = targetAt(dragPattern, dragLead, dragVoice, step);
	if (target < 0)
		return -1;
	int v = clamp((int) std::round(value), 0, MOD_MAX);
	*valuePtr(dragPattern, dragLead, dragVoice, target) = v;
	// New notes get the last note value set
	if (!perStep(dragLead))
		module->modDefault[dragLead] = v;
	return target;
}


void ModDisplay::onButton(const ButtonEvent& e) {
	if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
		Widget::onButton(e);
		return;
	}
	if (e.action == GLFW_PRESS) {
		dragging = false;
		if (e.pos.x < labelW()) {
			// Badge: switch the edited track's MOD mode
			if (e.pos.y < badgeH() + 2.f) {
				GridSequencer* m = module;
				trackSettingsEdit(m, "change MOD mode", [=]() {
					int& mode = m->modMode[m->selectedTrack];
					mode = (mode == MOD_PER_STEP) ? MOD_PER_NOTE : MOD_PER_STEP;
				});
			}
		}
		else {
			dragPattern = module->viewPattern();
			dragLead = module->selectedTrack;
			dragVoice = perStep(dragLead) ? 0 : voiceAt(e.pos.x, dragLead);
			dragViewStart = module->viewStart;
			module->savePattern(dragPattern, undoBefore);
			dragging = true;
			dragPos = e.pos;
			lastStep = stepAt(e.pos.x, dragViewStart);
			lastValue = valueAt(e.pos.y);
			edited = setAt(lastStep, lastValue);
			editedVoice = dragVoice;
		}
	}
	e.consume(this);
}


void ModDisplay::onDragMove(const DragMoveEvent& e) {
	if (!module || !dragging || e.button != GLFW_MOUSE_BUTTON_LEFT)
		return;
	dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
	int step = stepAt(dragPos.x, dragViewStart);
	float value = valueAt(dragPos.y);
	// Fill in the steps a fast drag skipped, along a straight line
	if (step != lastStep) {
		int dir = step > lastStep ? 1 : -1;
		for (int k = lastStep + dir; k != step; k += dir)
			setAt(k, lastValue + (value - lastValue) * (k - lastStep) / (float) (step - lastStep));
	}
	int target = setAt(step, value);
	if (target >= 0)
		edited = target;
	lastStep = step;
	lastValue = value;
}


void ModDisplay::onDragEnd(const DragEndEvent& e) {
	if (module && dragging)
		module->recordPatternEdit("change MOD value", dragPattern, undoBefore);
	dragging = false;
	edited = -1;
}


void ModDisplay::onHover(const HoverEvent& e) {
	hoverStep = -1;
	if (module && e.pos.x >= labelW()) {
		hoverStep = stepAt(e.pos.x, module->viewStart);
		hoverVoice = voiceAt(e.pos.x, module->selectedTrack);
	}
	e.consume(this);
}


void ModDisplay::onHoverScroll(const HoverScrollEvent& e) {
	if (module) {
		// Wheel nudges the value under the mouse by 1%; Shift+wheel or a horizontal wheel pans
		bool shift = (APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
		float panDelta = (e.scrollDelta.x != 0.f) ? e.scrollDelta.x : (shift ? e.scrollDelta.y : 0.f);
		if (panDelta != 0.f) {
			module->setViewStart(module->viewStart + (panDelta < 0.f ? 4 : -4));
		}
		else if (e.scrollDelta.y != 0.f && !dragging && e.pos.x >= labelW()) {
			dragPattern = module->viewPattern();
			dragLead = module->selectedTrack;
			dragVoice = perStep(dragLead) ? 0 : voiceAt(e.pos.x, dragLead);
			int target = targetAt(dragPattern, dragLead, dragVoice, stepAt(e.pos.x, module->viewStart));
			if (target >= 0) {
				int delta = e.scrollDelta.y > 0.f ? 1 : -1;
				module->patternEdit("change MOD value", dragPattern, [&]() {
					setAt(target, *valuePtr(dragPattern, dragLead, dragVoice, target) + delta);
				});
			}
		}
	}
	e.consume(this);
}


void ModDisplay::drawReadout(NVGcontext* vg, int track, int value, float x, float y, float w) {
	std::string text = string::f("%d%% %.2gV", value, module->modVoltage(track, value));
	nvgFontSize(vg, 11.f);
	bool right = x > box.size.x - mm2px(20.f);
	nvgTextAlign(vg, (right ? NVG_ALIGN_RIGHT : NVG_ALIGN_LEFT) | NVG_ALIGN_BOTTOM);
	float ty = (y - 2.f < mm2px(4.f)) ? y + mm2px(4.f) : y - 2.f;
	nvgFillColor(vg, nvgRGB(0xf0, 0xf2, 0xf6));
	nvgText(vg, right ? x + w : x, ty, text.c_str(), NULL);
}


void ModDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		Widget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	float lw = labelW(), cw = cellW(), h = box.size.y;
	int pattern = module ? module->viewPattern() : 0;
	int track = module ? module->selectedTrack : 0;
	int voices = module ? module->voiceCount(track) : 1;
	int viewStart = module ? module->viewStart : 0;
	int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;
	bool stepMode = module && perStep(track);
	const ModRange& range = MOD_RANGES[module ? clamp(module->modRange[track], 0, (int) MOD_RANGES.size() - 1) : 0];

	fillRect(vg, 0, 0, box.size.x, h, nvgRGB(0x0e, 0x10, 0x13));

	// Columns, shaded by beat
	for (int col = 0; col < VIEW_STEPS; col++) {
		bool beatGroup = ((viewStart + col) / 4) % 2 == 0;
		fillRect(vg, lw + col * cw + 0.5f, 0.5f, cw - 1.f, h - 1.f, beatGroup ? nvgRGB(0x22, 0x25, 0x2c) : nvgRGB(0x1b, 0x1e, 0x24));
	}

	// Left column: mode badge, then guide lines every 25% labelled with the output voltage
	fillRect(vg, 0.5f, 0.5f, lw - 1.f, h - 1.f, nvgRGB(0x20, 0x23, 0x2a));
	setFont(vg, DISPLAY_FONT);
	NVGcolor trackColor = TRACK_COLORS[track];
	fillRect(vg, 1.5f, 1.5f, lw - 3.f, badgeH(), nvgRGB(0x3a, 0x40, 0x4c), 1.5f);
	nvgFontSize(vg, 11.f);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, trackColor);
	nvgText(vg, lw / 2, 1.5f + badgeH() / 2, stepMode ? "STEP" : "NOTE", NULL);

	nvgFontSize(vg, 10.5f);
	for (int i = 0; i <= 4; i++) {
		float y = valueY(MOD_MAX * i / 4.f);
		fillRect(vg, lw, y - 0.5f, box.size.x - lw, 1.f, nvgRGBA(0xff, 0xff, 0xff, i == 0 ? 0x30 : 0x14));
		float volts = range.min + (range.max - range.min) * i / 4.f;
		std::string label = (range.min < 0.f && volts > 0.f) ? string::f("+%gV", volts) : string::f("%gV", volts);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | (i == 0 ? NVG_ALIGN_BOTTOM : NVG_ALIGN_MIDDLE));
		nvgFillColor(vg, nvgRGB(0xc4, 0xc9, 0xd2));
		nvgText(vg, lw / 2, y, label.c_str(), NULL);
	}

	// Steps past the edited track's end are inactive
	int inactiveCol = clamp(trackLen - viewStart, 0, VIEW_STEPS);
	if (inactiveCol < VIEW_STEPS)
		fillRect(vg, lw + inactiveCol * cw, 0, (VIEW_STEPS - inactiveCol) * cw, h, nvgRGBA(0x00, 0x00, 0x00, 0xa0));

	if (!module)
		return;

	// Playhead of the edited track
	int playStep = module->trackStep[track];
	if (module->step >= 0 && playStep >= viewStart && playStep < viewStart + VIEW_STEPS && module->trackPattern(track) == pattern)
		fillRect(vg, lw + (playStep - viewStart) * cw, 0, cw, h, nvgRGBA(0xff, 0xff, 0xff, 0x22));

	NVGcolor c = trackColor;
	if (module->muted[track])
		c.a = 0.5f;
	int shown = dragging ? edited : -1;
	int shownVoice = dragging ? editedVoice : 0;
	if (!dragging && hoverStep >= 0) {
		shownVoice = stepMode ? 0 : hoverVoice;
		shown = targetAt(pattern, track, shownVoice, hoverStep);
	}

	if (stepMode) {
		// A bar for every step of the track
		for (int col = 0; col < VIEW_STEPS; col++) {
			int s = viewStart + col;
			if (s >= trackLen)
				break;
			int value = module->stepMods[pattern][track][s];
			float x = lw + col * cw + 3.f, y = valueY(value);
			fillRect(vg, x, y, cw - 6.f, std::max(bottom() - y, 1.5f), c, 1.f);
		}
		// Notes (of every voice) marked along the bottom, to line the steps up with them
		for (int voice = laneOf(track, 0); voice < laneOf(track, voices); voice++) {
			for (int s = 0; s < trackLen; s++) {
				if (module->notes[pattern][voice][s] < 0)
					continue;
				int a = std::max(s, viewStart) - viewStart;
				int b = std::min(s + (int) module->lengths[pattern][voice][s], std::min(trackLen, viewStart + VIEW_STEPS)) - viewStart;
				if (a < b)
					fillRect(vg, lw + a * cw + 2.f, bottom() - 3.f, (b - a) * cw - 4.f, 3.f, nvgRGBA(0xf0, 0xf2, 0xf6, 0xb0), 1.f);
			}
		}
		if (shown >= viewStart && shown < viewStart + VIEW_STEPS) {
			int value = module->stepMods[pattern][track][shown];
			drawReadout(vg, track, value, lw + (shown - viewStart) * cw + 3.f, valueY(value), cw - 6.f);
		}
		return;
	}

	// Notes of the edited part: a bar at the start (in its voice's lane), a line along the rest
	float laneW = (cw - 6.f) / voices;
	for (int v = 0; v < voices; v++) {
		for (int s = 0; s < NUM_STEPS; s++) {
			int l = laneOf(track, v);
			if (module->notes[pattern][l][s] < 0)
				continue;
			int length = module->lengths[pattern][l][s];
			int a = std::max(s, viewStart) - viewStart;
			int b = std::min(s + length, viewStart + VIEW_STEPS) - viewStart;
			if (a >= b)
				continue;
			NVGcolor nc = c;
			if (s >= trackLen)
				nc.a = 0.25f;
			int value = module->mods[pattern][l][s];
			float y = valueY(value);
			bool startInView = s >= viewStart;
			float barX = lw + a * cw + 3.f + v * laneW;
			float lineX = startInView ? barX + laneW : lw + a * cw;
			float lineEnd = lw + b * cw - 3.f;
			if (lineEnd > lineX) {
				NVGcolor lc = nc;
				lc.a *= 0.6f;
				fillRect(vg, lineX, y - 1.f, lineEnd - lineX, 2.f, lc);
			}
			if (startInView) {
				fillRect(vg, barX, y, laneW - (voices > 1 ? 1.f : 0.f), std::max(bottom() - y, 1.5f), nc, 1.f);
				if (s == shown && v == shownVoice)
					drawReadout(vg, track, value, barX, y, laneW);
			}
		}
	}
}
