#pragma once
#include <cstdint>
#include <array>

uint32_t GetUInt31(const uint8_t* source);

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
private:
///*	union {
//		uint8_t a[4];
//		uint32_t b;
//	}*/uint32_t lt;
//	uint8_t flags;
//	uint32_t streamidentifier;
public:
	uint32_t length;
	FrameType type;
	FrameFlag flags;
	uint32_t streamidentifier;
	//void SetLength(uint32_t length);
	//uint32_t GetLength();
	//void SetType(FrameType type);
	//FrameType GetType();
	//void SetFlags(FrameFlag flags);
	//FrameFlag GetFlags();
	bool HasFlag(FrameFlag flag);
	//void SetStreamIdentifier(uint32_t identifier);
	//uint32_t GetStreamIdentifier();
	static Frame Parse(const uint8_t * source);
	std::array<uint8_t, 9> ToArray();
};