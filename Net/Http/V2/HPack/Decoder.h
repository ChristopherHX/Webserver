#pragma once
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
					const uint8_t * DecodeHeaderblock(const uint8_t * pos, const uint8_t * end, std::vector<std::pair<std::string, std::string>>& headerlist);

					static const uint8_t * DecodeInteger(const uint8_t * pos, uint64_t & integer, uint8_t bits);

					static const uint8_t * DecodeHuffmanString(const uint8_t * pos, std::string & string, long long length);

					static const uint8_t * DecodeString(const uint8_t * pos, std::string & string);
				};
			}
		}
	}
}