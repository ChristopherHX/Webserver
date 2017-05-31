#include "Connection.h"

void Http1Connection::SendResponse()
{
	auto header = response.ToHttp1();
	socket->SendAll((uint8_t*)header.data(), header.length());
}

void Http1Connection::SendData(uint8_t * buffer, int length, bool last)
{
	socket->SendAll(buffer, length);
}

void Http2Connection::SendResponse()
{
	auto body = response.ToHttp2(*encoder);
	Frame result;
	result.length = body.size();
	result.type = FrameType::HEADERS;
	result.flags = FrameFlag::END_HEADERS;
	result.streamidentifier = frame.streamidentifier;
	socket->SendAll(result.ToArray());
	socket->SendAll(body.data(), body.size());
}

void Http2Connection::SendData(uint8_t * buffer, int length, bool last)
{
	Frame result;
	result.length = length;
	result.type = FrameType::DATA;
	result.flags = last ? FrameFlag::END_STREAM : (FrameFlag)0;
	result.streamidentifier = frame.streamidentifier;
	socket->SendAll(result.ToArray());
	socket->SendAll(buffer, length);
}
