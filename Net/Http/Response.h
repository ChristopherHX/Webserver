#pragma once
#include "V2/HPack/Encoder.h"
#include "Header.h"
#include <sstream>
#include <string>
#include <vector>
#include <memory>

namespace Net
{
	namespace Http
	{
		class Response : public Header
		{
		public:
			int status;
			Response();
			bool Add(size_t hash, const std::pair<std::string, std::string> & pair) override;
			void EncodeHttp1(std::vector<uint8_t>::iterator & buffer) const override;
			void EncodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Encoder> encoder, std::vector<uint8_t>::iterator & buffer) const override;
		};
	}
}