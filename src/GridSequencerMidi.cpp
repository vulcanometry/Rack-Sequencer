#include "GridSequencer.hpp"


std::string GridSequencer::importMidi(const MidiFileData& midi, const std::string& name) {
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
