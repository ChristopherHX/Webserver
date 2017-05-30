#include "HPackEncoder.h"
#include "HPack.h"
#include <algorithm>

void HPack::Encoder::AppendHeaderBlock(std::vector<uint8_t>& buffer, const std::vector<std::pair<std::string, std::string>>& headerlist)
{
	for (auto & entry : headerlist) {
		auto res = std::find(StaticTable, StaticTable + 61, entry);
		if (res != (StaticTable + 61))
		{
			AppendInteger(buffer, (res - StaticTable) + 1, 0x80, 7);
		}
		else
		{
			auto res = std::find(dynamictable.begin(), dynamictable.end(), entry);
			if (res != dynamictable.end())
			{
				AppendInteger(buffer, (dynamictable.begin() - res) + 62, 0x80, 7);
			}
			else
			{
				auto res = std::find_if(StaticTable, StaticTable + 61, [&key = entry.first](const std::pair<std::string, std::string> & pair){ return pair.first == key; });
				if (res != (StaticTable + 61))
				{
					AppendInteger(buffer, (res - StaticTable) + 1, 0x40, 6);
				}
				else
				{
					auto res = std::find_if(dynamictable.begin(), dynamictable.end(), [&key = entry.first](const std::pair<std::string, std::string> & pair){ return pair.first == key; });
					if (res != dynamictable.end())
					{
						AppendInteger(buffer, (dynamictable.begin() - res) + 62, 0x40, 6);
					}
					else
					{
						AppendInteger(buffer, 0, 0x40, 6);
						AppendString(buffer, entry.first);
					}
				}
				AppendString(buffer, entry.second);
			}
		}
	}
}

void HPack::Encoder::AppendInteger(std::vector<uint8_t>& buffer, uint64_t integer, uint8_t head, uint8_t bits)
{
	uint8_t mask = (1 << bits) - 1;
	if (integer < mask)
	{
		buffer.push_back(head | integer);
	}
	else
	{
		buffer.push_back(head | mask);
		integer -= mask;
		while (integer >= 128)
		{
			buffer.push_back((integer % 128) | 128);
			integer >>= 7;
		}
		buffer.push_back(integer);
	}
}

void HPack::Encoder::AppendString(std::vector<uint8_t>& buffer, const std::string & source)
{
	AppendInteger(buffer, source.length(), 0, 7);
	size_t insert = buffer.size();
	buffer.resize(insert + source.length());
	memcpy(buffer.data() + insert, source.data(), source.length());
}
