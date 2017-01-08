#pragma once
#include "utility.h"
#include "Array.h"
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>

namespace Http2
{
	namespace HPack
	{
#ifdef _WIN32
		__declspec(dllimport)
#endif
		extern std::pair<uint32_t, uint8_t> StaticHuffmanTable[];
#ifdef _WIN32
		__declspec(dllimport) 
#endif
		extern std::pair<std::string, std::string> StaticTable[];

		class Encoder
		{
		private:
			std::vector<std::pair<std::string, std::string>> dynamictable;
		public:
			Encoder();
			void ClearDynamicTable();
			template<class _FwdIt>
			_FwdIt Headerblock(_FwdIt pos, const std::vector<std::pair<std::string, std::string>>& headerlist);
			template<class _FwdIt>
			static _FwdIt Integer(_FwdIt pos, uint64_t integer, uint8_t bits);
			static std::vector<uint8_t> Huffman(const std::string & string);
			template<class _FwdIt>
			static _FwdIt String(_FwdIt pos, const std::string & source);
			template<class _FwdIt>
			static _FwdIt StringH(_FwdIt pos, const std::string & source);
		};

		class Decoder
		{
		private:
			std::vector<std::pair<std::string, std::string>> dynamictable;
		public:
			Decoder();
			void ClearDynamicTable();
			template<class _FwdIt>
			_FwdIt Headerblock(_FwdIt pos, _FwdIt end, std::vector<std::pair<std::string, std::string>>& headerlist);
			template<class _FwdIt>
			static _FwdIt Integer(_FwdIt pos, uint64_t & integer, uint8_t bits);
			template<class _FwdIt>
			static _FwdIt Huffman(_FwdIt pos, std::string & string, long long length);
			template<class _FwdIt>
			static _FwdIt String(_FwdIt pos, std::string & string);
		};
		template<class _FwdIt>
		inline _FwdIt Encoder::Headerblock(_FwdIt pos, const std::vector<std::pair<std::string, std::string>>& headerlist)
		{
			for (auto & entry : headerlist) {
				auto res = std::find(StaticTable, StaticTable + 61, entry);
				if (res != (StaticTable + 61))
				{
					*pos = 0x80;
					pos = Integer(pos, (res - StaticTable) + 1, 7);
				}
				else
				{
					auto res = std::find(dynamictable.rbegin(), dynamictable.rend(), entry);
					if (res != dynamictable.rend())
					{
						*pos = 0x80;
						pos = Integer(pos, (res - dynamictable.rbegin()) + 62, 7);
					}
					else
					{
						*pos = 0x40;
						auto res = std::find_if(StaticTable, StaticTable + 61, [&key = entry.first](const std::pair<std::string, std::string> & pair){ return pair.first == key; });
						if (res != (StaticTable + 61))
						{
							pos = Integer(pos, (res - StaticTable) + 1, 6);
						}
						else
						{
							auto res = std::find_if(dynamictable.rbegin(), dynamictable.rend(), [&key = entry.first](const std::pair<std::string, std::string> & pair){ return pair.first == key; });
							if (res != dynamictable.rend())
							{
								pos = Integer(pos, (res - dynamictable.rbegin()) + 62, 6);
							}
							else
							{
								pos = Integer(pos, 0, 6);
								pos = StringH(pos, entry.first);
							}
						}
						pos = StringH(pos, entry.second);
					}
				}
			}
			return pos;
		}
		template<class _FwdIt>
		inline _FwdIt Encoder::Integer(_FwdIt pos, uint64_t integer, uint8_t bits)
		{
			uint8_t mask = (1 << bits) - 1;
			if (integer < mask)
			{
				*pos = (*pos & ~mask) | integer;
				++pos;
			}
			else
			{
				*pos++ |= mask;
				integer -= mask;
				while (integer >= 128)
				{
					*pos++ = (integer % 128) | 128;
					integer >>= 7;
				}
				*pos++ = integer;
			}
			return pos;
		}
		template<class _FwdIt>
		inline _FwdIt Encoder::String(_FwdIt pos, const std::string & source)
		{
			*pos = 0;
			return std::copy(source.begin(), source.end(), Integer(pos, source.length(), 7));
		}
		template<class _FwdIt>
		inline _FwdIt Encoder::StringH(_FwdIt pos, const std::string & source)
		{
			*pos = 0x80;
			std::vector<uint8_t> hstring = Huffman(source);
			return std::copy(hstring.begin(), hstring.end(), Integer(pos, hstring.size(), 7));
		}
		template<class _FwdIt>
		inline _FwdIt Decoder::Headerblock(_FwdIt pos, _FwdIt end, std::vector<std::pair<std::string, std::string>>& headerlist)
		{
			while (pos != end)
			{
				if ((*pos & 0x80) != 0)
				{
					uint64_t index;
					pos = Integer(pos, index, 7);
					if (index > (62 + dynamictable.size()) || index == 0)
					{
						throw std::runtime_error("Compressionsfehler invalid index");
					}
					auto & el = index < 62 ? StaticTable[index - 1] : *(dynamictable.rbegin() + (index - 62));
					headerlist.push_back(el);
				}
				else if ((*pos & 0x40) != 0)
				{
					uint64_t index;
					pos = Integer(pos, index, 6);
					std::string key, value;
					if (index == 0)
					{
						pos = String(pos, key);
					}
					else
					{
						if (index > (62 + dynamictable.size()) || index == 0)
						{
							throw std::runtime_error("Compressionsfehler invalid index");
						}
						key = (index < 62 ? HPack::StaticTable[index - 1] : *(dynamictable.rbegin() + (index - 62))).first;
					}
					if (key == "")
					{
						throw std::runtime_error("Compressionsfehler");
					}
					pos = String(pos, value);
					if (value == "")
					{
						throw std::runtime_error("Compressionsfehler");
					}
					headerlist.push_back({ key, value });
					dynamictable.push_back({ key, value });
				}
				else if ((*pos & 0x20) != 0)
				{
					uint64_t maxsize;
					pos = Integer(pos, maxsize, 5);
					//std::cout << "Compressions maxsize=" << maxsize << "\n";
				}
				else if ((*pos & 0x10) != 0)
				{
					uint64_t index;
					pos = Integer(pos, index, 4);
					std::string key, value;
					if (index == 0)
					{
						pos = String(pos, key);
					}
					else
					{
						if (index > (62 + dynamictable.size()) || index == 0)
						{
							throw std::runtime_error("Compressionsfehler invalid index");
						}
						key = (index < 62 ? HPack::StaticTable[index - 1] : *(dynamictable.rbegin() + (index - 62))).first;
					}
					if (key == "")
					{
						throw std::runtime_error("Compressionsfehler");
					}
					pos = String(pos, value);
					if (value == "")
					{
						throw std::runtime_error("Compressionsfehler");
					}
					headerlist.push_back({ key, value });
				}
				else
				{
					uint64_t index;
					pos = Integer(pos, index, 4);
					std::string key, value;
					if (index == 0)
					{
						pos = String(pos, key);
					}
					else
					{
						if (index > (62 + dynamictable.size()) || index == 0)
						{
							throw std::runtime_error("Compressionsfehler invalid index");
						}
						key = (index < 62 ? HPack::StaticTable[index - 1] : *(dynamictable.rbegin() + (index - 62))).first;
					}
					if (key == "")
					{
						throw std::runtime_error("Compressionsfehler");
					}
					pos = String(pos, value);
					if (value == "")
					{
						throw std::runtime_error("Compressionsfehler");
					}
					headerlist.push_back({ key, value });
				}
				if (pos > end)
				{
					throw std::runtime_error("Compressionsfehler");
					return end;
				}
			}
			return pos;
		}
		template<class _FwdIt>
		inline _FwdIt Decoder::Integer(_FwdIt pos, uint64_t & integer, uint8_t bits)
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
		template<class _FwdIt>
		inline _FwdIt Decoder::Huffman(_FwdIt pos, std::string & string, long long length)
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
				std::pair<uint32_t, uint8_t> *res = std::find_if(StaticHuffmanTable, StaticHuffmanTable + 256, [buffer = (uint32_t)(buffer >> (8 - (i % 8)))](const std::pair<uint32_t, uint8_t> & entry) -> bool {
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
		template<class _FwdIt>
		inline _FwdIt Decoder::String(_FwdIt pos, std::string & string)
		{
			uint64_t length;
			bool huffmanencoding = (*pos & 0x80) == 0x80;
			pos = Integer(pos, length, 7);
			if (huffmanencoding)
			{
				return Huffman(pos, string, length);
			}
			else
			{
				string = std::string(pos, pos + length);
				return pos += length;
			}
		}
	}
}
