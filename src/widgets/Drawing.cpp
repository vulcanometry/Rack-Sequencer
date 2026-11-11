#include "Drawing.hpp"
#include "../plugin.hpp"


void setFont(NVGcontext* vg, const char* path) {
	std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system(path));
	if (font && font->handle >= 0)
		nvgFontFaceId(vg, font->handle);
}

void fillRect(NVGcontext* vg, float x, float y, float w, float h, NVGcolor color, float radius) {
	nvgBeginPath(vg);
	if (radius > 0.f)
		nvgRoundedRect(vg, x, y, w, h, radius);
	else
		nvgRect(vg, x, y, w, h);
	nvgFillColor(vg, color);
	nvgFill(vg);
}
