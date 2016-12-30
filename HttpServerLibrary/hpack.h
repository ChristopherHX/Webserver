#pragma once
#include "utility.h"
#include "Array.h"

#include <string>

namespace Http2
{
	namespace HPack
	{
		template<class T1, class T2>
		bool KeySearch(const std::pair<T1, T2> &entry, const T1 & first)
		{
			return entry.first == first;
		}

		template<class T1, class T2>
		bool ValueSearch(const std::pair<T1, T2> &entry, const T2 & second)
		{
			return entry.second == second;
		}

		extern std::pair<uint32_t, uint8_t> StaticHuffmanTable[];
		extern std::pair<std::string, std::string> StaticTable[];

		//class Coder
		//{
		//protected:
		//	Utility::Array<std::pair<std::string, std::string>> dynamictable;
		//	Utility::RotateIterator<std::pair<std::string, std::string>> dynamictableend;
		//public:
		//	Coder();
		//	void ClearDynamicTable();
		//};

		class Encoder //: public Coder
		{
		private:
			Utility::Array<std::pair<std::string, std::string>> dynamictable;
			Utility::RotateIterator<std::pair<std::string, std::string>> dynamictableend;
		public:
			Encoder();
			void ClearDynamicTable();
			Utility::RotateIterator<uint8_t> Headerblock(Utility::RotateIterator<uint8_t> pos, Utility::RotateIterator<std::pair<std::string, std::string>> headerlistbegin, Utility::RotateIterator<std::pair<std::string, std::string>> headerlistend);
			static Utility::RotateIterator<uint8_t> Integer(Utility::RotateIterator<uint8_t> pos, uint64_t integer, uint8_t bits);
			static Utility::RotateIterator<uint8_t> Huffman(Utility::RotateIterator<uint8_t> pos, const std::string & string);
			static Utility::RotateIterator<uint8_t> String(Utility::RotateIterator<uint8_t> pos, const std::string & source);
			static Utility::RotateIterator<uint8_t> StringH(Utility::RotateIterator<uint8_t> pos, const std::string & source);
		};

		class Decoder //: public Coder
		{
		private:
			Utility::Array<std::pair<std::string, std::string>> dynamictable;
			Utility::RotateIterator<std::pair<std::string, std::string>> dynamictableend;
		public:
			Decoder();
			void ClearDynamicTable();
			Utility::RotateIterator<uint8_t> Headerblock(Utility::RotateIterator<uint8_t> pos, Utility::RotateIterator<uint8_t> end, Utility::RotateIterator<std::pair<std::string, std::string>> &headerlistend);
			static Utility::RotateIterator<uint8_t> Integer(Utility::RotateIterator<uint8_t> pos, uint64_t & integer, uint8_t bits);
			static Utility::RotateIterator<uint8_t> Huffman(Utility::RotateIterator<uint8_t> pos, std::string & string, long long length);
			static Utility::RotateIterator<uint8_t> String(Utility::RotateIterator<uint8_t> pos, std::string & string);
		};
	}
}