#pragma once
#include "../GridSequencer.hpp"


/**
 * Text box for the song's name or a track's, in the right-click menu and the rename popups.
 * Enter or Escape closes it.
 */
struct NameField : ui::TextField {
	// Looked up by id, not held as a pointer: the module can be deleted while the box is open
	// (Delete in the same context menu), and the box is only destroyed afterwards
	int64_t moduleId = -1;
	// The track whose name this edits; -1 = the song's
	int track = -1;
	// Name when the box opened; closing it records the rename as one undo step
	std::string oldName;

	void start(GridSequencer* m, int t = -1) {
		moduleId = m->id;
		track = t;
		oldName = m->nameOf(track);
		setText(oldName);
	}

	GridSequencer* getModule() {
		if (moduleId < 0 || !APP->engine)
			return nullptr;
		return dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId));
	}

	~NameField();

	void onChange(const ChangeEvent& e) override {
		if (GridSequencer* module = getModule())
			module->nameOf(track) = text;
		ui::TextField::onChange(e);
	}

	void onSelectKey(const SelectKeyEvent& e) override;
};


/**
 * Top line of the display. Left half: the song's name, then the selected track's (click either to
 * rename it). Right half: where playback is: transport (and song slot), the playing pattern, and
 * the bar and beat in it.
 */
struct InfoDisplay : Widget {
	GridSequencer* module = nullptr;
	// Right edge of the song name as last drawn, and where the track name starts and ends:
	// clicks there rename the song or the track
	float nameEnd = 0.f;
	float trackNameStart = 0.f, trackNameEnd = 0.f;

	/** Opens a popup with a text box for the song's name (track -1) or a track's. */
	void openNameEditor(int track);

	void onButton(const ButtonEvent& e) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};


/** Current tempo next to the TEMPO knob (measured from CLK IN when it is patched). */
struct TempoDisplay : Widget {
	GridSequencer* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override;
};
