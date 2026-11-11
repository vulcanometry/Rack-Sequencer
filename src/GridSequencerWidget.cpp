#include "GridSequencer.hpp"
#include "History.hpp"
#include "widgets/Controls.hpp"
#include "widgets/InfoDisplay.hpp"
#include "widgets/GridDisplay.hpp"
#include "widgets/ModDisplay.hpp"
#include "widgets/TrackIndicators.hpp"
#include "widgets/RulerDisplay.hpp"
#include "widgets/ScrollerDisplay.hpp"
#include "widgets/SongDisplay.hpp"
#include <osdialog.h>


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
