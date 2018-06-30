#pragma once
#include <cstdint>
#include <utility>
#include <string_view>

#if !defined(IMPORT) && defined(_WIN32)
#define IMPORT  __declspec( dllimport )
#else
#define IMPORT
#endif

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			namespace HPack
			{
				IMPORT extern const std::pair<std::string_view, std::string_view> StaticTable[61];
				IMPORT extern const std::pair<uint32_t, uint8_t> StaticHuffmanTable[257];
			}
		}
	}
}
