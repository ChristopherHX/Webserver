#include "Connection.h"
#include "Frame.h"
#include <algorithm>

using namespace Net::Http::V2;

void Connection::SendResponse(bool endstream)
{
	std::vector<uint8_t> buffer(1 << 20);
	auto end = buffer.begin();
	response.EncodeHttp2(encoder, end);
	Frame result;
	result.length = end - buffer.begin();
	result.type = Frame::Type::HEADERS;
	result.flags = (Frame::Flag)((uint8_t)Frame::Flag::END_HEADERS | (endstream ? (uint8_t)Frame::Flag::END_STREAM : 0));
	result.stream = frame.stream;
	socket->SendAll(result.ToArray());
	socket->SendAll(buffer.data(), result.length);
}

void Connection::SendData(const uint8_t* buffer, int length, bool endstream)
{
	Frame result;
	result.type = Frame::Type::DATA;
	result.flags = (Frame::Flag)0;
	result.stream = frame.stream;
	if (length > 0)
	{
		for (int i = 0; i < length; i += result.length)
		{
			result.length = std::min(512, length - i);
			if (endstream && result.length < 512)
				result.flags = Frame::Flag::END_STREAM;
			socket->SendAll(result.ToArray());
			socket->SendAll(buffer + i, result.length);
		}
	}
	else
	{
		if(endstream)
			result.flags = Frame::Flag::END_STREAM;
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
