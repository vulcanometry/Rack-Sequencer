#pragma once
#include "../GridSequencer.hpp"


struct GridDisplay : Widget {
	GridSequencer* module = nullptr;

	// Mouse editing:
	//   press an empty cell, drag right   -> draw a note of that length
	//   press a note's right end, drag    -> stretch / shorten it (on a poly track, dragging it up
	//                                        or down also moves the whole chord: every note that
	//                                        starts on that step)
	//   press elsewhere on a note, drag   -> move it (step and pitch)
	//   click a note without dragging     -> delete it
	//   shift+click a note                -> tie / untie it
	// On a poly track each note goes on the first of its voices (note lanes) that is free for the
	// note's whole length, so notes can stack up into chords. There a note never trims another
	// one; where every voice is busy, it can't go.
	enum DragMode { DRAG_NONE, DRAG_CREATE, DRAG_RESIZE, DRAG_MOVE };
	DragMode dragMode = DRAG_NONE;
	Vec dragPos;
	bool dragMoved = false;
	int dragPattern = 0;
	int dragLead = 0;    // the track being edited
	int dragTrack = 0;   // the voice lane the note was on when the drag began
	int dragStart = 0;   // start step of the note when the drag began
	int dragRow = 0;     // its row
	int dragLength = 1;  // its length
	bool dragTie = false;  // and whether it is tied
	int dragMod = DEFAULT_MOD;  // and its MOD value
	int dragProb = PROB_MAX;  // and its probability
	int grabOffset = 0;  // move: grabbed step relative to the note start
	int lastCol = -1;
	int lastRow = -1;
	// Where the edited note currently is, for the length readout
	int curTrack = 0;
	int curStart = 0;
	int curRow = 0;
	// Poly track: scale degrees the chord has been moved up or down by the grip drag
	int chordShift = 0;
	// The pattern when the drag began. Every drag move restores it before applying the edit, so
	// passing over other notes only changes them if the note is released there. Also for undo.
	GridSequencer::PatternSnapshot undoBefore;
	// View position when the drag began (follow-playhead doesn't shift the view mid-drag, but
	// this keeps the drag steady regardless)
	int dragViewStart = 0;

	float labelW() {
		return mm2px(9.f);
	}
	float cellW() {
		return (box.size.x - labelW()) / VIEW_STEPS;
	}
	float cellH() {
		return box.size.y / VISIBLE_ROWS;
	}

	/** Step (absolute, across pages) and row under `pos`, if it is on the grid. */
	bool cellAt(Vec pos, int* stepIndex, int* row);

	/** Like cellAt during a drag: may run past the page edge, clamped to the track's length. */
	void dragCellAt(Vec pos, int* stepIndex, int* row);

	bool poly() {
		return module->voiceCount(dragLead) > 1;
	}

	/**
	 * Puts the note being moved at `start` / `row`: on an ordinary track it trims whatever it
	 * lands on; on a poly track it goes on its own voice if that is free there, else on another
	 * free voice. Returns false if no voice is free.
	 */
	bool placeMoved(int start, int row);

	/**
	 * Moves every note of the edited poly track that starts on the dragged note's step `delta`
	 * scale degrees, as far as the grid allows.
	 */
	void shiftChord(int delta);

	void onButton(const ButtonEvent& e) override;

	void onDragMove(const DragMoveEvent& e) override;

	void onDragEnd(const DragEndEvent& e) override;

	void onHoverScroll(const HoverScrollEvent& e) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};
