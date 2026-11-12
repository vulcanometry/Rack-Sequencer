#pragma once
#include "plugin.hpp"
#include "Common.hpp"
#include "MidiFile.hpp"
#include <algorithm>
#include <cstring>
#include <mutex>


struct Clipboard {
	bool hasPattern = false;
	int8_t pattern[NUM_LANES][NUM_STEPS];
	uint8_t patternLengths[NUM_LANES][NUM_STEPS];
	uint8_t patternTies[NUM_LANES][NUM_STEPS];
	uint8_t patternMods[NUM_LANES][NUM_STEPS];
	uint8_t patternProbs[NUM_LANES][NUM_STEPS];
	uint8_t patternStepMods[NUM_TRACKS][NUM_STEPS];
	uint8_t patternTrackLengths[NUM_TRACKS];
	bool hasTrack = false;
	int trackVoices = 1;
	int8_t track[MAX_VOICES][NUM_STEPS];
	uint8_t trackLengths[MAX_VOICES][NUM_STEPS];
	uint8_t trackTies[MAX_VOICES][NUM_STEPS];
	uint8_t trackMods[MAX_VOICES][NUM_STEPS];
	uint8_t trackProbs[MAX_VOICES][NUM_STEPS];
	uint8_t trackStepMods[NUM_STEPS];
	uint8_t trackLength = DEFAULT_TRACK_LENGTH;
};

extern Clipboard clipboard;


struct GridSequencer : Module {
	enum ParamId {
		TEMPO_PARAM,
		// Former gate length knob; the slot is kept so saved patches keep their parameter IDs
		GATE_PARAM,
		ROOT_PARAM,
		SCALE_PARAM,
		PATTERN_PARAM,
		MODE_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		SONG_INSERT_PARAM,
		SONG_DELETE_PARAM,
		SCROLL_UP_PARAM,
		SCROLL_DOWN_PARAM,
		ENUMS(TRACK_PARAM, NUM_TRACKS),
		ENUMS(MUTE_PARAM, NUM_TRACKS),
		SONG_DUPLICATE_PARAM,
		SONG_SET_PARAM,
		SONG_EDIT_PARAM,
		SONG_CLEAR_PARAM,
		PATTERN_COPY_PARAM,
		PATTERN_PASTE_PARAM,
		PATTERN_CLEAR_PARAM,
		TRACK_COPY_PARAM,
		TRACK_PASTE_PARAM,
		TRACK_CLEAR_PARAM,
		SONG_ADD_PARAM,
		MOD_VIEW_PARAM,
		GATE_LENGTH_PARAM,
		PROB_VIEW_PARAM,
		SWING_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		RUN_INPUT,
		PATTERN_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(CV_OUTPUT, NUM_TRACKS),
		ENUMS(GATE_OUTPUT, NUM_TRACKS),
		CLOCK_OUTPUT,
		EOP_OUTPUT, // end of pattern
		RUN_OUTPUT,
		RESET_OUTPUT,
		ENUMS(MOD_OUTPUT, NUM_TRACKS),
		OUTPUTS_LEN
	};
	enum LightId {
		RUN_LIGHT,
		ENUMS(SONG_LIGHT, 2), // yellow = song loop, orange = song once
		ENUMS(TRACK_LIGHT, NUM_TRACKS),
		ENUMS(MUTE_LIGHT, NUM_TRACKS),
		ENUMS(GATE_LIGHT, NUM_TRACKS),
		MOD_VIEW_LIGHT,
		PROB_VIEW_LIGHT,
		LIGHTS_LEN
	};

	// Note data, per lane (see laneOf): scale-degree row per step, -1 = no note.
	int8_t notes[NUM_PATTERNS][NUM_LANES][NUM_STEPS];
	// Length in steps of the note starting at each step. Notes in a lane never overlap:
	// the steps a note covers after its start are always empty.
	uint8_t lengths[NUM_PATTERNS][NUM_LANES][NUM_STEPS];
	// 1 = the note starting at this step is tied: its gate stays high to its very end, so a
	// note that follows straight on doesn't retrigger.
	uint8_t ties[NUM_PATTERNS][NUM_LANES][NUM_STEPS];
	// MOD value (0..MOD_MAX) of the note starting at each step, sent from the track's MOD output
	uint8_t mods[NUM_PATTERNS][NUM_LANES][NUM_STEPS];
	// Chance (0..PROB_MAX %) that the note starting at each step plays when its step comes round
	uint8_t probs[NUM_PATTERNS][NUM_LANES][NUM_STEPS];
	// Per track: MOD output range (index into MOD_RANGES), and the MOD value new notes get
	// (the last one set on that track)
	int modRange[NUM_TRACKS] = {};
	int modDefault[NUM_TRACKS];
	// Per-step MOD values, used by tracks in MOD_PER_STEP mode (kept separately from the note
	// values, so switching modes loses nothing)
	uint8_t stepMods[NUM_PATTERNS][NUM_TRACKS][NUM_STEPS];
	int modMode[NUM_TRACKS] = {};
	// Per-step mode: glide from each step's value to the next (true), or jump at each step
	bool modSmooth[NUM_TRACKS];
	// Per-step mode: the step each track glides towards (pattern and step; step -1 = none, as at
	// the end of a play-once song), and how far through its current step each track is (0..1)
	int modNextPattern[NUM_TRACKS] = {};
	int modNextStep[NUM_TRACKS] = {};
	float modPhase[NUM_TRACKS] = {};
	// How fast each track steps (index into CLOCK_RATES; DEFAULT_CLOCK_RATE = with the clock)
	int clockRate[NUM_TRACKS];
	// Loop length of each track (1..NUM_STEPS). Notes past it are kept but not played.
	uint8_t trackLengths[NUM_PATTERNS][NUM_TRACKS];
	// Notes each track can play at once: 1 = an ordinary (mono) track, more = a poly track.
	// Lanes past a track's voice count keep their notes but don't play or show.
	int voices[NUM_TRACKS];
	// Poly tracks that also send each voice to its own mono jacks: voice 2 on the next track's
	// jacks, voice 3 on the one after, and so on
	bool spreadVoices[NUM_TRACKS] = {};
	// What each track's jacks send (worked out by routeOutputs): normally their own track
	// (ownerTrack[t] == t), or else voice ownerVoice[t] of an earlier poly track that spreads its
	// voices. A track whose jacks are taken like that doesn't play its own notes.
	int ownerTrack[NUM_TRACKS];
	int ownerVoice[NUM_TRACKS];
	// Pattern each track is pinned to (-1 = follows the song). A pinned track loops that
	// pattern's notes while the song moves on around it.
	int pinPattern[NUM_TRACKS];
	int8_t song[MAX_SONG_LENGTH];
	// Slots in the song; 0 = empty (a new module's song is empty until patterns are added)
	int songLength = 0;
	// Shown in the info bar and used as the file name when saving the song; "" = not named yet
	// (shown as "Untitled")
	std::string songName;
	// Each track's name, shown in the info bar while it is selected; "" = not named (shown as "Track N")
	std::string trackNames[NUM_TRACKS];
	int songCursor = 0;
	// A new module starts stopped
	bool running = false;
	bool muted[NUM_TRACKS] = {};
	int selectedTrack = 0;
	int scrollRow = 0;
	// First step shown in the grid (it shows VIEW_STEPS from there), and whether it follows the playhead
	int viewStart = 0;
	bool followPlayhead = true;

	// Playback state
	// Step within the pattern (0 .. patternLength-1); each track plays step % its own length
	int step = -1;
	int trackStep[NUM_TRACKS];
	// Pattern each track is looping right now (-1 = none). Follows pinPattern, except that a
	// track let go of carries on looping until the next pattern starts, then rejoins the song.
	int loopPattern[NUM_TRACKS];
	// Step a looping track is on within its loop: it counts on through pattern changes, and only
	// starts again from step 1 when playback is reset
	int loopStep[NUM_TRACKS];
	int songPos = 0;
	int playingPattern = 0;
	double phase = 1.0;
	// Time between clock steps (from TEMPO, or measured at CLK IN); how long the current step
	// lasts (swing makes it longer or shorter); time since it started
	double stepPeriod = 0.125;
	double stepDuration = 0.125;
	double stepTimer = 0.0;
	// Steps played since playback was reset (looping tracks count clock divisions on this)
	long clockCount = -1;
	// A swung step waiting to start: the time left until it does (-1 = none waiting)
	double swingWait = -1.0;
	// Per track, for its clock rate: time since its last step and how long that step lasts, and
	// which of its steps within the current clock step (sped-up tracks) comes next
	double trackTimer[NUM_TRACKS] = {};
	double trackDuration[NUM_TRACKS] = {};
	int nextSubStep[NUM_TRACKS] = {};
	// State before the last step, so a PAT IN trigger arriving just after it can redo it
	struct RedoState {
		long clockCount;
		int loopStep[NUM_TRACKS];
		int nextSubStep[NUM_TRACKS];
		int holdLeft[NUM_LANES];
	};
	RedoState redo;
	// PROB view was lit last time round (the MOD and PROB views turn each other off)
	bool prevProbView = false;
	double extClockTimer = 0.0;
	bool prevRunning = false;
	// PAT IN received: move to the next pattern on the next step
	bool patternJumpPending = false;
	// Run output: false = trigger on every start/stop, true = gate high while running
	bool runOutputGate = false;
	// Run input: false = toggle on each trigger, true = gate (high = running)
	bool runInputGate = false;
	// Per lane (voice): output values, and the note being held
	float cv[NUM_LANES] = {};
	bool gates[NUM_LANES] = {};
	// Steps left (including the current one) of the note each lane is holding, and its row
	int holdLeft[NUM_LANES] = {};
	int holdRow[NUM_LANES] = {};
	bool holdTie[NUM_LANES] = {};
	int holdMod[NUM_LANES] = {};
	float modOut[NUM_LANES] = {};
	// Per track: any of its voices' gates high (for the jack rings)
	bool trackGate[NUM_TRACKS] = {};

	// ---- Undo ----
	// Edits are recorded as before/after snapshots of the pattern or song they change. They can
	// happen on the engine thread (panel buttons) or the UI thread, so they are queued here and
	// the module widget moves them into Rack's undo history.

	struct PatternSnapshot {
		int8_t notes[NUM_LANES][NUM_STEPS];
		uint8_t lengths[NUM_LANES][NUM_STEPS];
		uint8_t ties[NUM_LANES][NUM_STEPS];
		uint8_t mods[NUM_LANES][NUM_STEPS];
		uint8_t probs[NUM_LANES][NUM_STEPS];
		uint8_t stepMods[NUM_TRACKS][NUM_STEPS];
		uint8_t trackLengths[NUM_TRACKS];
	};
	struct SongSnapshot {
		int8_t song[MAX_SONG_LENGTH];
		int length;
		int cursor;
	};
	struct Edit {
		std::string name;
		bool isSong = false;
		int pattern = 0;
		PatternSnapshot patternBefore, patternAfter;
		SongSnapshot songBefore, songAfter;
	};
	std::mutex editsMutex;
	std::vector<Edit> pendingEdits;

	dsp::SchmittTrigger clockTrigger, resetTrigger, runTrigger, patternTrigger;
	dsp::BooleanTrigger runButton, resetButton, insertButton, addButton, deleteButton, scrollUpButton, scrollDownButton;
	dsp::BooleanTrigger trackButtons[NUM_TRACKS], muteButtons[NUM_TRACKS];
	dsp::BooleanTrigger actionButtons[TRACK_CLEAR_PARAM - SONG_DUPLICATE_PARAM + 1];
	dsp::PulseGenerator runPulse, resetPulse, eopPulse;
	dsp::ClockDivider lightDivider;

	GridSequencer();

	// ---- Helpers ----

	int rootIndex() {
		return clamp((int) std::round(params[ROOT_PARAM].getValue()), 0, 11);
	}
	int scaleIndex() {
		return clamp((int) std::round(params[SCALE_PARAM].getValue()), 0, (int) SCALES.size() - 1);
	}
	int scaleSize() {
		return SCALES[scaleIndex()].notes.size();
	}
	int numRows() {
		return NUM_OCTAVES * scaleSize();
	}
	int maxScroll() {
		return std::max(0, numRows() - VISIBLE_ROWS);
	}
	int defaultScroll() {
		// Root of octave 4 (0V) a few rows above the bottom of the view
		return clamp(4 * scaleSize() - 5, 0, maxScroll());
	}
	int editPattern() {
		return clamp((int) std::round(params[PATTERN_PARAM].getValue()) - 1, 0, NUM_PATTERNS - 1);
	}
	bool songMode() {
		return params[MODE_PARAM].getValue() > 0.5f;
	}
	std::string displayName() {
		return songName.empty() ? "Untitled" : songName;
	}
	std::string trackDisplayName(int track) {
		return trackNames[track].empty() ? string::f("Track %d", track + 1) : trackNames[track];
	}
	/** The song's name (track -1) or a track's, for the name fields and their undo. */
	std::string& nameOf(int track) {
		return track < 0 ? songName : trackNames[track];
	}
	/** Song mode with a song to play; with an empty song, song mode plays the SEQ pattern. */
	bool playingSong() {
		return songMode() && songLength > 0;
	}
	bool songOnce() {
		return params[MODE_PARAM].getValue() > 1.5f;
	}
	/** Notes `track` can play at once (1 = mono). */
	int voiceCount(int track) {
		return clamp(voices[track], 1, MAX_VOICES);
	}
	void setVoices(int track, int count) {
		voices[track] = clamp(count, 1, MAX_VOICES);
		routeOutputs();
	}
	/** Per-track setup that changes what tracks play or send: undone as a whole. */
	struct TrackSettings {
		int voices[NUM_TRACKS];
		bool spreadVoices[NUM_TRACKS];
		int modMode[NUM_TRACKS];
		int modRange[NUM_TRACKS];
		int pinPattern[NUM_TRACKS];
		bool modSmooth[NUM_TRACKS];
		int clockRate[NUM_TRACKS];
	};
	void saveSettings(TrackSettings& t);
	void loadSettings(const TrackSettings& t);

	void setSpreadVoices(int track, bool spread) {
		spreadVoices[track] = spread;
		routeOutputs();
	}
	/** True if `track`'s jacks send another track's voice (so its own notes don't play). */
	bool isVoiceOutput(int track) {
		return ownerTrack[track] != track;
	}
	/**
	 * Works out ownerTrack / ownerVoice: each spreading poly track takes the jacks of the tracks
	 * after it for its voices 2, 3, ... A track whose jacks are taken can't spread its own; voices
	 * that would go past the last track stay on the poly cable only.
	 */
	void routeOutputs();

	/** True while `track` loops a pinned pattern instead of following the song. */
	bool looping(int track) {
		return loopPattern[track] >= 0;
	}
	/** Pattern whose notes `track` is playing. */
	int trackPattern(int track) {
		return looping(track) ? loopPattern[track] : playingPattern;
	}
	/**
	 * Pins `track` to the pattern it is playing (the one the grid shows for the song), so it
	 * loops that while the song moves on; or lets it go again, to rejoin the song when the next
	 * pattern starts.
	 */
	void togglePin(int track);
	/** Brings loopPattern up to date with pinPattern; at a pattern boundary, tracks let go of rejoin the song. */
	void syncLoops(bool boundary);

	bool modView() {
		return params[MOD_VIEW_PARAM].getValue() > 0.5f;
	}
	bool probView() {
		return params[PROB_VIEW_PARAM].getValue() > 0.5f;
	}

	const ClockRate& rate(int track) {
		return CLOCK_RATES[clamp(clockRate[track], 0, (int) CLOCK_RATES.size() - 1)];
	}
	/** Clock steps `track` takes to play through its length once in `pattern`. */
	int clockSteps(int pattern, int track) {
		const ClockRate& r = rate(track);
		return (trackLength(pattern, track) * r.div + r.mul - 1) / r.mul;
	}
	/** How late (as a fraction of a step) swing makes every second step: 0 = straight. */
	double swingLate() {
		return clamp(params[SWING_PARAM].getValue() / 50.f - 1.f, 0.f, 0.5f);
	}
	/** True if the next step is a swung one (the 2nd, 4th, ... of the pattern), so it comes late. */
	bool swingsNext() {
		return swingLate() > 0.0 && step >= 0 && step % 2 == 0 && step + 1 < patternLength(playingPattern);
	}
	/** Output voltage for a MOD value on a track. */
	float modVoltage(int track, float value) {
		const ModRange& r = MOD_RANGES[clamp(modRange[track], 0, (int) MOD_RANGES.size() - 1)];
		return r.min + (r.max - r.min) * clamp(value, 0.f, (float) MOD_MAX) / MOD_MAX;
	}
	/** While a song is playing, the grid follows the pattern being played. */
	bool following() {
		return playingSong() && running;
	}
	/** Pattern shown and edited in the grid: a looping track's own pattern, as that is what it plays. */
	int viewPattern() {
		if (looping(selectedTrack))
			return loopPattern[selectedTrack];
		return following() ? playingPattern : editPattern();
	}

	float rowToVoltage(int row);

	void scroll(int delta) {
		scrollRow = clamp(scrollRow + delta, 0, maxScroll());
	}

	void clearAll();

	void resetPlayback();

	int trackLength(int pattern, int track) {
		return clamp((int) trackLengths[pattern][track], 1, NUM_STEPS);
	}

	void setTrackLength(int pattern, int track, int length) {
		trackLengths[pattern][track] = clamp(length, 1, NUM_STEPS);
	}

	/** True if the track has a note (on any of its voices) that starts within its length. */
	bool trackHasNotes(int pattern, int track);

	/**
	 * Steps the pattern plays before moving on: its longest track with notes, or with no notes at
	 * all, its longest track (16 steps for a new pattern). Tracks count in clock steps, so a
	 * 16-step track at /2 lasts 32.
	 */
	int patternLength(int pattern);

	void setViewStart(int start) {
		viewStart = clamp(start, 0, NUM_STEPS - VIEW_STEPS);
	}

	// ---- Note editing (per lane) ----

	/** Start step of the note covering `col` in `lane`, or -1 if the step is silent. */
	int noteStartCovering(int pattern, int lane, int col);

	void removeNote(int pattern, int lane, int s) {
		notes[pattern][lane][s] = -1;
		lengths[pattern][lane][s] = 1;
		ties[pattern][lane][s] = 0;
	}

	/** Places a note within the track's length, trimming or removing whatever it overlaps in its lane. */
	void placeNote(int pattern, int lane, int s, int row, int length, bool tie = false, int mod = -1, int prob = PROB_MAX);

	/** True if no note in `lane` sounds anywhere in steps [s, s + length). */
	bool spanFree(int pattern, int lane, int s, int length);
	/** Steps from free step `s` up to the next note in `lane`. */
	int freeLength(int pattern, int lane, int s);
	/**
	 * A voice lane of `track` that is free for steps [s, s + length): `prefer` if it is (and is
	 * one of the track's voices), otherwise the first free one. -1 if every voice is busy.
	 */
	int freeVoice(int pattern, int track, int s, int length, int prefer = -1);
	/** Finds the note at (`step`, `row`) among the voices of `track`. */
	bool noteAt(int pattern, int track, int step, int row, int* lane, int* start);

	/** Clamps note lengths to the step grid and clears steps covered by earlier notes. */
	void normalizeLane(int pattern, int lane);

	/** Moves every note of a track (all its voices) `delta` scale degrees, unless that would push one off the grid. */
	bool transposeTrack(int pattern, int track, int delta);

	/**
	 * Rotates a track's notes (all its voices) and step MOD values `delta` steps within its
	 * length, wrapping around the end; see rotateLane.
	 */
	void rotateTrack(int pattern, int track, int delta);

	/**
	 * Rotates a lane's notes `delta` steps within `length`, wrapping around the end. A note may
	 * hang past the end on the way round (it plays cut short), so rotating back restores it.
	 * Notes stored past the track's end stay where they are.
	 */
	void rotateLane(int pattern, int lane, int length, int delta);

	/**
	 * Replaces a track's notes within its length with a Euclidean rhythm: `hits` notes spread
	 * as evenly as they go, the first on step 1. They are one step long, on the pitch of the
	 * track's first note (or the root of the middle octave).
	 */
	void euclideanFill(int pattern, int track, int hits);

	/** Inserts a song slot before or after the cursor and moves the cursor to it. */
	void songInsert(int pattern, bool after = true);

	void songDelete();

	/** Moves the slot at `from` to `to`, shifting the ones between; the cursor goes with it. */
	void songMove(int from, int to);

	void songClear() {
		songLength = 0;
		songCursor = 0;
		songPos = 0;
	}

	// ---- Clipboard (acts on the pattern shown in the grid) ----

	void copyPattern();
	void pastePattern();
	void clearPattern();
	void clearLane(int pattern, int lane) {
		std::memset(notes[pattern][lane], -1, sizeof(notes[0][0]));
		std::memset(lengths[pattern][lane], 1, sizeof(lengths[0][0]));
		std::memset(ties[pattern][lane], 0, sizeof(ties[0][0]));
	}
	// Track copy / paste / clear act on every voice of the selected track
	void copyTrack();
	void pasteTrack();
	void clearTrack();

	// ---- Undo snapshots ----

	void savePattern(int pattern, PatternSnapshot& snap);
	void loadPattern(int pattern, const PatternSnapshot& snap);
	void saveSong(SongSnapshot& snap) {
		std::memcpy(snap.song, song, sizeof(snap.song));
		snap.length = songLength;
		snap.cursor = songCursor;
	}
	void loadSong(const SongSnapshot& snap) {
		std::memcpy(song, snap.song, sizeof(song));
		songLength = clamp(snap.length, 0, MAX_SONG_LENGTH);
		songCursor = clamp(snap.cursor, 0, std::max(songLength - 1, 0));
	}

	void queueEdit(Edit&& edit) {
		std::lock_guard<std::mutex> lock(editsMutex);
		if (pendingEdits.size() < 64)
			pendingEdits.push_back(std::move(edit));
	}
	/** Records an edit of `pattern` made since `before` was saved, if anything changed. */
	void recordPatternEdit(const std::string& name, int pattern, const PatternSnapshot& before);
	/** Runs `f`, recording it as one undoable edit of `pattern`. */
	template <class F>
	void patternEdit(const std::string& name, int pattern, F f) {
		std::unique_ptr<PatternSnapshot> before(new PatternSnapshot);
		savePattern(pattern, *before);
		f();
		recordPatternEdit(name, pattern, *before);
	}
	/** Runs `f`, recording it as one undoable edit of the song. */
	template <class F>
	void songEdit(const std::string& name, F f) {
		SongSnapshot before;
		saveSong(before);
		f();
		recordSongEdit(name, before);
	}
	/** Records an edit of the song made since `before` was saved, if anything changed. */
	void recordSongEdit(const std::string& name, const SongSnapshot& before);

	// ---- Playback ----

	/**
	 * Advances one clock step, then steps each track whose clock rate has it step now. `swung`:
	 * this is a swung step, coming late (so it is that much shorter).
	 */
	void stepForward(bool swung = false);

	/** Puts back the state from before the last step, so it can be taken again. */
	void undoStep();

	/**
	 * Takes one step of `track` (`sub`: which of its steps within the clock step, for a sped-up
	 * track): starts its new notes, unless their probability says not, and counts down the ones
	 * it is holding.
	 */
	void trackStepForward(int track, int sub);

	/** Pattern that plays after the current one ends; -1 = none (a play-once song ends). */
	int nextPattern();

	/**
	 * For per-step MOD gliding: the step `track` plays next (after its step `sub` of the clock
	 * step). At the end of the pattern that is step 1 of the next pattern (the next song slot,
	 * or the SEQ pattern); with PAT IN patched the pattern is assumed to loop, as it does until
	 * a trigger arrives.
	 */
	void findNextModStep(int track, int sub);

	/** Follow playhead: when the edited track's playhead is out of view, show the 16-step block it is in. */
	void followPlayback();

	void advance();

	// ---- MIDI import ----

	/**
	 * Replaces the whole sequence with a MIDI file's notes. Each part (MIDI track and channel) goes
	 * on its own track, up to 8, as a poly track where its notes overlap; velocities become MOD
	 * values. Notes snap to 16th-note steps, and the scale becomes Chromatic (root C) so every
	 * pitch fits. The piece is cut into 64-step patterns (identical ones shared) strung together
	 * as the song. Returns a note of whatever didn't fit, or "" if everything did.
	 */
	std::string importMidi(const MidiFileData& midi, const std::string& name);

	// ---- Module overrides ----

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		clearAll();
	}

	void process(const ProcessArgs& args) override;

	json_t* dataToJson() override;

	/** Loads a lane's rows / lengths / ties / MOD values / probabilities from their strings (any may be null). */
	void laneFromStrings(int p, int l, const char* rows, const char* lens, const char* tieStr, const char* modStr, const char* probStr);

	void dataFromJson(json_t* rootJ) override;
};
