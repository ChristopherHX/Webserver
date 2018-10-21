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