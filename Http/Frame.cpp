#include "Frame.h"
#include "Socket.h"
#include <cstdio>
#include <cstdlib>

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
	frame.streamidentifier = ntohl((*(uint32_t*)(source + 5) << 1) & 0xfffffffe);
	return frame;
}

std::array<uint8_t, 9> Frame::ToArray()
{
	std::array<uint8_t, 9> frame;
	auto data = frame.data();
	*(uint32_t*)(data) = (htonl(length) >> 8) & 0x00ffffff;
	data[3] = (uint8_t)type;
	data[4] = (uint8_t)flags;
	*(uint32_t*)(data + 5) = (htonl(streamidentifier) >> 1) & 0x7fffffff;
	return frame;
}
