#pragma once
#include "../GridSequencer.hpp"


/**
 * Step ruler above the grid. Clicking or dragging along it sets where the edited track ends;
 * the wheel nudges it.
 */
struct RulerDisplay : Widget {
	GridSequencer* module = nullptr;
	bool dragging = false;
	Vec dragPos;
	// Pattern and track being changed, and the pattern before the drag (for undo)
	int dragPattern = 0;
	int dragTrack = 0;
	GridSequencer::PatternSnapshot undoBefore;

	float labelW() {
		return mm2px(9.f);
	}
	float cellW() {
		return (box.size.x - labelW()) / VIEW_STEPS;
	}
	int stepAt(float x) {
		int col = clamp((int) std::floor((x - labelW()) / cellW()), 0, VIEW_STEPS - 1);
		return module->viewStart + col;
	}
	void setLengthAt(float x) {
		module->setTrackLength(dragPattern, dragTrack, stepAt(x) + 1);
	}

	void onButton(const ButtonEvent& e) override;

	void onDragMove(const DragMoveEvent& e) override;

	void onDragEnd(const DragEndEvent& e) override {
		if (module && dragging)
			module->recordPatternEdit("change track length", dragPattern, undoBefore);
		dragging = false;
	}

	void onHoverScroll(const HoverScrollEvent& e) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};
