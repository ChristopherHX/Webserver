#pragma once
#include "HeaderImpl.h"
#include <vector>
#include <cstdint>
#include <memory>

namespace Net
{
	namespace Http
	{
		namespace V1 {
			class ResponseImpl : public HeaderImpl
			{
			public:
				void Encode(const Header * header, std::vector<uint8_t>::iterator & buffer) const override;
			};
		}
	}
}