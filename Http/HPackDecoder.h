#pragma once
#include <cstdint>
#include <vector>
#include <string>
namespace HPack
{
	class Decoder
	{
	private:
		std::vector<std::pair<std::string, std::string>> dynamictable;
	public:
		uint8_t * DecodeHeaderblock(uint8_t * pos, uint8_t * end, std::vector<std::pair<std::string, std::string>>& headerlist);

		static uint8_t * DecodeInteger(uint8_t * pos, uint64_t & integer, uint8_t bits);

		static uint8_t * DecodeHuffmanString(uint8_t * pos, std::string & string, long long length);

		static uint8_t * DecodeString(uint8_t * pos, std::string & string);
	};
}