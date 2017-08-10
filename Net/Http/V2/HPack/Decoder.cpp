#include "Decoder.h"
#include "HPack.h"
#include <algorithm>
#include <sstream>

using namespace Net::Http::V2::HPack;

void Decoder::DecodeHeaderblock(std::vector<uint8_t>::const_iterator & pos, size_t length, std::vector<std::pair<std::string, std::string>>& headerlist)
{
	auto end = pos + length;
	while (pos != end)
	{
		if ((*pos & 0x80) != 0)
		{
			uint64_t index = DecodeInteger(pos, 7);
			if (index >= (62 + dynamictable.size()) || index == 0)
			{
				throw std::runtime_error("Compressionsfehler invalid index");
			}
			auto & el = index < 62 ? StaticTable[index - 1] : *(dynamictable.end() - (index - 61));
			headerlist.push_back(el);
		}
		else if ((*pos & 0x40) != 0)
		{
			uint64_t index = DecodeInteger(pos, 6);
			std::string key, value;
			if (index == 0)
			{
				key = DecodeString(pos);
			}
			else
			{
				if (index > (62 + dynamictable.size()) || index == 0)
				{
					throw std::runtime_error("Compressionsfehler invalid index");
				}
				key = (index < 62 ? HPack::StaticTable[index - 1] : *(dynamictable.end() - (index - 61))).first;
			}
			if (key.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			value = DecodeString(pos);
			if (value.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			headerlist.push_back({ key, value });
			dynamictable.push_back({ key, value });
		}
		else if ((*pos & 0x20) != 0)
		{
			uint64_t maxsize = DecodeInteger(pos, 5);
		}
		else if ((*pos & 0x10) != 0)
		{
			uint64_t index = DecodeInteger(pos, 4);
			std::string key, value;
			if (index == 0)
			{
				key = DecodeString(pos);
			}
			else
			{
				if (index > (62 + dynamictable.size()) || index == 0)
				{
					throw std::runtime_error("Compressionsfehler invalid index");
				}
				key = (index < 62 ? HPack::StaticTable[index - 1] : *(dynamictable.end() - (index - 61))).first;
			}
			if (key.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			value = DecodeString(pos);
			if (value.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			headerlist.push_back({ key, value });
		}
		else
		{
			uint64_t index = DecodeInteger(pos, 4);
			std::string key, value;
			if (index == 0)
			{
				key = DecodeString(pos);
			}
			else
			{
				if (index > (62 + dynamictable.size()) || index == 0)
				{
					throw std::runtime_error("Compressionsfehler invalid index");
				}
				key = (index < 62 ? HPack::StaticTable[index - 1] : *(dynamictable.end() - (index - 61))).first;
			}
			if (key.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			value = DecodeString(pos);
			if (value.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			headerlist.push_back({ key, value });
		}
		if (pos > end)
		{
			throw std::runtime_error("Compressionsfehler");
		}
	}
}

uint64_t Decoder::DecodeInteger(std::vector<uint8_t>::const_iterator & pos, uint8_t bits)
{
	uint8_t mask = (1 << bits) - 1;
	uint64_t number = *pos & mask;
	if (number == mask)
	{
		uint64_t pbits = 0;
		do
		{
			number += (*++pos & 127) << pbits;
			pbits += 7;
		} while ((*pos & 128) == 128);
	}
	++pos;
	return number;
}

std::string Decoder::DecodeHuffmanString(std::vector<uint8_t>::const_iterator & pos, long long length)
{
	std::ostringstream strs;
	long long  i = 0, blength = length << 3;
	uint64_t buffer;
	auto end = pos + length;
	while (true)
	{
		{
			uint32_t count = std::min<uint32_t>(5, (end - pos));
			std::reverse_copy(pos, pos + count, ((uint8_t*)&buffer) + (5 - count));
		}
		const std::pair<uint32_t, uint8_t> *res = std::find_if(StaticHuffmanTable, StaticHuffmanTable + 256, [buffer = (uint32_t)(buffer >> (8 - (i % 8)))](const std::pair<uint32_t, uint8_t> & entry) -> bool {
			return entry.first == (buffer >> (32 - entry.second));
		});
		pos += (i % 8 + res->second) >> 3;
		if ((i += res->second) > blength | res >= (StaticHuffmanTable + 255))
		{
			pos = end;
			return strs.str();
		}
		strs << (char)(res - StaticHuffmanTable);
	}
}

std::string Decoder::DecodeString(std::vector<uint8_t>::const_iterator & pos)
{
	bool huffmanencoding = (*pos & 0x80) == 0x80;
	uint64_t length = DecodeInteger(pos, 7);
	if (huffmanencoding)
	{
		return DecodeHuffmanString(pos, length);
	}
	else
	{
		std::string string(pos, pos + length);
		pos += length;
		return string;
	}
}
