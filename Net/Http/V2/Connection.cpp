#include "Connection.h"
#include "Frame.h"

using namespace Net::Http::V2;

void Connection::SendResponse(bool endstream)
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

void Connection::SendData(uint8_t * buffer, int length, bool endstream)
{
	Frame result;
	result.length = length;
	result.type = FrameType::DATA;
	result.flags = endstream ? FrameFlag::END_STREAM : (FrameFlag)0;
	result.streamidentifier = frame.streamidentifier;
	socket->SendAll(result.ToArray());
	socket->SendAll(buffer, length);
}

void Connection::SetOnData(std::function<void(const uint8_t*buffer, uint32_t length)> ondata)
{
	stream->SetOnData(ondata);
}