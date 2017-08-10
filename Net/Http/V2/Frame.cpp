#include "Frame.h"
#include "../../Socket.h"
#include <cstdio>
#include <cstdlib>

using namespace Net::Http::V2;

uint16_t GetUInt16(std::vector<uint8_t>::const_iterator & buffer)
{
	uint16_t number = buffer[0] << 8 | buffer[1];
	buffer += 2;
	return number;
}

uint32_t GetUInt24(std::vector<uint8_t>::const_iterator & buffer)
{
	uint32_t number = buffer[0] << 16 | buffer[1] << 8 | buffer[2];
	buffer += 3;
	return number;
}

uint32_t GetUInt32(std::vector<uint8_t>::const_iterator & buffer)
{
	uint32_t number = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
	buffer += 4;
	return number;
}

uint32_t GetUInt31(std::vector<uint8_t>::const_iterator & buffer)
{
	return GetUInt32(buffer) & 0x7fffffff;
}

void AddUInt24(uint32_t number, std::vector<uint8_t>::iterator & destination)
{
	*destination++ = number >> 16;
	*destination++ = number >> 8;
	*destination++ = number;
}

void AddUInt32(uint32_t number, std::vector<uint8_t>::iterator & destination)
{
	*destination++ = number >> 24;
	*destination++ = number >> 16;
	*destination++ = number >> 8;
	*destination++ = number;
}

void AddUInt31(uint32_t number, std::vector<uint8_t>::iterator & destination)
{
	AddUInt32(number, destination);
	*(destination - 4) &= 0x7f;
}

Net::Http::V2::Frame::Frame()
{
}

Frame::Frame(std::vector<uint8_t>::const_iterator & buffer)
{
	length = GetUInt24(buffer);
	type = (FrameType)*buffer++;
	flags = (FrameFlag)*buffer++;
	streamidentifier = GetUInt31(buffer);
}

bool Frame::HasFlag(FrameFlag flag)
{
	return (uint8_t)flags & (uint8_t)flag;
}

std::vector<uint8_t> Frame::ToArray()
{
	std::vector<uint8_t> frame(9);
	auto iter = frame.begin();
	AddUInt24(length, iter);
	*iter++ = (uint8_t)type;
	*iter++ = (uint8_t)flags;
	AddUInt31(streamidentifier, iter);
	return frame;
}