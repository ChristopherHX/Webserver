#pragma once
#include <cstdint>

enum class Setting : uint16_t
{
	HEADER_TABLE_SIZE = 0x1,
	ENABLE_PUSH = 0x2,
	MAX_CONCURRENT_STREAMS = 0x3,
	INITIAL_WINDOW_SIZE = 0x4,
	MAX_FRAME_SIZE = 0x5,
	MAX_HEADER_LIST_SIZE = 0x6
};