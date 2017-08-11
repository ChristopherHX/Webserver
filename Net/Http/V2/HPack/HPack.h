#pragma once
#include <cstdint>
#include <string>
#include <utility>

#ifndef IMPORT
#define IMPORT  __declspec( dllimport )
#endif

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			namespace HPack
			{
				IMPORT extern const std::pair<std::string, std::string> StaticTable[61];
				IMPORT extern const std::pair<uint32_t, uint8_t> StaticHuffmanTable[257];
			}
		}
	}
}