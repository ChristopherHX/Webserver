#pragma once
#include <cstdint>
#include <vector>

uint32_t GetUInt31(const uint8_t * source);
void AddUInt31(uint32_t source, uint8_t * destination);

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			enum class FrameFlag : uint8_t
			{
				END_STREAM = 0x1,
				ACK = 0x1,
				END_HEADERS = 0x4,
				PADDED = 0x8,
				PRIORITY = 0x20
			};

			enum class FrameType : uint8_t
			{
				DATA = 0x0,
				HEADERS = 0x1,
				PRIORITY = 0x2,
				RST_STREAM = 0x3,
				SETTINGS = 0x4,
				PUSH_PROMISE = 0x5,
				PING = 0x6,
				GOAWAY = 0x7,
				WINDOW_UPDATE = 0x8,
				CONTINUATION = 0x9
			};

			struct Frame
			{
				uint32_t length;
				FrameType type;
				FrameFlag flags;
				uint32_t streamidentifier;
				bool HasFlag(FrameFlag flag);
				static Frame Parse(const uint8_t * source);
				std::vector<uint8_t> ToArray();
			};
		}
	}
}