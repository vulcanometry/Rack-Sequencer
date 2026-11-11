#pragma once
#include "../GridSequencer.hpp"


/** Song name text box, in the right-click menu and the rename popup. Enter or Escape closes it. */
struct SongNameField : ui::TextField {
	// Looked up by id, not held as a pointer: the module can be deleted while the box is open
	// (Delete in the same context menu), and the box is only destroyed afterwards
	int64_t moduleId = -1;
	// Name when the box opened; closing it records the rename as one undo step
	std::string oldName;

	void start(GridSequencer* m) {
		moduleId = m->id;
		oldName = m->songName;
		setText(m->songName);
	}

	GridSequencer* getModule() {
		if (moduleId < 0 || !APP->engine)
			return nullptr;
		return dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId));
	}

	~SongNameField();

	void onChange(const ChangeEvent& e) override {
		if (GridSequencer* module = getModule())
			module->songName = text;
		ui::TextField::onChange(e);
	}

	void onSelectKey(const SelectKeyEvent& e) override;
};


/**
 * Top line of the display. Left half: the song's name (click it to rename). Right half: where
 * playback is: transport (and song slot), the playing pattern, and the bar and beat in it.
 */
struct InfoDisplay : Widget {
	GridSequencer* module = nullptr;
	// Right edge of the name as last drawn: clicks left of it rename the song
	float nameEnd = 0.f;

	void onButton(const ButtonEvent& e) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};


/** Current tempo next to the TEMPO knob (measured from CLK IN when it is patched). */
struct TempoDisplay : Widget {
	GridSequencer* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override;
};
