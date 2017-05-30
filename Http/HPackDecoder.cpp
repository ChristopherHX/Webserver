#include "HPackDecoder.h"
#include "HPack.h"
#include <algorithm>
#include <sstream>

uint8_t * HPack::Decoder::DecodeHeaderblock(uint8_t * pos, uint8_t * end, std::vector<std::pair<std::string, std::string>>& headerlist)
{
	while (pos != end)
	{
		if ((*pos & 0x80) != 0)
		{
			uint64_t index;
			pos = DecodeInteger(pos, index, 7);
			if (index >= (62 + dynamictable.size()) || index == 0)
			{
				throw std::runtime_error("Compressionsfehler invalid index");
			}
			auto & el = index < 62 ? StaticTable[index - 1] : *(dynamictable.end() - (index - 61));
			headerlist.push_back(el);
		}
		else if ((*pos & 0x40) != 0)
		{
			uint64_t index;
			pos = DecodeInteger(pos, index, 6);
			std::string key, value;
			if (index == 0)
			{
				pos = DecodeString(pos, key);
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
			pos = DecodeString(pos, value);
			if (value.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			headerlist.push_back({ key, value });
			dynamictable.push_back({ key, value });
		}
		else if ((*pos & 0x20) != 0)
		{
			uint64_t maxsize;
			pos = DecodeInteger(pos, maxsize, 5);
		}
		else if ((*pos & 0x10) != 0)
		{
			uint64_t index;
			pos = DecodeInteger(pos, index, 4);
			std::string key, value;
			if (index == 0)
			{
				pos = DecodeString(pos, key);
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
			pos = DecodeString(pos, value);
			if (value.empty())
			{
				throw std::runtime_error("Compressionsfehler");
			}
			headerlist.push_back({ key, value });
		}
		else
		{
			uint64_t index;
			pos = DecodeInteger(pos, index, 4);
			std::string key, value;
			if (index == 0)
			{
				pos = DecodeString(pos, key);
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
			pos = DecodeString(pos, value);
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
	return pos;
}

uint8_t * HPack::Decoder::DecodeInteger(uint8_t * pos, uint64_t & integer, uint8_t bits)
{
	uint8_t mask = (1 << bits) - 1;
	if ((integer = *pos & mask) == mask)
	{
		uint64_t pbits = 0;
		do
		{
			integer += (*++pos & 127) << pbits;
			pbits += 7;
		} while ((*pos & 128) == 128);
	}
	return ++pos;
}

uint8_t * HPack::Decoder::DecodeHuffmanString(uint8_t * pos, std::string & string, long long length)
{
	std::ostringstream strs;
	long long  i = 0, blength = length << 3;
	uint64_t buffer;
	auto end = pos + length;
	while (true)
	{
		{
			uint8_t count = std::min<uint8_t>(5, (end - pos));
			std::reverse_copy(pos, pos + count, ((uint8_t*)&buffer) + (5 - count));
		}
		const std::pair<uint32_t, uint8_t> *res = std::find_if(StaticHuffmanTable, StaticHuffmanTable + 256, [buffer = (uint32_t)(buffer >> (8 - (i % 8)))](const std::pair<uint32_t, uint8_t> & entry) -> bool {
			return entry.first == (buffer >> (32 - entry.second));
		});
		pos += (i % 8 + res->second) >> 3;
		if ((i += res->second) > blength | res >= (StaticHuffmanTable + 255))
		{
			string = strs.str();
			return end;
		}
		strs << (char)(res - StaticHuffmanTable);
	}
}

uint8_t * HPack::Decoder::DecodeString(uint8_t * pos, std::string & string)
{
	uint64_t length;
	bool huffmanencoding = (*pos & 0x80) == 0x80;
	pos = DecodeInteger(pos, length, 7);
	if (huffmanencoding)
	{
		return DecodeHuffmanString(pos, string, length);
	}
	else
	{
		string = std::string(pos, pos + length);
		return pos += length;
	}
}
