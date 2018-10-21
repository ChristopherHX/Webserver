#pragma once
#include <cstdint>
#include <string>
#include <math.h>
#include <unordered_map>
#include <memory>

#include "V2/HPack/Encoder.h"

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			namespace HPack
			{
				class Decoder;
			}
		}
	}
}

namespace Net
{
	namespace Http
	{
		class Header
		{
		public:
			Header() : contentlength(0) {

			}
			static std::hash<std::string> hash_fn;
			std::string scheme;
			std::string authority;
			std::string contenttype;
			uintmax_t contentlength;
			std::unordered_multimap<std::string, std::string> headerlist;
			void Add(const std::pair<std::string, std::string> & pair);
			virtual bool Add(size_t hash, const std::pair<std::string, std::string> & pair);
			virtual void EncodeHttp1(std::vector<uint8_t>::iterator & buffer) const;
			virtual void EncodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Encoder> encoder, std::vector<uint8_t>::iterator & buffer) const;
			virtual void DecodeHttp1(std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end);
			virtual void DecodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Decoder> decoder, std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end);
		};
	}
}

#include "V2/HPack/Decoder.h"