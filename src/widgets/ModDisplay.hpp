#pragma once
#include "../GridSequencer.hpp"


/**
 * MOD view, shown in place of the note grid while the MOD button is lit. The badge at the top
 * left switches the edited track between its two MOD modes:
 *   NOTE: each note is a bar as high as its MOD value, with a line along its length. On a poly
 *         track each step's column is split into a lane per voice.
 *   STEP: every step of the track has its own bar; the notes are marked along the bottom.
 * Press to set a value, or drag across to draw values; the wheel nudges the one under the mouse.
 *
 * With `prob` set it is the PROB view instead (while the PROB button is lit): the same, for each
 * note's probability, which is always per note.
 */
struct ModDisplay : Widget {
	GridSequencer* module = nullptr;
	bool prob = false;
	bool dragging = false;
	Vec dragPos;
	int dragPattern = 0;
	int dragLead = 0;    // the track being edited
	int dragVoice = 0;   // NOTE mode: the voice lane (0..voices-1) the drag started in
	int dragViewStart = 0;
	int lastStep = 0;
	float lastValue = 0.f;
	// Value being shown: the one last edited, or the one under the mouse (step or note start,
	// and the voice it is on)
	int edited = -1;
	int editedVoice = 0;
	int hoverStep = -1;
	int hoverVoice = 0;
	GridSequencer::PatternSnapshot undoBefore;

	int maxValue() {
		return prob ? PROB_MAX : MOD_MAX;
	}
	std::string editName() {
		return prob ? "change note probability" : "change MOD value";
	}
	float labelW() {
		return mm2px(9.f);
	}
	float cellW() {
		return (box.size.x - labelW()) / VIEW_STEPS;
	}
	float badgeH() {
		return mm2px(5.f);
	}
	float top() {
		return mm2px(8.f);
	}
	float bottom() {
		return box.size.y - mm2px(1.f);
	}
	float valueY(float value) {
		return bottom() - (bottom() - top()) * value / maxValue();
	}
	float valueAt(float y) {
		return clamp((bottom() - y) / (bottom() - top()) * maxValue(), 0.f, (float) maxValue());
	}
	int stepAt(float x, int viewStart) {
		return viewStart + (int) std::floor((x - labelW()) / cellW());
	}
	/** Voice (0..voices-1) whose lane is under `x` in its step's column. */
	int voiceAt(float x, int lead);
	bool perStep(int lead) {
		return !prob && module->modMode[lead] == MOD_PER_STEP;
	}

	/** The step (STEP mode) or note start on `voice` (NOTE mode) that `step` edits, or -1. */
	int targetAt(int pattern, int lead, int voice, int step) {
		if (step < 0 || step >= module->trackLength(pattern, lead))
			return -1;
		return perStep(lead) ? step : module->noteStartCovering(pattern, laneOf(lead, voice), step);
	}
	uint8_t* valuePtr(int pattern, int lead, int voice, int target) {
		if (prob)
			return &module->probs[pattern][laneOf(lead, voice)][target];
		return perStep(lead) ? &module->stepMods[pattern][lead][target] : &module->mods[pattern][laneOf(lead, voice)][target];
	}

	/** Sets the value edited at `step` on the drag's voice. Returns the step / note start, or -1. */
	int setAt(int step, float value);

	void onButton(const ButtonEvent& e) override;

	void onDragMove(const DragMoveEvent& e) override;

	void onDragEnd(const DragEndEvent& e) override;

	void onHover(const HoverEvent& e) override;

	void onLeave(const LeaveEvent& e) override {
		hoverStep = -1;
	}

	void onHoverScroll(const HoverScrollEvent& e) override;

	void drawReadout(NVGcontext* vg, int track, int value, float x, float y, float w);

	void drawLayer(const DrawArgs& args, int layer) override;
};
