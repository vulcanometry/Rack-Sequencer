#include "plugin.hpp"
#include "MidiFile.hpp"
#include <osdialog.h>
#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

const int NUM_TRACKS = 8;
// Most notes a poly track can play at once
const int MAX_VOICES = 8;
// Tracks are 1..NUM_STEPS steps long; the grid shows them a page of VIEW_STEPS at a time.
const int NUM_STEPS = 64;
const int VIEW_STEPS = 16;
const int DEFAULT_TRACK_LENGTH = 16;
const int NUM_PATTERNS = 64;
const int MAX_SONG_LENGTH = 128;
const int NUM_OCTAVES = 8;
const int VISIBLE_ROWS = 15;

struct ScaleDef {
	const char* name;
	// Fits the compact SCALE display; the full name shows in the tooltip and info bar
	const char* shortName;
	std::vector<int> notes;
};

const std::vector<ScaleDef> SCALES = {
	{"Major", "Major", {0, 2, 4, 5, 7, 9, 11}},
	{"Minor", "Minor", {0, 2, 3, 5, 7, 8, 10}},
	{"Harm. Minor", "HarMin", {0, 2, 3, 5, 7, 8, 11}},
	{"Mel. Minor", "MelMin", {0, 2, 3, 5, 7, 9, 11}},
	{"Dorian", "Dorian", {0, 2, 3, 5, 7, 9, 10}},
	{"Phrygian", "Phryg", {0, 1, 3, 5, 7, 8, 10}},
	{"Lydian", "Lydian", {0, 2, 4, 6, 7, 9, 11}},
	{"Mixolydian", "Mixo", {0, 2, 4, 5, 7, 9, 10}},
	{"Locrian", "Locrn", {0, 1, 3, 5, 6, 8, 10}},
	{"Major Pent.", "MajPen", {0, 2, 4, 7, 9}},
	{"Minor Pent.", "MinPen", {0, 3, 5, 7, 10}},
	{"Blues", "Blues", {0, 3, 5, 6, 7, 10}},
	{"Whole Tone", "WhlTon", {0, 2, 4, 6, 8, 10}},
	{"Chromatic", "Chrom", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};

// Gate length is set in 5% steps: GATE_LENGTH_PARAM 1..GATE_STEPS
const int GATE_STEPS = 20;

const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const NVGcolor TRACK_COLORS[NUM_TRACKS] = {
	nvgRGB(0xff, 0x5c, 0x5c),
	nvgRGB(0xff, 0xa0, 0x40),
	nvgRGB(0xf2, 0xdc, 0x4a),
	nvgRGB(0x6c, 0xe0, 0x6c),
	nvgRGB(0x4c, 0xd4, 0xe4),
	nvgRGB(0x5c, 0x8c, 0xff),
	nvgRGB(0xb4, 0x74, 0xff),
	nvgRGB(0xff, 0x74, 0xc4),
};

const char* const DISPLAY_FONT = "res/fonts/ShareTechMono-Regular.ttf";
const char* const LABEL_FONT = "res/fonts/DejaVuSans.ttf";

/** Voltage range of a track's MOD output; a note's MOD value (0-100%) is scaled into it. */
struct ModRange {
	const char* name;
	float min, max;
};

const std::vector<ModRange> MOD_RANGES = {
	{"0 to 10 V", 0.f, 10.f},
	{"0 to 5 V", 0.f, 5.f},
	{"-5 to +5 V", -5.f, 5.f},
};

const int MOD_MAX = 100;
const int DEFAULT_MOD = 100;
const int DEFAULT_STEP_MOD = 0;

// MOD modes: one value per note (held while it plays), or one value for every step
enum ModMode { MOD_PER_NOTE, MOD_PER_STEP };

// Every track has MAX_VOICES note lanes. An ordinary track plays lane 0 only; a poly track of N
// voices plays lanes 0..N-1 together and sends them as an N-channel poly cable.
const int NUM_LANES = NUM_TRACKS * MAX_VOICES;

inline int laneOf(int track, int voice) {
	return track * MAX_VOICES + voice;
}
inline int trackOfLane(int lane) {
	return lane / MAX_VOICES;
}

struct Clipboard {
	bool hasPattern = false;
	int8_t pattern[NUM_LANES][NUM_STEPS];
	uint8_t patternLengths[NUM_LANES][NUM_STEPS];
	uint8_t patternTies[NUM_LANES][NUM_STEPS];
	uint8_t patternMods[NUM_LANES][NUM_STEPS];
	uint8_t patternStepMods[NUM_TRACKS][NUM_STEPS];
	uint8_t patternTrackLengths[NUM_TRACKS];
	bool hasTrack = false;
	int trackVoices = 1;
	int8_t track[MAX_VOICES][NUM_STEPS];
	uint8_t trackLengths[MAX_VOICES][NUM_STEPS];
	uint8_t trackTies[MAX_VOICES][NUM_STEPS];
	uint8_t trackMods[MAX_VOICES][NUM_STEPS];
	uint8_t trackStepMods[NUM_STEPS];
	uint8_t trackLength = DEFAULT_TRACK_LENGTH;
};

Clipboard clipboard;

/** Hex string of 2 digits per value, as patches store note data. */
std::string toHex(const uint8_t* values, int count, int maxValue) {
	std::string s;
	for (int i = 0; i < count; i++)
		s += string::f("%02x", clamp((int) values[i], 0, maxValue));
	return s;
}
/** Reads 2-digit hex values into `values` (up to NUM_STEPS); "--" entries are skipped. */
void fromHex(const char* str, uint8_t* values, int minValue, int maxValue) {
	std::string s = str ? str : "";
	for (int i = 0; i < NUM_STEPS && (size_t) (i * 2 + 1) < s.size(); i++) {
		std::string hex = s.substr(i * 2, 2);
		if (hex != "--")
			values[i] = clamp((int) std::strtol(hex.c_str(), nullptr, 16), minValue, maxValue);
	}
}

} // namespace


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
	// the end of a play-once song), and how far through the current step playback is (0..1)
	int modNextPattern[NUM_TRACKS] = {};
	int modNextStep[NUM_TRACKS] = {};
	float modPhase = 0.f;
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
	int8_t song[MAX_SONG_LENGTH];
	// Slots in the song; 0 = empty (a new module's song is empty until patterns are added)
	int songLength = 0;
	// Shown in the info bar and used as the file name when saving the song; "" = not named yet
	// (shown as "Untitled")
	std::string songName;
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
	int songPos = 0;
	int playingPattern = 0;
	double phase = 1.0;
	double stepPeriod = 0.125;
	double stepTimer = 0.0;
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

	GridSequencer() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(TEMPO_PARAM, 30.f, 300.f, 120.f, "Tempo", " BPM");

		std::vector<std::string> rootLabels(NOTE_NAMES, NOTE_NAMES + 12);
		configSwitch(ROOT_PARAM, 0.f, 11.f, 0.f, "Root note", rootLabels);
		std::vector<std::string> scaleLabels;
		for (const ScaleDef& s : SCALES)
			scaleLabels.push_back(s.name);
		configSwitch(SCALE_PARAM, 0.f, SCALES.size() - 1, 0.f, "Scale", scaleLabels);

		configParam(PATTERN_PARAM, 1.f, NUM_PATTERNS, 1.f, "Edit pattern");
		getParamQuantity(PATTERN_PARAM)->snapEnabled = true;
		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Play mode", {"Pattern", "Song (loop)", "Song (play once)"});

		configButton(RUN_PARAM, "Run / stop");
		configButton(RESET_PARAM, "Reset");
		configButton(SONG_INSERT_PARAM, "Insert SEQ pattern into song before cursor");
		configButton(SONG_ADD_PARAM, "Add SEQ pattern to song after cursor");
		configSwitch(MOD_VIEW_PARAM, 0.f, 1.f, 0.f, "Grid shows", {"Notes", "MOD values"});
		std::vector<std::string> gateLabels;
		for (int i = 1; i <= GATE_STEPS; i++)
			gateLabels.push_back(i == GATE_STEPS ? "100% (legato)" : string::f("%d%%", i * 100 / GATE_STEPS));
		configSwitch(GATE_LENGTH_PARAM, 1.f, GATE_STEPS, GATE_STEPS / 2, "Gate length", gateLabels);
		configButton(SONG_DELETE_PARAM, "Delete song slot at cursor");
		configButton(SONG_DUPLICATE_PARAM, "Duplicate song slot at cursor");
		configButton(SONG_SET_PARAM, "Set song slot at cursor to SEQ pattern");
		configButton(SONG_EDIT_PARAM, "Open pattern of song slot at cursor in SEQ");
		configButton(SONG_CLEAR_PARAM, "Clear song (removes every slot)");
		configButton(PATTERN_COPY_PARAM, "Copy pattern");
		configButton(PATTERN_PASTE_PARAM, "Paste pattern");
		configButton(PATTERN_CLEAR_PARAM, "Clear pattern");
		configButton(TRACK_COPY_PARAM, "Copy track");
		configButton(TRACK_PASTE_PARAM, "Paste track");
		configButton(TRACK_CLEAR_PARAM, "Clear track");
		configButton(SCROLL_UP_PARAM, "Scroll grid up");
		configButton(SCROLL_DOWN_PARAM, "Scroll grid down");
		for (int t = 0; t < NUM_TRACKS; t++) {
			configButton(TRACK_PARAM + t, string::f("Edit track %d", t + 1));
			configButton(MUTE_PARAM + t, string::f("Mute track %d", t + 1));
			configOutput(CV_OUTPUT + t, string::f("Track %d pitch (1V/oct; poly track: one channel per voice)", t + 1));
			configOutput(GATE_OUTPUT + t, string::f("Track %d gate (poly track: one channel per voice)", t + 1));
			configOutput(MOD_OUTPUT + t, string::f("Track %d MOD (per-note value, e.g. velocity; poly track: one channel per voice)", t + 1));
		}

		configInput(CLOCK_INPUT, "External clock (one step per pulse)");
		configInput(RESET_INPUT, "Reset");
		configOutput(CLOCK_OUTPUT, "Step clock");
		configOutput(EOP_OUTPUT, "End of pattern (trigger each time the sequencer moves on to the next pattern)");
		configOutput(RUN_OUTPUT,"Run (trigger on start/stop, or gate while running)");
		configOutput(RESET_OUTPUT, "Reset");
		configInput(RUN_INPUT, "Run (toggle on trigger, or gate high = run)");
		configInput(PATTERN_INPUT, "Next pattern (trigger; while patched, patterns change only on this)");

		lightDivider.setDivision(32);
		clearAll();
	}

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
		bool modSmooth[NUM_TRACKS];
	};
	void saveSettings(TrackSettings& t) {
		std::memcpy(t.voices, voices, sizeof(t.voices));
		std::memcpy(t.spreadVoices, spreadVoices, sizeof(t.spreadVoices));
		std::memcpy(t.modMode, modMode, sizeof(t.modMode));
		std::memcpy(t.modRange, modRange, sizeof(t.modRange));
		std::memcpy(t.modSmooth, modSmooth, sizeof(t.modSmooth));
	}
	void loadSettings(const TrackSettings& t) {
		std::memcpy(voices, t.voices, sizeof(voices));
		std::memcpy(spreadVoices, t.spreadVoices, sizeof(spreadVoices));
		std::memcpy(modMode, t.modMode, sizeof(modMode));
		std::memcpy(modRange, t.modRange, sizeof(modRange));
		std::memcpy(modSmooth, t.modSmooth, sizeof(modSmooth));
		routeOutputs();
	}

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
	void routeOutputs() {
		for (int t = 0; t < NUM_TRACKS; t++) {
			ownerTrack[t] = t;
			ownerVoice[t] = 0;
		}
		for (int t = 0; t < NUM_TRACKS; t++) {
			if (isVoiceOutput(t) || !spreadVoices[t])
				continue;
			for (int v = 1; v < voiceCount(t) && t + v < NUM_TRACKS; v++) {
				ownerTrack[t + v] = t;
				ownerVoice[t + v] = v;
			}
		}
		// The selected track is always one that plays its own notes
		selectedTrack = ownerTrack[clamp(selectedTrack, 0, NUM_TRACKS - 1)];
	}

	bool modView() {
		return params[MOD_VIEW_PARAM].getValue() > 0.5f;
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
	/** Pattern shown and edited in the grid. */
	int viewPattern() {
		return following() ? playingPattern : editPattern();
	}

	float rowToVoltage(int row) {
		const std::vector<int>& sc = SCALES[scaleIndex()].notes;
		int n = sc.size();
		int octave = row / n;
		int degree = row % n;
		float v = (octave - 4) + (rootIndex() + sc[degree]) / 12.f;
		return clamp(v, -10.f, 10.f);
	}

	void scroll(int delta) {
		scrollRow = clamp(scrollRow + delta, 0, maxScroll());
	}

	void clearAll() {
		std::memset(notes, -1, sizeof(notes));
		std::memset(lengths, 1, sizeof(lengths));
		std::memset(ties, 0, sizeof(ties));
		std::memset(mods, DEFAULT_MOD, sizeof(mods));
		std::memset(stepMods, DEFAULT_STEP_MOD, sizeof(stepMods));
		std::memset(trackLengths, DEFAULT_TRACK_LENGTH, sizeof(trackLengths));
		std::memset(song, 0, sizeof(song));
		for (int t = 0; t < NUM_TRACKS; t++) {
			modRange[t] = 0;
			modDefault[t] = DEFAULT_MOD;
			modMode[t] = MOD_PER_NOTE;
			modSmooth[t] = true;
			voices[t] = 1;
			spreadVoices[t] = false;
			muted[t] = false;
		}
		songLength = 0;
		songCursor = 0;
		songName.clear();
		selectedTrack = 0;
		routeOutputs();
		running = false;
		scrollRow = defaultScroll();
		viewStart = 0;
		resetPlayback();
	}

	void resetPlayback() {
		step = -1;
		songPos = 0;
		patternJumpPending = false;
		phase = 1.0;
		stepTimer = 0.0;
		playingPattern = playingSong() ? song[0] : editPattern();
		for (int t = 0; t < NUM_TRACKS; t++)
			trackStep[t] = -1;
		for (int l = 0; l < NUM_LANES; l++)
			holdLeft[l] = 0;
	}

	int trackLength(int pattern, int track) {
		return clamp((int) trackLengths[pattern][track], 1, NUM_STEPS);
	}

	void setTrackLength(int pattern, int track, int length) {
		trackLengths[pattern][track] = clamp(length, 1, NUM_STEPS);
	}

	/** True if the track has a note (on any of its voices) that starts within its length. */
	bool trackHasNotes(int pattern, int track) {
		if (isVoiceOutput(track))
			return false;
		int length = trackLength(pattern, track);
		for (int v = 0; v < voiceCount(track); v++)
			for (int s = 0; s < length; s++)
				if (notes[pattern][laneOf(track, v)][s] >= 0)
					return true;
		return false;
	}

	/**
	 * Steps the pattern plays before moving on: its longest track with notes, or with no notes at
	 * all, its longest track (16 steps for a new pattern).
	 */
	int patternLength(int pattern) {
		int length = 0;
		for (int t = 0; t < NUM_TRACKS; t++)
			if (trackHasNotes(pattern, t))
				length = std::max(length, trackLength(pattern, t));
		if (length == 0)
			for (int t = 0; t < NUM_TRACKS; t++)
				if (!isVoiceOutput(t))
					length = std::max(length, trackLength(pattern, t));
		return length;
	}

	void setViewStart(int start) {
		viewStart = clamp(start, 0, NUM_STEPS - VIEW_STEPS);
	}

	// ---- Note editing (per lane) ----

	/** Start step of the note covering `col` in `lane`, or -1 if the step is silent. */
	int noteStartCovering(int pattern, int lane, int col) {
		for (int s = col; s >= 0; s--) {
			if (notes[pattern][lane][s] >= 0)
				return (s + lengths[pattern][lane][s] > col) ? s : -1;
		}
		return -1;
	}

	void removeNote(int pattern, int lane, int s) {
		notes[pattern][lane][s] = -1;
		lengths[pattern][lane][s] = 1;
		ties[pattern][lane][s] = 0;
	}

	/** Places a note within the track's length, trimming or removing whatever it overlaps in its lane. */
	void placeNote(int pattern, int lane, int s, int row, int length, bool tie = false, int mod = -1) {
		int track = trackOfLane(lane);
		int end = trackLength(pattern, track);
		if (s >= end)
			return;
		length = clamp(length, 1, end - s);
		int prev = noteStartCovering(pattern, lane, s);
		if (prev >= 0 && prev < s)
			lengths[pattern][lane][prev] = s - prev;
		for (int i = s; i < s + length; i++)
			removeNote(pattern, lane, i);
		notes[pattern][lane][s] = row;
		lengths[pattern][lane][s] = length;
		ties[pattern][lane][s] = tie;
		mods[pattern][lane][s] = mod >= 0 ? mod : modDefault[track];
	}

	/** True if no note in `lane` sounds anywhere in steps [s, s + length). */
	bool spanFree(int pattern, int lane, int s, int length) {
		if (noteStartCovering(pattern, lane, s) >= 0)
			return false;
		for (int i = s + 1; i < std::min(s + length, NUM_STEPS); i++)
			if (notes[pattern][lane][i] >= 0)
				return false;
		return true;
	}
	/** Steps from free step `s` up to the next note in `lane`. */
	int freeLength(int pattern, int lane, int s) {
		int n = 1;
		while (s + n < NUM_STEPS && notes[pattern][lane][s + n] < 0)
			n++;
		return n;
	}
	/**
	 * A voice lane of `track` that is free for steps [s, s + length): `prefer` if it is (and is
	 * one of the track's voices), otherwise the first free one. -1 if every voice is busy.
	 */
	int freeVoice(int pattern, int track, int s, int length, int prefer = -1) {
		int end = std::min(s + length, trackLength(pattern, track)) - s;
		if (prefer >= laneOf(track, 0) && prefer < laneOf(track, voiceCount(track)) && spanFree(pattern, prefer, s, end))
			return prefer;
		for (int v = 0; v < voiceCount(track); v++)
			if (spanFree(pattern, laneOf(track, v), s, end))
				return laneOf(track, v);
		return -1;
	}
	/** Finds the note at (`step`, `row`) among the voices of `track`. */
	bool noteAt(int pattern, int track, int step, int row, int* lane, int* start) {
		for (int v = 0; v < voiceCount(track); v++) {
			int l = laneOf(track, v);
			int s = noteStartCovering(pattern, l, step);
			if (s >= 0 && notes[pattern][l][s] == row) {
				*lane = l;
				*start = s;
				return true;
			}
		}
		return false;
	}

	/** Clamps note lengths to the step grid and clears steps covered by earlier notes. */
	void normalizeLane(int pattern, int lane) {
		for (int s = 0; s < NUM_STEPS; s++) {
			if (notes[pattern][lane][s] < 0) {
				lengths[pattern][lane][s] = 1;
				ties[pattern][lane][s] = 0;
				continue;
			}
			int length = clamp((int) lengths[pattern][lane][s], 1, NUM_STEPS - s);
			lengths[pattern][lane][s] = length;
			for (int i = s + 1; i < s + length; i++)
				removeNote(pattern, lane, i);
			s += length - 1;
		}
	}

	/** Moves every note of a track (all its voices) `delta` scale degrees, unless that would push one off the grid. */
	bool transposeTrack(int pattern, int track, int delta) {
		for (int v = 0; v < voiceCount(track); v++) {
			int8_t* n = notes[pattern][laneOf(track, v)];
			for (int s = 0; s < NUM_STEPS; s++)
				if (n[s] >= 0 && (n[s] + delta < 0 || n[s] + delta >= numRows()))
					return false;
		}
		for (int v = 0; v < voiceCount(track); v++) {
			int8_t* n = notes[pattern][laneOf(track, v)];
			for (int s = 0; s < NUM_STEPS; s++)
				if (n[s] >= 0)
					n[s] += delta;
		}
		return true;
	}

	/**
	 * Rotates a track's notes (all its voices) and step MOD values `delta` steps within its
	 * length, wrapping around the end; see rotateLane.
	 */
	void rotateTrack(int pattern, int track, int delta) {
		int length = trackLength(pattern, track);
		for (int v = 0; v < voiceCount(track); v++)
			rotateLane(pattern, laneOf(track, v), length, delta);

		// Step MOD values rotate with it, step for step
		uint8_t* sm = stepMods[pattern][track];
		uint8_t newStepMods[NUM_STEPS];
		std::memcpy(newStepMods, sm, sizeof(newStepMods));
		for (int s = 0; s < length; s++)
			newStepMods[((s + delta) % length + length) % length] = sm[s];
		std::memcpy(sm, newStepMods, sizeof(newStepMods));
	}

	/**
	 * Rotates a lane's notes `delta` steps within `length`, wrapping around the end. A note may
	 * hang past the end on the way round (it plays cut short), so rotating back restores it.
	 * Notes stored past the track's end stay where they are.
	 */
	void rotateLane(int pattern, int lane, int length, int delta) {
		int8_t* n = notes[pattern][lane];
		uint8_t* l = lengths[pattern][lane];
		uint8_t* ti = ties[pattern][lane];
		uint8_t* mo = mods[pattern][lane];
		int8_t newNotes[NUM_STEPS];
		uint8_t newLengths[NUM_STEPS], newTies[NUM_STEPS], newMods[NUM_STEPS];
		std::memcpy(newNotes, n, sizeof(newNotes));
		std::memcpy(newLengths, l, sizeof(newLengths));
		std::memcpy(newTies, ti, sizeof(newTies));
		std::memcpy(newMods, mo, sizeof(newMods));
		for (int s = 0; s < length; s++) {
			newNotes[s] = -1;
			newLengths[s] = 1;
			newTies[s] = 0;
		}
		for (int s = 0; s < length; s++) {
			if (n[s] < 0)
				continue;
			int d = ((s + delta) % length + length) % length;
			newNotes[d] = n[s];
			newLengths[d] = l[s];
			newTies[d] = ti[s];
			newMods[d] = mo[s];
		}
		// A note that now runs into the next one is cut short where that one starts
		for (int s = 0; s < NUM_STEPS; s++) {
			if (newNotes[s] < 0)
				continue;
			int next = s + 1;
			while (next < NUM_STEPS && newNotes[next] < 0)
				next++;
			newLengths[s] = clamp((int) newLengths[s], 1, next - s);
		}
		std::memcpy(n, newNotes, sizeof(newNotes));
		std::memcpy(l, newLengths, sizeof(newLengths));
		std::memcpy(ti, newTies, sizeof(newTies));
		std::memcpy(mo, newMods, sizeof(newMods));
	}

	/** Inserts a song slot before or after the cursor and moves the cursor to it. */
	void songInsert(int pattern, bool after = true) {
		if (songLength >= MAX_SONG_LENGTH)
			return;
		int at = after ? std::min(songCursor + 1, songLength) : songCursor;
		for (int i = songLength; i > at; i--)
			song[i] = song[i - 1];
		song[at] = pattern;
		songLength++;
		songCursor = at;
	}

	void songDelete() {
		if (songLength <= 0)
			return;
		for (int i = songCursor; i < songLength - 1; i++)
			song[i] = song[i + 1];
		songLength--;
		songCursor = clamp(songCursor, 0, std::max(songLength - 1, 0));
	}

	/** Moves the slot at `from` to `to`, shifting the ones between; the cursor goes with it. */
	void songMove(int from, int to) {
		if (from < 0 || from >= songLength || to < 0 || to >= songLength || from == to)
			return;
		int8_t pattern = song[from];
		int dir = to > from ? 1 : -1;
		for (int i = from; i != to; i += dir)
			song[i] = song[i + dir];
		song[to] = pattern;
		songCursor = to;
		// Keep playing the same slot
		if (songPos == from)
			songPos = to;
		else if (from < songPos && songPos <= to)
			songPos--;
		else if (to <= songPos && songPos < from)
			songPos++;
	}

	void songClear() {
		songLength = 0;
		songCursor = 0;
		songPos = 0;
	}

	// ---- Clipboard (acts on the pattern shown in the grid) ----

	void copyPattern() {
		std::memcpy(clipboard.pattern, notes[viewPattern()], sizeof(clipboard.pattern));
		std::memcpy(clipboard.patternLengths, lengths[viewPattern()], sizeof(clipboard.patternLengths));
		std::memcpy(clipboard.patternTies, ties[viewPattern()], sizeof(clipboard.patternTies));
		std::memcpy(clipboard.patternMods, mods[viewPattern()], sizeof(clipboard.patternMods));
		std::memcpy(clipboard.patternStepMods, stepMods[viewPattern()], sizeof(clipboard.patternStepMods));
		std::memcpy(clipboard.patternTrackLengths, trackLengths[viewPattern()], sizeof(clipboard.patternTrackLengths));
		clipboard.hasPattern = true;
	}
	void pastePattern() {
		if (!clipboard.hasPattern)
			return;
		std::memcpy(notes[viewPattern()], clipboard.pattern, sizeof(clipboard.pattern));
		std::memcpy(lengths[viewPattern()], clipboard.patternLengths, sizeof(clipboard.patternLengths));
		std::memcpy(ties[viewPattern()], clipboard.patternTies, sizeof(clipboard.patternTies));
		std::memcpy(mods[viewPattern()], clipboard.patternMods, sizeof(clipboard.patternMods));
		std::memcpy(stepMods[viewPattern()], clipboard.patternStepMods, sizeof(clipboard.patternStepMods));
		std::memcpy(trackLengths[viewPattern()], clipboard.patternTrackLengths, sizeof(clipboard.patternTrackLengths));
	}
	void clearPattern() {
		std::memset(notes[viewPattern()], -1, sizeof(notes[0]));
		std::memset(lengths[viewPattern()], 1, sizeof(lengths[0]));
		std::memset(ties[viewPattern()], 0, sizeof(ties[0]));
		std::memset(stepMods[viewPattern()], DEFAULT_STEP_MOD, sizeof(stepMods[0]));
	}
	void clearLane(int pattern, int lane) {
		std::memset(notes[pattern][lane], -1, sizeof(notes[0][0]));
		std::memset(lengths[pattern][lane], 1, sizeof(lengths[0][0]));
		std::memset(ties[pattern][lane], 0, sizeof(ties[0][0]));
	}
	// Track copy / paste / clear act on every voice of the selected track
	void copyTrack() {
		int p = viewPattern(), t = selectedTrack;
		clipboard.trackVoices = voiceCount(t);
		for (int v = 0; v < clipboard.trackVoices; v++) {
			int l = laneOf(t, v);
			std::memcpy(clipboard.track[v], notes[p][l], sizeof(clipboard.track[v]));
			std::memcpy(clipboard.trackLengths[v], lengths[p][l], sizeof(clipboard.trackLengths[v]));
			std::memcpy(clipboard.trackTies[v], ties[p][l], sizeof(clipboard.trackTies[v]));
			std::memcpy(clipboard.trackMods[v], mods[p][l], sizeof(clipboard.trackMods[v]));
		}
		std::memcpy(clipboard.trackStepMods, stepMods[p][t], sizeof(clipboard.trackStepMods));
		clipboard.trackLength = trackLengths[p][t];
		clipboard.hasTrack = true;
	}
	void pasteTrack() {
		if (!clipboard.hasTrack)
			return;
		// Copied voices fill this track's voices; voices it has beyond them are cleared, and
		// voices the copy has beyond this track's are left out
		int p = viewPattern(), t = selectedTrack;
		for (int v = 0; v < voiceCount(t); v++) {
			int l = laneOf(t, v);
			if (v < clipboard.trackVoices) {
				std::memcpy(notes[p][l], clipboard.track[v], sizeof(notes[p][l]));
				std::memcpy(lengths[p][l], clipboard.trackLengths[v], sizeof(lengths[p][l]));
				std::memcpy(ties[p][l], clipboard.trackTies[v], sizeof(ties[p][l]));
				std::memcpy(mods[p][l], clipboard.trackMods[v], sizeof(mods[p][l]));
			}
			else {
				clearLane(p, l);
			}
		}
		std::memcpy(stepMods[p][t], clipboard.trackStepMods, sizeof(stepMods[p][t]));
		setTrackLength(p, t, clipboard.trackLength);
	}
	void clearTrack() {
		int p = viewPattern(), t = selectedTrack;
		for (int v = 0; v < voiceCount(t); v++)
			clearLane(p, laneOf(t, v));
		std::memset(stepMods[p][t], DEFAULT_STEP_MOD, sizeof(stepMods[0][0]));
	}

	// ---- Undo snapshots ----

	void savePattern(int pattern, PatternSnapshot& snap) {
		std::memcpy(snap.notes, notes[pattern], sizeof(snap.notes));
		std::memcpy(snap.lengths, lengths[pattern], sizeof(snap.lengths));
		std::memcpy(snap.ties, ties[pattern], sizeof(snap.ties));
		std::memcpy(snap.mods, mods[pattern], sizeof(snap.mods));
		std::memcpy(snap.stepMods, stepMods[pattern], sizeof(snap.stepMods));
		std::memcpy(snap.trackLengths, trackLengths[pattern], sizeof(snap.trackLengths));
	}
	void loadPattern(int pattern, const PatternSnapshot& snap) {
		std::memcpy(notes[pattern], snap.notes, sizeof(snap.notes));
		std::memcpy(lengths[pattern], snap.lengths, sizeof(snap.lengths));
		std::memcpy(ties[pattern], snap.ties, sizeof(snap.ties));
		std::memcpy(mods[pattern], snap.mods, sizeof(snap.mods));
		std::memcpy(stepMods[pattern], snap.stepMods, sizeof(snap.stepMods));
		std::memcpy(trackLengths[pattern], snap.trackLengths, sizeof(snap.trackLengths));
	}
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
	void recordPatternEdit(const std::string& name, int pattern, const PatternSnapshot& before) {
		// Edits are large (two pattern snapshots), so they live on the heap until queued
		std::unique_ptr<Edit> edit(new Edit);
		edit->name = name;
		edit->pattern = pattern;
		edit->patternBefore = before;
		savePattern(pattern, edit->patternAfter);
		if (std::memcmp(&edit->patternBefore, &edit->patternAfter, sizeof(PatternSnapshot)) != 0)
			queueEdit(std::move(*edit));
	}
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
	void recordSongEdit(const std::string& name, const SongSnapshot& before) {
		std::unique_ptr<Edit> edit(new Edit);
		edit->name = name;
		edit->isSong = true;
		edit->songBefore = before;
		saveSong(edit->songAfter);
		if (edit->songBefore.length != edit->songAfter.length
			|| std::memcmp(edit->songBefore.song, edit->songAfter.song, edit->songAfter.length) != 0)
			queueEdit(std::move(*edit));
	}

	/** Advances one step, then starts new notes and counts down the ones being held. */
	void stepForward() {
		advance();
		stepTimer = 0.0;
		for (int t = 0; t < NUM_TRACKS; t++) {
			int length = trackLength(playingPattern, t);
			int s = (step < 0) ? -1 : step % length;
			trackStep[t] = s;
			for (int v = 0; v < MAX_VOICES; v++) {
				int l = laneOf(t, v);
				// A new pattern starts every voice afresh
				if (holdLeft[l] > 0 && step != 0)
					holdLeft[l]--;
				else
					holdLeft[l] = 0;
				if (s < 0 || v >= voiceCount(t) || isVoiceOutput(t))
					continue;
				int row = notes[playingPattern][l][s];
				if (row >= 0) {
					// Notes end at the track's loop point
					holdLeft[l] = std::min((int) lengths[playingPattern][l][s], length - s);
					holdRow[l] = row;
					holdTie[l] = ties[playingPattern][l][s];
					holdMod[l] = mods[playingPattern][l][s];
				}
			}
		}
		findNextModSteps();
	}

	/**
	 * For per-step MOD gliding: the step each track plays next. At the end of the pattern that is
	 * step 1 of the next pattern (the next song slot, or the SEQ pattern); with PAT IN patched the
	 * pattern is assumed to loop, as it does until a trigger arrives.
	 */
	void findNextModSteps() {
		if (step < 0) {
			for (int t = 0; t < NUM_TRACKS; t++)
				modNextStep[t] = -1;
			return;
		}
		int nextPattern = playingPattern;
		int nextStep = step + 1;
		if (nextStep >= patternLength(playingPattern)) {
			nextStep = 0;
			if (!inputs[PATTERN_INPUT].isConnected()) {
				if (playingSong()) {
					int pos = songPos + 1;
					if (pos >= songLength)
						pos = songOnce() ? -1 : 0;
					nextPattern = (pos >= 0) ? song[pos] : -1;
				}
				else {
					nextPattern = editPattern();
				}
			}
		}
		for (int t = 0; t < NUM_TRACKS; t++) {
			modNextPattern[t] = std::max(nextPattern, 0);
			modNextStep[t] = (nextPattern >= 0) ? nextStep % trackLength(modNextPattern[t], t) : -1;
		}
	}

	/** Follow playhead: when the edited track's playhead is out of view, show the 16-step block it is in. */
	void followPlayback() {
		int playStep = trackStep[selectedTrack];
		if (followPlayhead && running && step >= 0 && playStep >= 0 && playingPattern == viewPattern()
			&& (playStep < viewStart || playStep >= viewStart + VIEW_STEPS))
			setViewStart(playStep / VIEW_STEPS * VIEW_STEPS);
	}

	void advance() {
		bool starting = step < 0;
		step++;
		// Without PAT IN the pattern moves on when it ends. With PAT IN patched, it loops
		// until a trigger there says to move on, so another sequencer can drive the changes.
		bool moveOn;
		if (inputs[PATTERN_INPUT].isConnected()) {
			moveOn = patternJumpPending;
			if (!moveOn && step >= patternLength(playingPattern))
				step = 0;
		}
		else {
			moveOn = step >= patternLength(playingPattern);
		}
		patternJumpPending = false;

		if (moveOn) {
			step = 0;
			eopPulse.trigger(1e-3f);
			if (playingSong()) {
				songPos++;
				if (songPos >= songLength) {
					if (songOnce()) {
						// End of the song: stop, ready to play again from the start
						running = false;
						resetPlayback();
						return;
					}
					songPos = 0;
				}
			}
		}
		if (moveOn || starting) {
			songPos = clamp(songPos, 0, std::max(songLength - 1, 0));
			playingPattern = playingSong() ? song[songPos] : editPattern();
		}
	}

	// ---- MIDI import ----

	/**
	 * Replaces the whole sequence with a MIDI file's notes. Each part (MIDI track and channel) goes
	 * on its own track, up to 8, as a poly track where its notes overlap; velocities become MOD
	 * values. Notes snap to 16th-note steps, and the scale becomes Chromatic (root C) so every
	 * pitch fits. The piece is cut into 64-step patterns (identical ones shared) strung together
	 * as the song. Returns a note of whatever didn't fit, or "" if everything did.
	 */
	std::string importMidi(const MidiFileData& midi, const std::string& name) {
		struct StepNote {
			int start, end, row, mod;
		};
		double ticksPerStep = midi.ppq / 4.0;
		std::vector<std::vector<StepNote>> parts(midi.parts.size());
		int outOfRange = 0;
		for (const MidiNote& n : midi.notes) {
			// Chromatic from C: row 48 is C4 (0 V), MIDI note 60
			int row = n.pitch - 12;
			if (row < 0 || row >= NUM_OCTAVES * 12) {
				outOfRange++;
				continue;
			}
			int start = (int) std::round(n.start / ticksPerStep);
			int end = std::max(start + 1, (int) std::round(n.end / ticksPerStep));
			parts[n.part].push_back({start, end, row, (int) std::round(n.velocity * MOD_MAX / 127.0)});
		}
		std::vector<int> used;
		for (size_t p = 0; p < parts.size(); p++)
			if (!parts[p].empty())
				used.push_back(p);
		int droppedParts = std::max(0, (int) used.size() - NUM_TRACKS);
		if ((int) used.size() > NUM_TRACKS)
			used.resize(NUM_TRACKS);

		// Each note goes on the first voice that is free when it starts
		struct Placed {
			int voice;
			StepNote note;
		};
		std::vector<std::vector<Placed>> trackNotes(NUM_TRACKS);
		int trackVoices[NUM_TRACKS];
		int droppedNotes = 0, totalSteps = 0;
		for (int t = 0; t < NUM_TRACKS; t++) {
			trackVoices[t] = 1;
			if (t >= (int) used.size())
				continue;
			std::vector<StepNote>& notes = parts[used[t]];
			std::stable_sort(notes.begin(), notes.end(), [](const StepNote& a, const StepNote& b) {
				return a.start < b.start || (a.start == b.start && a.row < b.row);
			});
			int voiceEnd[MAX_VOICES] = {};
			for (const StepNote& n : notes) {
				int v = 0;
				while (v < MAX_VOICES && voiceEnd[v] > n.start)
					v++;
				if (v == MAX_VOICES) {
					droppedNotes++;
					continue;
				}
				voiceEnd[v] = n.end;
				trackNotes[t].push_back({v, n});
				trackVoices[t] = std::max(trackVoices[t], v + 1);
				totalSteps = std::max(totalSteps, n.end);
			}
		}

		// Cut into patterns of NUM_STEPS steps; a note running on into the next pattern is split
		// there and tied, so it keeps sounding
		int chunks = std::max((totalSteps + NUM_STEPS - 1) / NUM_STEPS, 1);
		std::vector<std::unique_ptr<PatternSnapshot>> distinct;
		std::vector<int> slots;
		bool cutShort = false;
		for (int c = 0; c < chunks; c++) {
			std::unique_ptr<PatternSnapshot> snap(new PatternSnapshot);
			std::memset(snap->notes, -1, sizeof(snap->notes));
			std::memset(snap->lengths, 1, sizeof(snap->lengths));
			std::memset(snap->ties, 0, sizeof(snap->ties));
			std::memset(snap->mods, DEFAULT_MOD, sizeof(snap->mods));
			std::memset(snap->stepMods, DEFAULT_STEP_MOD, sizeof(snap->stepMods));
			int chunkStart = c * NUM_STEPS, chunkEnd = chunkStart + NUM_STEPS;
			// The last pattern is as long as what's left, in whole bars
			int length = std::min(NUM_STEPS, (totalSteps - chunkStart + 15) / 16 * 16);
			std::memset(snap->trackLengths, std::max(length, 16), sizeof(snap->trackLengths));
			for (int t = 0; t < NUM_TRACKS; t++) {
				for (const Placed& placed : trackNotes[t]) {
					int a = std::max(placed.note.start, chunkStart), b = std::min(placed.note.end, chunkEnd);
					if (a >= b)
						continue;
					int l = laneOf(t, placed.voice);
					snap->notes[l][a - chunkStart] = placed.note.row;
					snap->lengths[l][a - chunkStart] = b - a;
					snap->ties[l][a - chunkStart] = placed.note.end > chunkEnd;
					snap->mods[l][a - chunkStart] = placed.note.mod;
				}
			}
			int index = -1;
			for (size_t i = 0; i < distinct.size() && index < 0; i++)
				if (std::memcmp(distinct[i].get(), snap.get(), sizeof(PatternSnapshot)) == 0)
					index = i;
			if (index < 0) {
				if ((int) distinct.size() >= NUM_PATTERNS) {
					cutShort = true;
					break;
				}
				distinct.push_back(std::move(snap));
				index = distinct.size() - 1;
			}
			if ((int) slots.size() >= MAX_SONG_LENGTH) {
				cutShort = true;
				break;
			}
			slots.push_back(index);
		}

		// Replace everything
		running = false;
		std::memset(notes, -1, sizeof(notes));
		std::memset(lengths, 1, sizeof(lengths));
		std::memset(ties, 0, sizeof(ties));
		std::memset(mods, DEFAULT_MOD, sizeof(mods));
		std::memset(stepMods, DEFAULT_STEP_MOD, sizeof(stepMods));
		std::memset(trackLengths, DEFAULT_TRACK_LENGTH, sizeof(trackLengths));
		for (size_t i = 0; i < distinct.size(); i++)
			loadPattern(i, *distinct[i]);
		songLength = slots.size();
		for (int i = 0; i < songLength; i++)
			song[i] = slots[i];
		songCursor = 0;
		for (int t = 0; t < NUM_TRACKS; t++) {
			voices[t] = trackVoices[t];
			spreadVoices[t] = false;
			modMode[t] = MOD_PER_NOTE;
			modRange[t] = 0;
			modDefault[t] = DEFAULT_MOD;
			muted[t] = false;
		}
		selectedTrack = 0;
		routeOutputs();
		for (size_t i = 0; i < SCALES.size(); i++)
			if (std::string(SCALES[i].name) == "Chromatic")
				params[SCALE_PARAM].setValue(i);
		params[ROOT_PARAM].setValue(0.f);
		params[TEMPO_PARAM].setValue(clamp((float) midi.bpm, 30.f, 300.f));
		params[PATTERN_PARAM].setValue(1.f);
		params[MODE_PARAM].setValue(1.f);  // song, looping
		songName = name;
		viewStart = 0;
		// Show track 1's lowest notes
		int lowest = numRows();
		for (const Placed& placed : trackNotes[0])
			lowest = std::min(lowest, placed.note.row);
		scrollRow = (lowest < numRows()) ? clamp(lowest - 2, 0, maxScroll()) : defaultScroll();
		resetPlayback();

		std::string report;
		if (droppedParts > 0)
			report += string::f("Only the first %d of the %d parts were imported.\n", NUM_TRACKS, NUM_TRACKS + droppedParts);
		if (droppedNotes > 0)
			report += string::f("%d notes were left out where a part plays more than %d at once.\n", droppedNotes, MAX_VOICES);
		if (outOfRange > 0)
			report += string::f("%d notes were outside the grid's 8 octaves.\n", outOfRange);
		if (cutShort)
			report += string::f("The end was cut off: the piece needs more than %d different 4-bar patterns or %d song slots.\n", NUM_PATTERNS, MAX_SONG_LENGTH);
		return report;
	}

	// ---- Module overrides ----

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		clearAll();
	}

	void process(const ProcessArgs& args) override {
		const double dt = args.sampleTime;

		// Transport
		if (runButton.process(params[RUN_PARAM].getValue() > 0.f))
			running = !running;
		bool runEdge = runTrigger.process(inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f);
		if (runInputGate && inputs[RUN_INPUT].isConnected())
			running = runTrigger.isHigh();
		else if (runEdge)
			running = !running;
		if (running != prevRunning) {
			runPulse.trigger(1e-3f);
			prevRunning = running;
			// Starting in pattern mode plays the pattern shown in the grid. If something else was
			// playing before (an earlier SEQ pattern, or a song), start it from step 1.
			if (running && !playingSong() && playingPattern != editPattern())
				resetPlayback();
			// Stopping a song leaves the pattern that was playing in the grid: the grid shows
			// SEQ while stopped, so SEQ turns to it.
			if (!running && playingSong())
				params[PATTERN_PARAM].setValue(playingPattern + 1);
		}

		bool resetPressed = resetButton.process(params[RESET_PARAM].getValue() > 0.f);
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f) || resetPressed) {
			resetPlayback();
			resetPulse.trigger(1e-3f);
			// A clock edge that arrived just before the reset (e.g. from another sequencer
			// whose reset is a sample late) counts as the first step.
			if (inputs[CLOCK_INPUT].isConnected() && running && extClockTimer < 1e-3)
				stepForward();
		}

		// PAT IN: move on to the next pattern at the next step. Handled before the clock so a
		// trigger arriving with a clock edge takes effect on that edge.
		if (patternTrigger.process(inputs[PATTERN_INPUT].getVoltage(), 0.1f, 1.f)) {
			patternJumpPending = true;
			// Arriving just after a step was taken (a sample late): redo that step as the
			// first step of the next pattern.
			if (running && step >= 0 && stepTimer < 1e-3) {
				step--;
				stepForward();
			}
		}

		// Track selection, mutes, song editing, grid scrolling
		for (int t = 0; t < NUM_TRACKS; t++) {
			// On a track whose jacks send another track's voice, these act on that track
			if (trackButtons[t].process(params[TRACK_PARAM + t].getValue() > 0.f))
				selectedTrack = ownerTrack[t];
			if (muteButtons[t].process(params[MUTE_PARAM + t].getValue() > 0.f))
				muted[ownerTrack[t]] = !muted[ownerTrack[t]];
		}
		if (insertButton.process(params[SONG_INSERT_PARAM].getValue() > 0.f))
			songEdit("insert song slot", [&]() { songInsert(editPattern(), false); });
		if (addButton.process(params[SONG_ADD_PARAM].getValue() > 0.f))
			songEdit("add song slot", [&]() { songInsert(editPattern(), true); });
		if (deleteButton.process(params[SONG_DELETE_PARAM].getValue() > 0.f))
			songEdit("delete song slot", [&]() { songDelete(); });
		for (int id = SONG_DUPLICATE_PARAM; id <= TRACK_CLEAR_PARAM; id++) {
			if (!actionButtons[id - SONG_DUPLICATE_PARAM].process(params[id].getValue() > 0.f))
				continue;
			switch (id) {
				// (DUP, SET and EDIT need a slot at the cursor)
				case SONG_DUPLICATE_PARAM:
					if (songLength > 0)
						songEdit("duplicate song slot", [&]() { songInsert(song[songCursor]); });
					break;
				case SONG_SET_PARAM:
					if (songLength > 0)
						songEdit("set song slot", [&]() { song[songCursor] = editPattern(); });
					break;
				case SONG_EDIT_PARAM:
					if (songLength > 0)
						params[PATTERN_PARAM].setValue(song[songCursor] + 1);
					break;
				case SONG_CLEAR_PARAM: songEdit("clear song", [&]() { songClear(); }); break;
				case PATTERN_COPY_PARAM: copyPattern(); break;
				case PATTERN_PASTE_PARAM: patternEdit("paste pattern", viewPattern(), [&]() { pastePattern(); }); break;
				case PATTERN_CLEAR_PARAM: patternEdit("clear pattern", viewPattern(), [&]() { clearPattern(); }); break;
				case TRACK_COPY_PARAM: copyTrack(); break;
				case TRACK_PASTE_PARAM: patternEdit("paste track", viewPattern(), [&]() { pasteTrack(); }); break;
				case TRACK_CLEAR_PARAM: patternEdit("clear track", viewPattern(), [&]() { clearTrack(); }); break;
			}
		}
		if (scrollUpButton.process(params[SCROLL_UP_PARAM].getValue() > 0.f))
			scroll(1);
		if (scrollDownButton.process(params[SCROLL_DOWN_PARAM].getValue() > 0.f))
			scroll(-1);
		// Keep the view valid when the scale (and so the row count) changes
		scrollRow = clamp(scrollRow, 0, maxScroll());

		// Clock
		bool tick = false;
		extClockTimer += dt;
		if (inputs[CLOCK_INPUT].isConnected()) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				// Measure the incoming period so gate length can follow it
				if (extClockTimer > 1e-3 && extClockTimer < 10.0)
					stepPeriod = extClockTimer;
				extClockTimer = 0.0;
				if (running)
					tick = true;
			}
		}
		else {
			// Tempo is in quarter notes; each step is a 16th note
			stepPeriod = 60.0 / (params[TEMPO_PARAM].getValue() * 4.0);
			if (running) {
				phase += dt / stepPeriod;
				if (phase >= 1.0) {
					phase -= 1.0;
					if (phase >= 1.0)
						phase = 0.0;
					tick = true;
				}
			}
		}

		if (tick) {
			stepForward();
		}
		else {
			stepTimer += dt;
		}

		// Outputs
		// Gate length: where each note releases within its last step. At 100% the gate stays high
		// into the next note, so notes that follow each other directly are tied (legato).
		float gateLength = std::round(params[GATE_LENGTH_PARAM].getValue()) / GATE_STEPS;
		// How far through the step playback is, for per-step MOD gliding (held while stopped)
		if (running && stepPeriod > 0.0)
			modPhase = clamp((float) (stepTimer / stepPeriod), 0.f, 1.f);
		bool inGate = stepTimer < gateLength * stepPeriod || (gateLength >= 1.f && running);
		for (int t = 0; t < NUM_TRACKS; t++) {
			if (isVoiceOutput(t)) {
				// One voice of an earlier poly track (already worked out above), in mono
				int l = laneOf(ownerTrack[t], ownerVoice[t]);
				outputs[GATE_OUTPUT + t].setChannels(1);
				outputs[CV_OUTPUT + t].setChannels(1);
				outputs[MOD_OUTPUT + t].setChannels(1);
				outputs[CV_OUTPUT + t].setVoltage(cv[l]);
				outputs[GATE_OUTPUT + t].setVoltage(gates[l] ? 10.f : 0.f);
				outputs[MOD_OUTPUT + t].setVoltage(modOut[l]);
				trackGate[t] = gates[l];
				continue;
			}
			// A poly track sends one channel per voice on each of its jacks
			int channels = voiceCount(t);
			outputs[GATE_OUTPUT + t].setChannels(channels);
			outputs[CV_OUTPUT + t].setChannels(channels);
			outputs[MOD_OUTPUT + t].setChannels(channels);
			trackGate[t] = false;
			for (int v = 0; v < channels; v++) {
				int l = laneOf(t, v);
				// CV and MOD hold the last note's values between notes. In per-step mode MOD
				// instead follows the value of the step the track is on.
				if (holdLeft[l] > 0)
					cv[l] = rowToVoltage(holdRow[l]);
				// Per-step MOD doesn't depend on notes or gates: it flows from step to step
				if (modMode[t] == MOD_PER_STEP) {
					if (trackStep[t] >= 0) {
						float value = stepMods[playingPattern][t][trackStep[t]];
						if (modSmooth[t] && modNextStep[t] >= 0) {
							float next = stepMods[modNextPattern[t]][t][modNextStep[t]];
							value += (next - value) * modPhase;
						}
						modOut[l] = modVoltage(t, value);
					}
				}
				else if (holdLeft[l] > 0) {
					modOut[l] = modVoltage(t, holdMod[l]);
				}
				// A long note holds its gate through every step but its last, where it releases
				// at the gate length (a tied note holds on to the end). Stopping the transport
				// ends a held note.
				bool lastStepHeld = holdTie[l] ? running : inGate;
				bool held = holdLeft[l] > 1 ? running : (holdLeft[l] == 1 && lastStepHeld);
				gates[l] = held && !muted[t];
				// This track's rings follow its voices, except those spread to other tracks' jacks
				bool sentElsewhere = v > 0 && t + v < NUM_TRACKS && ownerTrack[t + v] == t;
				trackGate[t] = trackGate[t] || (gates[l] && !sentElsewhere);
				outputs[CV_OUTPUT + t].setVoltage(cv[l], v);
				outputs[GATE_OUTPUT + t].setVoltage(gates[l] ? 10.f : 0.f, v);
				outputs[MOD_OUTPUT + t].setVoltage(modOut[l], v);
			}
		}
		bool clockHigh = running && step >= 0 && stepTimer < stepPeriod * 0.5;
		outputs[CLOCK_OUTPUT].setVoltage(clockHigh ? 10.f : 0.f);
		bool runPulseHigh = runPulse.process(args.sampleTime);
		outputs[RUN_OUTPUT].setVoltage((runOutputGate ? running : runPulseHigh) ? 10.f : 0.f);
		outputs[RESET_OUTPUT].setVoltage(resetPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[EOP_OUTPUT].setVoltage(eopPulse.process(args.sampleTime) ? 10.f : 0.f);

		// Lights
		if (lightDivider.process()) {
			float lightTime = args.sampleTime * lightDivider.getDivision();
			lights[RUN_LIGHT].setBrightness(running);
			lights[SONG_LIGHT + 0].setBrightness(songMode() && !songOnce());
			lights[SONG_LIGHT + 1].setBrightness(songOnce());
			lights[MOD_VIEW_LIGHT].setBrightness(modView());
			for (int t = 0; t < NUM_TRACKS; t++) {
				lights[TRACK_LIGHT + t].setBrightness(ownerTrack[t] == selectedTrack ? 1.f : 0.12f);
				lights[MUTE_LIGHT + t].setBrightness(muted[t]);
				lights[GATE_LIGHT + t].setBrightnessSmooth(trackGate[t], lightTime);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "running", json_boolean(running));
		json_object_set_new(rootJ, "runOutputGate", json_boolean(runOutputGate));
		json_object_set_new(rootJ, "runInputGate", json_boolean(runInputGate));
		json_object_set_new(rootJ, "selectedTrack", json_integer(selectedTrack));
		json_object_set_new(rootJ, "scrollRow", json_integer(scrollRow));
		json_object_set_new(rootJ, "songCursor", json_integer(songCursor));

		json_t* mutedJ = json_array();
		for (int t = 0; t < NUM_TRACKS; t++)
			json_array_append_new(mutedJ, json_boolean(muted[t]));
		json_object_set_new(rootJ, "muted", mutedJ);

		json_t* songJ = json_array();
		for (int i = 0; i < songLength; i++)
			json_array_append_new(songJ, json_integer(song[i]));
		json_object_set_new(rootJ, "song", songJ);

		json_object_set_new(rootJ, "viewStart", json_integer(viewStart));
		json_object_set_new(rootJ, "followPlayback", json_boolean(followPlayhead));
		json_object_set_new(rootJ, "songName", json_string(songName.c_str()));

		json_t* modRangesJ = json_array();
		json_t* modDefaultsJ = json_array();
		json_t* modModesJ = json_array();
		json_t* modSmoothJ = json_array();
		json_t* voicesJ = json_array();
		json_t* spreadJ = json_array();
		for (int t = 0; t < NUM_TRACKS; t++) {
			json_array_append_new(spreadJ, json_boolean(spreadVoices[t]));
			json_array_append_new(modRangesJ, json_integer(modRange[t]));
			json_array_append_new(modDefaultsJ, json_integer(modDefault[t]));
			json_array_append_new(modModesJ, json_integer(modMode[t]));
			json_array_append_new(modSmoothJ, json_boolean(modSmooth[t]));
			json_array_append_new(voicesJ, json_integer(voiceCount(t)));
		}
		json_object_set_new(rootJ, "modRanges", modRangesJ);
		json_object_set_new(rootJ, "modDefaults", modDefaultsJ);
		json_object_set_new(rootJ, "modModes", modModesJ);
		json_object_set_new(rootJ, "modSmooth", modSmoothJ);
		json_object_set_new(rootJ, "trackVoices", voicesJ);
		json_object_set_new(rootJ, "spreadVoices", spreadJ);

		// Per-step MOD values: 2-digit hex per step, up to the last one that isn't the default
		json_t* stepModsJ = json_array();
		for (int p = 0; p < NUM_PATTERNS; p++) {
			json_t* tracksJ = json_array();
			for (int t = 0; t < NUM_TRACKS; t++) {
				int last = -1;
				for (int i = 0; i < NUM_STEPS; i++)
					if (stepMods[p][t][i] != DEFAULT_STEP_MOD)
						last = i;
				json_array_append_new(tracksJ, json_string(toHex(stepMods[p][t], last + 1, MOD_MAX).c_str()));
			}
			json_array_append_new(stepModsJ, tracksJ);
		}
		json_object_set_new(rootJ, "stepMods", stepModsJ);

		// Each lane is stored as strings of 2-digit hex values per step, up to its last note:
		// rows ("--" = off), note lengths, ties ("0"/"1" per step, up to the last tied note) and
		// MOD values. A track's first voice goes in "patterns", "noteLengths", "noteTies" and
		// "noteMods" ([pattern][track]); the other voices of poly tracks, where they have notes, in
		// "voiceNotes" as {pattern, track, voice, notes, lengths, ties, mods}.
		auto laneJson = [&](int p, int l, std::string* rows, std::string* lens, std::string* tieStr, std::string* modStr) {
			int last = -1, lastTie = -1;
			for (int i = 0; i < NUM_STEPS; i++) {
				if (notes[p][l][i] >= 0)
					last = i;
				if (notes[p][l][i] >= 0 && ties[p][l][i])
					lastTie = i;
			}
			for (int i = 0; i <= last; i++) {
				int8_t n = notes[p][l][i];
				*rows += (n < 0) ? "--" : string::f("%02x", n);
			}
			*lens = toHex(lengths[p][l], last + 1, NUM_STEPS);
			*modStr = toHex(mods[p][l], last + 1, MOD_MAX);
			for (int i = 0; i <= lastTie; i++)
				*tieStr += (notes[p][l][i] >= 0 && ties[p][l][i]) ? '1' : '0';
			return last >= 0;
		};
		json_t* patternsJ = json_array();
		json_t* noteLengthsJ = json_array();
		json_t* noteTiesJ = json_array();
		json_t* noteModsJ = json_array();
		json_t* trackLengthsJ = json_array();
		json_t* voiceNotesJ = json_array();
		for (int p = 0; p < NUM_PATTERNS; p++) {
			json_t* tracksJ = json_array();
			json_t* tracksLenJ = json_array();
			json_t* tracksTieJ = json_array();
			json_t* tracksModJ = json_array();
			json_t* trackLenJ = json_array();
			for (int t = 0; t < NUM_TRACKS; t++) {
				std::string rows, lens, tieStr, modStr;
				laneJson(p, laneOf(t, 0), &rows, &lens, &tieStr, &modStr);
				json_array_append_new(tracksJ, json_string(rows.c_str()));
				json_array_append_new(tracksLenJ, json_string(lens.c_str()));
				json_array_append_new(tracksTieJ, json_string(tieStr.c_str()));
				json_array_append_new(tracksModJ, json_string(modStr.c_str()));
				json_array_append_new(trackLenJ, json_integer(trackLength(p, t)));
				for (int v = 1; v < MAX_VOICES; v++) {
					std::string vRows, vLens, vTies, vMods;
					if (!laneJson(p, laneOf(t, v), &vRows, &vLens, &vTies, &vMods))
						continue;
					json_t* voiceJ = json_object();
					json_object_set_new(voiceJ, "pattern", json_integer(p));
					json_object_set_new(voiceJ, "track", json_integer(t));
					json_object_set_new(voiceJ, "voice", json_integer(v));
					json_object_set_new(voiceJ, "notes", json_string(vRows.c_str()));
					json_object_set_new(voiceJ, "lengths", json_string(vLens.c_str()));
					json_object_set_new(voiceJ, "ties", json_string(vTies.c_str()));
					json_object_set_new(voiceJ, "mods", json_string(vMods.c_str()));
					json_array_append_new(voiceNotesJ, voiceJ);
				}
			}
			json_array_append_new(patternsJ, tracksJ);
			json_array_append_new(noteLengthsJ, tracksLenJ);
			json_array_append_new(noteTiesJ, tracksTieJ);
			json_array_append_new(noteModsJ, tracksModJ);
			json_array_append_new(trackLengthsJ, trackLenJ);
		}
		json_object_set_new(rootJ, "patterns", patternsJ);
		json_object_set_new(rootJ, "noteLengths", noteLengthsJ);
		json_object_set_new(rootJ, "noteTies", noteTiesJ);
		json_object_set_new(rootJ, "noteMods", noteModsJ);
		json_object_set_new(rootJ, "voiceNotes", voiceNotesJ);
		json_object_set_new(rootJ, "trackLengths", trackLengthsJ);
		return rootJ;
	}

	/** Loads a lane's rows / lengths / ties / MOD values from their strings (any may be null). */
	void laneFromStrings(int p, int l, const char* rows, const char* lens, const char* tieStr, const char* modStr) {
		if (rows) {
			std::string s = rows;
			for (int i = 0; i < NUM_STEPS && (size_t) (i * 2 + 1) < s.size(); i++) {
				std::string hex = s.substr(i * 2, 2);
				if (hex != "--")
					notes[p][l][i] = (int8_t) clamp((int) std::strtol(hex.c_str(), nullptr, 16), 0, 127);
			}
		}
		if (lens)
			fromHex(lens, lengths[p][l], 1, NUM_STEPS);
		if (tieStr) {
			std::string s = tieStr;
			for (int i = 0; i < NUM_STEPS && (size_t) i < s.size(); i++)
				ties[p][l][i] = s[i] == '1';
		}
		if (modStr)
			fromHex(modStr, mods[p][l], 0, MOD_MAX);
	}

	void dataFromJson(json_t* rootJ) override {
		if (json_t* j = json_object_get(rootJ, "running"))
			running = json_boolean_value(j);
		// Loading a patch must not emit a run trigger (it would toggle a following sequencer)
		prevRunning = running;
		if (json_t* j = json_object_get(rootJ, "runOutputGate"))
			runOutputGate = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "runInputGate"))
			runInputGate = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "selectedTrack"))
			selectedTrack = clamp((int) json_integer_value(j), 0, NUM_TRACKS - 1);
		for (int t = 0; t < NUM_TRACKS; t++) {
			json_t* j = json_array_get(json_object_get(rootJ, "trackVoices"), t);
			voices[t] = clamp(j ? (int) json_integer_value(j) : 1, 1, MAX_VOICES);
			json_t* spreadJ = json_array_get(json_object_get(rootJ, "spreadVoices"), t);
			spreadVoices[t] = spreadJ && json_boolean_value(spreadJ);
		}

		if (json_t* mutedJ = json_object_get(rootJ, "muted")) {
			for (int t = 0; t < NUM_TRACKS; t++) {
				json_t* j = json_array_get(mutedJ, t);
				muted[t] = j && json_boolean_value(j);
			}
		}

		if (json_t* songJ = json_object_get(rootJ, "song")) {
			int len = std::min((int) json_array_size(songJ), MAX_SONG_LENGTH);
			for (int i = 0; i < len; i++)
				song[i] = clamp((int) json_integer_value(json_array_get(songJ, i)), 0, NUM_PATTERNS - 1);
			songLength = len;
		}
		json_t* songNameJ = json_object_get(rootJ, "songName");
		songName = (songNameJ && json_is_string(songNameJ)) ? json_string_value(songNameJ) : "";
		if (json_t* j = json_object_get(rootJ, "songCursor"))
			songCursor = clamp((int) json_integer_value(j), 0, std::max(songLength - 1, 0));

		// Note data. Patches from before MOD existed get the default MOD value for every note;
		// older patches still may have "lengths" (1 hex digit, length - 1) or no lengths at all
		// (every note 1 step).
		std::memset(notes, -1, sizeof(notes));
		std::memset(lengths, 1, sizeof(lengths));
		std::memset(ties, 0, sizeof(ties));
		std::memset(mods, DEFAULT_MOD, sizeof(mods));
		auto trackString = [&](const char* key, int p, int t) -> const char* {
			json_t* j = json_array_get(json_array_get(json_object_get(rootJ, key), p), t);
			return (j && json_is_string(j)) ? json_string_value(j) : nullptr;
		};
		for (int p = 0; p < NUM_PATTERNS; p++) {
			for (int t = 0; t < NUM_TRACKS; t++) {
				int l = laneOf(t, 0);
				laneFromStrings(p, l, trackString("patterns", p, t), trackString("noteLengths", p, t),
					trackString("noteTies", p, t), trackString("noteMods", p, t));
				if (!json_object_get(rootJ, "noteLengths")) {
					if (const char* old = trackString("lengths", p, t)) {
						std::string s = old;
						for (int i = 0; i < NUM_STEPS && (size_t) i < s.size(); i++) {
							char digit[2] = {s[i], 0};
							lengths[p][l][i] = clamp((int) std::strtol(digit, nullptr, 16) + 1, 1, NUM_STEPS);
						}
					}
				}
			}
		}
		// Extra voices of poly tracks
		json_t* voiceNotesJ = json_object_get(rootJ, "voiceNotes");
		for (size_t i = 0; i < json_array_size(voiceNotesJ); i++) {
			json_t* voiceJ = json_array_get(voiceNotesJ, i);
			int p = json_integer_value(json_object_get(voiceJ, "pattern"));
			int t = json_integer_value(json_object_get(voiceJ, "track"));
			int v = json_integer_value(json_object_get(voiceJ, "voice"));
			if (p < 0 || p >= NUM_PATTERNS || t < 0 || t >= NUM_TRACKS || v < 1 || v >= MAX_VOICES)
				continue;
			auto str = [&](const char* key) -> const char* {
				json_t* j = json_object_get(voiceJ, key);
				return (j && json_is_string(j)) ? json_string_value(j) : nullptr;
			};
			laneFromStrings(p, laneOf(t, v), str("notes"), str("lengths"), str("ties"), str("mods"));
		}

		for (int t = 0; t < NUM_TRACKS; t++) {
			json_t* rangeJ = json_array_get(json_object_get(rootJ, "modRanges"), t);
			modRange[t] = rangeJ ? clamp((int) json_integer_value(rangeJ), 0, (int) MOD_RANGES.size() - 1) : 0;
			json_t* defaultJ = json_array_get(json_object_get(rootJ, "modDefaults"), t);
			modDefault[t] = defaultJ ? clamp((int) json_integer_value(defaultJ), 0, MOD_MAX) : DEFAULT_MOD;
			json_t* modeJ = json_array_get(json_object_get(rootJ, "modModes"), t);
			modMode[t] = (modeJ && json_integer_value(modeJ) == MOD_PER_STEP) ? MOD_PER_STEP : MOD_PER_NOTE;
			json_t* smoothJ = json_array_get(json_object_get(rootJ, "modSmooth"), t);
			modSmooth[t] = !smoothJ || json_boolean_value(smoothJ);
		}
		std::memset(stepMods, DEFAULT_STEP_MOD, sizeof(stepMods));
		for (int p = 0; p < NUM_PATTERNS; p++)
			for (int t = 0; t < NUM_TRACKS; t++)
				if (const char* s = trackString("stepMods", p, t))
					fromHex(s, stepMods[p][t], 0, MOD_MAX);

		// Patches saved before track lengths existed are 16 steps throughout
		std::memset(trackLengths, DEFAULT_TRACK_LENGTH, sizeof(trackLengths));
		if (json_t* trackLengthsJ = json_object_get(rootJ, "trackLengths")) {
			for (int p = 0; p < NUM_PATTERNS; p++) {
				json_t* tracksJ = json_array_get(trackLengthsJ, p);
				if (!tracksJ)
					break;
				for (int t = 0; t < NUM_TRACKS; t++)
					if (json_t* j = json_array_get(tracksJ, t))
						setTrackLength(p, t, json_integer_value(j));
			}
		}

		// Briefly, poly tracks were groups of neighbouring tracks ("polyVoices"): fold each group
		// into its first track, the other tracks' notes becoming its extra voices
		if (!json_object_get(rootJ, "trackVoices")) {
			for (int t = 0; t < NUM_TRACKS;) {
				json_t* j = json_array_get(json_object_get(rootJ, "polyVoices"), t);
				int n = j ? clamp((int) json_integer_value(j), 1, std::min(4, NUM_TRACKS - t)) : 1;
				// The group's tracks keep sending its voices, as they did
				voices[t] = n;
				spreadVoices[t] = n > 1;
				for (int v = 1; v < n; v++) {
					for (int p = 0; p < NUM_PATTERNS; p++) {
						int from = laneOf(t + v, 0), to = laneOf(t, v);
						std::memcpy(notes[p][to], notes[p][from], sizeof(notes[p][to]));
						std::memcpy(lengths[p][to], lengths[p][from], sizeof(lengths[p][to]));
						std::memcpy(ties[p][to], ties[p][from], sizeof(ties[p][to]));
						std::memcpy(mods[p][to], mods[p][from], sizeof(mods[p][to]));
						clearLane(p, from);
					}
				}
				t += n;
			}
		}

		for (int p = 0; p < NUM_PATTERNS; p++)
			for (int l = 0; l < NUM_LANES; l++)
				normalizeLane(p, l);
		routeOutputs();

		if (json_t* j = json_object_get(rootJ, "scrollRow"))
			scrollRow = clamp((int) json_integer_value(j), 0, maxScroll());
		if (json_t* j = json_object_get(rootJ, "viewStart"))
			setViewStart(json_integer_value(j));
		else if (json_t* j = json_object_get(rootJ, "viewPage"))
			setViewStart(json_integer_value(j) * VIEW_STEPS);  // older patches saved a page
		// Saved as "followPlayback" since following became the default; the older "followPlayhead"
		// key was usually just the old default (off), so it is ignored.
		followPlayhead = true;
		if (json_t* j = json_object_get(rootJ, "followPlayback"))
			followPlayhead = json_boolean_value(j);
		resetPlayback();
	}
};


/** Undo / redo of a note, track, pattern or song edit. */
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

/** Undo / redo of renaming the song. */
struct SongNameAction : history::ModuleAction {
	std::string before, after;

	void undo() override {
		if (GridSequencer* m = dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId)))
			m->songName = before;
	}
	void redo() override {
		if (GridSequencer* m = dynamic_cast<GridSequencer*>(APP->engine->getModule(moduleId)))
			m->songName = after;
	}
};


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


// ---- Widgets ----

static void setFont(NVGcontext* vg, const char* path) {
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system(path));
	if (font && font->handle >= 0)
		nvgFontFaceId(vg, font->handle);
}

static void fillRect(NVGcontext* vg, float x, float y, float w, float h, NVGcolor color, float radius = 0.f) {
	nvgBeginPath(vg);
	if (radius > 0.f)
		nvgRoundedRect(vg, x, y, w, h, radius);
	else
		nvgRect(vg, x, y, w, h);
	nvgFillColor(vg, color);
	nvgFill(vg);
}

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

	int sideAt(Vec pos) {
		if (pos.x < arrowW())
			return -1;
		if (pos.x > box.size.x - arrowW())
			return 1;
		return 0;
	}

	void stepValue(int delta) {
		ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		int minValue = (int) std::round(pq->getMinValue());
		int maxValue = (int) std::round(pq->getMaxValue());
		int count = maxValue - minValue + 1;
		float oldValue = pq->getValue();
		int value = (int) std::round(oldValue) + delta;
		if (wrap)
			value = minValue + ((value - minValue) % count + count) % count;
		else
			value = clamp(value, minValue, maxValue);
		pq->setValue(value);

		history::ParamChange* h = new history::ParamChange;
		h->name = "change " + pq->getLabel();
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = oldValue;
		h->newValue = pq->getValue();
		APP->history->push(h);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
			int side = sideAt(e.pos);
			if (side != 0)
				stepValue(side);
		}
		// Keeps right-click menu and MIDI-map touch behaviour
		ParamWidget::onButton(e);
	}

	void onDoubleClick(const DoubleClickEvent& e) override {
		// Don't reset to default: a quick second click on an arrow is just another step
		e.consume(this);
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (e.scrollDelta.y > 0.f)
			stepValue(1);
		else if (e.scrollDelta.y < 0.f)
			stepValue(-1);
		e.consume(this);
	}

	void onHover(const HoverEvent& e) override {
		hoverSide = sideAt(e.pos);
		ParamWidget::onHover(e);
	}

	void onLeave(const LeaveEvent& e) override {
		hoverSide = 0;
		ParamWidget::onLeave(e);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			ParamWidget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		float w = box.size.x, h = box.size.y, aw = arrowW();
		fillRect(vg, 0, 0, w, h, nvgRGB(0x0e, 0x10, 0x13), 1.5f);

		// Arrows, lit while hovered
		auto arrow = [&](float cx, float dir, bool hot) {
			if (hot)
				fillRect(vg, dir < 0 ? 0.f : w - aw, 0, aw, h, nvgRGB(0x2c, 0x31, 0x3b), 1.5f);
			nvgBeginPath(vg);
			nvgMoveTo(vg, cx + dir * 2.5f, h / 2);
			nvgLineTo(vg, cx - dir * 2.5f, h / 2 - 4.f);
			nvgLineTo(vg, cx - dir * 2.5f, h / 2 + 4.f);
			nvgClosePath(vg);
			nvgFillColor(vg, hot ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0x9c, 0xa3, 0xb0));
			nvgFill(vg);
		};
		arrow(aw / 2, -1.f, hoverSide < 0);
		arrow(w - aw / 2, 1.f, hoverSide > 0);

		// Value
		std::string text;
		ParamQuantity* pq = getParamQuantity();
		if (!labels.empty()) {
			int i = pq ? (int) std::round(pq->getValue() - pq->getMinValue()) : 0;
			text = labels[clamp(i, 0, (int) labels.size() - 1)];
		}
		else if (pq) {
			if (format.empty())
				text = pq->getDisplayValueString();
			else
				text = string::f(format.c_str(), (int) std::round(pq->getValue()));
		}
		else if (!format.empty()) {
			text = string::f(format.c_str(), 1);
		}
		setFont(vg, DISPLAY_FONT);
		// Shrink text that doesn't fit between the arrows
		float fontSize = 10.5f;
		nvgFontSize(vg, fontSize);
		float textW = nvgTextBounds(vg, 0, 0, text.c_str(), NULL, NULL);
		float room = w - 2 * aw - 2.f;
		if (textW > room)
			nvgFontSize(vg, std::max(fontSize * room / textW, 7.f));
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, nvgRGB(0x9c, 0xdc, 0xff));
		nvgText(vg, w / 2, h / 2, text.c_str(), NULL);
	}
};

/** Static text drawn on the panel (Rack does not render SVG text). */
struct PanelLabel : TransparentWidget {
	std::string text;
	float fontSize = 9.5f;
	NVGcolor color = nvgRGB(0xe6, 0xe9, 0xee);

	void draw(const DrawArgs& args) override {
		setFont(args.vg, LABEL_FONT);
		nvgFontSize(args.vg, fontSize);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, color);
		nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
	}
};


/** Song name text box, in the right-click menu and the rename popup. Enter or Escape closes it. */
struct SongNameField : ui::TextField {
	GridSequencer* module = nullptr;
	// Name when the box opened; closing it records the rename as one undo step
	std::string oldName;

	void start(GridSequencer* m) {
		module = m;
		oldName = m->songName;
		setText(m->songName);
	}

	~SongNameField() {
		if (module && module->songName != oldName) {
			SongNameAction* h = new SongNameAction;
			h->moduleId = module->id;
			h->name = "rename song";
			h->before = oldName;
			h->after = module->songName;
			APP->history->push(h);
		}
	}

	void onChange(const ChangeEvent& e) override {
		if (module)
			module->songName = text;
		ui::TextField::onChange(e);
	}

	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER || e.key == GLFW_KEY_ESCAPE)) {
			if (ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>())
				overlay->requestDelete();
			e.consume(this);
			return;
		}
		ui::TextField::onSelectKey(e);
	}
};


/**
 * Top line of the display. Left half: the song's name (click it to rename). Right half: where
 * playback is: transport (and song slot), the playing pattern, and the bar and beat in it.
 */
struct InfoDisplay : Widget {
	GridSequencer* module = nullptr;
	// Right edge of the name as last drawn: clicks left of it rename the song
	float nameEnd = 0.f;

	void onButton(const ButtonEvent& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && e.pos.x < nameEnd + 8.f) {
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Song name"));
			SongNameField* field = new SongNameField;
			field->box.size.x = 260.f;
			field->placeholder = "Untitled";
			field->start(module);
			menu->addChild(field);
			APP->event->setSelectedWidget(field);
			field->selectAll();
			e.consume(this);
			return;
		}
		Widget::onButton(e);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x12, 0x14, 0x18), 2.f);
		float cy = box.size.y / 2;

		// Right half: transport (and song slot), playing pattern, bar.beat within the pattern
		bool running = module && module->running;
		std::string transport = "STOP";
		if (module && running)
			transport = module->playingSong()
				? string::f("%s %02d/%02d", module->songOnce() ? "ONCE" : "SONG", module->songPos + 1, module->songLength)
				: "LOOP";
		int playing = module ? module->playingPattern : 0;
		int step = module ? module->step : -1;
		std::string bar = (running && step >= 0) ? string::f("BAR %d.%d", step / 16 + 1, step % 16 / 4 + 1) : "BAR -.-";
		setFont(vg, DISPLAY_FONT);
		nvgFontSize(vg, 13.f);
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		float x = box.size.x - 6.f;
		nvgFillColor(vg, running ? nvgRGB(0xf0, 0xf2, 0xf6) : nvgRGB(0x8a, 0x90, 0x9a));
		x = x - nvgTextBounds(vg, 0, 0, bar.c_str(), NULL, NULL);
		nvgText(vg, box.size.x - 6.f, cy, bar.c_str(), NULL);
		std::string pattern = string::f("P%02d   ", playing + 1);
		// Green while the grid follows the song (then it can differ from SEQ)
		nvgFillColor(vg, (module && module->following()) ? nvgRGB(0x8c, 0xf0, 0x9c) : nvgRGB(0x9c, 0xdc, 0xff));
		nvgText(vg, x, cy, pattern.c_str(), NULL);
		x -= nvgTextBounds(vg, 0, 0, pattern.c_str(), NULL, NULL);
		nvgFillColor(vg, running ? nvgRGB(0x8c, 0xf0, 0x9c) : nvgRGB(0xff, 0x80, 0x70));
		nvgText(vg, x, cy, (transport + "   ").c_str(), NULL);

		// Left half: the song's name, shortened to fit
		setFont(vg, LABEL_FONT);
		nvgFontSize(vg, 15.f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		bool named = module && !module->songName.empty();
		std::string name = module ? module->displayName() : "Untitled";
		float room = box.size.x / 2 - 12.f;
		while (name.size() > 4 && nvgTextBounds(vg, 0, 0, name.c_str(), NULL, NULL) > room)
			name = name.substr(0, name.size() - 4) + "...";
		nvgFillColor(vg, named ? nvgRGB(0xf6, 0xe7, 0xb0) : nvgRGB(0x8a, 0x90, 0x9a));
		nameEnd = nvgText(vg, 6.f, cy, name.c_str(), NULL);
	}
};


/** Current tempo next to the TEMPO knob (measured from CLK IN when it is patched). */
struct TempoDisplay : Widget {
	GridSequencer* module = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x0e, 0x10, 0x13), 1.5f);

		bool ext = module && module->inputs[GridSequencer::CLOCK_INPUT].isConnected();
		float bpm = module ? 60.f / (module->stepPeriod * 4.f) : 120.f;
		std::string text = string::f("%.0f", bpm);
		setFont(vg, DISPLAY_FONT);
		nvgFontSize(vg, 14.f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		// Amber while following an external clock
		nvgFillColor(vg, ext ? nvgRGB(0xff, 0xc8, 0x60) : nvgRGB(0x9c, 0xdc, 0xff));
		nvgText(vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
	}
};


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
	bool cellAt(Vec pos, int* stepIndex, int* row) {
		float x = pos.x - labelW();
		if (x < 0.f || x >= box.size.x - labelW() || pos.y < 0.f || pos.y >= box.size.y)
			return false;
		int col = (int) (x / cellW());
		*stepIndex = module->viewStart + col;
		int visibleRow = VISIBLE_ROWS - 1 - (int) (pos.y / cellH());
		*row = module->scrollRow + visibleRow;
		return col >= 0 && col < VIEW_STEPS && *row >= 0 && *row < module->numRows();
	}

	/** Like cellAt during a drag: may run past the page edge, clamped to the track's length. */
	void dragCellAt(Vec pos, int* stepIndex, int* row) {
		int col = (int) std::floor((pos.x - labelW()) / cellW());
		int end = module->trackLength(dragPattern, dragLead);
		*stepIndex = clamp(dragViewStart + col, 0, end - 1);
		int visibleRow = VISIBLE_ROWS - 1 - (int) std::floor(pos.y / cellH());
		visibleRow = clamp(visibleRow, 0, VISIBLE_ROWS - 1);
		*row = clamp(module->scrollRow + visibleRow, 0, module->numRows() - 1);
	}

	bool poly() {
		return module->voiceCount(dragLead) > 1;
	}

	/**
	 * Puts the note being moved at `start` / `row`: on an ordinary track it trims whatever it
	 * lands on; on a poly track it goes on its own voice if that is free there, else on another
	 * free voice. Returns false if no voice is free.
	 */
	bool placeMoved(int start, int row) {
		int track = dragTrack;
		if (poly()) {
			track = module->freeVoice(dragPattern, dragLead, start, dragLength, dragTrack);
			if (track < 0)
				return false;
		}
		module->placeNote(dragPattern, track, start, row, dragLength, dragTie, dragMod);
		curTrack = track;
		curStart = start;
		curRow = row;
		return true;
	}

	/**
	 * Moves every note of the edited poly track that starts on the dragged note's step `delta`
	 * scale degrees, as far as the grid allows.
	 */
	void shiftChord(int delta) {
		for (int v = 0; v < module->voiceCount(dragLead); v++) {
			int n = module->notes[dragPattern][laneOf(dragLead, v)][dragStart];
			if (n >= 0)
				delta = clamp(delta, -n, module->numRows() - 1 - n);
		}
		for (int v = 0; v < module->voiceCount(dragLead); v++) {
			int8_t* n = &module->notes[dragPattern][laneOf(dragLead, v)][dragStart];
			if (*n >= 0)
				*n += delta;
		}
		chordShift = delta;
		curRow = dragRow + delta;
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			Widget::onButton(e);
			return;
		}
		if (e.action == GLFW_PRESS) {
			dragMode = DRAG_NONE;
			chordShift = 0;
			int col, row;
			// Steps past the track's end are inactive and can't be edited
			if (cellAt(e.pos, &col, &row) && col < module->trackLength(module->viewPattern(), module->selectedTrack)) {
				// Edits stay on this pattern even if song playback moves the grid on
				dragPattern = module->viewPattern();
				dragLead = module->selectedTrack;
				int track, s;
				bool onNote = module->noteAt(dragPattern, dragLead, col, row, &track, &s);

				// Shift+click on a note toggles its tie
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					if (onNote) {
						uint8_t* tie = &module->ties[dragPattern][track][s];
						module->patternEdit(*tie ? "untie note" : "tie note", dragPattern, [&]() {
							*tie = !*tie;
						});
					}
					e.consume(this);
					return;
				}

				dragViewStart = module->viewStart;
				module->savePattern(dragPattern, undoBefore);
				dragPos = e.pos;
				dragMoved = false;
				lastCol = col;
				lastRow = row;

				if (onNote) {
					dragTrack = track;
					dragStart = s;
					dragRow = row;
					dragLength = module->lengths[dragPattern][track][s];
					dragTie = module->ties[dragPattern][track][s];
					dragMod = module->mods[dragPattern][track][s];
					grabOffset = col - s;
					// The right 40% of a note's last step is its stretch handle
					float xInCell = e.pos.x - labelW() - (col - dragViewStart) * cellW();
					bool onEnd = col == s + dragLength - 1 && xInCell > cellW() * 0.6f;
					dragMode = onEnd ? DRAG_RESIZE : DRAG_MOVE;
				}
				else {
					// On a poly track the new note goes on the first free voice; none free, no note
					dragTrack = poly() ? module->freeVoice(dragPattern, dragLead, col, 1) : laneOf(dragLead, 0);
					if (dragTrack >= 0) {
						dragMode = DRAG_CREATE;
						dragStart = col;
						dragRow = row;
						dragLength = 1;
						dragTie = false;
						dragMod = module->modDefault[dragLead];
						module->placeNote(dragPattern, dragTrack, col, row, 1, false, dragMod);
					}
				}
				curTrack = dragTrack;
				curStart = dragStart;
				curRow = dragRow;
			}
		}
		e.consume(this);
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || dragMode == DRAG_NONE || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		int col, row;
		dragCellAt(dragPos, &col, &row);
		if (col == lastCol && row == lastRow)
			return;
		lastCol = col;
		lastRow = row;
		dragMoved = true;

		module->loadPattern(dragPattern, undoBefore);
		if (dragMode == DRAG_CREATE || dragMode == DRAG_RESIZE) {
			int length = std::max(1, col - dragStart + 1);
			if (poly()) {
				// The note stays on its voice, so it can only grow up to that voice's next note
				module->removeNote(dragPattern, dragTrack, dragStart);
				length = std::min(length, module->freeLength(dragPattern, dragTrack, dragStart));
			}
			module->placeNote(dragPattern, dragTrack, dragStart, dragRow, length, dragTie, dragMod);
			if (dragMode == DRAG_RESIZE && poly())
				shiftChord(row - dragRow);
		}
		else {
			module->removeNote(dragPattern, dragTrack, dragStart);
			int start = clamp(col - grabOffset, 0, module->trackLength(dragPattern, dragLead) - 1);
			// Where no voice is free, the note stays at the last place it fitted
			if (!placeMoved(start, row))
				placeMoved(curStart, curRow);
		}
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT || dragMode == DRAG_NONE)
			return;
		// A click on an existing note without dragging deletes it
		std::string name;
		if ((dragMode == DRAG_MOVE || dragMode == DRAG_RESIZE) && !dragMoved) {
			module->loadPattern(dragPattern, undoBefore);
			module->removeNote(dragPattern, dragTrack, dragStart);
			name = "delete note";
		}
		else {
			if (dragMode == DRAG_CREATE)
				name = "draw note";
			else if (dragMode == DRAG_RESIZE)
				name = chordShift != 0 ? "move chord" : "change note length";
			else
				name = "move note";
		}
		module->recordPatternEdit(name, dragPattern, undoBefore);
		dragMode = DRAG_NONE;
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module) {
			// Wheel scrolls pitch; Shift+wheel or a horizontal wheel pans by a beat.
			// Alt+wheel transposes the edited track a degree; Alt+Shift+wheel rotates it a step.
			int mods = APP->window->getMods() & RACK_MOD_MASK;
			bool shift = mods == GLFW_MOD_SHIFT;
			float panDelta = (e.scrollDelta.x != 0.f) ? e.scrollDelta.x : (shift ? e.scrollDelta.y : 0.f);
			float altDelta = (e.scrollDelta.y != 0.f) ? e.scrollDelta.y : e.scrollDelta.x;
			int pattern = module->viewPattern();
			int track = module->selectedTrack;
			if (mods & GLFW_MOD_ALT) {
				// Not mid-drag: the drag restores the pattern it saved when it began
				if (dragMode == DRAG_NONE && altDelta != 0.f) {
					if (mods & GLFW_MOD_SHIFT) {
						module->patternEdit("rotate track", pattern, [&]() {
							module->rotateTrack(pattern, track, altDelta < 0.f ? 1 : -1);
						});
					}
					else {
						module->patternEdit("transpose track", pattern, [&]() {
							module->transposeTrack(pattern, track, altDelta > 0.f ? 1 : -1);
						});
					}
				}
			}
			else if (panDelta != 0.f)
				module->setViewStart(module->viewStart + (panDelta < 0.f ? 4 : -4));
			else if (e.scrollDelta.y > 0.f)
				module->scroll(1);
			else if (e.scrollDelta.y < 0.f)
				module->scroll(-1);
		}
		e.consume(this);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		int scale = module ? module->scaleIndex() : 0;
		int root = module ? module->rootIndex() : 0;
		const std::vector<int>& sc = SCALES[scale].notes;
		int n = sc.size();
		int rows = NUM_OCTAVES * n;
		int scrollRow = module ? module->scrollRow : 4 * n - 5;
		int pattern = module ? module->viewPattern() : 0;
		int track = module ? module->selectedTrack : 0;
		int viewStart = module ? module->viewStart : 0;
		int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;
		float lw = labelW(), cw = cellW(), ch = cellH();

		fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x0e, 0x10, 0x13));
		setFont(vg, DISPLAY_FONT);
		nvgFontSize(vg, 11.5f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		// Row labels and cell backgrounds
		for (int vr = 0; vr < VISIBLE_ROWS; vr++) {
			int row = scrollRow + vr;
			if (row >= rows)
				break;
			float y = box.size.y - (vr + 1) * ch;
			int degree = row % n;
			int semi = root + sc[degree];
			int octave = row / n + semi / 12;
			bool isRoot = degree == 0;

			fillRect(vg, 0.5f, y + 0.5f, lw - 1.f, ch - 1.f, isRoot ? nvgRGB(0x3a, 0x40, 0x4c) : nvgRGB(0x20, 0x23, 0x2a));
			nvgFillColor(vg, isRoot ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xc4, 0xc9, 0xd2));
			std::string label = string::f("%s%d", NOTE_NAMES[semi % 12], octave);
			nvgText(vg, lw / 2, y + ch / 2, label.c_str(), NULL);

			for (int col = 0; col < VIEW_STEPS; col++) {
				bool beatGroup = ((viewStart + col) / 4) % 2 == 0;
				NVGcolor c;
				if (isRoot)
					c = beatGroup ? nvgRGB(0x30, 0x35, 0x40) : nvgRGB(0x29, 0x2d, 0x37);
				else
					c = beatGroup ? nvgRGB(0x22, 0x25, 0x2c) : nvgRGB(0x1b, 0x1e, 0x24);
				fillRect(vg, lw + col * cw + 0.5f, y + 0.5f, cw - 1.f, ch - 1.f, c);
			}
		}

		// Steps past the edited track's end are inactive
		int inactiveCol = clamp(trackLen - viewStart, 0, VIEW_STEPS);
		if (inactiveCol < VIEW_STEPS)
			fillRect(vg, lw + inactiveCol * cw, 0, (VIEW_STEPS - inactiveCol) * cw, box.size.y, nvgRGBA(0x00, 0x00, 0x00, 0xa0));

		if (!module)
			return;

		// Playhead of the edited track
		int playStep = module->trackStep[track];
		if (module->step >= 0 && playStep >= viewStart && playStep < viewStart + VIEW_STEPS && module->playingPattern == pattern) {
			fillRect(vg, lw + (playStep - viewStart) * cw, 0, cw, box.size.y, nvgRGBA(0xff, 0xff, 0xff, 0x22));
		}

		auto rowToY = [&](int row) {
			return box.size.y - (row - scrollRow + 1) * ch;
		};
		auto visible = [&](int row) {
			return row >= scrollRow && row < scrollRow + VISIBLE_ROWS && row < rows;
		};
		// Part of a note's span [s, s + length) in view, as columns [*a, *b)
		auto spanOnPage = [&](int s, int length, int* a, int* b) {
			*a = std::max(s, viewStart) - viewStart;
			*b = std::min(s + length, viewStart + VIEW_STEPS) - viewStart;
			return *a < *b;
		};

		// Ghost notes of the other tracks (all their voices)
		for (int t = 0; t < NUM_TRACKS; t++) {
			if (t == track || module->isVoiceOutput(t))
				continue;
			NVGcolor c = TRACK_COLORS[t];
			c.a = module->muted[t] ? 0.15f : 0.35f;
			int otherLen = module->trackLength(pattern, t);
			for (int v = 0; v < module->voiceCount(t); v++) {
				int l = laneOf(t, v);
				for (int s = 0; s < otherLen; s++) {
					int row = module->notes[pattern][l][s];
					int a, b;
					if (row >= 0 && visible(row) && spanOnPage(s, module->lengths[pattern][l][s], &a, &b))
						fillRect(vg, lw + a * cw + 3.f, rowToY(row) + 3.f, (b - a) * cw - 6.f, ch - 6.f, c, 1.f);
				}
			}
		}

		// Notes of the edited track (every voice of a poly track), with arrows for notes out of view
		for (int voice = laneOf(track, 0); voice < laneOf(track, module->voiceCount(track)); voice++) {
			for (int s = 0; s < NUM_STEPS; s++) {
				int row = module->notes[pattern][voice][s];
				int length = module->lengths[pattern][voice][s];
				int a, b;
				if (row < 0 || !spanOnPage(s, length, &a, &b))
					continue;
				NVGcolor c = TRACK_COLORS[track];
				if (s >= trackLen)
					c.a = 0.25f;  // kept, but past the track's end
				else if (module->muted[track])
					c.a = 0.5f;
				float x = lw + a * cw;
				if (visible(row)) {
					float y = rowToY(row);
					float w = (b - a) * cw;
					fillRect(vg, x + 1.5f, y + 1.5f, w - 3.f, ch - 3.f, c, 1.5f);
					// MOD value: a dark strip along the bottom, as long as the value
					if (s >= viewStart && module->modMode[track] == MOD_PER_NOTE) {
						float modW = (w - 6.f) * module->mods[pattern][voice][s] / (float) MOD_MAX;
						if (modW > 0.f)
							fillRect(vg, x + 3.f, y + ch - 4.5f, modW, 1.5f, nvgRGBA(0x00, 0x00, 0x00, 0x70));
					}
					// Stretch grip on the note's right end, if it ends in view
					if (s + length <= viewStart + VIEW_STEPS) {
						nvgBeginPath(vg);
						for (int i = 0; i < 2; i++) {
							float gx = x + w - 4.5f - i * 2.5f;
							nvgMoveTo(vg, gx, y + ch * 0.3f);
							nvgLineTo(vg, gx, y + ch * 0.7f);
						}
						nvgStrokeColor(vg, nvgRGBA(0x00, 0x00, 0x00, 0x80));
						nvgStrokeWidth(vg, 1.f);
						nvgStroke(vg);
					}
					// Tie: a slur from the note's end over into the next step
					if (module->ties[pattern][voice][s] && s + length < viewStart + VIEW_STEPS) {
						float ex = x + w;
						nvgBeginPath(vg);
						nvgMoveTo(vg, ex - cw * 0.45f, y + ch * 0.3f);
						nvgBezierTo(vg, ex - cw * 0.2f, y - ch * 0.1f, ex + cw * 0.2f, y - ch * 0.1f, ex + cw * 0.45f, y + ch * 0.3f);
						nvgStrokeColor(vg, nvgRGB(0xf0, 0xf2, 0xf6));
						nvgStrokeWidth(vg, 1.5f);
						nvgLineCap(vg, NVG_ROUND);
						nvgStroke(vg);
					}
					// Length readout while the note is being drawn, stretched or moved
					if (dragMode != DRAG_NONE && dragPattern == pattern && curTrack == voice && s == curStart && row == curRow) {
						std::string len = string::f("%d", length);
						nvgFontSize(vg, 11.f);
						nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
						nvgFillColor(vg, nvgRGB(0x10, 0x12, 0x16));
						nvgText(vg, x + 4.f, y + ch / 2, len.c_str(), NULL);
					}
				}
				else {
					bool above = row >= scrollRow + VISIBLE_ROWS;
					float tipY = above ? 1.f : box.size.y - 1.f;
					float baseY = above ? 5.f : box.size.y - 5.f;
					nvgBeginPath(vg);
					nvgMoveTo(vg, x + cw / 2, tipY);
					nvgLineTo(vg, x + cw / 2 + 4.f, baseY);
					nvgLineTo(vg, x + cw / 2 - 4.f, baseY);
					nvgClosePath(vg);
					nvgFillColor(vg, c);
					nvgFill(vg);
				}
			}
		}
	}
};


/**
 * MOD view, shown in place of the note grid while the MOD button is lit. The badge at the top
 * left switches the edited track between its two MOD modes:
 *   NOTE: each note is a bar as high as its MOD value, with a line along its length. On a poly
 *         track each step's column is split into a lane per voice.
 *   STEP: every step of the track has its own bar; the notes are marked along the bottom.
 * Press to set a value, or drag across to draw values; the wheel nudges the one under the mouse.
 */
struct ModDisplay : Widget {
	GridSequencer* module = nullptr;
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
		return bottom() - (bottom() - top()) * value / MOD_MAX;
	}
	float valueAt(float y) {
		return clamp((bottom() - y) / (bottom() - top()) * MOD_MAX, 0.f, (float) MOD_MAX);
	}
	int stepAt(float x, int viewStart) {
		return viewStart + (int) std::floor((x - labelW()) / cellW());
	}
	/** Voice (0..voices-1) whose lane is under `x` in its step's column. */
	int voiceAt(float x, int lead) {
		int voices = module->voiceCount(lead);
		float inCell = (x - labelW()) / cellW();
		inCell -= std::floor(inCell);
		return clamp((int) (inCell * voices), 0, voices - 1);
	}
	bool perStep(int lead) {
		return module->modMode[lead] == MOD_PER_STEP;
	}

	/** The step (STEP mode) or note start on `voice` (NOTE mode) that `step` edits, or -1. */
	int targetAt(int pattern, int lead, int voice, int step) {
		if (step < 0 || step >= module->trackLength(pattern, lead))
			return -1;
		return perStep(lead) ? step : module->noteStartCovering(pattern, laneOf(lead, voice), step);
	}
	uint8_t* valuePtr(int pattern, int lead, int voice, int target) {
		return perStep(lead) ? &module->stepMods[pattern][lead][target] : &module->mods[pattern][laneOf(lead, voice)][target];
	}

	/** Sets the value edited at `step` on the drag's voice. Returns the step / note start, or -1. */
	int setAt(int step, float value) {
		int target = targetAt(dragPattern, dragLead, dragVoice, step);
		if (target < 0)
			return -1;
		int v = clamp((int) std::round(value), 0, MOD_MAX);
		*valuePtr(dragPattern, dragLead, dragVoice, target) = v;
		// New notes get the last note value set
		if (!perStep(dragLead))
			module->modDefault[dragLead] = v;
		return target;
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			Widget::onButton(e);
			return;
		}
		if (e.action == GLFW_PRESS) {
			dragging = false;
			if (e.pos.x < labelW()) {
				// Badge: switch the edited track's MOD mode
				if (e.pos.y < badgeH() + 2.f) {
					GridSequencer* m = module;
					trackSettingsEdit(m, "change MOD mode", [=]() {
						int& mode = m->modMode[m->selectedTrack];
						mode = (mode == MOD_PER_STEP) ? MOD_PER_NOTE : MOD_PER_STEP;
					});
				}
			}
			else {
				dragPattern = module->viewPattern();
				dragLead = module->selectedTrack;
				dragVoice = perStep(dragLead) ? 0 : voiceAt(e.pos.x, dragLead);
				dragViewStart = module->viewStart;
				module->savePattern(dragPattern, undoBefore);
				dragging = true;
				dragPos = e.pos;
				lastStep = stepAt(e.pos.x, dragViewStart);
				lastValue = valueAt(e.pos.y);
				edited = setAt(lastStep, lastValue);
				editedVoice = dragVoice;
			}
		}
		e.consume(this);
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || !dragging || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		int step = stepAt(dragPos.x, dragViewStart);
		float value = valueAt(dragPos.y);
		// Fill in the steps a fast drag skipped, along a straight line
		if (step != lastStep) {
			int dir = step > lastStep ? 1 : -1;
			for (int k = lastStep + dir; k != step; k += dir)
				setAt(k, lastValue + (value - lastValue) * (k - lastStep) / (float) (step - lastStep));
		}
		int target = setAt(step, value);
		if (target >= 0)
			edited = target;
		lastStep = step;
		lastValue = value;
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (module && dragging)
			module->recordPatternEdit("change MOD value", dragPattern, undoBefore);
		dragging = false;
		edited = -1;
	}

	void onHover(const HoverEvent& e) override {
		hoverStep = -1;
		if (module && e.pos.x >= labelW()) {
			hoverStep = stepAt(e.pos.x, module->viewStart);
			hoverVoice = voiceAt(e.pos.x, module->selectedTrack);
		}
		e.consume(this);
	}

	void onLeave(const LeaveEvent& e) override {
		hoverStep = -1;
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module) {
			// Wheel nudges the value under the mouse by 1%; Shift+wheel or a horizontal wheel pans
			bool shift = (APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			float panDelta = (e.scrollDelta.x != 0.f) ? e.scrollDelta.x : (shift ? e.scrollDelta.y : 0.f);
			if (panDelta != 0.f) {
				module->setViewStart(module->viewStart + (panDelta < 0.f ? 4 : -4));
			}
			else if (e.scrollDelta.y != 0.f && !dragging && e.pos.x >= labelW()) {
				dragPattern = module->viewPattern();
				dragLead = module->selectedTrack;
				dragVoice = perStep(dragLead) ? 0 : voiceAt(e.pos.x, dragLead);
				int target = targetAt(dragPattern, dragLead, dragVoice, stepAt(e.pos.x, module->viewStart));
				if (target >= 0) {
					int delta = e.scrollDelta.y > 0.f ? 1 : -1;
					module->patternEdit("change MOD value", dragPattern, [&]() {
						setAt(target, *valuePtr(dragPattern, dragLead, dragVoice, target) + delta);
					});
				}
			}
		}
		e.consume(this);
	}

	void drawReadout(NVGcontext* vg, int track, int value, float x, float y, float w) {
		std::string text = string::f("%d%% %.2gV", value, module->modVoltage(track, value));
		nvgFontSize(vg, 11.f);
		bool right = x > box.size.x - mm2px(20.f);
		nvgTextAlign(vg, (right ? NVG_ALIGN_RIGHT : NVG_ALIGN_LEFT) | NVG_ALIGN_BOTTOM);
		float ty = (y - 2.f < mm2px(4.f)) ? y + mm2px(4.f) : y - 2.f;
		nvgFillColor(vg, nvgRGB(0xf0, 0xf2, 0xf6));
		nvgText(vg, right ? x + w : x, ty, text.c_str(), NULL);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		float lw = labelW(), cw = cellW(), h = box.size.y;
		int pattern = module ? module->viewPattern() : 0;
		int track = module ? module->selectedTrack : 0;
		int voices = module ? module->voiceCount(track) : 1;
		int viewStart = module ? module->viewStart : 0;
		int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;
		bool stepMode = module && perStep(track);
		const ModRange& range = MOD_RANGES[module ? clamp(module->modRange[track], 0, (int) MOD_RANGES.size() - 1) : 0];

		fillRect(vg, 0, 0, box.size.x, h, nvgRGB(0x0e, 0x10, 0x13));

		// Columns, shaded by beat
		for (int col = 0; col < VIEW_STEPS; col++) {
			bool beatGroup = ((viewStart + col) / 4) % 2 == 0;
			fillRect(vg, lw + col * cw + 0.5f, 0.5f, cw - 1.f, h - 1.f, beatGroup ? nvgRGB(0x22, 0x25, 0x2c) : nvgRGB(0x1b, 0x1e, 0x24));
		}

		// Left column: mode badge, then guide lines every 25% labelled with the output voltage
		fillRect(vg, 0.5f, 0.5f, lw - 1.f, h - 1.f, nvgRGB(0x20, 0x23, 0x2a));
		setFont(vg, DISPLAY_FONT);
		NVGcolor trackColor = TRACK_COLORS[track];
		fillRect(vg, 1.5f, 1.5f, lw - 3.f, badgeH(), nvgRGB(0x3a, 0x40, 0x4c), 1.5f);
		nvgFontSize(vg, 11.f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, trackColor);
		nvgText(vg, lw / 2, 1.5f + badgeH() / 2, stepMode ? "STEP" : "NOTE", NULL);

		nvgFontSize(vg, 10.5f);
		for (int i = 0; i <= 4; i++) {
			float y = valueY(MOD_MAX * i / 4.f);
			fillRect(vg, lw, y - 0.5f, box.size.x - lw, 1.f, nvgRGBA(0xff, 0xff, 0xff, i == 0 ? 0x30 : 0x14));
			float volts = range.min + (range.max - range.min) * i / 4.f;
			std::string label = (range.min < 0.f && volts > 0.f) ? string::f("+%gV", volts) : string::f("%gV", volts);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | (i == 0 ? NVG_ALIGN_BOTTOM : NVG_ALIGN_MIDDLE));
			nvgFillColor(vg, nvgRGB(0xc4, 0xc9, 0xd2));
			nvgText(vg, lw / 2, y, label.c_str(), NULL);
		}

		// Steps past the edited track's end are inactive
		int inactiveCol = clamp(trackLen - viewStart, 0, VIEW_STEPS);
		if (inactiveCol < VIEW_STEPS)
			fillRect(vg, lw + inactiveCol * cw, 0, (VIEW_STEPS - inactiveCol) * cw, h, nvgRGBA(0x00, 0x00, 0x00, 0xa0));

		if (!module)
			return;

		// Playhead of the edited track
		int playStep = module->trackStep[track];
		if (module->step >= 0 && playStep >= viewStart && playStep < viewStart + VIEW_STEPS && module->playingPattern == pattern)
			fillRect(vg, lw + (playStep - viewStart) * cw, 0, cw, h, nvgRGBA(0xff, 0xff, 0xff, 0x22));

		NVGcolor c = trackColor;
		if (module->muted[track])
			c.a = 0.5f;
		int shown = dragging ? edited : -1;
		int shownVoice = dragging ? editedVoice : 0;
		if (!dragging && hoverStep >= 0) {
			shownVoice = stepMode ? 0 : hoverVoice;
			shown = targetAt(pattern, track, shownVoice, hoverStep);
		}

		if (stepMode) {
			// A bar for every step of the track
			for (int col = 0; col < VIEW_STEPS; col++) {
				int s = viewStart + col;
				if (s >= trackLen)
					break;
				int value = module->stepMods[pattern][track][s];
				float x = lw + col * cw + 3.f, y = valueY(value);
				fillRect(vg, x, y, cw - 6.f, std::max(bottom() - y, 1.5f), c, 1.f);
			}
			// Notes (of every voice) marked along the bottom, to line the steps up with them
			for (int voice = laneOf(track, 0); voice < laneOf(track, voices); voice++) {
				for (int s = 0; s < trackLen; s++) {
					if (module->notes[pattern][voice][s] < 0)
						continue;
					int a = std::max(s, viewStart) - viewStart;
					int b = std::min(s + (int) module->lengths[pattern][voice][s], std::min(trackLen, viewStart + VIEW_STEPS)) - viewStart;
					if (a < b)
						fillRect(vg, lw + a * cw + 2.f, bottom() - 3.f, (b - a) * cw - 4.f, 3.f, nvgRGBA(0xf0, 0xf2, 0xf6, 0xb0), 1.f);
				}
			}
			if (shown >= viewStart && shown < viewStart + VIEW_STEPS) {
				int value = module->stepMods[pattern][track][shown];
				drawReadout(vg, track, value, lw + (shown - viewStart) * cw + 3.f, valueY(value), cw - 6.f);
			}
			return;
		}

		// Notes of the edited part: a bar at the start (in its voice's lane), a line along the rest
		float laneW = (cw - 6.f) / voices;
		for (int v = 0; v < voices; v++) {
			for (int s = 0; s < NUM_STEPS; s++) {
				int l = laneOf(track, v);
				if (module->notes[pattern][l][s] < 0)
					continue;
				int length = module->lengths[pattern][l][s];
				int a = std::max(s, viewStart) - viewStart;
				int b = std::min(s + length, viewStart + VIEW_STEPS) - viewStart;
				if (a >= b)
					continue;
				NVGcolor nc = c;
				if (s >= trackLen)
					nc.a = 0.25f;
				int value = module->mods[pattern][l][s];
				float y = valueY(value);
				bool startInView = s >= viewStart;
				float barX = lw + a * cw + 3.f + v * laneW;
				float lineX = startInView ? barX + laneW : lw + a * cw;
				float lineEnd = lw + b * cw - 3.f;
				if (lineEnd > lineX) {
					NVGcolor lc = nc;
					lc.a *= 0.6f;
					fillRect(vg, lineX, y - 1.f, lineEnd - lineX, 2.f, lc);
				}
				if (startInView) {
					fillRect(vg, barX, y, laneW - (voices > 1 ? 1.f : 0.f), std::max(bottom() - y, 1.5f), nc, 1.f);
					if (s == shown && v == shownVoice)
						drawReadout(vg, track, value, barX, y, laneW);
				}
			}
		}
	}
};


/**
 * Bar beside the track numbers joining a poly track to the tracks whose jacks send its other
 * voices, in its colour.
 */
struct GroupBrackets : TransparentWidget {
	GridSequencer* module = nullptr;
	// Track rows: y of the first one and the spacing, in mm
	float firstY = 0.f, pitch = 0.f;

	void draw(const DrawArgs& args) override {
		if (!module)
			return;
		for (int t = 0; t < NUM_TRACKS; t++) {
			if (module->isVoiceOutput(t))
				continue;
			int last = t;
			while (last + 1 < NUM_TRACKS && module->ownerTrack[last + 1] == t)
				last++;
			if (last == t)
				continue;
			float y0 = mm2px(firstY + t * pitch - 4.f);
			float y1 = mm2px(firstY + last * pitch + 4.f);
			fillRect(args.vg, 0.f, y0, box.size.x, y1 - y0, TRACK_COLORS[t], box.size.x / 2);
		}
	}
};


/**
 * Under a poly track's number, how many notes it plays at once ("x3"). Left out when it sends its
 * voices to other tracks' jacks: the bar beside the numbers shows that group instead.
 */
struct VoiceCountLabel : TransparentWidget {
	GridSequencer* module = nullptr;
	int track = 0;

	void draw(const DrawArgs& args) override {
		if (!module)
			return;
		bool grouped = module->isVoiceOutput(track)
			|| (track + 1 < NUM_TRACKS && module->ownerTrack[track + 1] == track);
		if (grouped || module->voiceCount(track) < 2)
			return;
		std::string text = string::f("x%d", module->voiceCount(track));
		setFont(args.vg, DISPLAY_FONT);
		nvgFontSize(args.vg, 10.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, TRACK_COLORS[track]);
		nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
	}
};


/**
 * Ring around a track's gate or CV jack, lit while its gate is high, in the colour of the track
 * whose notes the jacks send.
 */
struct OutputGlow : Widget {
	GridSequencer* module = nullptr;
	int track = 0;

	NVGcolor color() {
		return TRACK_COLORS[module ? module->ownerTrack[track] : track];
	}

	void ring(NVGcontext* vg, float inner, float outer, NVGcolor c) {
		float cx = box.size.x / 2, cy = box.size.y / 2;
		nvgBeginPath(vg);
		nvgCircle(vg, cx, cy, outer);
		nvgCircle(vg, cx, cy, inner);
		nvgPathWinding(vg, NVG_HOLE);
		nvgFillColor(vg, c);
		nvgFill(vg);
	}

	void draw(const DrawArgs& args) override {
		// Unlit: a faint ring in the track colour
		NVGcolor c = color();
		c.a = 0.22f;
		ring(args.vg, mm2px(4.15f), mm2px(4.85f), c);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			float b = module->lights[GridSequencer::GATE_LIGHT + track].getBrightness();
			if (b > 0.01f) {
				NVGcolor c = color();
				c.a = b;
				ring(args.vg, mm2px(4.15f), mm2px(4.85f), c);
				// Soft halo outside the ring
				float cx = box.size.x / 2, cy = box.size.y / 2;
				NVGcolor glow = c, clear = c;
				glow.a = 0.35f * b;
				clear.a = 0.f;
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, cx, cy, mm2px(5.3f));
				nvgCircle(args.vg, cx, cy, mm2px(4.85f));
				nvgPathWinding(args.vg, NVG_HOLE);
				nvgFillPaint(args.vg, nvgRadialGradient(args.vg, cx, cy, mm2px(4.85f), mm2px(5.3f), glow, clear));
				nvgFill(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};


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

	void onButton(const ButtonEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			Widget::onButton(e);
			return;
		}
		if (e.action == GLFW_PRESS) {
			dragging = false;
			if (e.pos.x >= labelW()) {
				dragPattern = module->viewPattern();
				dragTrack = module->selectedTrack;
				module->savePattern(dragPattern, undoBefore);
				setLengthAt(e.pos.x);
				dragging = true;
				dragPos = e.pos;
			}
		}
		e.consume(this);
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || !dragging || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		setLengthAt(dragPos.x);
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (module && dragging)
			module->recordPatternEdit("change track length", dragPattern, undoBefore);
		dragging = false;
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			int delta = e.scrollDelta.y > 0.f ? 1 : -1;
			int pattern = module->viewPattern();
			int track = module->selectedTrack;
			module->patternEdit("change track length", pattern, [&]() {
				module->setTrackLength(pattern, track, module->trackLength(pattern, track) + delta);
			});
		}
		e.consume(this);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		float lw = labelW(), cw = cellW(), h = box.size.y;
		int pageStart = module ? module->viewStart : 0;
		int pattern = module ? module->viewPattern() : 0;
		int track = module ? module->selectedTrack : 0;
		int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;
		NVGcolor trackColor = TRACK_COLORS[track];

		fillRect(vg, 0, 0, box.size.x, h, nvgRGB(0x0e, 0x10, 0x13));

		// Corner label
		fillRect(vg, 0.5f, 0.5f, lw - 1.f, h - 1.f, nvgRGB(0x20, 0x23, 0x2a));
		setFont(vg, DISPLAY_FONT);
		nvgFontSize(vg, 9.5f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, nvgRGB(0xb0, 0xb6, 0xc0));
		nvgText(vg, lw / 2, h / 2, "STEP", NULL);

		// Steps: active ones shaded by beat, step numbers on each beat
		nvgFontSize(vg, 9.5f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		for (int col = 0; col < VIEW_STEPS; col++) {
			int s = pageStart + col;
			bool active = s < trackLen;
			bool beatGroup = (s / 4) % 2 == 0;
			NVGcolor c = active ? (beatGroup ? nvgRGB(0x2c, 0x31, 0x3b) : nvgRGB(0x24, 0x28, 0x31)) : nvgRGB(0x12, 0x14, 0x17);
			fillRect(vg, lw + col * cw + 0.5f, 0.5f, cw - 1.f, h - 1.f, c);
			if (s % 4 == 0) {
				nvgFillColor(vg, active ? nvgRGB(0xd0, 0xd4, 0xdc) : nvgRGB(0x50, 0x56, 0x60));
				std::string num = string::f("%d", s + 1);
				nvgText(vg, lw + col * cw + 2.f, h / 2, num.c_str(), NULL);
			}
		}

		// Playhead of the edited track
		if (module && module->step >= 0 && module->playingPattern == pattern) {
			int playStep = module->trackStep[track];
			if (playStep >= pageStart && playStep < pageStart + VIEW_STEPS)
				fillRect(vg, lw + (playStep - pageStart) * cw + 0.5f, h - 2.f, cw - 1.f, 2.f, trackColor);
		}

		// End marker: a bar in the track colour after the last step, with a handle
		int endCol = trackLen - pageStart;
		if (endCol >= 1 && endCol <= VIEW_STEPS) {
			float x = lw + endCol * cw;
			fillRect(vg, x - 2.5f, 0, 2.5f, h, trackColor);
			nvgBeginPath(vg);
			nvgMoveTo(vg, x - 2.5f, 0);
			nvgLineTo(vg, x - 7.f, 0);
			nvgLineTo(vg, x - 2.5f, h / 2);
			nvgClosePath(vg);
			nvgFillColor(vg, trackColor);
			nvgFill(vg);
		}
	}
};


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

	void onButton(const ButtonEvent& e) override {
		if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			Widget::onButton(e);
			return;
		}
		if (e.action == GLFW_PRESS) {
			dragging = false;
			if (e.pos.x >= labelW()) {
				// Outside the thumb: centre the view on the clicked step first
				if (e.pos.x < thumbX() || e.pos.x > thumbX() + thumbW())
					panThumbTo(e.pos.x - thumbW() / 2);
				grabOffset = e.pos.x - thumbX();
				dragX = e.pos.x;
				dragging = true;
			}
		}
		e.consume(this);
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || !dragging || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragX += e.mouseDelta.x / getAbsoluteZoom();
		panThumbTo(dragX - grabOffset);
	}

	void onDragEnd(const DragEndEvent& e) override {
		dragging = false;
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module) {
			float delta = (e.scrollDelta.x != 0.f) ? e.scrollDelta.x : e.scrollDelta.y;
			if (delta != 0.f)
				module->setViewStart(module->viewStart + (delta < 0.f ? 1 : -1));
		}
		e.consume(this);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		float lw = labelW(), sw = stepW(), h = box.size.y;
		int viewStart = module ? module->viewStart : 0;
		int pattern = module ? module->viewPattern() : 0;
		int track = module ? module->selectedTrack : 0;
		int trackLen = module ? module->trackLength(pattern, track) : DEFAULT_TRACK_LENGTH;

		fillRect(vg, 0, 0, box.size.x, h, nvgRGB(0x0e, 0x10, 0x13));

		// Corner: the range of steps in view
		fillRect(vg, 0.5f, 0.5f, lw - 1.f, h - 1.f, nvgRGB(0x20, 0x23, 0x2a));
		setFont(vg, DISPLAY_FONT);
		nvgFontSize(vg, 9.f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, nvgRGB(0xd0, 0xd4, 0xdc));
		std::string range = string::f("%d-%d", viewStart + 1, viewStart + VIEW_STEPS);
		nvgText(vg, lw / 2, h / 2, range.c_str(), NULL);

		// The edited track's active length, with a divider every 16 steps
		fillRect(vg, lw, 0.5f, trackLen * sw, h - 1.f, nvgRGB(0x24, 0x28, 0x31));
		for (int s = VIEW_STEPS; s < NUM_STEPS; s += VIEW_STEPS)
			fillRect(vg, lw + s * sw - 0.5f, 0, 1.f, h, nvgRGB(0x3a, 0x40, 0x4c));

		if (module) {
			// Notes, drawn at their pitch so the overview shows each line's shape
			int rows = module->numRows();
			auto noteY = [&](int row) {
				return 1.5f + (1.f - (row + 0.5f) / rows) * (h - 3.f);
			};
			for (int pass = 0; pass < 2; pass++) {
				for (int t = 0; t < NUM_TRACKS; t++) {
					// Other tracks first, faintly; then the edited track (all its voices) on top
					bool edited = t == track;
					if ((pass == 0) == edited || module->isVoiceOutput(t))
						continue;
					NVGcolor c = TRACK_COLORS[t];
					c.a = edited ? 1.f : 0.3f;
					int length = module->trackLength(pattern, t);
					for (int v = 0; v < module->voiceCount(t); v++) {
						int l = laneOf(t, v);
						for (int s = 0; s < length; s++) {
							int row = module->notes[pattern][l][s];
							if (row < 0)
								continue;
							float w = std::max(std::min((int) module->lengths[pattern][l][s], length - s) * sw - 1.f, 1.5f);
							fillRect(vg, lw + s * sw, noteY(row) - 1.f, w, 2.f, c);
						}
					}
				}
			}

			// Playhead of the edited track
			int playStep = module->trackStep[track];
			if (module->step >= 0 && playStep >= 0 && module->playingPattern == pattern)
				fillRect(vg, lw + playStep * sw, 0, std::max(sw, 1.5f), h, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		}

		// Thumb: the steps shown in the grid
		float tx = lw + viewStart * sw, tw = VIEW_STEPS * sw;
		fillRect(vg, tx, 0.5f, tw, h - 1.f, nvgRGBA(0xff, 0xff, 0xff, dragging ? 0x30 : 0x1c), 2.f);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, tx + 0.5f, 1.f, tw - 1.f, h - 2.f, 2.f);
		nvgStrokeColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xb0));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}
};


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
	int slotAt(Vec pos) {
		if (pos.x < labelW())
			return -1;
		int col = clamp((int) ((pos.x - labelW()) / slotW()), 0, SLOTS - 1);
		return page() * SLOTS + col;
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS) {
			Widget::onButton(e);
			return;
		}
		int i = slotAt(e.pos);
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (i < 0) {
				// Label area flips to the next page of the song
				int next = (page() + 1) * SLOTS;
				module->songCursor = next < module->songLength ? next : 0;
			}
			else if (i < module->songLength) {
				// Select the slot and show its pattern in the grid; dragging then moves it
				module->songCursor = i;
				module->params[GridSequencer::PATTERN_PARAM].setValue(module->song[i] + 1);
				module->saveSong(undoBefore);
				dragging = true;
				dragMoved = false;
				dragX = e.pos.x;
				lastEdgeMove = 0.0;
			}
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && i >= module->songLength) {
			// Empty slots: add the edit pattern at the end of the song
			GridSequencer* m = module;
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuItem(string::f("Add edit pattern (P%02d) to the end of the song", m->editPattern() + 1), "", [=]() {
				m->songEdit("add song slot", [=]() {
					m->songCursor = std::max(m->songLength - 1, 0);
					m->songInsert(m->editPattern(), true);
				});
			}, m->songLength >= MAX_SONG_LENGTH));
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && i >= 0 && i < module->songLength) {
			module->songCursor = i;
			GridSequencer* m = module;
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel(string::f("Song slot %d (pattern %02d)", i + 1, m->song[i] + 1)));
			menu->addChild(createMenuItem(string::f("Set to edit pattern (P%02d)", m->editPattern() + 1), "", [=]() {
				m->songEdit("set song slot", [=]() {
					m->song[i] = m->editPattern();
				});
			}));
			menu->addChild(createMenuItem("Open this pattern in editor", "", [=]() {
				m->params[GridSequencer::PATTERN_PARAM].setValue(m->song[i] + 1);
			}));
			menu->addChild(createMenuItem("Insert edit pattern before (INS)", "", [=]() {
				m->songEdit("insert song slot", [=]() {
					m->songCursor = i;
					m->songInsert(m->editPattern(), false);
				});
			}, m->songLength >= MAX_SONG_LENGTH));
			menu->addChild(createMenuItem("Add edit pattern after (ADD)", "", [=]() {
				m->songEdit("add song slot", [=]() {
					m->songCursor = i;
					m->songInsert(m->editPattern(), true);
				});
			}, m->songLength >= MAX_SONG_LENGTH));
			menu->addChild(createMenuItem("Duplicate slot", "", [=]() {
				m->songEdit("duplicate song slot", [=]() {
					m->songCursor = i;
					m->songInsert(m->song[i]);
				});
			}, m->songLength >= MAX_SONG_LENGTH));
			menu->addChild(createMenuItem("Delete slot", "", [=]() {
				m->songEdit("delete song slot", [=]() {
					m->songCursor = i;
					m->songDelete();
				});
			}));
			e.consume(this);
		}
		else {
			Widget::onButton(e);
		}
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (dragging && e.button == GLFW_MOUSE_BUTTON_LEFT)
			dragX += e.mouseDelta.x / getAbsoluteZoom();
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (module && dragging && dragMoved)
			module->recordSongEdit("move song slot", undoBefore);
		dragging = false;
	}

	void step() override {
		Widget::step();
		if (!module || !dragging)
			return;
		int from = module->songCursor;
		int to = from;
		if (dragX < labelW() || dragX >= box.size.x) {
			// Past an end of the strip: step along a slot at a time
			double now = system::getTime();
			if (now - lastEdgeMove >= 0.25) {
				lastEdgeMove = now;
				to = from + (dragX < labelW() ? -1 : 1);
			}
		}
		else {
			int col = (int) ((dragX - labelW()) / slotW());
			to = page() * SLOTS + clamp(col, 0, SLOTS - 1);
		}
		to = clamp(to, 0, module->songLength - 1);
		if (to != from) {
			module->songMove(from, to);
			dragMoved = true;
		}
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module) {
			e.consume(this);
			return;
		}
		int delta = (e.scrollDelta.y > 0.f) ? 1 : (e.scrollDelta.y < 0.f ? -1 : 0);
		int i = slotAt(e.pos);
		if (i < 0)
			module->songCursor = clamp(module->songCursor + delta, 0, std::max(module->songLength - 1, 0));
		else if (i < module->songLength) {
			GridSequencer* m = module;
			m->songEdit("change song slot", [=]() {
				m->song[i] = clamp(m->song[i] + delta, 0, NUM_PATTERNS - 1);
			});
		}
		e.consume(this);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		float lw = labelW(), sw = slotW();
		int length = module ? module->songLength : 0;
		int pages = std::max((length + SLOTS - 1) / SLOTS, 1);

		fillRect(vg, 0, 0, box.size.x, box.size.y, nvgRGB(0x0e, 0x10, 0x13));
		setFont(vg, DISPLAY_FONT);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		fillRect(vg, 0.5f, 0.5f, lw - 1.f, box.size.y - 1.f, nvgRGB(0x20, 0x23, 0x2a));
		nvgFillColor(vg, nvgRGB(0xc8, 0xcc, 0xd4));
		nvgFontSize(vg, 10.5f);
		nvgText(vg, lw / 2, box.size.y * 0.32f, "SONG", NULL);
		std::string pageStr = string::f("%d/%d", page() + 1, pages);
		nvgFillColor(vg, nvgRGB(0xb0, 0xb6, 0xc0));
		nvgText(vg, lw / 2, box.size.y * 0.72f, pageStr.c_str(), NULL);

		nvgFontSize(vg, 12.5f);
		for (int col = 0; col < SLOTS; col++) {
			int i = page() * SLOTS + col;
			float x = lw + col * sw;
			if (i >= length) {
				fillRect(vg, x + 1.f, 1.f, sw - 2.f, box.size.y - 2.f, nvgRGB(0x15, 0x17, 0x1b), 1.5f);
				continue;
			}
			bool playing = module && module->playingSong() && module->step >= 0 && module->songPos == i;
			fillRect(vg, x + 1.f, 1.f, sw - 2.f, box.size.y - 2.f, playing ? nvgRGB(0x3c, 0xa0, 0x58) : nvgRGB(0x26, 0x2a, 0x33), 1.5f);
			if (module && module->songCursor == i) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x + 1.5f, 1.5f, sw - 3.f, box.size.y - 3.f, 1.5f);
				// The slot being dragged gets a heavier white outline
				bool moving = dragging && dragMoved;
				nvgStrokeColor(vg, moving ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xf2, 0xdc, 0x4a));
				nvgStrokeWidth(vg, moving ? 2.f : 1.2f);
				nvgStroke(vg);
			}
			int p = module ? module->song[i] : 0;
			nvgFillColor(vg, nvgRGB(0xf0, 0xf2, 0xf6));
			std::string s = string::f("%02d", p + 1);
			nvgText(vg, x + sw / 2, box.size.y / 2, s.c_str(), NULL);
		}
	}
};


struct GridSequencerWidget : ModuleWidget {
	GridDisplay* grid = nullptr;
	ModDisplay* modDisplay = nullptr;

	GridSequencerWidget(GridSequencer* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GridSequencer.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Left column: two sub-columns of controls, labels above each control
		const float colA = 9.5f, colB = 23.5f, labelDy = 7.f;
		auto rowY = [](int row) {
			return 14.f + row * 15.3f;
		};

		// Pattern, scale, root and gate length: name on the left, value display with < > on the right
		const float nameX = 7.5f, stepperX = 21.35f, stepperW = 17.5f;
		auto addStepper = [&](float y, const char* label, int paramId, const char* format, std::vector<std::string> labels = {}, bool wrap = true) {
			addLabel(Vec(nameX, y), label, 8.5f);
			ValueStepper* stepper = createParam<ValueStepper>(Vec(), module, paramId);
			stepper->box.size = mm2px(Vec(stepperW, 5.4f));
			stepper->box.pos = mm2px(Vec(stepperX, y)).minus(stepper->box.size.div(2));
			stepper->format = format;
			stepper->labels = labels;
			stepper->wrap = wrap;
			addParam(stepper);
		};
		std::vector<std::string> scaleNames;
		for (const ScaleDef& sc : SCALES)
			scaleNames.push_back(sc.shortName);
		std::vector<std::string> gateNames;
		for (int i = 1; i <= GATE_STEPS; i++)
			gateNames.push_back(string::f("%d%%", i * 100 / GATE_STEPS));
		addStepper(8.6f, "SEQ", GridSequencer::PATTERN_PARAM, "%02d");
		addStepper(16.2f, "SCALE", GridSequencer::SCALE_PARAM, "", scaleNames);
		addStepper(23.8f, "ROOT", GridSequencer::ROOT_PARAM, "");
		addStepper(31.4f, "GATE", GridSequencer::GATE_LENGTH_PARAM, "", gateNames, false);

		// Knobs
		const float knobY = 44.2f;
		addLabel(Vec(colA, knobY - 6.6f), "TEMPO");
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(colA, knobY)), module, GridSequencer::TEMPO_PARAM));
		addLabel(Vec(colB, knobY - 6.6f), "BPM");
		TempoDisplay* tempo = createWidget<TempoDisplay>(mm2px(Vec(colB - 6.f, knobY - 3.2f)));
		tempo->box.size = mm2px(Vec(12.f, 6.4f));
		tempo->module = module;
		addChild(tempo);

		// Transport and clock inputs
		addLabel(Vec(colA, rowY(3) - labelDy), "RUN");
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(colA, rowY(3))), module, GridSequencer::RUN_PARAM, GridSequencer::RUN_LIGHT));
		addLabel(Vec(colB, rowY(3) - labelDy), "RESET");
		addParam(createParamCentered<VCVButton>(mm2px(Vec(colB, rowY(3))), module, GridSequencer::RESET_PARAM));
		addLabel(Vec(colA, rowY(4) - labelDy), "RUN IN");
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, rowY(4))), module, GridSequencer::RUN_INPUT));
		addLabel(Vec(colB, rowY(4) - labelDy), "RST IN");
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, rowY(4))), module, GridSequencer::RESET_INPUT));
		addLabel(Vec(colA, rowY(5) - labelDy), "CLK IN");
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colA, rowY(5))), module, GridSequencer::CLOCK_INPUT));
		addLabel(Vec(colB, rowY(5) - labelDy), "PAT IN");
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colB, rowY(5))), module, GridSequencer::PATTERN_INPUT));

		// Outputs (patch CLK / RUN / RST to CLK IN / RUN IN / RST IN of another instance to sync it)
		const NVGcolor outLabel = nvgRGB(0xff, 0xff, 0xff);
		addLabel(Vec(colA, rowY(6) - labelDy), "RUN", 9.5f, outLabel);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, rowY(6))), module, GridSequencer::RUN_OUTPUT));
		addLabel(Vec(colB, rowY(6) - labelDy), "RST", 9.5f, outLabel);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, rowY(6))), module, GridSequencer::RESET_OUTPUT));
		addLabel(Vec(colA, rowY(7) - labelDy), "CLK", 9.5f, outLabel);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colA, rowY(7))), module, GridSequencer::CLOCK_OUTPUT));
		addLabel(Vec(colB, rowY(7) - labelDy), "EOP", 9.5f, outLabel);
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colB, rowY(7))), module, GridSequencer::EOP_OUTPUT));

		// Displays
		const float displayX = 35.f, displayW = 129.f;
		InfoDisplay* info = createWidget<InfoDisplay>(mm2px(Vec(displayX, 3.6f)));
		info->box.size = mm2px(Vec(displayW, 7.6f));
		info->module = module;
		addChild(info);

		RulerDisplay* ruler = createWidget<RulerDisplay>(mm2px(Vec(displayX, 12.6f)));
		ruler->box.size = mm2px(Vec(displayW, 5.f));
		ruler->module = module;
		addChild(ruler);

		grid = createWidget<GridDisplay>(mm2px(Vec(displayX, 18.f)));
		grid->box.size = mm2px(Vec(displayW, VISIBLE_ROWS * 5.2f));
		grid->module = module;
		addChild(grid);

		// MOD view takes the grid's place while the MOD button is lit
		modDisplay = createWidget<ModDisplay>(grid->box.pos);
		modDisplay->box.size = grid->box.size;
		modDisplay->module = module;
		modDisplay->visible = false;
		addChild(modDisplay);

		ScrollerDisplay* scroller = createWidget<ScrollerDisplay>(mm2px(Vec(displayX, 96.6f)));
		scroller->box.size = mm2px(Vec(displayW, 4.6f));
		scroller->module = module;
		addChild(scroller);

		SongDisplay* songDisplay = createWidget<SongDisplay>(mm2px(Vec(displayX, 102.f)));
		songDisplay->box.size = mm2px(Vec(displayW, 9.5f));
		songDisplay->module = module;
		addChild(songDisplay);

		// Grid scrolling
		const float scrollX = 170.6f;
		addLabel(Vec(scrollX, 16.f), "UP", 9.f);
		addParam(createParamCentered<TL1105>(mm2px(Vec(scrollX, 21.f)), module, GridSequencer::SCROLL_UP_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(scrollX, 93.f)), module, GridSequencer::SCROLL_DOWN_PARAM));
		addLabel(Vec(scrollX, 98.f), "DN", 9.f);

		// Grid view: notes / MOD values
		addLabel(Vec(scrollX, 51.f), "MOD", 8.5f);
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(scrollX, 57.f)), module, GridSequencer::MOD_VIEW_PARAM, GridSequencer::MOD_VIEW_LIGHT));

		// Song, pattern and track function buttons below the song strip
		struct FunctionButton {
			const char* label;
			int paramId;
		};
		const float groupHeaderY = 113.7f, buttonLabelY = 117.3f, buttonY = 122.3f;
		const float buttonPitch = 9.4f, groupGap = 2.5f;
		float groupX = displayX;
		auto addGroup = [&](const char* title, NVGcolor titleColor, std::vector<FunctionButton> buttons) {
			float width = buttons.size() * buttonPitch;
			addLabel(Vec(groupX + width / 2, groupHeaderY), title, 8.5f, titleColor);
			for (size_t i = 0; i < buttons.size(); i++) {
				Vec pos = Vec(groupX + (i + 0.5f) * buttonPitch, buttonY);
				addLabel(Vec(pos.x, buttonLabelY), buttons[i].label, 8.5f);
				if (buttons[i].paramId == GridSequencer::MODE_PARAM)
					addParam(createLightParamCentered<ModeButton>(mm2px(pos), module, GridSequencer::MODE_PARAM, GridSequencer::SONG_LIGHT));
				else
					addParam(createParamCentered<VCVButton>(mm2px(pos), module, buttons[i].paramId));
			}
			groupX += width + groupGap;
		};
		addGroup("SONG", nvgRGB(0xf2, 0xdc, 0x4a), {
			{"MODE", GridSequencer::MODE_PARAM},
			{"INS", GridSequencer::SONG_INSERT_PARAM},
			{"ADD", GridSequencer::SONG_ADD_PARAM},
			{"DUP", GridSequencer::SONG_DUPLICATE_PARAM},
			{"DEL", GridSequencer::SONG_DELETE_PARAM},
			{"SET", GridSequencer::SONG_SET_PARAM},
			{"EDIT", GridSequencer::SONG_EDIT_PARAM},
			{"CLR", GridSequencer::SONG_CLEAR_PARAM},
		});
		addGroup("PATTERN", nvgRGB(0x9c, 0xdc, 0xff), {
			{"COPY", GridSequencer::PATTERN_COPY_PARAM},
			{"PASTE", GridSequencer::PATTERN_PASTE_PARAM},
			{"CLR", GridSequencer::PATTERN_CLEAR_PARAM},
		});
		addGroup("TRACK", nvgRGB(0xff, 0xa0, 0x40), {
			{"COPY", GridSequencer::TRACK_COPY_PARAM},
			{"PASTE", GridSequencer::TRACK_PASTE_PARAM},
			{"CLR", GridSequencer::TRACK_CLEAR_PARAM},
		});

		// Tracks
		const float headerY = 9.f;
		const float numX = 177.8f, selectX = 185.f, muteX = 195.5f, gateX = 206.4f, cvX = 216.6f, modX = 226.8f;
		addLabel(Vec(selectX, headerY), "TRACK");
		addLabel(Vec(muteX, headerY), "MUTE");
		addLabel(Vec(gateX, headerY), "GATE", 9.5f, nvgRGB(0xff, 0xff, 0xff));
		addLabel(Vec(cvX, headerY), "CV", 9.5f, nvgRGB(0xff, 0xff, 0xff));
		addLabel(Vec(modX, headerY), "MOD", 9.5f, nvgRGB(0xff, 0xff, 0xff));
		GroupBrackets* brackets = createWidget<GroupBrackets>(mm2px(Vec(175.4f, 0.f)));
		brackets->box.size = mm2px(Vec(0.9f, 128.5f));
		brackets->module = module;
		brackets->firstY = 19.f;
		brackets->pitch = 13.f;
		addChild(brackets);
		for (int t = 0; t < NUM_TRACKS; t++) {
			float y = 19.f + t * 13.f;
			// Number, nudged up to leave room for a poly track's voice count below it
			addLabel(Vec(numX, y - 1.6f), string::f("%d", t + 1), 12.f, TRACK_COLORS[t]);
			VoiceCountLabel* voiceLabel = createWidget<VoiceCountLabel>(mm2px(Vec(numX - 2.6f, y + 1.4f)));
			voiceLabel->box.size = mm2px(Vec(5.2f, 3.6f));
			voiceLabel->module = module;
			voiceLabel->track = t;
			addChild(voiceLabel);

			auto* select = createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(selectX, y)), module, GridSequencer::TRACK_PARAM + t, GridSequencer::TRACK_LIGHT + t);
			select->getLight()->baseColors[0] = TRACK_COLORS[t];
			addParam(select);
			addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(muteX, y)), module, GridSequencer::MUTE_PARAM + t, GridSequencer::MUTE_LIGHT + t));

			// Gate and CV jacks light up in the track colour while the gate is high
			for (float x : {gateX, cvX}) {
				OutputGlow* glow = new OutputGlow;
				glow->box.size = mm2px(Vec(11.f, 11.f));
				glow->box.pos = mm2px(Vec(x, y)).minus(glow->box.size.div(2));
				glow->module = module;
				glow->track = t;
				addChild(glow);
			}
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(gateX, y)), module, GridSequencer::GATE_OUTPUT + t));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(cvX, y)), module, GridSequencer::CV_OUTPUT + t));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(modX, y)), module, GridSequencer::MOD_OUTPUT + t));
		}

		addLabel(Vec(203.5f, 121.8f), "THE GRID", 14.f, nvgRGB(0xe8, 0xec, 0xf2));
	}

	void step() override {
		ModuleWidget::step();
		GridSequencer* m = getModule<GridSequencer>();
		if (!m)
			return;

		// Notes or MOD values in the grid area
		modDisplay->visible = m->modView();
		grid->visible = !modDisplay->visible;

		// Keep the playing steps in view, but not in the middle of a drag on the panel
		Widget* dragged = APP->event->getDraggedWidget();
		if (!(dragged && dragged->isDescendantOf(this)))
			m->followPlayback();
		std::vector<GridSequencer::Edit> edits;
		{
			std::lock_guard<std::mutex> lock(m->editsMutex);
			edits.swap(m->pendingEdits);
		}
		for (GridSequencer::Edit& edit : edits) {
			GridEditAction* action = new GridEditAction;
			action->moduleId = m->id;
			action->name = edit.name;
			action->edit = std::move(edit);
			APP->history->push(action);
		}
	}

	void addLabel(Vec posMm, const std::string& text, float fontSize = 9.5f, NVGcolor color = nvgRGB(0xe6, 0xe9, 0xee)) {
		PanelLabel* label = new PanelLabel;
		label->box.size = mm2px(Vec(30.f, 5.f));
		label->box.pos = mm2px(posMm).minus(label->box.size.div(2));
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		addChild(label);
	}

	/** Songs are saved as Rack presets, so they also show in the module's Preset menu. */
	std::string songDirectory() {
		std::string dir = model->getUserPresetDirectory();
		system::createDirectories(dir);
		return dir;
	}

	/** Asks for a file; "" if cancelled. */
	std::string fileDialog(osdialog_file_action action, const std::string& filename, const char* filterSpec) {
		std::string dir = songDirectory();
		osdialog_filters* filters = osdialog_filters_parse(filterSpec);
		char* pathC = osdialog_file(action, dir.c_str(), filename.empty() ? NULL : filename.c_str(), filters);
		osdialog_filters_free(filters);
		if (!pathC)
			return "";
		std::string path = pathC;
		std::free(pathC);
		return path;
	}

	void saveSongDialog() {
		GridSequencer* m = getModule<GridSequencer>();
		std::string name = m->songName.empty() ? "Untitled" : m->songName;
		std::string path = fileDialog(OSDIALOG_SAVE, name + ".vcvm", "THE GRID song (.vcvm):vcvm");
		if (path.empty())
			return;
		if (system::getExtension(path) != ".vcvm")
			path += ".vcvm";
		// An unnamed song takes the file's name
		if (m->songName.empty())
			m->songName = system::getStem(path);
		try {
			save(path);
		}
		catch (Exception& e) {
			osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, (std::string("Couldn't save the song: ") + e.what()).c_str());
		}
	}

	void loadSongDialog() {
		std::string path = fileDialog(OSDIALOG_OPEN, "", "THE GRID song (.vcvm):vcvm");
		if (path.empty())
			return;
		try {
			loadAction(path);
		}
		catch (Exception& e) {
			osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, (std::string("Couldn't load the song: ") + e.what()).c_str());
			return;
		}
		GridSequencer* m = getModule<GridSequencer>();
		if (m && m->songName.empty())
			m->songName = system::getStem(path);
	}

	void importMidiDialog() {
		GridSequencer* m = getModule<GridSequencer>();
		std::string path = fileDialog(OSDIALOG_OPEN, "", "MIDI file (.mid):mid,midi");
		if (path.empty() || !m)
			return;
		MidiFileData midi;
		std::string error;
		if (!readMidiFile(path, midi, error)) {
			osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Couldn't import the MIDI file. " + error).c_str());
			return;
		}
		// Undoable as a whole
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "import MIDI file";
		h->moduleId = m->id;
		h->oldModuleJ = toJson();
		std::string report = m->importMidi(midi, system::getStem(path));
		h->newModuleJ = toJson();
		APP->history->push(h);
		if (!report.empty())
			osdialog_message(OSDIALOG_INFO, OSDIALOG_OK, ("Imported, but not everything fitted:\n\n" + report).c_str());
	}

	void appendContextMenu(Menu* menu) override {
		GridSequencer* module = getModule<GridSequencer>();
		if (!module)
			return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Pattern"));
		menu->addChild(createMenuItem("Copy pattern", "", [=]() {
			module->copyPattern();
		}));
		menu->addChild(createMenuItem("Paste pattern", "", [=]() {
			module->patternEdit("paste pattern", module->viewPattern(), [=]() {
				module->pastePattern();
			});
		}, !clipboard.hasPattern));
		menu->addChild(createMenuItem("Clear pattern", "", [=]() {
			module->patternEdit("clear pattern", module->viewPattern(), [=]() {
				module->clearPattern();
			});
		}));

		menu->addChild(new MenuSeparator);
		int track = module->selectedTrack;
		int voices = module->voiceCount(track);
		if (voices > 1)
			menu->addChild(createMenuLabel(string::f("Track %d (poly, %d voices)", track + 1, voices)));
		else
			menu->addChild(createMenuLabel(string::f("Track %d", track + 1)));
		std::vector<std::string> voiceLabels;
		for (int v = 1; v <= MAX_VOICES; v++)
			voiceLabels.push_back(v == 1 ? "1 (mono)" : string::f("%d", v));
		menu->addChild(createIndexSubmenuItem("Poly voices (notes at once)", voiceLabels,
			[=]() { return (size_t) (module->voiceCount(track) - 1); },
			[=](size_t i) {
				trackSettingsEdit(module, "change poly voices", [=]() {
					module->setVoices(track, (int) i + 1);
				});
			}));
		if (voices > 1) {
			int last = std::min(track + voices - 1, NUM_TRACKS - 1);
			std::string jacks = (last > track) ? string::f("tracks %d-%d", track + 2, last + 1) : "no room";
			menu->addChild(createBoolMenuItem("Also send each voice to its own mono jacks", jacks,
				[=]() { return module->spreadVoices[track]; },
				[=](bool spread) {
					trackSettingsEdit(module, "send voices to mono jacks", [=]() {
						module->setSpreadVoices(track, spread);
					});
				},
				last <= track));
		}
		menu->addChild(createMenuItem("Copy track", "", [=]() {
			module->copyTrack();
		}));
		menu->addChild(createMenuItem("Paste track", "", [=]() {
			module->patternEdit("paste track", module->viewPattern(), [=]() {
				module->pasteTrack();
			});
		}, !clipboard.hasTrack));
		menu->addChild(createMenuItem("Clear track", "", [=]() {
			module->patternEdit("clear track", module->viewPattern(), [=]() {
				module->clearTrack();
			});
		}));
		menu->addChild(createMenuItem(string::f("Set every track in this pattern to %d steps", module->trackLength(module->viewPattern(), module->selectedTrack)), "", [=]() {
			int pattern = module->viewPattern();
			int length = module->trackLength(pattern, module->selectedTrack);
			module->patternEdit("set track lengths", pattern, [=]() {
				for (int t = 0; t < NUM_TRACKS; t++)
					module->setTrackLength(pattern, t, length);
			});
		}));
		auto transposeItem = [=](const char* label, int delta) {
			return createMenuItem(label, delta > 0 ? "Alt+wheel up" : "Alt+wheel down", [=]() {
				int pattern = module->viewPattern();
				module->patternEdit("transpose track", pattern, [=]() {
					module->transposeTrack(pattern, module->selectedTrack, delta);
				});
			});
		};
		menu->addChild(transposeItem("Transpose track up one degree", 1));
		menu->addChild(transposeItem("Transpose track down one degree", -1));
		auto rotateItem = [=](const char* label, int delta) {
			return createMenuItem(label, delta > 0 ? "Alt+Shift+wheel down" : "Alt+Shift+wheel up", [=]() {
				int pattern = module->viewPattern();
				module->patternEdit("rotate track", pattern, [=]() {
					module->rotateTrack(pattern, module->selectedTrack, delta);
				});
			});
		};
		menu->addChild(rotateItem("Rotate track one step later", 1));
		menu->addChild(rotateItem("Rotate track one step earlier", -1));
		std::vector<std::string> rangeLabels;
		for (const ModRange& r : MOD_RANGES)
			rangeLabels.push_back(r.name);
		menu->addChild(createIndexSubmenuItem("MOD mode", {"Per note", "Per step"},
			[=]() { return (size_t) module->modMode[track]; },
			[=](size_t i) {
				trackSettingsEdit(module, "change MOD mode", [=]() {
					module->modMode[track] = (int) i;
				});
			}));
		menu->addChild(createBoolMenuItem("Per-step MOD glides from step to step", "",
			[=]() { return module->modSmooth[track]; },
			[=](bool smooth) {
				trackSettingsEdit(module, "change MOD glide", [=]() {
					module->modSmooth[track] = smooth;
				});
			}));
		menu->addChild(createIndexSubmenuItem("MOD output range", rangeLabels,
			[=]() { return (size_t) module->modRange[track]; },
			[=](size_t i) {
				trackSettingsEdit(module, "change MOD range", [=]() {
					module->modRange[track] = (int) i;
				});
			}));
		menu->addChild(createMenuItem("Set every note's MOD value to the last one set", string::f("%d%%", module->modDefault[module->selectedTrack]), [=]() {
			int pattern = module->viewPattern();
			int track = module->selectedTrack;
			module->patternEdit("set MOD values", pattern, [=]() {
				for (int v = 0; v < module->voiceCount(track); v++)
					for (int i = 0; i < NUM_STEPS; i++)
						module->mods[pattern][laneOf(track, v)][i] = module->modDefault[track];
			});
		}));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Song"));
		SongNameField* nameField = new SongNameField;
		nameField->box.size.x = 220.f;
		nameField->placeholder = "Song name";
		nameField->start(module);
		menu->addChild(nameField);
		menu->addChild(createMenuItem("Load song...", "", [=]() {
			loadSongDialog();
		}));
		menu->addChild(createMenuItem("Save song...", "", [=]() {
			saveSongDialog();
		}));
		menu->addChild(createMenuItem("Import MIDI file...", "", [=]() {
			importMidiDialog();
		}));
		menu->addChild(createMenuItem("Clear song", "", [=]() {
			module->songEdit("clear song", [=]() {
				module->songClear();
			});
		}));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Sync"));
		menu->addChild(createBoolPtrMenuItem("RUN output: gate while running (instead of start/stop trigger)", "", &module->runOutputGate));
		menu->addChild(createBoolPtrMenuItem("RUN IN: gate, high = run (instead of toggle on trigger)", "", &module->runInputGate));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("View"));
		menu->addChild(createBoolPtrMenuItem("Follow playhead (show the steps that are playing)", "", &module->followPlayhead));
		menu->addChild(createMenuItem("Reset view to middle octave", "", [=]() {
			module->scrollRow = module->defaultScroll();
		}));
	}
};


Model* modelGridSequencer = createModel<GridSequencer, GridSequencerWidget>("GridSequencer");
