#include "GridSequencer.hpp"


GridSequencer::GridSequencer() {
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
	configSwitch(PROB_VIEW_PARAM, 0.f, 1.f, 0.f, "Grid shows", {"Notes", "Note probabilities"});
	// Set in the right-click menu: 50% = straight, 75% = every second step half a step late
	configParam(SWING_PARAM, 50.f, 75.f, 50.f, "Swing", "%");
	// (Not snapped: the menu slider moves in steps smaller than 1%)
	getParamQuantity(SWING_PARAM)->displayPrecision = 2;
	getParamQuantity(SWING_PARAM)->randomizeEnabled = false;
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
		configButton(TRACK_PARAM + t, string::f("Edit track %d (Shift+click: loop its pattern while the song moves on)", t + 1));
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


void GridSequencer::saveSettings(TrackSettings& t) {
	std::memcpy(t.voices, voices, sizeof(t.voices));
	std::memcpy(t.spreadVoices, spreadVoices, sizeof(t.spreadVoices));
	std::memcpy(t.modMode, modMode, sizeof(t.modMode));
	std::memcpy(t.modRange, modRange, sizeof(t.modRange));
	std::memcpy(t.pinPattern, pinPattern, sizeof(t.pinPattern));
	std::memcpy(t.modSmooth, modSmooth, sizeof(t.modSmooth));
	std::memcpy(t.clockRate, clockRate, sizeof(t.clockRate));
}


void GridSequencer::loadSettings(const TrackSettings& t) {
	std::memcpy(voices, t.voices, sizeof(voices));
	std::memcpy(spreadVoices, t.spreadVoices, sizeof(spreadVoices));
	std::memcpy(modMode, t.modMode, sizeof(modMode));
	std::memcpy(modRange, t.modRange, sizeof(modRange));
	std::memcpy(pinPattern, t.pinPattern, sizeof(pinPattern));
	std::memcpy(modSmooth, t.modSmooth, sizeof(modSmooth));
	std::memcpy(clockRate, t.clockRate, sizeof(clockRate));
	routeOutputs();
}


void GridSequencer::togglePin(int track) {
	if (pinPattern[track] >= 0)
		pinPattern[track] = -1;
	// Still looping on its way back to the song: keep it looping
	else if (looping(track))
		pinPattern[track] = loopPattern[track];
	else
		pinPattern[track] = following() ? playingPattern : editPattern();
}


void GridSequencer::syncLoops(bool boundary) {
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (pinPattern[t] >= 0) {
			// Newly pinned: the loop carries on from the step the track is on, so nothing jumps
			if (!looping(t))
				loopStep[t] = trackStep[t];
			loopPattern[t] = pinPattern[t];
		}
		else if (boundary) {
			loopPattern[t] = -1;
		}
	}
}


void GridSequencer::routeOutputs() {
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


float GridSequencer::rowToVoltage(int row) {
	const std::vector<int>& sc = SCALES[scaleIndex()].notes;
	int n = sc.size();
	int octave = row / n;
	int degree = row % n;
	float v = (octave - 4) + (rootIndex() + sc[degree]) / 12.f;
	return clamp(v, -10.f, 10.f);
}


void GridSequencer::clearAll() {
	std::memset(notes, -1, sizeof(notes));
	std::memset(lengths, 1, sizeof(lengths));
	std::memset(ties, 0, sizeof(ties));
	std::memset(mods, DEFAULT_MOD, sizeof(mods));
	std::memset(probs, PROB_MAX, sizeof(probs));
	std::memset(stepMods, DEFAULT_STEP_MOD, sizeof(stepMods));
	std::memset(trackLengths, DEFAULT_TRACK_LENGTH, sizeof(trackLengths));
	std::memset(song, 0, sizeof(song));
	for (int t = 0; t < NUM_TRACKS; t++) {
		clockRate[t] = DEFAULT_CLOCK_RATE;
		modRange[t] = 0;
		modDefault[t] = DEFAULT_MOD;
		modMode[t] = MOD_PER_NOTE;
		modSmooth[t] = true;
		voices[t] = 1;
		spreadVoices[t] = false;
		pinPattern[t] = -1;
		trackNames[t].clear();
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


void GridSequencer::resetPlayback() {
	step = -1;
	songPos = 0;
	patternJumpPending = false;
	phase = 1.0;
	stepTimer = 0.0;
	stepDuration = stepPeriod;
	clockCount = -1;
	swingWait = -1.0;
	playingPattern = playingSong() ? song[0] : editPattern();
	for (int t = 0; t < NUM_TRACKS; t++) {
		trackStep[t] = -1;
		loopPattern[t] = pinPattern[t];
		loopStep[t] = -1;
		trackTimer[t] = 0.0;
		trackDuration[t] = stepPeriod;
		nextSubStep[t] = 0;
	}
	for (int l = 0; l < NUM_LANES; l++)
		holdLeft[l] = 0;
}


bool GridSequencer::trackHasNotes(int pattern, int track) {
	if (isVoiceOutput(track))
		return false;
	int length = trackLength(pattern, track);
	for (int v = 0; v < voiceCount(track); v++)
		for (int s = 0; s < length; s++)
			if (notes[pattern][laneOf(track, v)][s] >= 0)
				return true;
	return false;
}


int GridSequencer::patternLength(int pattern) {
	int length = 0;
	// Looping tracks play their own loop, so they don't set how long the pattern lasts
	for (int t = 0; t < NUM_TRACKS; t++)
		if (!looping(t) && trackHasNotes(pattern, t))
			length = std::max(length, clockSteps(pattern, t));
	if (length == 0)
		for (int t = 0; t < NUM_TRACKS; t++)
			if (!isVoiceOutput(t))
				length = std::max(length, clockSteps(pattern, t));
	return std::max(length, 1);
}


void GridSequencer::stepForward(bool swung) {
	redo.clockCount = clockCount;
	std::memcpy(redo.loopStep, loopStep, sizeof(loopStep));
	std::memcpy(redo.nextSubStep, nextSubStep, sizeof(nextSubStep));
	std::memcpy(redo.holdLeft, holdLeft, sizeof(holdLeft));
	// A sped-up track takes any of its steps the clock step had no time left for (a faster
	// external clock), so a looping one stays on count
	if (step >= 0) {
		for (int t = 0; t < NUM_TRACKS; t++)
			while (nextSubStep[t] > 0 && nextSubStep[t] < rate(t).mul)
				trackStepForward(t, nextSubStep[t]++);
	}

	advance();
	stepTimer = 0.0;
	if (step < 0) {
		// A play-once song has ended
		for (int t = 0; t < NUM_TRACKS; t++) {
			trackStep[t] = -1;
			modNextStep[t] = -1;
			nextSubStep[t] = 0;
		}
		return;
	}
	clockCount++;
	// Swing: the step before a swung one lasts longer by as much as the swung one comes late,
	// and the swung one that much shorter
	double late = swingLate();
	stepDuration = stepPeriod * (swung ? 1.0 - late : (swingsNext() ? 1.0 + late : 1.0));
	// A pin takes hold straight away; a track let go of rejoins the song when a pattern starts
	syncLoops(step == 0);
	for (int t = 0; t < NUM_TRACKS; t++) {
		const ClockRate& r = rate(t);
		nextSubStep[t] = 1;
		// A slowed-down track steps on every div-th clock step: counted from the pattern's
		// start, or for a looping track (which plays on through patterns) from the reset
		long count = looping(t) ? clockCount : step;
		if (count % r.div == 0)
			trackStepForward(t, 0);
	}
}


void GridSequencer::undoStep() {
	step--;
	clockCount = redo.clockCount;
	std::memcpy(loopStep, redo.loopStep, sizeof(loopStep));
	std::memcpy(nextSubStep, redo.nextSubStep, sizeof(nextSubStep));
	std::memcpy(holdLeft, redo.holdLeft, sizeof(holdLeft));
}


void GridSequencer::trackStepForward(int t, int sub) {
	const ClockRate& r = rate(t);
	int pattern = trackPattern(t);
	int length = trackLength(pattern, t);
	int s;
	if (looping(t))
		s = loopStep[t] = (loopStep[t] + 1) % length;
	else
		s = (r.mul > 1 ? step * r.mul + sub : step / r.div) % length;
	trackStep[t] = s;
	trackTimer[t] = 0.0;
	trackDuration[t] = (r.mul > 1) ? stepDuration / r.mul : (r.div > 1 ? stepPeriod * r.div : stepDuration);
	// A new pattern starts every voice afresh, except on looping tracks, which play on through it
	bool restart = step == 0 && sub == 0 && !looping(t);
	for (int v = 0; v < MAX_VOICES; v++) {
		int l = laneOf(t, v);
		if (holdLeft[l] > 0 && !restart)
			holdLeft[l]--;
		else
			holdLeft[l] = 0;
		if (v >= voiceCount(t) || isVoiceOutput(t))
			continue;
		int row = notes[pattern][l][s];
		if (row < 0)
			continue;
		// The note may sit this time out
		int prob = probs[pattern][l][s];
		if (prob < PROB_MAX && random::uniform() * PROB_MAX >= prob) {
			holdLeft[l] = 0;
			continue;
		}
		// Notes end at the track's loop point
		holdLeft[l] = std::min((int) lengths[pattern][l][s], length - s);
		holdRow[l] = row;
		holdTie[l] = ties[pattern][l][s];
		holdMod[l] = mods[pattern][l][s];
	}
	findNextModStep(t, sub);
}


int GridSequencer::nextPattern() {
	if (inputs[PATTERN_INPUT].isConnected())
		return playingPattern;
	if (!playingSong())
		return editPattern();
	int pos = songPos + 1;
	if (pos >= songLength)
		pos = songOnce() ? -1 : 0;
	return (pos >= 0) ? song[pos] : -1;
}


void GridSequencer::findNextModStep(int t, int sub) {
	const ClockRate& r = rate(t);
	// The track's next step is the next of its steps within this clock step (sped up), or comes
	// with a later clock step: the next pattern's first, if this one ends before then
	bool boundary = false;
	if (r.mul == 1 || sub + 1 >= r.mul)
		boundary = step + r.div >= patternLength(playingPattern);
	int next = boundary ? nextPattern() : playingPattern;
	if (next < 0) {
		modNextPattern[t] = 0;
		modNextStep[t] = -1;
		return;
	}
	// A looping track goes on round its own loop, unless it rejoins the song at the next pattern
	if (looping(t) && (pinPattern[t] >= 0 || !boundary)) {
		modNextPattern[t] = loopPattern[t];
		modNextStep[t] = (loopStep[t] + 1) % trackLength(loopPattern[t], t);
		return;
	}
	modNextPattern[t] = next;
	modNextStep[t] = boundary ? 0 : (trackStep[t] + 1) % trackLength(next, t);
}


void GridSequencer::followPlayback() {
	int playStep = trackStep[selectedTrack];
	if (followPlayhead && running && step >= 0 && playStep >= 0 && trackPattern(selectedTrack) == viewPattern()
		&& (playStep < viewStart || playStep >= viewStart + VIEW_STEPS))
		setViewStart(playStep / VIEW_STEPS * VIEW_STEPS);
}


void GridSequencer::advance() {
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


void GridSequencer::process(const ProcessArgs& args) {
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
			undoStep();
			stepForward();
		}
	}

	// The MOD and PROB views both take the grid's place: lighting one turns the other off
	if (modView() && probView()) {
		if (prevProbView)
			params[PROB_VIEW_PARAM].setValue(0.f);
		else
			params[MOD_VIEW_PARAM].setValue(0.f);
	}
	prevProbView = probView();

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
	// While stopped there is no next pattern to wait for: pins and releases apply at once
	if (!running)
		syncLoops(true);

	// Clock
	bool tick = false;
	stepTimer += dt;
	for (int t = 0; t < NUM_TRACKS; t++)
		trackTimer[t] += dt;
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

	// Swing: every second step comes late. It starts when its time is up, or at the latest
	// with the next clock.
	if (!running)
		swingWait = -1.0;
	if (swingWait >= 0.0) {
		swingWait -= dt;
		if (swingWait < 0.0 || tick) {
			swingWait = -1.0;
			stepForward(true);
		}
	}
	if (tick) {
		if (swingsNext())
			swingWait = swingLate() * stepPeriod;
		else
			stepForward();
	}
	// Sped-up tracks take their further steps through the clock step
	if (running && step >= 0) {
		for (int t = 0; t < NUM_TRACKS; t++) {
			int mul = rate(t).mul;
			while (nextSubStep[t] > 0 && nextSubStep[t] < mul && stepTimer >= nextSubStep[t] * stepDuration / mul)
				trackStepForward(t, nextSubStep[t]++);
		}
	}

	// Outputs
	// Gate length: where each note releases within its last step. At 100% the gate stays high
	// into the next note, so notes that follow each other directly are tied (legato).
	float gateLength = std::round(params[GATE_LENGTH_PARAM].getValue()) / GATE_STEPS;
	for (int t = 0; t < NUM_TRACKS; t++) {
		// How far through its step each track is, for per-step MOD gliding (held while stopped)
		if (running && trackDuration[t] > 0.0)
			modPhase[t] = clamp((float) (trackTimer[t] / trackDuration[t]), 0.f, 1.f);
	}
	for (int t = 0; t < NUM_TRACKS; t++) {
		// Steps are as long as the track's clock rate makes them
		bool inGate = trackTimer[t] < gateLength * trackDuration[t] || (gateLength >= 1.f && running);
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
					float value = stepMods[trackPattern(t)][t][trackStep[t]];
					if (modSmooth[t] && modNextStep[t] >= 0) {
						float next = stepMods[modNextPattern[t]][t][modNextStep[t]];
						value += (next - value) * modPhase[t];
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
	// The clock output follows the steps as played, swing included
	bool clockHigh = running && step >= 0 && stepTimer < stepDuration * 0.5;
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
		lights[PROB_VIEW_LIGHT].setBrightness(probView());
		for (int t = 0; t < NUM_TRACKS; t++) {
			lights[TRACK_LIGHT + t].setBrightness(ownerTrack[t] == selectedTrack ? 1.f : 0.12f);
			lights[MUTE_LIGHT + t].setBrightness(muted[t]);
			lights[GATE_LIGHT + t].setBrightnessSmooth(trackGate[t], lightTime);
		}
	}
}
