#include "Connection.h"

using namespace Net::Http::V1;

void Connection::SendResponse(bool endstream)
{
	auto header = response.ToHttp1();
	socket->SendAll((uint8_t*)header.data(), header.length());
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