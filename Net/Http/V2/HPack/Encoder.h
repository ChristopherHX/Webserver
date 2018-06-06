#pragma once
#include "HPack.h"
#include <algorithm>
#include <vector>
#include <deque>
#include <string>
#include <utility>

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			namespace HPack
			{
				class Encoder
				{
				private:
					std::deque<std::pair<std::string, std::string>> dynamictable;
				public:
					template<class T1, class T2> void AddHeaderBlock(T1 & buffer, const T2& headerlist)
					{
						std::hash<std::string> hash_fn;
						for (const std::pair<std::string, std::string> & entry : headerlist) {
							auto pred = [&hash_fn, hash = hash_fn(entry.first)](const std::pair<std::string, std::string> & pair) {
								return hash == hash_fn(pair.first);
							};
							auto tentry = std::find_if(StaticTable, std::end(StaticTable), pred);
							if (tentry != std::end(StaticTable))
							{
								auto eentry = std::find(tentry, std::end(StaticTable), entry);
								if (eentry != std::end(StaticTable))
								{
									AddInteger(buffer, (eentry - StaticTable) + 1, 0x80, 7);
								}
								else
								{
									AddInteger(buffer, (tentry - StaticTable) + 1, 0, 4);
									AddString(buffer, entry.second.begin(), entry.second.end());
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
										AddString(buffer, entry.second.begin(), entry.second.end());
									}
								}
								else
								{
									AddInteger(buffer, 0, 0x40, 6);
									AddString(buffer, entry.first.begin(), entry.first.end());
									AddString(buffer, entry.second.begin(), entry.second.end());
									dynamictable.push_front(entry);
								}
							}
						}
					}

					template<class T> void AddInteger(T &buffer, uint64_t integer, uint8_t head, uint8_t bits)
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
								*buffer++ = (uint8_t)((integer % 0x80) | 0x80);
								integer >>= 7;
							}
							*buffer++ = (uint8_t)integer;
						}
					}

					template<class T1, class T2> void AddString(T1 &buffer, const T2 & beg, const T2 & end)
					{
						AddInteger(buffer, end - beg, 0, 7);
						buffer = std::copy(beg, end, buffer);
					}
				};
			}
		}
	}
}