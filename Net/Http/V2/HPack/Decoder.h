#pragma once
#include "../../Request.h"
#include <cstdint>
#include <vector>
#include <string>

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			namespace HPack
			{
				class Decoder
				{
				private:
					std::vector<std::pair<std::string, std::string>> dynamictable;
				public:
					void DecodeHeaderblock(Net::Http::Request * request, std::vector<uint8_t>::const_iterator & pos, size_t length);

					static uint64_t DecodeInteger(std::vector<uint8_t>::const_iterator & pos, uint8_t bits);

					static std::string DecodeHuffmanString(std::vector<uint8_t>::const_iterator & pos, long long length);

					static std::string DecodeString(std::vector<uint8_t>::const_iterator & pos);
				};
			}
		}
	}
}