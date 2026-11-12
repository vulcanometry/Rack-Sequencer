#include "Common.hpp"


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

const std::vector<ModRange> MOD_RANGES = {
	{"0 to 10 V", 0.f, 10.f},
	{"0 to 5 V", 0.f, 5.f},
	{"-5 to +5 V", -5.f, 5.f},
};
