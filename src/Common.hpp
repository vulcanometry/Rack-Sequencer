#pragma once
#include <rack.hpp>

using namespace rack;


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

extern const std::vector<ScaleDef> SCALES;

// Gate length is set in 5% steps: GATE_LENGTH_PARAM 1..GATE_STEPS
const int GATE_STEPS = 20;

extern const char* const NOTE_NAMES[12];

extern const NVGcolor TRACK_COLORS[NUM_TRACKS];

const char* const DISPLAY_FONT = "res/fonts/ShareTechMono-Regular.ttf";
const char* const LABEL_FONT = "res/fonts/DejaVuSans.ttf";

/** Voltage range of a track's MOD output; a note's MOD value (0-100%) is scaled into it. */
struct ModRange {
	const char* name;
	float min, max;
};

extern const std::vector<ModRange> MOD_RANGES;

const int MOD_MAX = 100;
const int DEFAULT_MOD = 100;
const int DEFAULT_STEP_MOD = 0;

// MOD modes: one value per note (held while it plays), or one value for every step
enum ModMode { MOD_PER_NOTE, MOD_PER_STEP };

// Chance (0..PROB_MAX %) that a note plays each time its step comes round
const int PROB_MAX = 100;

/** How fast a track steps: once every `div` sequencer steps, or `mul` times per step. */
struct ClockRate {
	const char* name;
	int div, mul;
};

extern const std::vector<ClockRate> CLOCK_RATES;

// Index of x1 in CLOCK_RATES
const int DEFAULT_CLOCK_RATE = 4;

// Every track has MAX_VOICES note lanes. An ordinary track plays lane 0 only; a poly track of N
// voices plays lanes 0..N-1 together and sends them as an N-channel poly cable.
const int NUM_LANES = NUM_TRACKS * MAX_VOICES;

inline int laneOf(int track, int voice) {
	return track * MAX_VOICES + voice;
}
inline int trackOfLane(int lane) {
	return lane / MAX_VOICES;
}
