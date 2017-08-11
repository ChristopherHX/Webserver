#include "Connection.h"

using namespace Net::Http::V1;

void Connection::SendResponse(bool endstream)
{
	std::vector<uint8_t> buffer(1 << 20);
	auto end = buffer.begin();
	response.EncodeHttp1(end);
	socket->SendAll(buffer.data(), end - buffer.begin());
}

void Connection::SendData(const uint8_t * buffer, int length, bool endstream)
{
	socket->SendAll(buffer, length);
}

void Connection::OnData(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)
{
	if (_ondata)
		_ondata(buffer, length);
}

void Connection::SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata)
{
	_ondata = ondata;
}