#include "Encoder.h"
#include "HPack.h"
#include <algorithm>

using namespace Net::Http::V2::HPack;

void Encoder::AppendHeaderBlock(std::vector<uint8_t>& buffer, const std::vector<std::pair<std::string, std::string>>& headerlist)
{
	for (auto & entry : headerlist) {
		auto pred = [&entry](const std::pair<std::string, std::string> & pair) {
			return entry.first == pair.first;
		};
		auto tentry = std::find_if(StaticTable, StaticTable + 61, pred);
		if (tentry != (StaticTable + 61))
		{
			auto eentry = std::find(tentry, StaticTable + 61, entry);
			if (eentry != (StaticTable + 61))
			{
				AppendInteger(buffer, (eentry - StaticTable) + 1, 0x80, 7);
			}
			else
			{
				AppendInteger(buffer, (tentry - StaticTable) + 1, 0, 4);
				AppendString(buffer, entry.second);
			}
		}
		else
		{
			auto res = std::find_if(dynamictable.begin(), dynamictable.end(), pred);
			if (res != dynamictable.end())
			{
				auto eentry = std::find(res, dynamictable.end(), entry);
				if (eentry != dynamictable.end())
				{
					AppendInteger(buffer, (eentry - dynamictable.begin()) + 1, 0x80, 7);
				}
				else
				{
					AppendInteger(buffer, (res - dynamictable.begin()) + 62, 0, 4);
					AppendString(buffer, entry.second);
				}
			}
			else
			{
				AppendInteger(buffer, 0, 0x40, 6);
				AppendString(buffer, entry.first);
				AppendString(buffer, entry.second);
				dynamictable.push_front(entry);
			}
		}
	}
}

void Encoder::AppendInteger(std::vector<uint8_t>& buffer, uint64_t integer, uint8_t head, uint8_t bits)
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

void Encoder::AppendString(std::vector<uint8_t>& buffer, const std::string & source)
{
	AppendInteger(buffer, source.length(), 0, 7);
	size_t insert = buffer.size();
	buffer.resize(insert + source.length());
	memcpy(buffer.data() + insert, source.data(), source.length());
}
