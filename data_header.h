#pragma once
#include <cstdint>

struct data_header_t {
	uint16_t framerate;
	uint16_t width, height;
	uint16_t frameCount;
};