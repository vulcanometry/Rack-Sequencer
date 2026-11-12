#include "GridSequencer.hpp"
#include <cstring>


Clipboard clipboard;


int GridSequencer::noteStartCovering(int pattern, int lane, int col) {
	for (int s = col; s >= 0; s--) {
		if (notes[pattern][lane][s] >= 0)
			return (s + lengths[pattern][lane][s] > col) ? s : -1;
	}
	return -1;
}


void GridSequencer::placeNote(int pattern, int lane, int s, int row, int length, bool tie, int mod, int prob) {
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
	probs[pattern][lane][s] = clamp(prob, 0, PROB_MAX);
}


bool GridSequencer::spanFree(int pattern, int lane, int s, int length) {
	if (noteStartCovering(pattern, lane, s) >= 0)
		return false;
	for (int i = s + 1; i < std::min(s + length, NUM_STEPS); i++)
		if (notes[pattern][lane][i] >= 0)
			return false;
	return true;
}


int GridSequencer::freeLength(int pattern, int lane, int s) {
	int n = 1;
	while (s + n < NUM_STEPS && notes[pattern][lane][s + n] < 0)
		n++;
	return n;
}


int GridSequencer::freeVoice(int pattern, int track, int s, int length, int prefer) {
	int end = std::min(s + length, trackLength(pattern, track)) - s;
	if (prefer >= laneOf(track, 0) && prefer < laneOf(track, voiceCount(track)) && spanFree(pattern, prefer, s, end))
		return prefer;
	for (int v = 0; v < voiceCount(track); v++)
		if (spanFree(pattern, laneOf(track, v), s, end))
			return laneOf(track, v);
	return -1;
}


bool GridSequencer::noteAt(int pattern, int track, int step, int row, int* lane, int* start) {
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


void GridSequencer::normalizeLane(int pattern, int lane) {
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


bool GridSequencer::transposeTrack(int pattern, int track, int delta) {
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


void GridSequencer::rotateTrack(int pattern, int track, int delta) {
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


void GridSequencer::rotateLane(int pattern, int lane, int length, int delta) {
	int8_t* n = notes[pattern][lane];
	uint8_t* l = lengths[pattern][lane];
	uint8_t* ti = ties[pattern][lane];
	uint8_t* mo = mods[pattern][lane];
	uint8_t* pr = probs[pattern][lane];
	int8_t newNotes[NUM_STEPS];
	uint8_t newLengths[NUM_STEPS], newTies[NUM_STEPS], newMods[NUM_STEPS], newProbs[NUM_STEPS];
	std::memcpy(newNotes, n, sizeof(newNotes));
	std::memcpy(newLengths, l, sizeof(newLengths));
	std::memcpy(newTies, ti, sizeof(newTies));
	std::memcpy(newMods, mo, sizeof(newMods));
	std::memcpy(newProbs, pr, sizeof(newProbs));
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
		newProbs[d] = pr[s];
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
	std::memcpy(pr, newProbs, sizeof(newProbs));
}


void GridSequencer::euclideanFill(int pattern, int track, int hits) {
	int length = trackLength(pattern, track);
	hits = clamp(hits, 0, length);
	int row = -1;
	for (int s = 0; s < length && row < 0; s++)
		for (int v = 0; v < voiceCount(track) && row < 0; v++)
			if (notes[pattern][laneOf(track, v)][s] >= 0)
				row = notes[pattern][laneOf(track, v)][s];
	if (row < 0)
		row = clamp(4 * scaleSize(), 0, numRows() - 1);
	// Notes stored past the track's end are kept
	for (int v = 0; v < voiceCount(track); v++)
		for (int s = 0; s < length; s++)
			removeNote(pattern, laneOf(track, v), s);
	for (int s = 0; s < length; s++)
		if (s * hits % length < hits)
			placeNote(pattern, laneOf(track, 0), s, row, 1);
}


void GridSequencer::songInsert(int pattern, bool after) {
	if (songLength >= MAX_SONG_LENGTH)
		return;
	int at = after ? std::min(songCursor + 1, songLength) : songCursor;
	for (int i = songLength; i > at; i--)
		song[i] = song[i - 1];
	song[at] = pattern;
	songLength++;
	songCursor = at;
}


void GridSequencer::songDelete() {
	if (songLength <= 0)
		return;
	for (int i = songCursor; i < songLength - 1; i++)
		song[i] = song[i + 1];
	songLength--;
	songCursor = clamp(songCursor, 0, std::max(songLength - 1, 0));
}


void GridSequencer::songMove(int from, int to) {
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


void GridSequencer::copyPattern() {
	std::memcpy(clipboard.pattern, notes[viewPattern()], sizeof(clipboard.pattern));
	std::memcpy(clipboard.patternLengths, lengths[viewPattern()], sizeof(clipboard.patternLengths));
	std::memcpy(clipboard.patternTies, ties[viewPattern()], sizeof(clipboard.patternTies));
	std::memcpy(clipboard.patternMods, mods[viewPattern()], sizeof(clipboard.patternMods));
	std::memcpy(clipboard.patternProbs, probs[viewPattern()], sizeof(clipboard.patternProbs));
	std::memcpy(clipboard.patternStepMods, stepMods[viewPattern()], sizeof(clipboard.patternStepMods));
	std::memcpy(clipboard.patternTrackLengths, trackLengths[viewPattern()], sizeof(clipboard.patternTrackLengths));
	clipboard.hasPattern = true;
}


void GridSequencer::pastePattern() {
	if (!clipboard.hasPattern)
		return;
	std::memcpy(notes[viewPattern()], clipboard.pattern, sizeof(clipboard.pattern));
	std::memcpy(lengths[viewPattern()], clipboard.patternLengths, sizeof(clipboard.patternLengths));
	std::memcpy(ties[viewPattern()], clipboard.patternTies, sizeof(clipboard.patternTies));
	std::memcpy(mods[viewPattern()], clipboard.patternMods, sizeof(clipboard.patternMods));
	std::memcpy(probs[viewPattern()], clipboard.patternProbs, sizeof(clipboard.patternProbs));
	std::memcpy(stepMods[viewPattern()], clipboard.patternStepMods, sizeof(clipboard.patternStepMods));
	std::memcpy(trackLengths[viewPattern()], clipboard.patternTrackLengths, sizeof(clipboard.patternTrackLengths));
}


void GridSequencer::clearPattern() {
	std::memset(notes[viewPattern()], -1, sizeof(notes[0]));
	std::memset(lengths[viewPattern()], 1, sizeof(lengths[0]));
	std::memset(ties[viewPattern()], 0, sizeof(ties[0]));
	std::memset(stepMods[viewPattern()], DEFAULT_STEP_MOD, sizeof(stepMods[0]));
}


void GridSequencer::copyTrack() {
	int p = viewPattern(), t = selectedTrack;
	clipboard.trackVoices = voiceCount(t);
	for (int v = 0; v < clipboard.trackVoices; v++) {
		int l = laneOf(t, v);
		std::memcpy(clipboard.track[v], notes[p][l], sizeof(clipboard.track[v]));
		std::memcpy(clipboard.trackLengths[v], lengths[p][l], sizeof(clipboard.trackLengths[v]));
		std::memcpy(clipboard.trackTies[v], ties[p][l], sizeof(clipboard.trackTies[v]));
		std::memcpy(clipboard.trackMods[v], mods[p][l], sizeof(clipboard.trackMods[v]));
		std::memcpy(clipboard.trackProbs[v], probs[p][l], sizeof(clipboard.trackProbs[v]));
	}
	std::memcpy(clipboard.trackStepMods, stepMods[p][t], sizeof(clipboard.trackStepMods));
	clipboard.trackLength = trackLengths[p][t];
	clipboard.hasTrack = true;
}


void GridSequencer::pasteTrack() {
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
			std::memcpy(probs[p][l], clipboard.trackProbs[v], sizeof(probs[p][l]));
		}
		else {
			clearLane(p, l);
		}
	}
	std::memcpy(stepMods[p][t], clipboard.trackStepMods, sizeof(stepMods[p][t]));
	setTrackLength(p, t, clipboard.trackLength);
}


void GridSequencer::clearTrack() {
	int p = viewPattern(), t = selectedTrack;
	for (int v = 0; v < voiceCount(t); v++)
		clearLane(p, laneOf(t, v));
	std::memset(stepMods[p][t], DEFAULT_STEP_MOD, sizeof(stepMods[0][0]));
}


void GridSequencer::savePattern(int pattern, PatternSnapshot& snap) {
	std::memcpy(snap.notes, notes[pattern], sizeof(snap.notes));
	std::memcpy(snap.lengths, lengths[pattern], sizeof(snap.lengths));
	std::memcpy(snap.ties, ties[pattern], sizeof(snap.ties));
	std::memcpy(snap.mods, mods[pattern], sizeof(snap.mods));
	std::memcpy(snap.probs, probs[pattern], sizeof(snap.probs));
	std::memcpy(snap.stepMods, stepMods[pattern], sizeof(snap.stepMods));
	std::memcpy(snap.trackLengths, trackLengths[pattern], sizeof(snap.trackLengths));
}


void GridSequencer::loadPattern(int pattern, const PatternSnapshot& snap) {
	std::memcpy(notes[pattern], snap.notes, sizeof(snap.notes));
	std::memcpy(lengths[pattern], snap.lengths, sizeof(snap.lengths));
	std::memcpy(ties[pattern], snap.ties, sizeof(snap.ties));
	std::memcpy(mods[pattern], snap.mods, sizeof(snap.mods));
	std::memcpy(probs[pattern], snap.probs, sizeof(snap.probs));
	std::memcpy(stepMods[pattern], snap.stepMods, sizeof(snap.stepMods));
	std::memcpy(trackLengths[pattern], snap.trackLengths, sizeof(snap.trackLengths));
}


void GridSequencer::recordPatternEdit(const std::string& name, int pattern, const PatternSnapshot& before) {
	// Edits are large (two pattern snapshots), so they live on the heap until queued
	std::unique_ptr<Edit> edit(new Edit);
	edit->name = name;
	edit->pattern = pattern;
	edit->patternBefore = before;
	savePattern(pattern, edit->patternAfter);
	if (std::memcmp(&edit->patternBefore, &edit->patternAfter, sizeof(PatternSnapshot)) != 0)
		queueEdit(std::move(*edit));
}


void GridSequencer::recordSongEdit(const std::string& name, const SongSnapshot& before) {
	std::unique_ptr<Edit> edit(new Edit);
	edit->name = name;
	edit->isSong = true;
	edit->songBefore = before;
	saveSong(edit->songAfter);
	if (edit->songBefore.length != edit->songAfter.length
		|| std::memcmp(edit->songBefore.song, edit->songAfter.song, edit->songAfter.length) != 0)
		queueEdit(std::move(*edit));
}
