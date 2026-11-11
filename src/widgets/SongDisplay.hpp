#pragma once
#include "../GridSequencer.hpp"


struct SongDisplay : Widget {
	GridSequencer* module = nullptr;
	static const int SLOTS = 16;
	// Dragging a slot moves it through the song; held past either end of the strip, it keeps
	// moving a slot at a time onto the next / previous page
	bool dragging = false;
	bool dragMoved = false;
	float dragX = 0.f;
	double lastEdgeMove = 0.0;
	GridSequencer::SongSnapshot undoBefore;

	float labelW() {
		return mm2px(9.f);
	}
	float slotW() {
		return (box.size.x - labelW()) / SLOTS;
	}
	int page() {
		return module ? module->songCursor / SLOTS : 0;
	}
	/** Song index under the mouse, or -1 for the label area. */
	int slotAt(Vec pos);

	void onButton(const ButtonEvent& e) override;

	void onDragMove(const DragMoveEvent& e) override {
		if (dragging && e.button == GLFW_MOUSE_BUTTON_LEFT)
			dragX += e.mouseDelta.x / getAbsoluteZoom();
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (module && dragging && dragMoved)
			module->recordSongEdit("move song slot", undoBefore);
		dragging = false;
	}

	void step() override;

	void onHoverScroll(const HoverScrollEvent& e) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};
