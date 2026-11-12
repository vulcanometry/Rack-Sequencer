#pragma once
#include "GridSequencer.hpp"
#include <cstring>


/** Undo / redo of a change to the tracks' setup (voices, MOD mode, range, glide). */
struct TrackSettingsAction : history::ModuleAction {
	GridSequencer::TrackSettings before, after;

	void apply(const GridSequencer::TrackSettings& settings) {
		if (GridSequencer* m = dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId)))
			m->loadSettings(settings);
	}
	void undo() override {
		apply(before);
	}
	void redo() override {
		apply(after);
	}
};

/** Runs `f` (which changes track settings) as one undoable step called `name`. */
template <class F>
void trackSettingsEdit(GridSequencer* m, const std::string& name, F f) {
	TrackSettingsAction* h = new TrackSettingsAction;
	h->moduleId = m->id;
	h->name = name;
	m->saveSettings(h->before);
	f();
	m->saveSettings(h->after);
	if (std::memcmp(&h->before, &h->after, sizeof(h->before)) == 0) {
		delete h;
		return;
	}
	APP->history->push(h);
}

/** Undo / redo of renaming the song (track -1) or a track. */
struct NameAction : history::ModuleAction {
	int track = -1;
	std::string before, after;

	void undo() override {
		if (GridSequencer* m = dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId)))
			m->nameOf(track) = before;
	}
	void redo() override {
		if (GridSequencer* m = dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId)))
			m->nameOf(track) = after;
	}
};


/** Undo / redo of a note, track, pattern or song edit. */
struct GridEditAction : history::ModuleAction {
	GridSequencer::Edit edit;

	void apply(bool after) {
		GridSequencer* m = dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId));
		if (!m)
			return;
		if (edit.isSong)
			m->loadSong(after ? edit.songAfter : edit.songBefore);
		else
			m->loadPattern(edit.pattern, after ? edit.patternAfter : edit.patternBefore);
	}
	void undo() override {
		apply(false);
	}
	void redo() override {
		apply(true);
	}
};
