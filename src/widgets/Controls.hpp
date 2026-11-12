#pragma once
#include "../GridSequencer.hpp"


/** Two-colour light for the play mode: yellow = song loop, orange = song once. */
struct SongModeLight : GrayModuleLightWidget {
	SongModeLight() {
		addBaseColor(nvgRGB(0xf2, 0xdc, 0x4a));
		addBaseColor(nvgRGB(0xff, 0x70, 0x20));
	}
};


/** Button that cycles Pattern -> Song (loop) -> Song (once) on each press. */
struct ModeButton : LightButton<VCVButton, MediumSimpleLight<SongModeLight>> {
	ModeButton() {
		momentary = false;
		addFrame(Svg::load(asset::system("res/ComponentLibrary/VCVButton_1.svg")));
	}
};


/**
 * TRACK button: selects the track. Shift+click also pins it to the pattern it is playing, so it
 * loops that while the song moves on (or lets it go again).
 */
struct TrackButton : VCVLightBezel<WhiteLight> {
	void onButton(const ButtonEvent& e) override;
};


/**
 * Value display with < and > arrows for stepping through a whole-number parameter
 * (wrapping around at the ends). Click an arrow or use the mouse wheel to step; right-click
 * for the usual parameter menu. Each step is undoable.
 */
struct ValueStepper : ParamWidget {
	// printf format for the value (e.g. "%02d"); empty = the parameter's own display text
	std::string format;
	// If set, the text for each value from the parameter's minimum up (overrides format)
	std::vector<std::string> labels;
	// Step past the last value back to the first (and the other way), or stop at the ends
	bool wrap = true;
	// -1 / +1 while the mouse is over the left / right arrow, 0 otherwise
	int hoverSide = 0;

	ValueStepper() {
		box.size = mm2px(Vec(27.f, 5.4f));
	}

	float arrowW() {
		return mm2px(3.f);
	}

	int sideAt(Vec pos);

	void stepValue(int delta);

	void onButton(const ButtonEvent& e) override;

	void onDoubleClick(const DoubleClickEvent& e) override {
		// Don't reset to default: a quick second click on an arrow is just another step
		e.consume(this);
	}

	void onHoverScroll(const HoverScrollEvent& e) override;

	void onHover(const HoverEvent& e) override {
		hoverSide = sideAt(e.pos);
		ParamWidget::onHover(e);
	}

	void onLeave(const LeaveEvent& e) override {
		hoverSide = 0;
		ParamWidget::onLeave(e);
	}

	void drawLayer(const DrawArgs& args, int layer) override;
};


/** Static text drawn on the panel (Rack does not render SVG text). */
struct PanelLabel : TransparentWidget {
	std::string text;
	float fontSize = 9.5f;
	NVGcolor color = nvgRGB(0xe6, 0xe9, 0xee);

	void draw(const DrawArgs& args) override;
};
