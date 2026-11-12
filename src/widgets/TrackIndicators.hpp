#pragma once
#include "../GridSequencer.hpp"


/**
 * Bar beside the track numbers joining a poly track to the tracks whose jacks send its other
 * voices, in its colour.
 */
struct GroupBrackets : TransparentWidget {
	GridSequencer* module = nullptr;
	// Track rows: y of the first one and the spacing, in mm
	float firstY = 0.f, pitch = 0.f;

	void draw(const DrawArgs& args) override;
};


/**
 * Under a track's number: the pattern it loops ("P03", on a tab of the track colour; blinking
 * while it waits for the next pattern to rejoin the song), or else for a poly track how many notes
 * it plays at once ("x3"). The voice count is left out when the track sends its voices to other
 * tracks' jacks: the bar beside the numbers shows that group instead.
 */
struct TrackBadge : TransparentWidget {
	GridSequencer* module = nullptr;
	int track = 0;

	void draw(const DrawArgs& args) override;
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

	void ring(NVGcontext* vg, float inner, float outer, NVGcolor c);

	void draw(const DrawArgs& args) override;

	void drawLayer(const DrawArgs& args, int layer) override;
};
