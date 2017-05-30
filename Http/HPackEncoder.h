#pragma once
#include <vector>
#include <string>

namespace HPack
{
	class Encoder
	{
	private:
		std::vector<std::pair<std::string, std::string>> dynamictable;
	public:
		void AppendHeaderBlock(std::vector<uint8_t> &buffer, const std::vector<std::pair<std::string, std::string>>& headerlist);

		void AppendInteger(std::vector<uint8_t> &buffer, uint64_t integer, uint8_t head, uint8_t bits);

		void AppendString(std::vector<uint8_t> &buffer, const std::string & source);
	};
}