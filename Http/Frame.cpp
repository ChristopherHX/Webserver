#include "Frame.h"
#include "Socket.h"
#include <cstdio>
#include <cstdlib>

uint32_t GetUInt31(const uint8_t* source)
{
	return ((source[0] << 24) & 0x7f000000) | ((source[1] << 16) & 0x00ff0000) | ((source[2] << 8) & 0x0000ff00) | (source[3] & 0x000000ff);
}
void AddUInt31(uint32_t source, uint8_t* destination)
{
	destination[3] = source & 0xff;
	destination[2] = (source >> 8) & 0xff;
	destination[1] = (source >> 16) & 0xff;
	destination[0] = (source >> 24) & 0x7f;
}

//void Frame::SetLength(uint32_t length)
//{
//	lt &= 0xff000000;
//	lt |= htonl(length << 8) & 0xffffff;
//}
//
//uint32_t Frame::GetLength()
//{
//	return (ntohl(lt) >> 8) & 0xffffff;
//}
//
//void Frame::SetType(FrameType type)
//{
//	lt &= 0xffffff;
//	lt |= ((uint8_t)type << 24) & 0xff000000;
//}
//
//FrameType Frame::GetType()
//{
//	return (FrameType)((lt >> 24) & 0xff);
//}
//
//void Frame::SetFlags(FrameFlag flags)
//{
//	this->flags = (uint8_t)flags;
//}
//
//FrameFlag Frame::GetFlags()
//{
//	return (FrameFlag)flags;
//}
//
bool Frame::HasFlag(FrameFlag flag)
{
	return (uint8_t)flags & (uint8_t)flag;
}
//
//void Frame::SetStreamIdentifier(uint32_t identifier)
//{
//	streamidentifier = htonl(identifier >> 1) & 0x7fffffff;
//}
//
//uint32_t Frame::GetStreamIdentifier()
//{
//	return ntohl(streamidentifier) << 1;
//}

Frame Frame::Parse(const uint8_t * source)
{
	Frame frame;
	frame.length = ntohl((*(uint32_t*)(source) << 8) & 0xffffff00);
	frame.type = (FrameType)source[3];
	frame.flags = (FrameFlag)source[4];
	frame.streamidentifier = GetUInt31(source + 5);
	return frame;
}

std::array<uint8_t, 9> Frame::ToArray()
{
	std::array<uint8_t, 9> frame;
	auto data = frame.data();
	*(uint32_t*)(data) = (htonl(length) >> 8) & 0x00ffffff;
	data[3] = (uint8_t)type;
	data[4] = (uint8_t)flags;
	AddUInt31(streamidentifier, data + 5);
	return frame;
}
