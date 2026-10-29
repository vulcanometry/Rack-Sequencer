#pragma once
#include <string>
#include <vector>


/** A note read from a Standard MIDI File, in ticks. */
struct MidiNote {
	// Index into MidiFileData::parts
	int part;
	int pitch;
	int velocity;
	long start;
	long end;
};

/** The notes of a Standard MIDI File (format 0 or 1), split into parts by track and channel. */
struct MidiFileData {
	// Ticks per quarter note
	int ppq = 96;
	// First tempo in the file (120 if it has none)
	double bpm = 120.0;
	// "Track name (ch N)" for each part, in the order the parts first appear
	std::vector<std::string> parts;
	std::vector<MidiNote> notes;
};

/** Reads a .mid file. Returns false and sets `error` if it can't. */
bool readMidiFile(const std::string& path, MidiFileData& data, std::string& error);
