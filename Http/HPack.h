#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace HPack
{
	extern const std::pair<std::string, std::string> StaticTable[61];
	extern const std::pair<uint32_t, uint8_t> StaticHuffmanTable[];
}