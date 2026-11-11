#pragma once
#include <rack.hpp>

using namespace rack;


void setFont(NVGcontext* vg, const char* path);
void fillRect(NVGcontext* vg, float x, float y, float w, float h, NVGcolor color, float radius = 0.f);
