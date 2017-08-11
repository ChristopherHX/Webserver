#include "Encoder.h"


using namespace Net::Http::V2::HPack;

void Encoder::AddHeaderBlock(std::vector<uint8_t>::iterator & buffer, const std::vector<std::pair<std::string, std::string>>& headerlist)
{
	std::hash<std::string> hash_fn;
	for (auto & entry : headerlist) {
		auto pred = [&hash_fn, hash = hash_fn(entry.first)](const std::pair<std::string, std::string> & pair) {
			return hash == hash_fn(pair.first);
		};
		auto tentry = std::find_if(StaticTable, StaticTable + 61, pred);
		if (tentry != (StaticTable + 61))
		{
			auto eentry = std::find(tentry, StaticTable + 61, entry);
			if (eentry != (StaticTable + 61))
			{
				AddInteger(buffer, (eentry - StaticTable) + 1, 0x80, 7);
			}
			else
			{
				AddInteger(buffer, (tentry - StaticTable) + 1, 0, 4);
				AddString(buffer, entry.second);
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
					AddInteger(buffer, (eentry - dynamictable.begin()) + 1, 0x80, 7);
				}
				else
				{
					AddInteger(buffer, (res - dynamictable.begin()) + 62, 0, 4);
					AddString(buffer, entry.second);
				}
			}
			else
			{
				AddInteger(buffer, 0, 0x40, 6);
				AddString(buffer, entry.first);
				AddString(buffer, entry.second);
				dynamictable.push_front(entry);
			}
		}
	}
}

void Encoder::AddInteger(std::vector<uint8_t>::iterator &buffer, uint64_t integer, uint8_t head, uint8_t bits)
{
	*buffer = head;
	uint8_t mask = (1 << bits) - 1;
	if (integer < mask)
	{
		*buffer++ |= integer;
	}
	else
	{
		*buffer++ |= mask;
		integer -= mask;
		while (integer >= 0x80)
		{
			*buffer++ = (integer % 0x80) | 0x80;
			integer >>= 7;
		}
		*buffer++ = integer;
	}
}

void Encoder::AddString(std::vector<uint8_t>::iterator &buffer, const std::string & source)
{
	AddInteger(buffer, source.length(), 0, 7);
	buffer = std::copy(source.begin(), source.end(), buffer);
}
