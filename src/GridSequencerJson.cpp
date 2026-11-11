#include "GridSequencer.hpp"


/** Hex string of 2 digits per value, as patches store note data. */
static std::string toHex(const uint8_t* values, int count, int maxValue) {
	std::string s;
	for (int i = 0; i < count; i++)
		s += string::f("%02x", clamp((int) values[i], 0, maxValue));
	return s;
}
/** Reads 2-digit hex values into `values` (up to NUM_STEPS); "--" entries are skipped. */
static void fromHex(const char* str, uint8_t* values, int minValue, int maxValue) {
	std::string s = str ? str : "";
	for (int i = 0; i < NUM_STEPS && (size_t) (i * 2 + 1) < s.size(); i++) {
		std::string hex = s.substr(i * 2, 2);
		if (hex != "--")
			values[i] = clamp((int) std::strtol(hex.c_str(), nullptr, 16), minValue, maxValue);
	}
}


json_t* GridSequencer::dataToJson() {
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


void GridSequencer::laneFromStrings(int p, int l, const char* rows, const char* lens, const char* tieStr, const char* modStr) {
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


void GridSequencer::dataFromJson(json_t* rootJ) {
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
