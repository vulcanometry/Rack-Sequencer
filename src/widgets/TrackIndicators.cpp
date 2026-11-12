#include "TrackIndicators.hpp"
#include "Drawing.hpp"


void GroupBrackets::draw(const DrawArgs& args) {
	if (!module)
		return;
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (module->isVoiceOutput(t))
			continue;
		int last = t;
		while (last + 1 < NUM_TRACKS && module->ownerTrack[last + 1] == t)
			last++;
		if (last == t)
			continue;
		float y0 = mm2px(firstY + t * pitch - 4.f);
		float y1 = mm2px(firstY + last * pitch + 4.f);
		fillRect(args.vg, 0.f, y0, box.size.x, y1 - y0, TRACK_COLORS[t], box.size.x / 2);
	}
}


void VoiceCountLabel::draw(const DrawArgs& args) {
	if (!module)
		return;
	bool grouped = module->isVoiceOutput(track)
		|| (track + 1 < NUM_TRACKS && module->ownerTrack[track + 1] == track);
	if (grouped || module->voiceCount(track) < 2)
		return;
	std::string text = string::f("x%d", module->voiceCount(track));
	setFont(args.vg, DISPLAY_FONT);
	nvgFontSize(args.vg, 10.f);
	nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(args.vg, TRACK_COLORS[track]);
	nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
}


void OutputGlow::ring(NVGcontext* vg, float inner, float outer, NVGcolor c) {
	float cx = box.size.x / 2, cy = box.size.y / 2;
	nvgBeginPath(vg);
	nvgCircle(vg, cx, cy, outer);
	nvgCircle(vg, cx, cy, inner);
	nvgPathWinding(vg, NVG_HOLE);
	nvgFillColor(vg, c);
	nvgFill(vg);
}


void OutputGlow::draw(const DrawArgs& args) {
	// Unlit: a faint ring in the track colour
	NVGcolor c = color();
	c.a = 0.22f;
	ring(args.vg, mm2px(4.15f), mm2px(4.85f), c);
}


void OutputGlow::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1 && module) {
		float b = module->lights[GridSequencer::GATE_LIGHT + track].getBrightness();
		if (b > 0.01f) {
			NVGcolor c = color();
			c.a = b;
			ring(args.vg, mm2px(4.15f), mm2px(4.85f), c);
			// Soft halo outside the ring
			float cx = box.size.x / 2, cy = box.size.y / 2;
			NVGcolor glow = c, clear = c;
			glow.a = 0.35f * b;
			clear.a = 0.f;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(5.3f));
			nvgCircle(args.vg, cx, cy, mm2px(4.85f));
			nvgPathWinding(args.vg, NVG_HOLE);
			nvgFillPaint(args.vg, nvgRadialGradient(args.vg, cx, cy, mm2px(4.85f), mm2px(5.3f), glow, clear));
			nvgFill(args.vg);
		}
	}
	Widget::drawLayer(args, layer);
}
