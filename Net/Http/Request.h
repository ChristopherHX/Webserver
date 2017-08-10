#pragma once
#include "../Socket.h"
#include "V2/HPack/Decoder.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace Net
{
	namespace Http
	{
		class Request
		{
		public:
			static Request ParseHttp1(const uint8_t * buffer, int length);
			void ParseUri(const std::string &path);
			void DecodeHeaderblock(std::shared_ptr<V2::HPack::Decoder> & decoder, std::vector<uint8_t>::const_iterator & buffer, int length);
			std::string method;
			std::string uri;
			std::string path;
			std::string query;
			std::string contenttype;
			uintmax_t contentlength;
			std::string scheme;
			std::string authority;
			std::unordered_map<std::string, std::string> headerlist;
		};
	}
}