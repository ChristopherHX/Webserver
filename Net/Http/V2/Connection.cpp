#include "Connection.h"
#include "Frame.h"
#include <algorithm>

using namespace Net::Http::V2;

void Connection::SendResponse(bool endstream)
{
	auto body = response.ToHttp2(encoder);
	Frame result;
	result.length = body.size();
	result.type = FrameType::HEADERS;
	result.flags = (FrameFlag)((uint8_t)FrameFlag::END_HEADERS | (endstream ? (uint8_t)FrameFlag::END_STREAM : 0));
	result.streamidentifier = frame.streamidentifier;
	socket->SendAll(result.ToArray());
	socket->SendAll(body.data(), body.size());
}

void Connection::SendData(const uint8_t* buffer, int length, bool endstream)
{
	Frame result;
	result.type = FrameType::DATA;
	result.flags = (FrameFlag)0;
	result.streamidentifier = frame.streamidentifier;
	if (length > 0)
	{
		for (int i = 0; i < length; i += result.length)
		{
			result.length = std::min(512, length - i);
			if (endstream && result.length < 512)
				result.flags = FrameFlag::END_STREAM;
			socket->SendAll(result.ToArray());
			socket->SendAll(buffer + i, result.length);
		}
	}
	else
	{
		if(endstream)
			result.flags = FrameFlag::END_STREAM;
		result.length = 0;
		socket->SendAll(result.ToArray());
	}
}

void Connection::SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata)
{
	stream->SetOnData(ondata);
}

void Net::Http::Connection::SendData(std::vector<uint8_t>& buffer, int length, bool endstream)
{
	return SendData(buffer.data(), length, endstream);
}
