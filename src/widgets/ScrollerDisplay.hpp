#pragma once
#include "../GridSequencer.hpp"


/**
 * Horizontal scroller under the grid: an overview of all NUM_STEPS steps (with the notes drawn
 * at their pitch) and a thumb marking the VIEW_STEPS steps shown in the grid. Drag the thumb to
 * pan, click elsewhere to centre the view there, or use the wheel to pan one step at a time.
 */
struct ScrollerDisplay : Widget {
	GridSequencer* module = nullptr;
	bool dragging = false;
	float dragX = 0.f;       // mouse x during a drag
	float grabOffset = 0.f;  // mouse x minus the thumb's left edge when grabbed

	float labelW() {
		return mm2px(9.f);
	}
	float stepW() {
		return (box.size.x - labelW()) / NUM_STEPS;
	}
	float thumbX() {
		return labelW() + module->viewStart * stepW();
	}
	float thumbW() {
		return VIEW_STEPS * stepW();
	}
	void panThumbTo(float left) {
		module->setViewStart((int) std::round((left - labelW()) / stepW()));
	}

	void onButton(const ButtonEvent& e) override;

	void onDragMove(const DragMoveEvent& e) override;

	void onDragEnd(const DragEndEvent& e) override {
		dragging = false;
	}

	void onHoverScroll(const HoverScrollEvent& e) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};
