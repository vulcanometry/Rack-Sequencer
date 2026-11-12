#include "Controls.hpp"
#include "Drawing.hpp"
#include "../History.hpp"


void TrackButton::onButton(const ButtonEvent& e) {
	GridSequencer* m = dynamic_cast<GridSequencer*>(module);
	if (m && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
		// On a track whose jacks send another track's voice, this acts on that track (as selecting does)
		int track = m->ownerTrack[paramId - GridSequencer::TRACK_PARAM];
		trackSettingsEdit(m, m->pinPattern[track] >= 0 ? "stop looping track" : "loop track", [=]() {
			m->togglePin(track);
		});
	}
	// The press goes on to select the track as usual
	VCVLightBezel<WhiteLight>::onButton(e);
}


int ValueStepper::sideAt(Vec pos) {
	if (pos.x < arrowW())
		return -1;
	if (pos.x > box.size.x - arrowW())
		return 1;
	return 0;
}


void ValueStepper::stepValue(int delta) {
	ParamQuantity* pq = getParamQuantity();
	if (!pq)
		return;
	int minValue = (int) std::round(pq->getMinValue());
	int maxValue = (int) std::round(pq->getMaxValue());
	int count = maxValue - minValue + 1;
	float oldValue = pq->getValue();
	int value = (int) std::round(oldValue) + delta;
	if (wrap)
		value = minValue + ((value - minValue) % count + count) % count;
	else
		value = clamp(value, minValue, maxValue);
	pq->setValue(value);

	history::ParamChange* h = new history::ParamChange;
	h->name = "change " + pq->getLabel();
	h->moduleId = module->id;
	h->paramId = paramId;
	h->oldValue = oldValue;
	h->newValue = pq->getValue();
	APP->history->push(h);
}


void ValueStepper::onButton(const ButtonEvent& e) {
	if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
		int side = sideAt(e.pos);
		if (side != 0)
			stepValue(side);
	}
	// Keeps right-click menu and MIDI-map touch behaviour
	ParamWidget::onButton(e);
}


void ValueStepper::onHoverScroll(const HoverScrollEvent& e) {
	if (e.scrollDelta.y > 0.f)
		stepValue(1);
	else if (e.scrollDelta.y < 0.f)
		stepValue(-1);
	e.consume(this);
}


void ValueStepper::drawLayer(const DrawArgs& args, int layer) {
	if (layer != 1) {
		ParamWidget::drawLayer(args, layer);
		return;
	}
	NVGcontext* vg = args.vg;
	float w = box.size.x, h = box.size.y, aw = arrowW();
	fillRect(vg, 0, 0, w, h, nvgRGB(0x0e, 0x10, 0x13), 1.5f);

	// Arrows, lit while hovered
	auto arrow = [&](float cx, float dir, bool hot) {
		if (hot)
			fillRect(vg, dir < 0 ? 0.f : w - aw, 0, aw, h, nvgRGB(0x2c, 0x31, 0x3b), 1.5f);
		nvgBeginPath(vg);
		nvgMoveTo(vg, cx + dir * 2.5f, h / 2);
		nvgLineTo(vg, cx - dir * 2.5f, h / 2 - 4.f);
		nvgLineTo(vg, cx - dir * 2.5f, h / 2 + 4.f);
		nvgClosePath(vg);
		nvgFillColor(vg, hot ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0x9c, 0xa3, 0xb0));
		nvgFill(vg);
	};
	arrow(aw / 2, -1.f, hoverSide < 0);
	arrow(w - aw / 2, 1.f, hoverSide > 0);

	// Value
	std::string text;
	ParamQuantity* pq = getParamQuantity();
	if (!labels.empty()) {
		int i = pq ? (int) std::round(pq->getValue() - pq->getMinValue()) : 0;
		text = labels[clamp(i, 0, (int) labels.size() - 1)];
	}
	else if (pq) {
		if (format.empty())
			text = pq->getDisplayValueString();
		else
			text = string::f(format.c_str(), (int) std::round(pq->getValue()));
	}
	else if (!format.empty()) {
		text = string::f(format.c_str(), 1);
	}
	setFont(vg, DISPLAY_FONT);
	// Shrink text that doesn't fit between the arrows
	float fontSize = 10.5f;
	nvgFontSize(vg, fontSize);
	float textW = nvgTextBounds(vg, 0, 0, text.c_str(), NULL, NULL);
	float room = w - 2 * aw - 2.f;
	if (textW > room)
		nvgFontSize(vg, std::max(fontSize * room / textW, 7.f));
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, nvgRGB(0x9c, 0xdc, 0xff));
	nvgText(vg, w / 2, h / 2, text.c_str(), NULL);
}


void PanelLabel::draw(const DrawArgs& args) {
	setFont(args.vg, LABEL_FONT);
	nvgFontSize(args.vg, fontSize);
	nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFillColor(args.vg, color);
	nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
}
