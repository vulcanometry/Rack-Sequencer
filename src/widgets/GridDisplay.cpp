#include "GridDisplay.hpp"
#include "Drawing.hpp"


bool GridDisplay::cellAt(Vec pos, int* stepIndex, int* row) {
	float x = pos.x - labelW();
	if (x < 0.f || x >= box.size.x - labelW() || pos.y < 0.f || pos.y >= box.size.y)
		return false;
	int col = (int) (x / cellW());
	*stepIndex = module->viewStart + col;
	int visibleRow = VISIBLE_ROWS - 1 - (int) (pos.y / cellH());
	*row = module->scrollRow + visibleRow;
	return col >= 0 && col < VIEW_STEPS && *row >= 0 && *row < module->numRows();
}


void GridDisplay::dragCellAt(Vec pos, int* stepIndex, int* row) {
	int col = (int) std::floor((pos.x - labelW()) / cellW());
	int end = module->trackLength(dragPattern, dragLead);
	*stepIndex = clamp(dragViewStart + col, 0, end - 1);
	int visibleRow = VISIBLE_ROWS - 1 - (int) std::floor(pos.y / cellH());
	visibleRow = clamp(visibleRow, 0, VISIBLE_ROWS - 1);
	*row = clamp(module->scrollRow + visibleRow, 0, module->numRows() - 1);
}


bool GridDisplay::placeMoved(int start, int row) {
	int track = dragTrack;
	if (poly()) {
		track = module->freeVoice(dragPattern, dragLead, start, dragLength, dragTrack);
		if (track < 0)
			return false;
	}
	module->placeNote(dragPattern, track, start, row, dragLength, dragTie, dragMod, dragProb);
	curTrack = track;
	curStart = start;
	curRow = row;
	return true;
}


void GridDisplay::shiftChord(int delta) {
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


void GridDisplay::onButton(const ButtonEvent& e) {
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
				dragProb = module->probs[dragPattern][track][s];
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
					dragProb = PROB_MAX;
					module->placeNote(dragPattern, dragTrack, col, row, 1, false, dragMod, dragProb);
				}
			}
			curTrack = dragTrack;
			curStart = dragStart;
			curRow = dragRow;
		}
	}
	e.consume(this);
}


void GridDisplay::onDragMove(const DragMoveEvent& e) {
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
		module->placeNote(dragPattern, dragTrack, dragStart, dragRow, length, dragTie, dragMod, dragProb);
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


void GridDisplay::onDragEnd(const DragEndEvent& e) {
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


void GridDisplay::onHoverScroll(const HoverScrollEvent& e) {
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


void GridDisplay::drawLayer(const DrawArgs& args, int layer) {
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
	if (module->step >= 0 && playStep >= viewStart && playStep < viewStart + VIEW_STEPS && module->trackPattern(track) == pattern) {
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
			// A note that doesn't always play is fainter, the less likely it is
			c.a *= 0.4f + 0.6f * module->probs[pattern][voice][s] / (float) PROB_MAX;
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
