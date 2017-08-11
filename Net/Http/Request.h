#pragma once
#include "../Socket.h"
#include "Header.h"	
#include "V2/HPack/Decoder.h"
#include "Header.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace Net
{
	namespace Http
	{
		class Request : public Header
		{
		public:
			void ParseUri(const std::string &path);
			std::string method;
			std::string uri;
			std::string path;
			std::string query;
			bool Add(size_t hash, const std::pair<std::string, std::string> & pair) override;
			void DecodeHttp1(std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end) override;
		};
	}
}