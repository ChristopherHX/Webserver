#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include "V2/HPack/Encoder.h"
#include "V2/HPack/Decoder.h"

namespace Net
{
	namespace Http
	{
		class Header
		{
		public:
			static std::hash<std::string> hash_fn;
			std::string scheme;
			std::string authority;
			std::string contenttype;
			uintmax_t contentlength;
			std::unordered_multimap<std::string, std::string> headerlist;
			void Add(const std::pair<std::string, std::string> & pair);
			virtual bool Add(size_t hash, const std::pair<std::string, std::string> & pair);
			virtual void EncodeHttp1(std::vector<uint8_t>::iterator & buffer);
			virtual void EncodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Encoder> &encoder, std::vector<uint8_t>::iterator & buffer);
			virtual void DecodeHttp1(std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end);
			virtual void DecodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Decoder> &decoder, std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end);
		};
	}
}