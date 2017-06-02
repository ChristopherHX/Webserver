#include "Connection.h"

void Http1Connection::SendResponse(bool endstream)
{
	auto header = response.ToHttp1();
	socket->SendAll((uint8_t*)header.data(), header.length());
}

void Http1Connection::SendData(uint8_t * buffer, int length, bool endstream)
{
	socket->SendAll(buffer, length);
}

void Http1Connection::OnData(const uint8_t * buffer, uint32_t length)
{
	if (!_ondata)
		throw std::runtime_error("No Data Handler");
	_ondata(buffer, length);
}

void Http1Connection::SetOnData(std::function<void(const uint8_t*buffer, uint32_t length)> ondata)
{
	_ondata = ondata;
}

void Http2Connection::SendResponse(bool endstream)
{
	auto body = response.ToHttp2(*encoder);
	Frame result;
	result.length = body.size();
	result.type = FrameType::HEADERS;
	result.flags = (FrameFlag)((uint8_t)FrameFlag::END_HEADERS | (endstream ? (uint8_t)FrameFlag::END_STREAM : 0));
	result.streamidentifier = frame.streamidentifier;
	socket->SendAll(result.ToArray());
	socket->SendAll(body.data(), body.size());
}

void Http2Connection::SendData(uint8_t * buffer, int length, bool endstream)
{
	Frame result;
	result.length = length;
	result.type = FrameType::DATA;
	result.flags = endstream ? FrameFlag::END_STREAM : (FrameFlag)0;
	result.streamidentifier = frame.streamidentifier;
	socket->SendAll(result.ToArray());
	socket->SendAll(buffer, length);
}

void Http2Connection::SetOnData(std::function<void(const uint8_t*buffer, uint32_t length)> ondata)
{
	stream->SetOnData(ondata);
}
