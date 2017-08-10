#pragma once
#include <cstdint>
#include <vector>

uint16_t GetUInt16(std::vector<uint8_t>::const_iterator & buffer);
uint32_t GetUInt31(std::vector<uint8_t>::const_iterator & buffer);
uint32_t GetUInt32(std::vector<uint8_t>::const_iterator & buffer);
void AddUInt31(uint32_t number, std::vector<uint8_t>::iterator & destination);
void AddUInt32(uint32_t number, std::vector<uint8_t>::iterator & destination);

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
			public:
				uint32_t length;
				FrameType type;
				FrameFlag flags;
				uint32_t streamidentifier;
				Frame();
				Frame(std::vector<uint8_t>::const_iterator & buffer);
				bool HasFlag(FrameFlag flag);
				std::vector<uint8_t> ToArray();
			};
		}
	}
}